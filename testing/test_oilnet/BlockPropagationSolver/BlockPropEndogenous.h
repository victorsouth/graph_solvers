#include "test_oilnet.h"

TEST(BlockEndogenousSolver, HandlesOrientedMeasurementsOnBridges) {

    using namespace oil_transport;
    using namespace graphlib;

    auto task_settings = oil_transport::transport_task_settings::default_values();
    task_settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");


    auto [models, graph] =
        graph_builder<qsm_pipe_transport_solver_t>::build_transport(
            siso_data, task_settings.structured_transport_solver.endogenous_options);

    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    graph_quantity_value_t uzr2_measure;
    uzr2_measure.type = measurement_type::vol_flow_std;
    uzr2_measure.data.value = 2.0;
    uzr2_measure = siso_data.name_to_binding.at("Uzr2");

    graph_quantity_value_t vlv3_measure;
    vlv3_measure.type = measurement_type::density_std;
    vlv3_measure.data.value = 840;
    vlv3_measure = graph_place_binding_t(graph_binding_type::Edge2, 4, graph_edge_side_t::Outlet);

    std::vector<graph_quantity_value_t> measurements{
        uzr1_measure, uzr2_measure,
        vlv3_measure
    };

    transport_block_solver_t<qsm_pipe_transport_solver_t> transport_block_solver(
        graph, task_settings.structured_transport_solver.endogenous_options, task_settings.structured_transport_solver.block_solver);
    auto [flow_measurements, endogenous] = classify_measurements(measurements);

    transport_block_solver.solve(flow_measurements, endogenous);

    task_common_result_t<qsm_pipe_transport_solver_t> graph_result(graph);

    auto endogenous_at_pipe_inlet = (*get_single_edge_result(siso_data.name_to_binding.at("Sink").edge,
        graph_result.sources));
    auto density_at_pipe_inlet = endogenous_at_pipe_inlet.density_std.value;

    ASSERT_TRUE(vlv3_measure.data.value <= density_at_pipe_inlet &&
        density_at_pipe_inlet <= vlv3_measure.data.value);

}