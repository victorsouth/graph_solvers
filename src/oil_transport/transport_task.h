#pragma once

namespace oil_transport {
;


/// @brief Вариант транспортного солвера полной распространимости
using transport_graph_flow_propagator_t = graphlib::graph_flow_propagator_t<
    transport_model_incidences_t,
    graph_quantity_value_t
>;


/// @brief Вариант транспортного солвера полной распространимости
using transport_graph_solver_t =
    graphlib::transport_graph_solver_template_t
    <
    transport_model_incidences_t,
    graph_quantity_value_t,
    pde_solvers::endogenous_values_t,
    transport_mixer_t,
    pde_solvers::endogenous_selector_t
    >;


/// @brief Алгоритм распространения измерений расходов graph_quantity_value_t по дереву блоков с транспортными моделями transport_model_incidences_t
using transport_block_flow_propagator = graphlib::block_flow_propagator<
    transport_model_incidences_t,
    graph_quantity_value_t
>;

/// @brief Алгоритм распростренения измерений эндогенных свойств по графу блоков, построенному на транспортном графе.
/// Расчетные значения эндогенных свойств - pde_solvers::endogenous_values_t
/// Рассчет свойств смеси описан в transport_mixer_t
template <typename PipeSolver>
using transport_block_endogenous_propagator = graphlib::block_endogenous_propagator<
    transport_model_incidences_t,
    transport_pipe_model_t<PipeSolver>,
    graph_quantity_value_t,
    pde_solvers::endogenous_values_t,
    transport_mixer_t,
    pde_solvers::endogenous_selector_t
>;

/// @brief Вариант транспортного солвера для графа с полной распространимости на дереве блоков
template <typename PipeSolver>
using transport_block_solver_t = graphlib::transport_block_solver_template_t<
    transport_model_incidences_t,
    transport_pipe_model_t<PipeSolver>,
    graph_quantity_value_t,
    pde_solvers::endogenous_values_t,
    transport_mixer_t,
    pde_solvers::endogenous_selector_t
>;

/// @brief Коррекция избыточных измерений расхода в транспортной задаче
typedef graphlib::redundant_flows_corrector<
    transport_model_incidences_t,
    graph_quantity_value_t
> transport_redundant_flows_corrector;

/// @brief Временной ряд значений управления для транспортной задачи
/// По каждому тегу получаем пару: вектор временных меток и вектор значений
typedef std::vector< // по каждому тегу...
    std::pair< // .. получаем пару ..
    std::vector<time_t>,  // .. временных меток ..
    std::vector<bool>   // .. и значений
    >
>  transport_control_timeseries_t;




/// @brief Возвращает расчетные результаты для ребра с заданным индексом среди результатов по всем ребрам данного класса - 
/// вектора пар <индекс ребра; расчетный результат по ребру>
/// @tparam VectorPairWithSecondArbitraryType Вектор пар расчетных результатов по ребрам
/// @param edge Индекс ребра, по которому запрашиваем результат
/// @param results Вектор с расчетными результатами
template <typename VectorPairResult>
inline typename VectorPairResult::value_type::second_type
    get_single_edge_result(size_t edge, const VectorPairResult& results)
{
    auto it = std::find_if(results.begin(), results.end(),
        [edge](const auto& result_item) { return result_item.first == edge; });

    if (it == results.end())
        throw std::runtime_error("Trying to get result for edge not presented in Results. Check edge type - pipe, source, lumped.");

    return (*it).second;
}


/// @brief Результаты расчета по типам объектов:
/// источники/потребители, трубы, сосредоточенные объекты.
template <typename PipeSolver, typename LumpedCalcParametersType = pde_solvers::endogenous_values_t>
struct task_common_result_t {
    /// @brief Расчетные профили эндогенных параметров в трубах
    std::vector<std::pair<size_t, const pipe_common_result_t<PipeSolver>*>> pipes;
    /// @brief Расчетные значения эндогенный свойств на выходах односторонних ребер
    std::vector<std::pair<size_t, const LumpedCalcParametersType*>> sources;
    /// @brief Расчетные значения эндогенный свойств на выходах двусторонних сосредоточенных (!) ребер
    std::vector<std::pair< size_t, const std::array<LumpedCalcParametersType*, 2> >> lumpeds;

private:
    
