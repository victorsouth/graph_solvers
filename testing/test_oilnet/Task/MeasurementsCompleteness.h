#include "test_oilnet.h"


/// @brief Способность обрабатывать измерения при отключении/подключении источника
/// Измерения заданы только по ветви с первым поставщиком (из двух). 
/// Подключаем второй поставщик с неизвестным расходом - измерений недостаточно
TEST(MeasurementCompleteness, HandlesSourceSwitching) {
    using namespace oil_transport;
    using namespace graphlib;

    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");

    //auto [models, graph] = transport_graph_builder::build(siso_data, settings);

    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    graph_quantity_value_t bik1_measure;
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 850;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 1);

    std::vector<graph_quantity_value_t> measurements{
        uzr1_measure, 
        bik1_measure, 
    };
    std::vector < std::vector<graph_quantity_value_t>> measurements_batch{
        measurements, measurements };

    graph_binding_t src2_binding = siso_data.name_to_binding.at("Src2");
    auto src2 = dynamic_cast<transport_source_parameters_t*>(
        siso_data.transport_props.first[src2_binding.edge].get());
    src2->is_initially_zeroflow = true; // изначально отключаем второй поставщик - одного расхода становится достаточно

    auto source_control = std::make_unique<transport_source_control_t>();
    source_control->binding = src2_binding;
    source_control->is_zeroflow = false; // включаем 
    std::vector<std::unique_ptr<transport_object_control_t>> controls_batch(1);
    controls_batch[0] = std::move(source_control);

    transport_task_full_propagatable<qsm_pipe_transport_solver_t> task(siso_data, settings);
    auto completeness = 
        check_measurements_completeness(task, controls_batch, measurements_batch);

    ASSERT_TRUE(completeness[0].empty());
    ASSERT_FALSE(completeness[1].empty());
}



/// @brief Способность обрабатывать измерения при отключении/подключении источника
/// Измерения заданы только по ветви с первым поставщиком (из двух). 
/// Подключаем второй поставщик с неизвестным расходом - измерений недостаточно
TEST(MeasurementCompleteness, HandlesValveSwitching) {
    using namespace oil_transport;
    using namespace graphlib;

    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_two_sources_no_bik");

    //auto [models, graph] = transport_graph_builder::build(siso_data, settings);

    graph_quantity_value_t uzr1_measure;
    uzr1_measure.type = measurement_type::vol_flow_std;
    uzr1_measure.data.value = 1.0;
    uzr1_measure = siso_data.name_to_binding.at("Uzr1");

    graph_quantity_value_t bik1_measure;
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 850;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 1);

    std::vector<graph_quantity_value_t> measurements{
        uzr1_measure, bik1_measure,
    };
    std::vector < std::vector<graph_quantity_value_t>> measurements_batch{
        measurements, measurements };

    // изначально отключаем второй поставщик за счет задвижки,
    // одного расхода становится достаточно
    graph_binding_t vlv2_binding = siso_data.name_to_binding.at("Vlv2");
    auto vlv2 = dynamic_cast<transport_lumped_parameters_t*>(
        siso_data.transport_props.second[vlv2_binding.edge].get());
    vlv2->is_initially_opened = false; 

    auto source_control = std::make_unique<transport_lumped_control_t>();
    source_control->binding = vlv2_binding;
    source_control->is_opened = true; // открываем задвижку ко второму поставщику
    std::vector<std::unique_ptr<transport_object_control_t>> controls_batch(1);
    controls_batch[0] = std::move(source_control);

    transport_task_full_propagatable<qsm_pipe_transport_solver_t> task(siso_data, settings);
    auto completeness =
        check_measurements_completeness(task, controls_batch, measurements_batch);

    ASSERT_TRUE(completeness[0].empty());
    ASSERT_FALSE(completeness[1].empty());
}