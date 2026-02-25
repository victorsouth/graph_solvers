#include "test_oilnet.h"


TEST(CustomSolver, DISABLED_Develop) {

    using namespace oil_transport;
    using namespace graphlib;

    auto task_settings = oil_transport::transport_task_settings::default_values();
    task_settings.structured_transport_solver.endogenous_options.temperature = true;

    graph_siso_data siso_data = 
        json_graph_parser_t::parse_transport<pde_solvers::qsm_noniso_T_properties_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");

    // TODO: Добавить в расчетный слой qsm_noniso_T_solver_t расход
    
    //auto [models, graph] =
    //    graph_builder<pde_solvers::qsm_noniso_T_solver_t>::build_transport(siso_data, task_settings);

    //graph_quantity_value_t uzr1_measure;
    //uzr1_measure.type = measurement_type::vol_flow_std;
    //uzr1_measure.data.value = 1.0;
    //uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    //graph_quantity_value_t uzr2_measure;
    //uzr2_measure.type = measurement_type::vol_flow_std;
    //uzr2_measure.data.value = 2.0;
    //uzr2_measure = siso_data.name_to_binding.at("Uzr2");

    //graph_quantity_value_t bik1_measure;
    //bik1_measure.type = measurement_type::temperature;
    //bik1_measure.data.value = 310;
    //bik1_measure = graph_binding_t(graph_binding_type::Vertex, 1);

    //graph_quantity_value_t bik2_measure;
    //bik2_measure.type = measurement_type::temperature;
    //bik2_measure.data.value = 320;
    //bik2_measure = graph_binding_t(graph_binding_type::Vertex, 4);

    //std::vector<graph_quantity_value_t> measurements{
    //    uzr1_measure, uzr2_measure,
    //    bik1_measure, bik2_measure
    //};


    //transport_graph_solver_t transport_graph_solver(
    //    graph, task_settings.structured_transport_solver.endogenous_options, task_settings.graph_solver);
    //double dt = 30;
    //auto [flow_measurements, endogenous] = classify_measurements(measurements);

    //transport_graph_solver.step(dt, flow_measurements, endogenous);

    
    //task_common_result_t<pde_solvers::qsm_noniso_T_solver_t> graph_result(graph);

    //auto endogenous_at_pipe_inlet = *(get_single_edge_result(siso_data.name_to_binding.at("Vlv3").edge,
    //    graph_result.lumpeds)).at(1);
    //auto density_at_pipe_inlet = endogenous_at_pipe_inlet.temperature.value;

    //ASSERT_TRUE(bik1_measure.data.value <= density_at_pipe_inlet &&
    //    density_at_pipe_inlet <= bik2_measure.data.value);

}