    /// @brief Указатель на модели труб для единообразной работы с транспортными и гидравлическими моделями
    /// Если солвер реализует только транспортный интерфейс, используем transport_pipe_model_t
    /// Иначе используем hydraulic_pipe_model_t
    using pipe_model_ptr_t = std::conditional_t<
        pde_solvers::is_transport_only_solver_v<PipeSolver>,
        transport_pipe_model_t<PipeSolver>*,
        hydraulic_pipe_model_t<PipeSolver>*
    >;

    /// @brief Модели только ТРУБ с привязками в MIMO графе. 
    vector<std::pair<size_t, pipe_model_ptr_t>> pipes_models;

public:
    task_common_result_t() = default;
    /// @brief Сопоставляет идентификаторам объектов графа контейнеры расчетных результатов
    /// @param graph Транспортрный граф
    task_common_result_t(const transport_graph_t& graph)
    {
        auto edges = graph.get_edge_binding_list();
        for (const auto& edge_binding : edges) {
            oil_transport_model_t* model = graph.get_model_incidences(edge_binding).model;
            void* buffer_pointer = model->get_edge_buffer();
            if (buffer_pointer == nullptr) {
                throw std::runtime_error("Model has null buffer");
            }

            // TODO: сделать как в билдере матчинг обработки буфера по type_info модели

            if (auto transport_pipe_model = dynamic_cast<transport_pipe_model_t<PipeSolver>*>(model)) {
                pipes_models.emplace_back(edge_binding.edge, transport_pipe_model);
                pipes.push_back(std::make_pair(edge_binding.edge, &transport_pipe_model->get_result()));
                continue;
            }
            else if (auto source_model = dynamic_cast<transport_source_model_t*>(model)) {
                // для поставщиков/потребителей результат расчета всегда будет лежать в его буфере - 
                auto buffer = reinterpret_cast<LumpedCalcParametersType*>(buffer_pointer);
                sources.emplace_back(edge_binding.edge, buffer);
            }
            else if (auto lumped_model = dynamic_cast<transport_lumped_model_t*>(model)) {
                // здесь все сломается, если будут другие
                // для двухсторонних объектов результат расчета тоже лежит в буфере, как у поставщиков, только сам буфер другой
                // buffer_pointer указывает на buffers_for_getter, который содержит указатели на те же объекты,
                // на которые ссылаются buffer_in и buffer_out
                auto* buffers_for_getter = reinterpret_cast<std::array<LumpedCalcParametersType*, 2>*>(buffer_pointer);
                // Извлекаем указатели напрямую из buffers_for_getter
                std::array<LumpedCalcParametersType*, 2> buffer_array{ 
                    buffers_for_getter->at(0), 
                    buffers_for_getter->at(1) 
                };
                lumpeds.emplace_back(edge_binding.edge, buffer_array);
            }
            else {
                throw std::runtime_error("Unknown model type");
            }
        }
    }

    /// @brief Формирует в полях класса указатели на общие 
    /// расчетные результаты (расходы + эндогены) по объектам гидравлического графа
    task_common_result_t(const hydraulic_graph_t& graph)
    {
        auto edges = graph.get_edge_binding_list();
        for (const auto& edge_binding : edges) {
            hydraulic_model_t* model = graph.get_model_incidences(edge_binding).model;
            void* buffer_pointer = model->get_edge_buffer();
            if (buffer_pointer == nullptr) {
                throw std::runtime_error("Model has null buffer");
            }

            // TODO: сделать как в билдере матчинг обработки буфера по type_info модели

            if (auto pipe_model = dynamic_cast<hydraulic_pipe_model_t<PipeSolver>*>(model)) {
                pipes_models.emplace_back(edge_binding.edge, pipe_model);
                pipes.push_back(std::make_pair(edge_binding.edge, &pipe_model->get_result()));
            }
            else if (auto source_model = dynamic_cast<qsm_hydro_source_model_t*>(model)) {
                // для поставщиков/потребителей результат расчета всегда будет лежать в его буфере - 
                auto buffer = reinterpret_cast<LumpedCalcParametersType*>(buffer_pointer);
                sources.emplace_back(edge_binding.edge, buffer);
            }
            else {
                // Не-труба и не-приток считаются сосредоточенными объектами в онтологии транспортных моделей.
                // Общими для транспортных и гидравлических моделей результатами являются эндогенные свойства
                // buffer_pointer указывает на buffers_for_getter, который содержит указатели на те же объекты,
                // на которые ссылаются buffer_in и buffer_out
                auto* buffers_for_getter = reinterpret_cast<std::array<LumpedCalcParametersType*, 2>*>(buffer_pointer);
                // Извлекаем указатели напрямую из buffers_for_getter
                std::array<LumpedCalcParametersType*, 2> buffer_array{
                    buffers_for_getter->at(0),
                    buffers_for_getter->at(1)
                };
                lumpeds.emplace_back(edge_binding.edge, buffer_array);
            }
        }
    }

