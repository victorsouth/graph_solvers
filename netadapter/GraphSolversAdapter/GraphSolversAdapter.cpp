// Определение макроса для экспорта функций из DLL
#ifdef _WIN32
    #ifdef GRAPHSOLVERSADAPTER_EXPORTS
        #define GRAPHSOLVERSADAPTER_API __declspec(dllexport)
    #else
        #define GRAPHSOLVERSADAPTER_API __declspec(dllimport)
    #endif
#else
    #define GRAPHSOLVERSADAPTER_API
#endif

// Подключаем напрямую необходимые заголовки, не используя graph_solvers.h
#include "oil_transport.h"
#include <memory>
#include <string>
#include <cstring>
#include <vector>

/// @brief Структура для хранения модели
struct GraphSolversModel {
    /// @brief Настройки расчетной задачи
    oil_transport::hydro_transport_task_settings settings;

    /// @brief Данные графа для однофазного потока (SISO - Single Input Single Output)
    oil_transport::graph_siso_data siso_data;
    /// @brief Сопоставление "привязка - имя ребра"
    std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash> edge_id_to_name;
    /// @brief Экземпляр расчетной задачи
    std::unique_ptr<oil_transport::hydro_transport_task_t> task;

    /// @brief Временное хранилище для строки состояния, возвращаемой из GetState
    std::string stateString;
    /// @brief Временное хранилище для строки результата, возвращаемой из GetResult
    std::string result_buffer;
    GraphSolversModel() {
        // Заглушка конструктора
        stateString = "{}";
        result_buffer = "{}";
    }
    ~GraphSolversModel() {
        // Заглушка деструктора
    }
};

using GraphSolversModelHandle = void*;


extern "C" {

GRAPHSOLVERSADAPTER_API GraphSolversModelHandle createModel() {
    try {
        auto* model = new GraphSolversModel();
        return reinterpret_cast<GraphSolversModelHandle>(model);
    } catch (...) {
        return nullptr;
    }
}

GRAPHSOLVERSADAPTER_API void deleteModel(GraphSolversModelHandle model) {
    if (model != nullptr) {
        auto* m = reinterpret_cast<GraphSolversModel*>(model);
        delete m;
    }
}

GRAPHSOLVERSADAPTER_API int Init(
    GraphSolversModelHandle model,
    const char* Topology,
    const char* Objects,
    const char* Settings)
{
    if (model == nullptr) {
        return 1;
    }
    
    if (Topology == nullptr || Objects == nullptr) {
        return 1;
    }
    
    try {
        
        using pipe_parameters = pde_solvers::iso_nonbaro_pipe_solver_t::pipe_parameters_type;
        
        auto* m = reinterpret_cast<GraphSolversModel*>(model);
        
        // Парсим схему из JSON-строк Topology и Objects
        // Используем комбинированную задачу (гидравлика + транспорт)
        // TODO: Шаблонный тип для параметров трубы можно забыть поменять.
        m->siso_data = 
            oil_transport::json_graph_parser_t::parse_hydro_transport_from_strings<
            pipe_parameters>(
            std::string(Topology),
            std::string(Objects));

        m->edge_id_to_name = m->siso_data.create_edge_binding_to_name();

        std::istringstream settings_stream(Settings);
        m->settings = oil_transport::settings_parser_json::parse(settings_stream);

        // TODO: облагородить обработку профилей
        make_pipes_uniform_profile_handled<pipe_parameters>(
            m->settings.pipe_coordinate_step, &m->siso_data.hydro_props.second);

        make_pipes_uniform_profile_handled<pipe_parameters>(
            m->settings.pipe_coordinate_step, &m->siso_data.transport_props.second);

        m->task = std::make_unique<oil_transport::hydro_transport_task_t>(
            m->siso_data, 
            m->settings);



        return 0;
    } catch (...) {
        return 1;
    }
}

GRAPHSOLVERSADAPTER_API int Solve(
    GraphSolversModelHandle model,
    const char* Bounds)
{
    if (model == nullptr) {
        return 1;
    }
    
    try {
        auto* m = reinterpret_cast<GraphSolversModel*>(model);

        std::istringstream bounds_stream(Bounds);
        oil_transport::hydrotransport_bounds_t bounds = 
            oil_transport::bounds_parser_json::parse_single_layer(bounds_stream,
            m->siso_data.name_to_binding);
        
        m->task->solve(bounds.measurements, true /* Сброс достоверности */);

        return 0;
    }
    catch (...) {
        return 1;
    }
}

GRAPHSOLVERSADAPTER_API int QuasistaticStep(
    GraphSolversModelHandle model,
    const char* Bounds)
{
    if (model == nullptr) {
        return 1;
    }

    try {
        auto* m = reinterpret_cast<GraphSolversModel*>(model);

        std::istringstream bounds_stream(Bounds);
        oil_transport::hydrotransport_bounds_t bounds =
            oil_transport::bounds_parser_json::parse_single_layer(bounds_stream,
                m->siso_data.name_to_binding);

        const auto& transport_controls = bounds.get_transport_controls();
        const auto& hydraulic_controls = bounds.get_hydraulic_controls();
        m->task->send_controls(transport_controls, hydraulic_controls);

        // TODO: time_step?
        double time_step = 30;
        m->task->step(time_step, bounds.measurements);

        return 0;
    }
    catch (...) {
        return 1;
    }
}

GRAPHSOLVERSADAPTER_API void SetState(
    GraphSolversModelHandle model,
    const char* State)
{
    if (model == nullptr) {
        return;
    }
    
    // Заглушка - пока не реализовано
}

GRAPHSOLVERSADAPTER_API const char* GetState(
    GraphSolversModelHandle model)
{
    if (model == nullptr) {
        return nullptr;
    }
    
    auto* m = reinterpret_cast<GraphSolversModel*>(model);
    // Заглушка - возвращаем пустой JSON объект
    // В будущем здесь будет сериализация состояния из hydro_transport_task_t
    m->stateString = "{}";
    return m->stateString.c_str();
}

GRAPHSOLVERSADAPTER_API const char* GetResult(
    GraphSolversModelHandle model)
{
    if (model == nullptr) {
        return nullptr;
    }
    
    auto* m = reinterpret_cast<GraphSolversModel*>(model);

    const auto& results = m->task->get_result();
    const auto& graph = m->task->get_transport_graph();

    std::stringstream json_output_stream;

    using json_writer = oil_transport::step_results_json_writer<pde_solvers::iso_nonbaro_pipe_solver_t>;

    json_writer::start_json_structure(json_output_stream);
    // TODO: Всегда выводится слой с индексом ноль. Счетчик шагов ввести?
    json_writer::write_single_layer(results, 0, m->edge_id_to_name, graph, json_output_stream, false);
    json_writer::end_json_structure(json_output_stream);

    m->result_buffer = json_output_stream.str();
    return m->result_buffer.c_str();
}

GRAPHSOLVERSADAPTER_API void freeString(const char* str) {
    // Заглушка - пока не нужно освобождать память
    // В будущем здесь будет освобождение строк, выделенных через new
}

} // extern "C"

