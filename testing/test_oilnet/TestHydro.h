#pragma once

#include "test_oilnet.h"

/// @brief Модуль квазистационарного гидравлического моделирования
namespace qsm_hydro {
;
/// @brief структура параметров модели
struct qsm_hydro_source_properties : public oil_transport::model_parameters_t {
    /// @brief параметры флюида
    pde_solvers::oil_parameters_t oil;
    /// @brief объемный расход
    double std_volflow{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief давление
    double pressure{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Конструктор по умолчанию
    qsm_hydro_source_properties() = default;
    /// @brief Конструктор из JSON данных
    /// @param json_data  Данные в формате JSON
    qsm_hydro_source_properties(const oil_transport::source_json_data& json_data) {
        oil.density.nominal_density = json_data.density;
        pressure = json_data.pressure;
        std_volflow = json_data.std_volflow;
        if (json_data.is_initially_zeroflow) {
            pressure = std::numeric_limits<double>::quiet_NaN();
            std_volflow = 0;
        }
    }
};
/// @brief Источник в квазистационарной гидравлической модели
class qsm_hydro_source : public graphlib::closing_relation1 {
    qsm_hydro_source_properties properties;
public:
    /// @brief Конструктор
    /// @param properties Свойства источника
    qsm_hydro_source(const qsm_hydro_source_properties& properties)
        : properties(properties)
    {
    }
    /// @brief Возвращает известное давление для P-притоков. 
    /// Если объект не является P-притоком, возвращает NaN
    virtual double get_known_pressure() {
        return properties.pressure;
    }
    /// @brief Возвращает известный расход для Q-притоков. 
    /// Если объект не является Q-притоком, возвращает NaN
    /// Тип расхода (объемный, массовый) не регламентируется и решается на уровне реализации замыкающих соотношений
    /// Нужно, чтобы все замыкающие соотношения подразумевали один и тот же вид расхода
    virtual double get_known_flow() {
        return properties.std_volflow;
    }
    /// @brief Расход висячей дуги при заданном давлении
    virtual double calculate_flow(double pressure) override {
        throw std::runtime_error("not supported");
    }
    /// @brief Производная расход висячей дуги по давлению
    virtual double calculate_flow_derivative(double pressure) override {
        throw std::runtime_error("not supported");
    }

    /// @brief Запись значения расхода
    virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        throw std::runtime_error("No buffer to update");
    }

    /// @brief Для одностороннего ребра запись двух давлений не определена
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
        throw std::runtime_error("Must not be called for edge type I");
    }

    /// @brief Запись давления
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
        throw std::runtime_error("No buffer to update");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("No buffer to read from");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    /// @param incidence +1 - выход ребра. -1 - вход ребра.
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) {
        throw std::runtime_error("Must not be called for edge type I");
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("No buffer to read from");
    }

};
/// @brief Свойства местного сопротивления для квазистационарной гидравлической модели
struct qsm_hydro_local_resistance_properties : public oil_transport::model_parameters_t {
    /// @brief Коэффициент местного сопротивления
    double xi{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Диаметр сечения
    double diameter{ std::numeric_limits<double>::quiet_NaN() };

    qsm_hydro_local_resistance_properties() = default;
    /// @brief Конструктор, инициализирующий свойства местного сопротивления из JSON-данных клапана
    /// @param json_data JSON-данные клапана, содержащие параметры местного сопротивления
    qsm_hydro_local_resistance_properties(const oil_transport::valve_json_data& json_data) {
        xi = 150;
        diameter = 0.5;
    }
};
/// @brief Класс местного сопротивления для квазистационарной гидравлической модели
class qsm_hydro_local_resistance : public graphlib::closing_relation2 {
    /// @brief свойства местного сопротивления
    qsm_hydro_local_resistance_properties properties;
public:
    /// @brief Конструктор
    /// @param properties Свойства местного сопротивления
    qsm_hydro_local_resistance(const qsm_hydro_local_resistance_properties& properties)
        : properties(properties)
    {
    }
private:
public:
    /// @brief Расчет расхода через местное сопротивление
    /// @param pressure_in давление на входе
    /// @param pressure_out давление на выходе
    /// @return расход
    virtual double calculate_flow(double pressure_in, double pressure_out) override {
        double S = pde_solvers::circle_area(properties.diameter);//Площадь поперечного сечения
        double density = 850; // взять из буфера        
        double dp = pressure_in - pressure_out;//перепад давления
        double v = fixed_solvers::ssqrt(2 * dp / (properties.xi * density));// скорость потока
        double flow = v * S;// объемный расход
        return flow;
    }
    /// @brief Расчет производных расхода по давлениям на входе и выходе
    /// @param pressure_in давление на входе
    /// @param pressure_out давление на выходе
    /// @return // массив производных
    virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) override {
        throw std::runtime_error("not implemented");
    }

