#include "test_oilnet.h"




TEST(FullPropagatableSolverFlow, HandlesSufficientMeasurements) {
    using namespace oil_transport;
    auto settings = oil_transport::transport_task_settings::default_values();

    oil_transport::graph_siso_data siso_data =
        oil_transport::json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, settings.structured_transport_solver.endogenous_options);


    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = oil_transport::measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    graph_quantity_value_t uzr2_measure;
    uzr2_measure.type = oil_transport::measurement_type::vol_flow_std;
    uzr2_measure.data.value = 2.0;
    uzr2_measure = siso_data.name_to_binding.at("Uzr2");


    std::vector<oil_transport::graph_quantity_value_t> measurements{
        uzr1_measure, uzr2_measure
    };

    transport_graph_flow_propagator_t flow_propagator(graph, settings.structured_transport_solver.graph_solver);
    auto [flows1, flows2, not_visited, useless_measurements] = flow_propagator.propagate(measurements);

    // Ветвь с УЗР1 имеет его расход
    ASSERT_NEAR(flows2[0], flows1[0], 1e-4); // УЗР1 и поставщик1
    ASSERT_NEAR(flows2[0], flows2[1], 1e-4); // УЗР1 и задвижка1
    // Ветвь с УЗР2 имеет его расход
    ASSERT_NEAR(flows2[2], flows1[1], 1e-4); // УЗР2 и поставщик2
    ASSERT_NEAR(flows2[2], flows2[3], 1e-4); // УЗР2 и задвижка2
    // Ветвь с трубой имеет суммарный расход
    ASSERT_NEAR(flows2[0] + flows2[2], flows2[4], 1e-4); // УЗР1 + УЗР2 и труба
    ASSERT_NEAR(flows2[0] + flows2[2], flows1[2], 1e-4); // УЗР1 + УЗР2 и потребитель
    // Нет непосещенных ребер
    ASSERT_TRUE(not_visited.empty());
    // Нет лишних замеров
    ASSERT_EQ(useless_measurements.size(), 0);
    
}
