#include "test_oilnet.h"
#include "utils/task_test_utils.h"

/// @brief Проверка способности выполнить декомпозированный квазистац. расчет на простейшей схеме из двух задвижек
TEST(DecomposedQuasistaticCalc, HandlesSimpleScheme_WhenNoPipes)
{
    using namespace oil_transport;
    using namespace graphlib;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;
    using transport_solver = qsm_pipe_transport_solver_t;

    // Arrange - Формируем граф с моделями
    auto transport_settings = transport_task_settings::default_values();
    transport_settings.structured_transport_solver.endogenous_options.density_std = true;

    auto [transport_siso_data, hydro_siso_data] =
        json_graph_parser_t::parse_hydro_transport<transport_solver::pipe_parameters_type, hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "valve_control_test/scheme/");

    auto& src_properties = hydro_siso_data.get_properties<qsm_hydro_source_parameters>("Src", qsm_problem_type::HydroTransport);
    src_properties.pressure = 110e3;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    // Создаем измерения для транспортного расчета
    constexpr double density_measure = 850.0;
    std::vector<graph_quantity_value_t> measurements{
        {transport_siso_data.name_to_binding.at("Vlv1"), measurement_type::vol_flow_std, 1.0},
        {graph_binding_t(graph_binding_type::Vertex, 1), measurement_type::density_std, density_measure}
    };

    // Создаем хранилище профилей по трубам и объектам
    historical_layers_storage_t<pde_solvers::iso_nonbaro_pipe_solver_t> storage;

    // Создаем маппинг от edge_id к имени объекта из topology.json (включая трубы, sources, lumpeds)
    auto edge_id_to_object_name = transport_siso_data.create_edge_binding_to_name();

    transport_task_block_tree<transport_solver> task(transport_siso_data, transport_settings);

    // Act - Выполняем транспортный расчет. Сохраняем расходы и эндогенные свойства
    const size_t N = 5;
    double time_step = 30.0;
    for (size_t step = 0; step < N; ++step) {
        task.step(time_step, measurements);

        // Получаем результаты расчета
        const auto& result = task.get_result();

        // Сохраняем расчетные слои в хранилище
        storage.save_results(step, result, edge_id_to_object_name);
    }

    // Выполняем гидравлический расчет по предпосчитанным эндогенам
    auto hydro_settings = hydro_task_settings::default_values();
    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    storage.restore_results(hydro_task.get_graph(), N - 1, edge_id_to_object_name);
    hydro_task.solve();

    // Assert - Расходы из МД в последовательных ребрах совпадают.
    // Давление убывает вдоль направления потока.
    // Плотность в буфере гидравлических моделей взята из транспортного расчета
    const auto& result = hydro_task.get_result();

    const auto& vlv1_buffer_in = result.lumpeds.at(0).second[0];
    const auto& vlv1_buffer_out = result.lumpeds.at(0).second[1];
    const auto& vlv2_buffer_out = result.lumpeds.at(1).second[1];

    ASSERT_NEAR(vlv1_buffer_in->vol_flow, vlv2_buffer_out->vol_flow, 1e-6);

    double pressure_vertex0 = vlv1_buffer_in->pressure;
    double pressure_vertex1 = vlv1_buffer_out->pressure;
    double pressure_vertex2 = vlv2_buffer_out->pressure;

    AssertBetween(pressure_vertex1, pressure_vertex0, pressure_vertex2);

    ASSERT_EQ(vlv2_buffer_out->density_std.value, density_measure);
}