    /// @brief Зачитывает текущие профили эндогенных свойств по трубам из pipes_list
    void update_pipe_result()
    {
        for (const auto& [id, pipe_model] : pipes_models) {
            pipe_model->get_result();
        }
    }

};

/// @brief Настройки таска изолированного транспортного расчета
struct transport_task_settings {
    /// @brief Настройки солвера уровня структурированного графа для транспортной задачи
    graphlib::structured_transport_solver_settings_t structured_transport_solver;

    /// @brief Шаг по координате для трубы
    double pipe_coordinate_step{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Возвращает настройки транспортной задачи по умолчанию
    static transport_task_settings default_values() {
        transport_task_settings result;
        result.structured_transport_solver = graphlib::structured_transport_solver_settings_t::default_values();
        result.pipe_coordinate_step = 200;
        return result;
    }
};

/// @brief Однослойные (на один временной шаг) расчетные задачи на графе всей схемы
/// @tparam TransportSolver Транспортный солвер - на графе или на дереве блоков
/// @tparam PipeSolver Солвер трубы
template <typename TransportSolver, typename PipeSolver, typename LumpedCalcParametersType = oil_transport::endohydro_values_t>
class transport_task_template {
public:
    /// @brief Тип солвера трубы
    using pipe_solver_type = PipeSolver;
    /// @brief Тип параметров для сосредоточенных объектов
    using lumped_calc_parameters_type = LumpedCalcParametersType;
    /// @brief Базовый тип управления объектами
    using base_control_type = oil_transport::transport_object_control_t;

private:
    /// @brief Транспортный солвер уровня несвязного графа
    using structured_transport_solver_t = graphlib::structured_transport_solver_t<
        TransportSolver,
        transport_graph_t,
        graph_quantity_value_t,
        pde_solvers::endogenous_selector_t,
        transport_pipe_model_t<PipeSolver>
    >;

private:
    /// @brief Контейнер моделей и буферов
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
    /// @brief Транспортный граф с полной топологией, ссылки на модели из models
    transport_graph_t graph;
    /// @brief Результаты расчета, классифицированные по типам объектов: источники, трубы, остальные объекты 
    task_common_result_t<PipeSolver, LumpedCalcParametersType> result_structure;
    /// @brief Параметры расчетной задачи
    transport_task_settings settings;

private:
    /// @brief Этот изврат нужен, чтобы не делать unique_ptr для task_common_result_t
    transport_task_template(
        std::tuple<
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
        transport_graph_t
        >&& models_and_graph)
        : models(std::get<0>(std::move(models_and_graph)))
        , graph(std::get<1>(std::move(models_and_graph)))
        , result_structure(graph)
{ }

public:

    /// @brief Конструктор транспортного таска из моделей и графа
    /// @param models Модели графа (транспортные и гидравлические)
    /// @param graph Транспортный граф
    /// @param _settings Настройки транспортной задачи
    transport_task_template(
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>&& _models,
        transport_graph_t&& _graph,
        const transport_task_settings& _settings)
        : models(std::move(_models))
        , graph(std::move(_graph))
        , settings(_settings)
        , result_structure(graph)
    { }

    /// @brief Вызывает graph_builder на основе siso_data
    /// @param siso_data 
    /// @param _settings 
    transport_task_template(const graph_siso_data& siso_data,
        const transport_task_settings& _settings)
        : transport_task_template(graph_builder<PipeSolver, LumpedCalcParametersType>::build_transport(
            siso_data, _settings.structured_transport_solver.endogenous_options))
    {
        this->settings = _settings;
    }
private:
    /// @brief Возвращает настройки в зависимости от используемого PipeSolver
    const auto& get_solver_settings() const {
        if constexpr (std::is_same<TransportSolver, transport_block_solver_t<PipeSolver>>::value) {
            return settings.structured_transport_solver.block_solver;
        }
        else {
            return settings.structured_transport_solver.graph_solver;
        }
    }
public:
    /// @brief Сбрасывает код достоверности всех объектов
    /// @details Используется после анализа статического кода достоверности
    void reset_confidence() {
        models.reset_confidence();
    }
    /// @brief Статический транспортный расчет для анализа полноты измерений
    /// @param measurements Вектор измерений (расходы и эндогены смешаны)
    /// @param confidence_reset Флаг сброса кода достоверности после решения
    /// @return Информация о полноте и использовании измерений
    graphlib::transport_problem_info<graph_quantity_value_t> solve(
        const std::vector<graph_quantity_value_t>& measurements, bool confidence_reset)
    {
        auto [flow, endogenous] = classify_measurements(measurements);
        return solve(flow, endogenous, confidence_reset);
    }