    /// @brief Запись значения расхода
    virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        throw std::runtime_error("No buffer to update");
    }

    /// @brief Запись давлений на входе и выходе
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
        throw std::runtime_error("No buffer to update");
    }

    /// @brief Для двустороннего ребра запись одного давления не определена
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    /// @param incidence +1 - выход ребра. -1 - вход ребра.
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) {
        throw std::runtime_error("No buffer to read from");
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("No buffer to read from");
    }
};
/// @brief свойства гидравлических объектов
struct hydro_objects_properties {
    /// @brief Свойства клапана/местного сопротивления
    using valve = qsm_hydro_local_resistance_properties;
    /// @brief Свойства трубы/участка трубопровода
    using pipe = oil_transport::qsm_pipe_transport_parameters_t;
    /// @brief Свойства источника (притока)
    using source = qsm_hydro_source_properties;
    /// @brief Свойства насоса
    using pump = oil_transport::transport_lumped_parameters_t;
};

// TODO: Перенести как метод в graph_json_data
// сделать вместо hydro_objects_properties шаблон

/// @brief Получение параметров гидравлических объектов из JSON данных графа
/// @param data Данные графа в формате JSON
/// @return Словарь параметров объектов с именами в качестве ключей
inline std::unordered_map<
    std::string,
    std::unique_ptr<oil_transport::model_parameters_t>
> get_hydro_object_parameters(const oil_transport::graph_json_data& data)
{
    std::unordered_map <std::string, std::unique_ptr<oil_transport::model_parameters_t>> result;

    // Проверка на неподдерживаемые типы объектов
    if (!data.lumpeds.empty()) {
        throw std::runtime_error("Cannot create hydraulic model for transport-only lumped object");
    }
    // Конвертация источников, клапанов и насосов
    oil_transport::graph_json_data::convert_objects<hydro_objects_properties::source>(
        data.sources, result);
    oil_transport::graph_json_data::convert_objects<hydro_objects_properties::valve>(
        data.valves, result);
    oil_transport::graph_json_data::convert_objects<hydro_objects_properties::pump>(
        data.pumps, result);

    /// @brief Лямбда-функция для конвертации параметров труб
    auto copy_and_convert_pipes = [&](const auto& parameters) {
        for (const auto& [name, params] : parameters) {
            using SolverPipeParameters = hydro_objects_properties::pipe;
            using TransportPipeType = oil_transport::transport_pipe_parameters_t<SolverPipeParameters>;

            const pde_solvers::pipe_json_data& json_pipe = params;
            SolverPipeParameters solver_pipe(json_pipe);
            TransportPipeType transport_pipe(solver_pipe);

            result[name] = std::move(std::make_unique<TransportPipeType>(transport_pipe));
        }
        };

    ///Конвертация параметров труб
    copy_and_convert_pipes(data.pipes);

    return result;


}


}

/// @brief Тест для разработки квазистационарного гидравлического решателя
TEST(NodalSolver, DISABLED_DevelopQuasistatic)
{
    using namespace graphlib;
    using namespace qsm_hydro;
    using namespace oil_transport;
    // Настройка параметров расчета
    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    // Загрузка данных схемы
    std::string scheme_folder = get_scheme_folder("two_valves");

    graph_json_data data = json_graph_parser_t::load_json_data(scheme_folder, "", qsm_problem_type::Hydraulic);
    //auto tparams = data.get_transport_object_parameters<qsm_pipe_transport_parameters_t>();

    // Получение параметров объектов
    auto hparams = get_hydro_object_parameters(data);

    graph_siso_data siso_data =
        json_graph_parser_t::prepare_hydro_siso_graph(hparams, data);


    /*transport_graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            scheme_folder);*/
    /*make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.props2);*/

    /// Построение транспортного графа и моделей
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, settings.structured_transport_solver.endogenous_options);

    // Источник с заданным расходом
    qsm_hydro_source_properties src;
    src.std_volflow = 1.0;
    qsm_hydro_source src_model(src);

    // Сток с заданным давлением
    qsm_hydro_source_properties sink;
    sink.pressure = 1e5;
    qsm_hydro_source sink_model(sink);

    // Два местных сопротивления (клапана)
    qsm_hydro_local_resistance_properties resist1;
    qsm_hydro_local_resistance resist1_model(resist1);

    qsm_hydro_local_resistance_properties resist2;
    qsm_hydro_local_resistance resist2_model(resist2);

}