/// @brief Проверка способности выполнить декомпозированный квазистац. расчет на схеме с трубой
TEST(DecomposedQuasistaticCalc, HandlesSimpleSchemeWithPipe)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;
    using transport_solver = qsm_pipe_transport_solver_t;

    // Arrange - Формируем граф с моделями
    auto transport_settings = transport_task_settings::default_values();
    transport_settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data_transport =
        json_graph_parser_t::parse_transport<transport_solver::pipe_parameters_type>(
            get_schemes_folder() + "hydro_pipe_test");

    make_pipes_uniform_profile_handled<transport_solver::pipe_parameters_type>(
        transport_settings.pipe_coordinate_step, &siso_data_transport.transport_props.second);

    // Создаем измерения
    constexpr double density_measure = 850.0;
    std::vector<graph_quantity_value_t> measurements{
        {siso_data_transport.name_to_binding.at("Vlv1"), measurement_type::vol_flow_std, 1.0},
        {graph_binding_t(graph_binding_type::Vertex, 1), measurement_type::density_std, density_measure}
    };

    // Создаем хранилище профилей по трубам и объектам
    historical_layers_storage_t<hydro_solver> storage;

    // Создаем маппинг от edge_id к имени объекта из topology.json (включая трубы, sources, lumpeds)
    auto edge_id_to_object_name = siso_data_transport.create_edge_binding_to_name();

    transport_task_block_tree<transport_solver> task(siso_data_transport, transport_settings);

    const double initial_density = task.get_result().pipes.front().second->layer->density_std.value.front();

    // Act - Выполняем транспортный расчет. Сохраняем расходы и эндогенные свойства
    const size_t N = 5;
    double time_step = 30.0;
    for (size_t step = 0; step < N; ++step) {
        task.step(time_step, measurements);

        // Получаем результаты расчета
        const auto& result = task.get_result();

        // Сохраняем расчетные слои в хранилище
        storage.save_results(step, result, edge_id_to_object_name);
    }

    // Зачитка гидравлических параметров
    graph_siso_data siso_data_hydro =
        json_graph_parser_t::parse_hydro<hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "hydro_pipe_test");

    auto hydro_settings = hydro_task_settings::default_values();

    // TODO: объединить формирования профиля и для транспортных, и для гидравлических свойств. Почему они вообще отдельно хранятся для трубы???
    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &siso_data_hydro.hydro_props.second);

    hydro_task_t hydro_task(siso_data_hydro, hydro_settings);

    // Выполняем гидравлический расчет по эндогенам, предпосчитанным на транспортном этапе
    storage.restore_results(hydro_task.get_graph(), N - 1, edge_id_to_object_name);
    hydro_task.solve();

    // Assert - Для всех ребрер рассчитаны расходы и давления
    const auto& result = hydro_task.get_result();
    AssertAllEdgesHydroResultFinite(result);

    // Проверяем наличие нескольких партий в трубе
    const auto& layer = *result.pipes.front().second->layer;
    for (const auto& d : layer.density_std.value) {
        AssertBetween(d, initial_density, density_measure);
    }
}
/// @brief Проверяет возможность получения диагностики численного метода для схемы из нескольких компонентов связности 
TEST(HydroOnlyTask, ReturnsNodalSolverAnalysis_WhenMultipleSubgraphs)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "two_components/");

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;
    hydro_settings.structured_solver.analysis_mode = analysis_execution_mode::always;
    hydro_settings.structured_solver.analysis_parameters = 
        structured_hydro_solver_settings_t::get_enabled_analysis_settings();

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    graphlib::structured_flow_distribution_result full_result = hydro_task.solve();

    // Assert - пришла диагностика по обоим компонентам связности
    const size_t components_count = 2;
    ASSERT_EQ(full_result.results_by_subgraphs.size(), components_count);
    AssertResidualsAnalysisNonEmpty(full_result);
}

/// @brief Проверяет корректность режима "никогда не собирать диагностику". 
TEST(HydroOnlyTask, ReturnsEmptyAnalysisAndNonEmptyFlowResult_WhenAnalysisModeNever)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Схема из двух компонентов связности
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "two_components/");

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;
    hydro_settings.structured_solver.analysis_mode = analysis_execution_mode::never;
    hydro_settings.structured_solver.analysis_parameters =
        structured_hydro_solver_settings_t::get_enabled_analysis_settings();

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act - расчет при отключенном сборе диагностики
    graphlib::structured_flow_distribution_result full_result = hydro_task.solve();
    
    // Assert - при отключенной диагностике диагностика по подграфам не формируется, 
    // расчётный результат (давления, расходы) не пустой
    const size_t components_count = 2;
    ASSERT_EQ(full_result.results_by_subgraphs.size(), components_count);

    AssertResidualsAnalysisEmpty(full_result);
    AssertFlowResultNonEmpty(full_result);
}

