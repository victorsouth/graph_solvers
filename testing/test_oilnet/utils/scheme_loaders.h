#pragma once

#include "test_oilnet.h"

namespace oil_transport {
;

/// @brief Загружает данные схемы из JSON и формирует SISO граф для совмещенного гидравлического и транспортного расчета
/// @tparam SolverPipeParameters Тип параметров трубы для солвера
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @return Кортеж из JSON данных и SISO графа
template <
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
> 
inline std::tuple<graph_json_data, graph_siso_data> load_hydro_transport_siso_graph(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings)
{
    graph_json_data json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::HydroTransport);

    auto hparams = json_data.get_hydro_object_parameters<SolverPipeParameters>();
    auto tparams = json_data.get_transport_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data =
        json_graph_parser_t::prepare_hydro_transport_siso_graph(
            tparams, hparams, json_data);

    return { std::move(json_data), std::move(siso_data) };

}


/// @brief Зачитывает схему из JSON, строит транспортные и гидравлические модели и графы.
/// @tparam PipeSolver            Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters  Тип параметров трубы, используемый при построении моделей
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::endohydro_values_t,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    transport_graph_t,
    oil_transport::hydraulic_graph_t
> load_hydro_transport_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }

    std::tie(*json_data, *siso_data) = load_hydro_transport_siso_graph<SolverPipeParameters>(
        scheme_folder, tags_filename, settings
    );
    auto [models_with_buffers, transport_graph, hydro_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_hydro_transport(
            *siso_data, settings.structured_transport_solver.endogenous_options);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(transport_graph),
        std::move(hydro_graph));
}

/// @brief Зачитывает схему из JSON и строит транспортные модели и граф
/// @tparam PipeSolver Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters Тип параметров трубы, используемый при построении моделей
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @param json_data Указатель на структуру для сохранения JSON данных (опционально)
/// @param siso_data Указатель на структуру для сохранения SISO данных (опционально)
/// @return Кортеж из моделей с буферами и транспортного графа
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::transport_values,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    transport_graph_t
> load_transport_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    *json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::Transport);

    auto tparams = json_data->get_transport_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }
    *siso_data =
        json_graph_parser_t::prepare_transport_siso_graph(tparams, *json_data);

    auto [models_with_buffers, transport_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_transport(
            *siso_data, settings.structured_transport_solver.endogenous_options);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(transport_graph)
    );
}


/// @brief Зачитывает схему из JSON и строит гидравлические модели и граф
/// @tparam PipeSolver Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters Тип параметров трубы, используемый при построении моделей
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @param json_data Указатель на структуру для сохранения JSON данных (опционально)
/// @param siso_data Указатель на структуру для сохранения SISO данных (опционально)
/// @return Кортеж из моделей с буферами и гидравлического графа
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::endohydro_values_t,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    oil_transport::hydraulic_graph_t
> load_hydro_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const hydro_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    *json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::Hydraulic);

    auto hparams = json_data->get_hydro_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }
    *siso_data =
        json_graph_parser_t::prepare_hydro_siso_graph(hparams, *json_data);

    auto [models_with_buffers, hydro_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_hydro(*siso_data);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(hydro_graph));
}

} 

namespace hydro_task_test_helpers {
;
/// @brief Подготавливает схему с трубой и измерения для тестов
/// @param solution_method Метод решения задачи
/// @param pipe_coordinate_step Шаг по координате для трубы (NaN означает использование значения по умолчанию из settings)
/// @param source_pressure Давление источника
/// @param density_measure Значение плотности для измерения
/// @param dummy_density Значение dummy плотности для измерения
/// @return Кортеж из настроек (settings), данных схемы (siso_data) и измерений (measurements)
inline std::tuple<oil_transport::hydro_transport_task_settings, oil_transport::graph_siso_data, std::vector<oil_transport::graph_quantity_value_t>> 
prepare_pipe_scheme_with_measurements(
    oil_transport::hydro_transport_solution_method solution_method,
    double pipe_coordinate_step,
    double source_pressure,
    double density_measure,
    double dummy_density) {
    
    using pipe_solver_type = pde_solvers::iso_nonbaro_pipe_solver_t;
    
    auto settings = oil_transport::hydro_transport_task_settings::get_default_settings();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    settings.solution_method = solution_method;
    if (std::isfinite(pipe_coordinate_step)) {
        settings.pipe_coordinate_step = pipe_coordinate_step;
    }

    auto siso_data =
        oil_transport::json_graph_parser_t::parse_hydro_transport<pipe_solver_type::pipe_parameters_type>(
            get_schemes_folder() + "hydro_pipe_test");

    // Формируем профили для гидравлических и транспортных свойств
    oil_transport::make_pipes_uniform_profile_handled<pipe_solver_type::pipe_parameters_type>(
        settings.pipe_coordinate_step, &siso_data.hydro_props.second);

    oil_transport::make_pipes_uniform_profile_handled<pipe_solver_type::pipe_parameters_type>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    auto& src_properties = siso_data.get_properties<oil_transport::qsm_hydro_source_parameters>("Src", oil_transport::qsm_problem_type::HydroTransport);
    src_properties.pressure = source_pressure;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    // Создаем измерения - dummy чтобы начальная недостоверная партия c усредненными свойствами не совпала с density_measure
    std::vector<oil_transport::graph_quantity_value_t> measurements{
        {siso_data.name_to_binding.at("Vlv1"), oil_transport::measurement_type::vol_flow_std, 1.0},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 1), oil_transport::measurement_type::density_std, density_measure},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 0), oil_transport::measurement_type::density_std, dummy_density},
    };

    return std::make_tuple(std::move(settings), std::move(siso_data), std::move(measurements));
}

/// @brief Подготавливает схему с задвижкой и измерения для тестов
/// @param solution_method Метод решения задачи (может быть не задан, если не используется)
/// @param source_pressure Давление источника
/// @param vol_flow_measure_value Значение расхода для измерения
/// @param vol_flow_measure_binding Привязка для измерения расхода (имя объекта, например "Vlv1" или "Src")
/// @param density_measure_value Значение плотности для измерения
/// @return Кортеж из настроек (settings), данных схемы (siso_data) и измерений (measurements)
inline std::tuple<oil_transport::hydro_transport_task_settings, oil_transport::graph_siso_data, std::vector<oil_transport::graph_quantity_value_t>> 
prepare_valve_scheme(
    std::optional<oil_transport::hydro_transport_solution_method> solution_method,
    double source_pressure,
    double vol_flow_measure_value,
    const std::string& vol_flow_measure_binding,
    double density_measure_value) {
    
    auto settings = oil_transport::hydro_transport_task_settings::get_default_settings();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    if (solution_method.has_value()) {
        settings.solution_method = solution_method.value();
    }

    auto siso_data =
        oil_transport::json_graph_parser_t::parse_hydro_transport<oil_transport::qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "valve_control_test/scheme/");

    auto& src_properties = siso_data.get_properties<oil_transport::qsm_hydro_source_parameters>("Src", oil_transport::qsm_problem_type::HydroTransport);
    src_properties.pressure = source_pressure;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    // Создаем измерения
    std::vector<oil_transport::graph_quantity_value_t> measurements{
        {siso_data.name_to_binding.at(vol_flow_measure_binding), oil_transport::measurement_type::vol_flow_std, vol_flow_measure_value},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 1), oil_transport::measurement_type::density_std, density_measure_value},
    };

    return std::make_tuple(std::move(settings), std::move(siso_data), std::move(measurements));
}

}

