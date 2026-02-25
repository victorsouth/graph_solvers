#include "test_oilnet.h"




TEST(FullPropagatableSolverEndogenous, HandlesSchemeWithMixing) {
    using namespace oil_transport;
    using namespace graphlib;


    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, settings.structured_transport_solver.endogenous_options);


    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    graph_quantity_value_t uzr2_measure;
    uzr2_measure.type = measurement_type::vol_flow_std;
    uzr2_measure.data.value = 2.0;
    uzr2_measure = siso_data.name_to_binding.at("Uzr2");

    graph_quantity_value_t bik1_measure;
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 850;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 1);

    graph_quantity_value_t bik2_measure;
    bik2_measure.type = measurement_type::density_std;
    bik2_measure.data.value = 860;
    bik2_measure = graph_binding_t(graph_binding_type::Vertex, 4);



    std::vector<graph_quantity_value_t> measurements{
        uzr1_measure, uzr2_measure,
        bik1_measure, bik2_measure
    };


    transport_graph_solver_t transport_graph_solver(
        graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver.graph_solver);
    auto [flow_measurements, endogenous] = classify_measurements(measurements);
    double dt = 30;
    transport_graph_solver.step(dt, flow_measurements, endogenous);

    task_common_result_t<qsm_pipe_transport_solver_t> graph_result(graph);

    auto endogenous_at_pipe_inlet = *(get_single_edge_result(siso_data.name_to_binding.at("Vlv3").edge,
        graph_result.lumpeds)).at(1);
    auto density_at_pipe_inlet = endogenous_at_pipe_inlet.density_std.value;

    ASSERT_TRUE(bik1_measure.data.value <= density_at_pipe_inlet &&
        density_at_pipe_inlet <= bik2_measure.data.value);
    
}

/// @brief Провеяет возможность создания транспортных моделей на гидротранспортных буферах
TEST(FullPropagatableSolverEndogenous, HandlesModelsWithHydroTransportBuffers) {
    using namespace oil_transport;
    using namespace graphlib;

    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    // Схема
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");

    graph_json_data data;
    graph_siso_data siso_data;
    auto [models_with_buffers, transport_graph, hydro_graph] =
        load_hydro_transport_scheme<>(scheme_folder, "", settings, &data, &siso_data);


    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Vlv1");

    graph_quantity_value_t bik1_measure;
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 870;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 0); // измерение после притока

    std::vector<graph_quantity_value_t> measurements{
        uzr1_measure, bik1_measure
    };

    transport_graph_solver_t transport_graph_solver(
        transport_graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver.graph_solver);
    auto [flow_measurements, endogenous] = classify_measurements(measurements);
    double dt = 30;
    transport_graph_solver.step(dt, flow_measurements, endogenous);

    task_common_result_t<qsm_pipe_transport_solver_t> graph_result(transport_graph);

    auto endogenous_at_sink = *(get_single_edge_result(siso_data.name_to_binding.at("Snk").edge,
        graph_result.sources));
    auto density_at_sink = endogenous_at_sink.density_std.value;

    ASSERT_EQ(bik1_measure.data.value, density_at_sink);
}