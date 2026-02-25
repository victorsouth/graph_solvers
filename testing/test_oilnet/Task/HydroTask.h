#include "test_oilnet.h"
#include "utils/task_test_utils.h"

TEST(HydroOnlyTask, UseCase)
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

    // Создаем измерения
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

    // ВЫполняем гидравлический расчет по предпосчитанным эндогенам
    auto hydro_settings = hydro_task_settings::default_values();
    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    storage.restore_results(hydro_task.get_graph(), N-1, edge_id_to_object_name);
    hydro_task.solve();

    // Assert
    // Проверяем наличие плотности, рассчитанного расхода, давления в буферах
    const auto& result = hydro_task.get_result();

    const auto& vlv1_buffer_in = result.lumpeds.at(0).second[0];
    const auto& vlv1_buffer_out = result.lumpeds.at(0).second[1];
    const auto& vlv2_buffer_out = result.lumpeds.at(1).second[1];
    
    ASSERT_NEAR(vlv1_buffer_in->vol_flow, vlv2_buffer_out->vol_flow, 1e-6);

    double pressure_vertex0 = vlv1_buffer_in->pressure;
    double pressure_vertex1 = vlv1_buffer_out->pressure;
    double pressure_vertex2 = vlv2_buffer_out->pressure;

    ASSERT_BETWEEN(pressure_vertex1, pressure_vertex0, pressure_vertex2);

    ASSERT_EQ(vlv2_buffer_out->density_std.value, density_measure);
}

TEST(HydroOnlyTask, PipeUseCase)
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

    // ВЫполняем гидравлический расчет по предпосчитанным эндогенам

    graph_siso_data siso_data_hydro =
        json_graph_parser_t::parse_hydro<hydro_solver::pipe_parameters_type>(
            get_schemes_folder() + "hydro_pipe_test");

    auto hydro_settings = hydro_task_settings::default_values();

    // TODO: объединить формирования профиля и для транспортных, и для гидравлических свойств. Почему они вообще отдельно хранятся для трубы???
    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &siso_data_hydro.hydro_props.second);

    hydro_task_t hydro_task(siso_data_hydro, hydro_settings);

    storage.restore_results(hydro_task.get_graph(), N - 1, edge_id_to_object_name);
    hydro_task.solve();

    // Assert
    // Проверяем наличие плотности, рассчитанного расхода, давления в буферах
    const auto& result = hydro_task.get_result();

    AssertSourcesHydroResultFinite(result);
    AssertLumpedHydroResultFinite(result);
    AssertPipesHydroResultFinite(result);

    // Проверяем наличие нескольких партий в трубе
    const auto& layer = *result.pipes.front().second->layer;
    for (const auto& d : layer.density_std.value) {
        ASSERT_BETWEEN(d, initial_density, density_measure);
    }
}