/// @brief При режиме if_failed: при сходящемся расчёте диагностика пустая; после «поломки» расчёта повторный solve даёт непустую диагностику
TEST(HydroOnlyTask, FillsAnalysisOnlyIfNotConverged_WhenAnalysisModeIfFailed)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;

    // Arrange - зачитка схемы из двух компонентов связности
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "two_components/");
    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;
    hydro_settings.structured_solver.analysis_mode = analysis_execution_mode::if_failed;
    hydro_settings.structured_solver.analysis_parameters =
        structured_hydro_solver_settings_t::get_enabled_analysis_settings();

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task_ok(hydro_siso_data, hydro_settings);
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task_ok.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act #1 - Выполняем заведомо успешный расчет
    graphlib::structured_flow_distribution_result result_ok = hydro_task_ok.solve();

    // Arrange #1 - Диагностика отсутствует, т.к. не возникло ошибки расчета
    AssertResidualsAnalysisEmpty(result_ok);

    // Arrange #2 - Ограничиваем максимальное количество итераций, чтобы расчет заведомо не сошелся
    hydro_settings.structured_solver.nodal_solver.max_iterations = 1;

    hydro_task_t hydro_task_fail(hydro_siso_data, hydro_settings);
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task_fail.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act #2 - Выполняем заведомо несходящийся расчет
    graphlib::structured_flow_distribution_result result_fail = hydro_task_fail.solve();

    // Arrange #2 - Присутствует диагностика по каждому подграфу
    AssertResidualsAnalysisNonEmpty(result_fail);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка при краевых условиях PP
TEST(HydroOnlyTask, HandlesTypicalMainPumpStation_WhenDisabledReserveLine_PP)
{
    using namespace oil_transport;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "main_pump_station_with_line_pipe/");

    // Формируем условие PP
    auto& src_properties = hydro_siso_data.get_properties<qsm_hydro_source_parameters>("RP1", qsm_problem_type::HydroTransport);
    src_properties.pressure = 300e3;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка
/// при работе резервных ниток в режиме лупинга при краевых условиях PP
TEST(HydroOnlyTask, HandlesTypicalMainPumpStation_WhenEnabledReserveLine_PP)
{
    using namespace oil_transport;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "main_pump_station_with_line_pipe/");

    // Формируем условие PP
    auto& src_properties = hydro_siso_data.get_properties<qsm_hydro_source_parameters>("RP1", qsm_problem_type::HydroTransport);
    src_properties.pressure = 300e3;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Формирование переключений для открытия задвижек на резервные нитки
    std::vector<std::string> bypass_valves_to_open{
        "LU1.2B_V1", "LU1.2B_V2", "LU1.2B_V3", "LU1.2B_V4", "LU1.2B_V5", "LU1.2B_V6",
        "LU1.4B_V1", "LU1.4B_V2", "LU1.4B_V3", "LU1.4B_V4", "LU1.4B_V5", "LU1.4B_V6"
    };

    using valve_control_t = qsm_hydro_local_resistance_control_t;
    double opened_valve_position = 1.0;

    for (const std::string& valve : bypass_valves_to_open) {
        auto binding = hydro_siso_data.name_to_binding.at(valve);
        valve_control_t control;
        control.binding = binding;
        control.opening = opened_valve_position;
        hydro_task.send_control(control);
    }

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка при краевых условиях QP
TEST(HydroOnlyTask, HandlesTypicalMainPumpStation_WhenDisabledReserveLine_QP)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "main_pump_station_with_line_pipe/");

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка
/// при работе резервных ниток в режиме лупинга при краевых условиях QP
TEST(HydroOnlyTask, HandlesTypicalMainPumpStation_WhenEnabledReserveLine_QP)
{
    using namespace oil_transport;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "main_pump_station_with_line_pipe/");

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;

    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Формирование переключений для открытия задвижек на резервные нитки
    std::vector<std::string> bypass_valves_to_open{
        "LU1.2B_V1", "LU1.2B_V2", "LU1.2B_V3", "LU1.2B_V4", "LU1.2B_V5", "LU1.2B_V6",
        "LU1.4B_V1", "LU1.4B_V2", "LU1.4B_V3", "LU1.4B_V4", "LU1.4B_V5", "LU1.4B_V6"
    };

    using valve_control_t = qsm_hydro_local_resistance_control_t;
    double opened_valve_position = 1.0;

    for (const std::string& valve : bypass_valves_to_open) {
        auto binding = hydro_siso_data.name_to_binding.at(valve);
        valve_control_t control;
        control.binding = binding;
        control.opening = opened_valve_position;
        hydro_task.send_control(control);
    }

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}