    /// @brief Статический транспортный расчет для анализа полноты измерений
    /// Эндогены, распространенные статическим алгоритмом, остаются в буферах с флагом недостоверности
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @param confidence_reset Флаг сброса кода достоверности после решения
    /// @return Информация о полноте и использовании измерений
    graphlib::transport_problem_info<graph_quantity_value_t> solve(
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        bool confidence_reset)
    {
        structured_transport_solver_t structured_solver(
            graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver);
        
        graphlib::transport_problem_info<graph_quantity_value_t> graph_result = 
            structured_solver.solve(flow_measurements, endogenous_measurements);

        if (confidence_reset)
            models.reset_confidence();

        structured_solver.advance_pipe_buffers();

        return graph_result;
    }
    /// @brief Выполняет статический расчет и формирует исключение при отсутствии полноты измерений
    /// @param measurements Вектор измерений (расходы + эндогенные свойства)
    void solve_handled(
        const std::vector<graph_quantity_value_t>& measurements)
    {
        std::vector<graphlib::transport_problem_completeness_info>
            completeness = solve(measurements);

        for (const graphlib::transport_problem_completeness_info& subgraph_completness : completeness)
        {
            if (!subgraph_completness.lacking_flows.empty()) {
                throw graphlib::graph_exception(subgraph_completness.lacking_flows,
                    std::wstring(L"Для графа не выполнено свойство полной распространимости расхода") +
                    L"Невозможно вычислить все расходы в ребрах графа");
            }
            if (!subgraph_completness.lacking_endogenious.empty()) {
                throw graphlib::graph_exception(subgraph_completness.lacking_endogenious,
                    std::wstring(L"Невозможно вычислить все расходы в ребрах графа"));
            }
        }
    }


    /// @brief Выполняет переключение объекта графа
    /// @param control Управление объектом
    void send_control(const transport_object_control_t& control) {
        auto& edge = graph.get_edge(control.binding);
        edge.model->send_control(control);
    }

    /// @brief Расчет на один шаг с автоматической классификацией измерений
    /// @param time_step Временной шаг
    /// @param measurements Вектор измерений (расходы и эндогены смешаны)
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step(
        double time_step,
        const std::vector<graph_quantity_value_t>& measurements)
    {
        auto [flow, endogenous] = classify_measurements(measurements);
        return step(time_step, flow, endogenous);
    }

    /// @brief Расчет на один шаг на основе измерений расходов и измерений эндогенных свойств
    /// @param time_step Временной шаг
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step(double time_step,
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements)
    {
        // Используем structured_transport_solver_t для решения задачи
        structured_transport_solver_t structured_solver(
            graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver);
        
        graphlib::transport_problem_info<graph_quantity_value_t> graph_result = 
            structured_solver.step(time_step, flow_measurements, endogenous_measurements);

        structured_solver.advance_pipe_buffers();
        return graph_result;
    }
    /// @brief Возвращает структуру для зачитки результатов расчета
    const task_common_result_t<PipeSolver, LumpedCalcParametersType>& get_result() {
        result_structure.update_pipe_result();
        return result_structure;
    }

    /// @brief Возвращает ссылку на транспортный граф
    const transport_graph_t& get_graph() const {
        return graph;
    }

