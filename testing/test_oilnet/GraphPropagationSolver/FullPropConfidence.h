#include "test_oilnet.h"


TEST(FullPropagatableSolverConfidence, HandlesTailInconfidence) {
    using namespace oil_transport;
    using namespace graphlib;


    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_tail_source");
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, settings.structured_transport_solver.endogenous_options);


    graph_quantity_value_t src2_measure;
    src2_measure = siso_data.name_to_binding.at("Src2");
    src2_measure.type = measurement_type::vol_flow_std;
    src2_measure.data.value = 0.10;

    graph_quantity_value_t uzr_measure;
    uzr_measure = siso_data.name_to_binding.at("Uzr");
    uzr_measure.type = measurement_type::vol_flow_std;
    uzr_measure.data.value = 2.0;

    graph_quantity_value_t bik1_measure;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 0); // После поставщика на входе трубы
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 843;


    std::vector<graph_quantity_value_t> measurements{
        src2_measure, uzr_measure,
        bik1_measure
    };


    transport_graph_solver_t transport_graph_solver(
        graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver.graph_solver);
    auto [flow_measurements, endogenous] = classify_measurements(measurements);
    transport_graph_solver.solve(flow_measurements, endogenous);

    task_common_result_t<qsm_pipe_transport_solver_t> graph_result(graph);
    const pipe_common_result_t<qsm_pipe_transport_solver_t>* pipe_result = graph_result.pipes.front().second;

    bool is_confident_density_layer = pipe_result->layer->density_std.is_confident_layer();
    ASSERT_TRUE(is_confident_density_layer);

    models.reset_confidence();
    is_confident_density_layer = pipe_result->layer->density_std.is_confident_layer();
    ASSERT_FALSE(is_confident_density_layer);
}
