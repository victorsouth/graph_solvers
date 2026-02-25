#include "test_oilnet.h"


TEST(BlockSolverConfidence, HandlesTailInconfidence) {
    using namespace oil_transport;
    using namespace graphlib;


    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;

    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_tail_source");
    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);
    auto [models, graph] = graph_builder<qsm_pipe_transport_solver_t>::build_transport(
        siso_data, settings.structured_transport_solver.endogenous_options);


    graph_quantity_value_t src2_measure(siso_data.name_to_binding.at("Src2"),
        measurement_type::vol_flow_std, 0.10);

    graph_quantity_value_t uzr_measure;
    uzr_measure.type = measurement_type::vol_flow_std;
    uzr_measure.data.value = 2.0;
    uzr_measure = siso_data.name_to_binding.at("Uzr");

    graph_quantity_value_t bik1_measure;
    bik1_measure.type = measurement_type::density_std;
    bik1_measure.data.value = 843;
    bik1_measure = graph_binding_t(graph_binding_type::Vertex, 0); // После поставщика на входе трубы


    std::vector<graph_quantity_value_t> measurements{
        src2_measure, uzr_measure, bik1_measure
    };

    transport_block_solver_t<qsm_pipe_transport_solver_t> transport_block_solver(
        graph, settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver.block_solver);
    constexpr double static_time_step = std::numeric_limits<double>::infinity();

    auto [flow_measurements, endogenous] = classify_measurements(measurements);
    transport_block_solver.step(static_time_step, flow_measurements, endogenous);

    task_common_result_t<qsm_pipe_transport_solver_t> graph_result(graph);
    const pipe_common_result_t<qsm_pipe_transport_solver_t>* pipe_result = graph_result.pipes.front().second;

    bool is_confident_density_layer = pipe_result->layer->density_std.is_confident_layer();
    ASSERT_TRUE(is_confident_density_layer);

    models.reset_confidence();
    is_confident_density_layer = pipe_result->layer->density_std.is_confident_layer();
    ASSERT_FALSE(is_confident_density_layer);
}

auto prepare_single_pipe_scheme() {

    using namespace oil_transport;
    using namespace graphlib;

    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    settings.pipe_coordinate_step = 200;
    
    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "pipe_with_tail_source");

    auto& src2_properties = siso_data.get_properties<transport_source_parameters_t>("Src2");
    src2_properties.is_initially_zeroflow = true;

    auto& pipe_properties = siso_data.get_properties<qsm_pipe_transport_parameters_t>("Pipe");
    pipe_properties.profile.coordinates = { 0.0, 1000.0 };

    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    return siso_data;
}

/// @brief Оценка времени вытеснения начальной партии в трубе
inline double get_pipe_displacement_time(double diameter, double pipe_length, double vol_flow) {
    double S = pde_solvers::circle_area(diameter);
    double Q = vol_flow;
    double v = Q / S;
    double L = pipe_length;
    double T_displacement = L / v;
    return T_displacement;
}

inline auto calculate_displacement_time(
    oil_transport::transport_task_block_tree<oil_transport::qsm_pipe_transport_solver_t>& task,
    const oil_transport::transport_batch_data& batch_data
    )
{
    oil_transport::batch_endogenous_scenario processor(task, batch_data.astro_times);

    // здесь отдельно расходы и эндогены!
    oil_transport::transport_batch::perform(processor, batch_data.times,
        batch_data.flow_measurements, batch_data.endogenous_measurements, batch_data.controls);

    auto calculations = processor.get_calculations();

    auto confident_indices = processor.get_confident_indices();
    // в принципе достоверным становится только один параметр (по идее, выходная плотность)
    if (confident_indices.size() != 1) {
        throw std::runtime_error("It must be single confident output parameter");
    }

    oil_transport::graph_quantity_binding_t calc_binding = confident_indices.begin()->first;


    // проверяем, что временной становится достоверным в предсказанное время
    auto sink_density_confident_indices = confident_indices.at(calc_binding);
    if (sink_density_confident_indices.empty()) {
        throw std::runtime_error("No displacement reached");
    }

    double dt = batch_data.times[1] - batch_data.times[0];
    double T_displacement_calc = dt * sink_density_confident_indices.front();

    return std::make_pair(calc_binding, T_displacement_calc);

}