    /// @brief Выполняет несколько переключений объектов графа
    void send_controls(const std::vector<const transport_object_control_t*>& controls) {
        for (const transport_object_control_t* control : controls) {
            if (control != nullptr) {
                send_control(*control);
            }
        }
    }
};

template <typename PipeSolver>
using transport_task_full_propagatable =
    transport_task_template<transport_graph_solver_t, PipeSolver>;

template <typename PipeSolver>
using transport_task_block_tree =
    transport_task_template<transport_block_solver_t<PipeSolver>, PipeSolver>;


inline auto prepare_control_tags_and_bindings(const graph_siso_data& siso_data)
{
    // Определение привязок тегов переключений на графе
    std::vector< graphlib::graph_binding_t> control_bindings;
    std::vector<control_tag_info_t> control_tags; // для зачитки тегов
    
    // По двусторонним ребрам
    for (size_t index = 0; index < siso_data.controls2.size(); ++index) {
        control_bindings.emplace_back(graphlib::graph_binding_t(graphlib::graph_binding_type::Edge2, index));
        control_tags.emplace_back(siso_data.controls2[index]);
    }

    // По односторонним ребрам
    for (size_t index = 0; index < siso_data.controls1.size(); ++index) {
        control_bindings.emplace_back(graphlib::graph_binding_t(graphlib::graph_binding_type::Edge1, index));
        control_tags.emplace_back(siso_data.controls1[index]);
    }

    return std::make_tuple(
        std::move(control_bindings),
        std::move(control_tags));
};

/// @brief Зачитка временных рядов переключений из предобработканных csv файлов (содержат только bool значения)
/// @param control_info Информация о тегах переключений
/// @param tags_folder Папка с csv файлами ("база данных" тегов)
inline transport_control_timeseries_t get_control_timeseries(const std::vector<control_tag_info_t>& control_info,
    const std::string& tags_folder)
{
        
    std::vector<tag_info_t> info_pde_solvers_format;
    for (const auto& raw_info : control_info)
    {
        // Преобразуем информацию в формат pde_solvers
        tag_info_t converted;
        converted.name = raw_info.name + "_converted";
        info_pde_solvers_format.emplace_back(converted); // Далее зачитаем преобразованный файл

        // Формируем новый файл
        const std::string& original_filename = tags_folder + "/" + raw_info.name;

        const auto& conversion_mapping = conversion_rules.at(raw_info.type).conversion_map;
        const int exception_value = conversion_rules.at(raw_info.type).exception_value;

        convert_csv_values(original_filename, conversion_mapping, exception_value);
    }

    csv_multiple_tag_reader reader(info_pde_solvers_format, tags_folder);
    transport_control_timeseries_t control_data = reader.read_csvs<bool>();

    return control_data;
}

/// @brief Временные ряды переключений с информацией об исходном теге и привязке на графе
struct bound_control_tags_data_t {  
    /// @brief Временные ряды переключений
    transport_control_timeseries_t timeseries;
    /// @brief Привязки тегов переключений на графе
    std::vector< graphlib::graph_binding_t> bindings;
    /// @brief Список тегов переключений (метаинформация о тегах состояний для объектов графа)
    std::vector<control_tag_info_t> tags;

    /// @brief Конструктор структуры для временных рядов переключений
    /// @param timeseries Временной ряд переключений
    /// @param bindings Вектор привязок к графу
    /// @param tags Вектор тегов переключений
    bound_control_tags_data_t(transport_control_timeseries_t& timeseries, std::vector< graphlib::graph_binding_t>& bindings,
        std::vector<control_tag_info_t>& tags)
        : timeseries(timeseries),
        bindings(bindings),
        tags(tags)
    { }

    /// @brief Формирует данные о переключениях на графе
    /// @param siso_data Топология графа, параметры ребер, привязки тегов измерений и переключений
    /// @param tags_folder Директория с файлами временных рядов переключений 
    /// @return Данные о переключениях в формате bound_controls_data_t
    static bound_control_tags_data_t prepare_data(const graph_siso_data& siso_data, 
        const std::string& tags_folder) {

        auto [bindings, tags] = prepare_control_tags_and_bindings(siso_data);
        transport_control_timeseries_t controls_timeseries = get_control_timeseries(tags, tags_folder);
        bound_control_tags_data_t result(controls_timeseries, bindings, tags);
        return result;

    }
};

/// @brief Временные ряды измерений с информацией об исходном теге и привязке на графе
struct bound_measurement_tags_data_t {
    /// @brief Временной ряд измерений с привязкой на графе graph_vector_timeseries_t
    graph_vector_timeseries_t timeseries;
    /// @brief Список тегов измерений;
    std::vector<tag_info_t> measurement_tags;

    /// @brief Конструктор данных привязанных тегов измерений
    /// @param timeseries Временной ряд измерений графа
    /// @param measurement_tags Вектор тегов измерений
    bound_measurement_tags_data_t(graph_vector_timeseries_t& timeseries, std::vector<tag_info_t>& measurement_tags)
        : timeseries(timeseries),
        measurement_tags(measurement_tags)
    { }

