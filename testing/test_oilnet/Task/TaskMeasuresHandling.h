#include "test_oilnet.h"

/// @brief Проверяет корректность выдачи результата эндогенного результата, 
/// когда используется привязка измерения к концу ребра
TEST(TaskMeasuresHandling, RoutesSidedEndognous) {
    using namespace oil_transport;
    using namespace graphlib;


    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_tail_source");
    siso_data.get_properties<transport_source_parameters_t>("Src2").is_initially_zeroflow = true;

    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    graph_quantity_value_t src1_measure(siso_data.name_to_binding.at("Src1"),
        measurement_type::vol_flow_std, 1.0);
    graph_quantity_value_t uzr_measure(
        /* Здесь привязка на выход (Outlet) ребра */
        graphlib::graph_place_binding_t(siso_data.name_to_binding.at("Uzr"), graphlib::graph_edge_side_t::Outlet),
        measurement_type::density_std, 844.0
    );

    std::vector<graph_quantity_value_t> measurements{ src1_measure, uzr_measure };

    transport_task_block_tree<qsm_pipe_transport_solver_t> task(siso_data, settings);
    auto solve_result = task.solve({ src1_measure }, { uzr_measure }, false);
    const auto& output_quantity = solve_result.measurement_status.endogenous.front();

    bool are_equal_quantity_bindings =
        static_cast<const graph_quantity_binding_t&>(output_quantity) ==
        static_cast<const graph_quantity_binding_t&>(uzr_measure);

    ASSERT_TRUE(are_equal_quantity_bindings);

}

/// @brief Проверяет наличие исключения при попытке выполнить транспортный расчет 
/// с измерениями расхода, привязанными к несуществующим объектам
TEST(TaskMeasuresHandling, ThrowsExceptionIfNoFlowsPassed) {
    using namespace oil_transport;
    using namespace graphlib;


    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_tail_source");

    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    std::vector<graph_quantity_value_t> fake_measurements{ 
        { graph_binding_t(graphlib::graph_binding_type::Edge2, 1234), measurement_type::vol_flow_std, 1.0},
        { graph_binding_t(graphlib::graph_binding_type::Edge1, 5678), measurement_type::density_std, 1000 }
    };

    transport_task_block_tree<qsm_pipe_transport_solver_t> task(siso_data, settings);

    // Act, Assert - Ожидаем исключение
    EXPECT_ANY_THROW(task.solve(fake_measurements, false));
}