/// @brief Проверяет способность отслеживать расчетную достоверность эндогенного параметра
TEST(BlockSolverConfidence, HandlesDynamicConfidence) {
    using namespace oil_transport;

    oil_transport::graph_siso_data siso_data = prepare_single_pipe_scheme();

    auto settings = oil_transport::transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    auto [models, graph] = 
        graph_builder<qsm_pipe_transport_solver_t>::build_transport(
            siso_data, settings.structured_transport_solver.endogenous_options);

    graph_quantity_value_t src1_density_measure(siso_data.name_to_binding.at("Src1"),
        measurement_type::density_std, 832);
    graph_quantity_value_t sink_density_measure(siso_data.name_to_binding.at("Sink"),
        measurement_type::density_std, 832);
    graph_quantity_value_t uzr_flow_measure(siso_data.name_to_binding.at("Uzr"),
        measurement_type::vol_flow_std, 2.0);

    auto& pipe_properties = siso_data.get_properties<qsm_pipe_transport_parameters_t>("Pipe");

    double T_displacement = get_pipe_displacement_time(
        pipe_properties.diameter, pipe_properties.profile.get_length(), uzr_flow_measure.data.value);
    constexpr double dt = 60;
    std::size_t N = (std::size_t)(2 * T_displacement / dt);

    transport_batch_data batch_data = transport_batch_data::constant_batch(
        dt, N, { uzr_flow_measure }, { src1_density_measure, sink_density_measure });
    transport_task_block_tree<qsm_pipe_transport_solver_t> task(siso_data, settings);

    auto [calc_binding, T_displacement_calc] = calculate_displacement_time(task, batch_data);
    // проверяем, что единственный достоверный параметры действительно выходная плотность
    bool is_output_density =
        calc_binding ==
        static_cast<const oil_transport::graph_quantity_binding_t&>(sink_density_measure);
    ASSERT_TRUE(is_output_density);

    double T_error = T_displacement_calc - T_displacement;
    ASSERT_LE(std::abs(T_error), dt);
}


/// @brief Проверяет способность отслеживать расчетную достоверность эндогенного параметра hotTU
TEST(BlockSolverConfidence, HandlesDynamicConfidence_HotTU1) {
    using namespace oil_transport;
    using namespace graphlib;

    // Зарузка схемы
    //std::cout << "Trying to load scheme from: " << get_schemes_folder() + "hot_tu1" << std::endl;
    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "hot_tu1/scheme");
    
    // от ВВ
    auto settings = transport_task_settings::default_values();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    settings.pipe_coordinate_step = 200;

    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    siso_data.get_properties<transport_lumped_parameters_t>("Src1_V1").is_initially_opened = false;
    siso_data.get_properties<transport_lumped_parameters_t>("Src2_V1").is_initially_opened = false;

    //

    auto [models, graph] =
        graph_builder<qsm_pipe_transport_solver_t>::build_transport(
            siso_data, settings.structured_transport_solver.endogenous_options);

    graph_quantity_value_t rp1_density(
        siso_data.name_to_binding.at("RP1"),
        measurement_type::density_std, 832);

    graph_quantity_value_t nps1_flow(
        siso_data.name_to_binding.at("NPS1"),
        measurement_type::vol_flow_std, 1.0);

    graph_quantity_value_t rp2_density(
        siso_data.name_to_binding.at("RP2"),
        measurement_type::density_std, 830);

    std::vector<std::string> pipeline_names = {
        "LU1.1",
        "LU1.2A",   
        "LU1.3",
        "LU1.4A",   
        "LU1.5",
        "LU2.1",
        "LU2.2A",
        "LU2.3",
        "LU3.1",
        "LU4.1",
        "LU4.2A",
        "LU4.3",
        "LU5.1",
        "LU6.1",
        "LU6.2A",
        "LU6.3",
    };

    double total_length = 0.0;
    for (const auto& name : pipeline_names) {
        const auto& props = siso_data.get_properties<qsm_pipe_transport_parameters_t>(name);
        total_length += props.profile.get_length();
    }

    double diameter = 0.0;
    {
        const auto& props = siso_data.get_properties<qsm_pipe_transport_parameters_t>(pipeline_names.front());
        diameter = props.diameter;
    }

    double flow_rate = nps1_flow.data.value;
    double T_displacement = get_pipe_displacement_time(
        diameter, total_length, flow_rate);

    constexpr double dt = 60.0;
    std::size_t N = static_cast<std::size_t>(2 * T_displacement / dt);

    transport_batch_data batch_data = transport_batch_data::constant_batch(
        dt, N,
        { nps1_flow },   
        { rp1_density, rp2_density }
    );

    transport_task_block_tree<qsm_pipe_transport_solver_t> task(siso_data, settings);

    auto [calc_binding, T_displacement_calc] = calculate_displacement_time(task, batch_data);

    bool is_output_density =
        calc_binding == static_cast<const graph_quantity_binding_t&>(rp2_density);
    ASSERT_TRUE(is_output_density);

    double T_error = T_displacement_calc - T_displacement;
    ASSERT_LE(std::abs(T_error), T_displacement * 0.02); // экспертно задали 2% ошибку, было 1.25%

    // Дебаг
    std::cout << "Общая длина = " << total_length << " m\n";
    std::cout << "Проверочная = " << T_displacement / 3600.0 << " h\n";
    std::cout << "Расчетная = " << T_displacement_calc / 3600.0 << " h\n";
}