    /// @brief Подготавливает данные привязанных тегов измерений из SISO-данных
    /// @param siso_data SISO-данные графа
    /// @param tags_folder Папка с тегами измерений
    /// @return Данные привязанных тегов измерений
    static bound_measurement_tags_data_t prepare_data(const graph_siso_data& siso_data,
        const std::string& tags_folder) {

        // Зачитка тегов измерений
        auto [measurement_bindings, measurement_types, measurement_tags] =
            siso_data.prepare_measurements_tags_and_bindings();

        auto tag_timeseries = graph_vector_timeseries_t::from_files(
            tags_folder, measurement_tags,
            measurement_bindings, measurement_types);

        bound_measurement_tags_data_t result(tag_timeseries, measurement_tags);
        return result;
    }
};


/// @brief Зачитка таска, измерений и переключений из файлов
/// @param task_settings Настройки расчета
/// @param scheme_folder Папка с json-файлами расчетной схемы
/// @param tags_filename Путь к файлу с привязками тегов
/// @param controls_filename Путь к файлу с привязками переключений
/// @param tags_folder Папка с .csv-файлами тегов и переключений
/// @return Кортеж: 
/// Буфер моделей; 
/// Граф моделей; 
/// Временые ряды измерений с привязкой на графе;
/// Временые ряды измерений с привязкой на графе;
inline auto prepare_task_and_measurements_and_controls(
    const transport_task_settings& task_settings,
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const std::string& tags_folder
)
{
    graph_siso_data siso_data = json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
        scheme_folder, tags_filename);

    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        task_settings.pipe_coordinate_step, &siso_data.transport_props.second);
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, task_settings.structured_transport_solver.endogenous_options);

    // Зачитка тегов переключений по объектам и временных рядов переключений
    auto controls_data = bound_control_tags_data_t::prepare_data(siso_data, tags_folder);

    // Зачитка тегов измерений
    auto measurements_data = bound_measurement_tags_data_t::prepare_data(siso_data, tags_folder);

    return std::make_tuple(
        std::move(models),
        std::move(graph),
        std::move(measurements_data),
        std::move(controls_data)
    );
}

/// @brief Зачитка таска, измерений (без переключений!) из файлов
/// Зачитка возможна только если настройка time_grid_settings позволяет использовать начальные состояния объектов
/// @param task_settings Настройки расчета
/// @param scheme_folder Папка с json-файлами расчетной схемы
/// @param tags_filename Путь к файлу с привязками тегов
/// @param tags_folder Папка с .csv-файлами тегов и переключений
/// @return Кортеж: 
/// Буфер моделей; 
/// Граф моделей; 
/// Временной ряд измерений с привязкой на графе graph_vector_timeseries_t;
inline auto prepare_task_and_measurements(
    const transport_task_settings& task_settings,
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const std::string& tags_folder
)
{
    auto [models, graph, measurements_data,
        controls_data] = prepare_task_and_measurements_and_controls(
            task_settings, scheme_folder, tags_filename, tags_folder);

    return std::make_tuple(
        std::move(models),
        std::move(graph),
        std::move(measurements_data)
    );

}



/// @brief Проверяет, что в пакетном режиме все расчеты имеют полноту измерений
/// Подразумевается, что за кадром проведена работа по отбору управлений, влияющих на топологию
/// И отобранным управлениям по времени сопоставленные временные срезы измерений
/// @param topology_controls_batch 
/// @param measurement_batch 
inline std::vector<graphlib::transport_problem_info<graph_quantity_value_t>>
    check_measurements_completeness(
        transport_task_full_propagatable<qsm_pipe_transport_solver_t>& task,
        const std::vector<std::unique_ptr<transport_object_control_t>>& topology_controls_batch,
        const std::vector<std::vector<graph_quantity_value_t>>& measurement_batch)
{
    if (topology_controls_batch.size() + 1 != measurement_batch.size()) {
        throw std::runtime_error("Control and measurement batch must have equal size");
    }

    std::vector<graphlib::transport_problem_info<graph_quantity_value_t>>
        completeness;
    // нулевой слой - до первых переключений
    completeness.emplace_back(task.solve(measurement_batch[0], false));
    for (size_t index = 0; index < topology_controls_batch.size(); ++index) {
        task.send_control(*topology_controls_batch[index].get());
        completeness.emplace_back(task.solve(measurement_batch[index + 1], false));
    }

    return completeness;
}








}