/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка при краевых условиях PP
TEST(HydroOnlyTask, HandlesTypicalLinearPart_WhenDisabledReserveLine_PP)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "line_pipe/");

    // Формируем условие PP
    auto& src_properties = hydro_siso_data.get_properties<qsm_hydro_source_parameters>("Src", qsm_problem_type::HydroTransport);
    src_properties.pressure = 300e3;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(
        hydro_siso_data, 50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех проточных ребрах притоке и отборе 
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка
/// при работе резервных ниток в режиме лупинга при краевых условиях PP
TEST(HydroOnlyTask, HandlesTypicalLinearPart_WhenEnabledReserveLine_PP)
{
    using namespace oil_transport;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "line_pipe/");

    // Формируем условие PP
    auto& src_properties = hydro_siso_data.get_properties<qsm_hydro_source_parameters>("Src", qsm_problem_type::HydroTransport);
    src_properties.pressure = 300e3;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(
        hydro_siso_data, 50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Формирование переключений для открытия задвижек на резервные нитки
    std::vector<std::string> bypass_valves_to_open{
        "LU1.2B_V1", "LU1.2B_V2", "LU1.2B_V3", "LU1.2B_V4", "LU1.2B_V5", "LU1.2B_V6",
        "LU1.4B_V1", "LU1.4B_V2", "LU1.4B_V3", "LU1.4B_V4", "LU1.4B_V5", "LU1.4B_V6"
    };

    using valve_control_t = qsm_hydro_local_resistance_control_t;
    double opened_valve_position = 1.0;
    auto controls = prepare_controls_for_multiple_edges<valve_control_t>(
        &valve_control_t::opening, opened_valve_position,
        hydro_siso_data.name_to_binding,
        bypass_valves_to_open);
    send_multiple_typed_controls(controls, &hydro_task);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех ребрах
    const auto& result = hydro_task.get_result();
    AssertAllEdgesHydroResultFinite(result);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка при краевых условиях QP
TEST(HydroOnlyTask, HandlesTypicalLinearPart_WhenDisabledReserveLine_QP)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "line_pipe/");

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(
        hydro_siso_data, 50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверка возможности гидравлического расчета схемы типового линейного участка
/// при работе резервных ниток в режиме лупинга при краевых условиях QP
TEST(HydroOnlyTask, HandlesTypicalLinearPart_WhenEnabledReserveLine_QP)
{
    using namespace oil_transport;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типового ЛУ. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "line_pipe/");

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(
        hydro_siso_data, 50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Формирование переключений для открытия задвижек на резервные нитки
    std::vector<std::string> bypass_valves_to_open{
        "LU1.2B_V1", "LU1.2B_V2", "LU1.2B_V3", "LU1.2B_V4", "LU1.2B_V5", "LU1.2B_V6",
        "LU1.4B_V1", "LU1.4B_V2", "LU1.4B_V3", "LU1.4B_V4", "LU1.4B_V5", "LU1.4B_V6"
    };

    using valve_control_t = qsm_hydro_local_resistance_control_t;
    double opened_valve_position = 1.0;
    auto controls = prepare_controls_for_multiple_edges<valve_control_t>(
        &valve_control_t::opening, opened_valve_position,
        hydro_siso_data.name_to_binding,
        bypass_valves_to_open);
    send_multiple_typed_controls(controls, &hydro_task);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на всех ребрах
    const auto& result = hydro_task.get_result();
    AssertAllEdgesHydroResultFinite(result);
}

/// @brief Проверка возможности гидравлического расчета схемы типовой НПС
TEST(HydroOnlyTask, HandlesTypicalOilPumpStation)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Зачитка схемы типовой НПС. Инициализация эндогенных свойств в буферах
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "oil_pump_station/");

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(hydro_siso_data,
        50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Act - гидравлический расчет по предпосчитанным эндогенам
    hydro_task.solve();

    // Assert - расчитаны гидравлические параметры на проточных ребрах
    AssertFlowsSubgraphsHydroResultsFinite<hydro_solver>(hydro_task);
}

/// @brief Проверяет возможность решения гидравлической задачи на схеме, 
/// в которой индексация вершин не последовательная и начинается не с нуля
TEST(HydroOnlyTask, HandlesChainScheme_WhenVertexIndicesAreNonSequential)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Вполняем гидравлический расчет по предпосчитанным эндогенам
    // Arrange - Зачитка схемы из четырех последовательных задвижек
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<typename hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "chain_scheme_nonsequential_vertices");

    auto hydro_task = prepare_hydro_task_for_test_calc<hydro_solver>(
        hydro_siso_data, 50e3 /* разброс давлений */, 850 /* начальная плотность */);

    // Act - расчет с помощью МД
    hydro_task.solve();

    // Assert
    // Проверяем наличие плотности, рассчитанного расхода, давления в буферах
    const auto& result = hydro_task.get_result();

    AssertAllEdgesHydroResultFinite(result);
}
