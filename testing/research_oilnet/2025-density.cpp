#include <fixed/fixed.h>
#include <pde_solvers/pde_solvers.h>
#include <oil_transport.h>


#include "research_oilnet.h"


#include <filesystem>


//#include <boost/algorithm/string/split.hpp>
//#include <boost/algorithm/string/classification.hpp>
//#include <boost/date_time/local_time/local_time.hpp>
//#include <boost/units/base_units/metric/hour.hpp>
//#include <boost/units/base_units/temperature/celsius.hpp>
//#include <boost/units/physical_dimensions/temperature.hpp>
//#include <boost/units/systems/temperature/celsius.hpp>
//#include <boost/units/systems/si.hpp>

#include "boost/pfr.hpp"

/// @brief Представление временных рядов по объемному расходу и плотности
/// Очень частное представление - один вход, один выход
/// Используется для схемы с разветвлением, поскольку на ответвлении все равно нет данных
struct timeseries_data {
    /// @brief Плотность на входе
    std::vector<double> density_in;
    /// @brief Плотность на выходе
    std::vector<double> density_out;
    /// @brief Объемный расход на входе
    std::vector<double> volflow_in;
    /// @brief Объемный расход на выходе
    std::vector<double> volflow_out;
    /// @brief Конструктор из файла
    /// @param filename Имя файла с данными временного ряда
    timeseries_data(const char* filename)
    {
        std::ifstream file(filename); // Открываем файл для чтения
        std::string line;

        std::vector<std::vector<double>> data;

        while (getline(file, line)) {
            std::stringstream ss(line);
            std::string value;
            size_t k = 0;
            while (getline(ss, value, ',')) {
                if (k == data.size()) { // Если вектора для столбца ещё нет, создаём его
                    data.emplace_back();
                }
                data[k].push_back(std::stod(value)); // Записываем значение в столбец
                k++; //1825
            }
        }

        file.close();

        density_in = data[1];
        density_out = data[0];
        volflow_in = data[3];
        volflow_out = data[2];

        for (double& q : volflow_in) {
            q /= 3600;
        }
        for (double& q : volflow_out) {
            q /= 3600;
        }

    }
    /// @brief Возвращает Dt, гарантирующий, что по всем трубам системы 
    /// не будет Cr > 1
    /// Пока что очень частное решение на основе одной трубы
    /// @param data 
    /// @return 
    double calc_ideal_dt(std::vector <pde_solvers::PipeQAdvection>& models) const {
        //double v = models[1].getEquationsCoeffs(0, 0); // !!! В РУЧНУЮ ДЛЯ 1-ого РЕБРА
        auto it = std::max_element(volflow_in.begin(), volflow_in.end(),
            [](double q1, double q2) {
                return abs(q1) < abs(q2);
            });

        double Q_max = abs(*it);

        double v_max = Q_max / models[1].get_pipe().wall.getArea();
        const auto& x = models[1].get_grid();
        double dx = x[1] - x[0];

        double dt_ideal = dx / v_max;
        return dt_ideal;
    }
};




TEST(QSMFlowsDensity, Research)
{
    // TODO: Починить
    /*

    using namespace graphlib;
    using namespace oil_transport;

    std::string path = prepare_test_folder();
    timeseries_data data((path + "sikn_02_2024.csv").c_str());

    auto task_settings = task_full_propagatable_settings::get_default_settings();
    transport_graph_siso_data siso_data =
        json_graph_parser_t::parse<qsm_pipe_transport_parameters_t>(path + "cold_tu");    
    make_pipes_uniform_profile_handled<qsm_pipe_transport_parameters_t>(
        task_settings.pipe_coordinate_step, &siso_data.transport_props.second);

    auto [measurement_bindings, measurement_types, measurement_tags] = 
        siso_data.prepare_measurements_tags_and_bindings();

    auto tag_timeseries = graph_vector_timeseries_t::from_files(
        "01.08.2021 00:00:00", 
        "01.09.2021 00:00:00",
        path + "cold_tu/tags_db", measurement_tags, 
        measurement_bindings, measurement_types);

    std::time_t t = tag_timeseries.get_start_date();
    auto measurement_layer = tag_timeseries(t);

    oil_transport::task_full_propagatable task(siso_data, task_settings);
    double dt = 60;
    task.step(dt, measurement_layer);


    // Есть подозрение, что юзкейсы будут странные
    // Как хардкодом задать замер? Информация об именах теряется. Надо делать по имени.

    */
}




TEST(QSMFlowsDensity, Flows_CondensatePipe)
{
    using namespace oil_transport;

    std::string scheme_folder = get_scheme_folder("condensate_pipe/density_scheme_valves");
    std::string tags_filename = scheme_folder + "/tags_data_flows_only.json";
    std::string tags_folder = get_schemes_folder() + "condensate_pipe/tags_db/cleaned";
    std::string corrected_flows_folder = prepare_test_folder("flows_corrected");
    std::string research_outfile_name = prepare_test_folder() + "flows.csv";

    auto task_settings = transport_task_settings::default_values();
 
    auto [models, graph, measurements_data] = prepare_task_and_measurements(
        task_settings, scheme_folder, tags_filename, tags_folder);

    auto& timeseries = measurements_data.timeseries.get_time_series_object();

    double dt = 3600;
    auto [times, astro_times] = time_grid_generator::prepare_time_grid(
        dt, timeseries.get_start_date(), timeseries.get_end_date());

    auto batch_measurements = prepare_batch_measurements(
        measurements_data.timeseries, astro_times);

    batch_flow_balance_analyzer processor(graph, astro_times, 
        graphlib::redundant_flows_corrector_settings_t::default_values());
    transport_batch::perform(processor, times, batch_measurements);

    processor.write_result(research_outfile_name);
    processor.write_corrected_flows(
        corrected_flows_folder, measurements_data.measurement_tags);

}


TEST(QSMFlowsDensity, Density_CondensatePipe)
{
    using namespace oil_transport;

    std::string scheme_folder = get_schemes_folder() + "condensate_pipe/density_scheme";
    std::string tags_filename = scheme_folder + "/tags_data_flows_density_final.json";
    std::string tags_folder = get_schemes_folder() + "condensate_pipe/tags_db/flows_recon_and_extended";

    auto task_settings = transport_task_settings::default_values();
    task_settings.structured_transport_solver.rebind_zeroflow_measurements = false;
    task_settings.structured_transport_solver.block_solver.use_simple_block_flow_heuristic = true;
    task_settings.structured_transport_solver.endogenous_options.density_std = true;

    auto [models, graph, measurements_data] = prepare_task_and_measurements(
        task_settings, scheme_folder, tags_filename, tags_folder);

    double dt = 60;

    auto batch_settings = batch_control_settings_t::get_default_settings();
    transport_batch_data batch_data(graph, batch_settings, dt, measurements_data);
    transport_task_block_tree<qsm_pipe_transport_solver_t> task(std::move(models), std::move(graph), task_settings);

    flow_ident_settings ident_settings;
    ident_settings.flow_to_correct = std::vector<oil_transport::graph_quantity_binding_t>(2);
    ident_settings.flow_to_correct[0] = batch_data.flow_measurements[0][0];
    ident_settings.flow_to_correct[1] = batch_data.flow_measurements[0][1];

    flow_ident_criteria ident(task, batch_data, ident_settings);
    Eigen::VectorXd ident_params = ident.estimation();

    // Чисто для отладки
    //auto r = ident.residuals(ident_params);
    //auto J = ident.jacobian_dense(ident_params);

    {
        //fixed_solver_parameters_t<-1, 0, golden_section_search> parameters;
        //parameters.line_search.function_decrement_factor = 1.0;
        //parameters.line_search.iteration_count = 2;
        //fixed_solver_result_t<-1> result;
        //fixed_optimize_gauss_newton::optimize(ident, ident_params, parameters, &result);
        //if (result.result_code != numerical_result_code_t::Converged) {
        //    throw std::runtime_error("Gauss-Newton optimizer not converged");
        //}
        //ident_params = result.argument;
    }

    auto [corrected_flows, processor] = ident.perform_batch(ident_params);


    auto calculations = processor.get_calculations();
    auto measurements = processor.get_measurements();
    //auto confident_indices = processor.get_confident_indices();
    std::string calc_folder = prepare_test_folder("output/calc");
    write_csv_tag_files(calc_folder, batch_data.astro_times, calculations);
    std::string meas_folder = prepare_test_folder("output/measurements");
    write_csv_tag_files(meas_folder, batch_data.astro_times, measurements);


    // Запуск без идентификации
    /*batch_endogenous_scenario processor(task, batch_data.astro_times);
    oil_transport::transport_batch::perform(processor,
        batch_data.times, batch_data.flow_measurements, batch_data.endogenous_measurements, batch_data.controls);*/


}

TEST(QSMFlowsDensity, Density_hottu)
{
    using namespace oil_transport;

    std::string scheme_folder = get_schemes_folder() + "hot_tu1/scheme"; 
    std::string tags_filename = scheme_folder + "/tags_data_flows_density_final.json";
    std::string tags_folder = get_schemes_folder() + "hot_tu1/tags_db/flows_recon_and_extended";

    auto task_settings = transport_task_settings::default_values();
    task_settings.structured_transport_solver.rebind_zeroflow_measurements = false;
    task_settings.structured_transport_solver.block_solver.use_simple_block_flow_heuristic = true;
    task_settings.structured_transport_solver.endogenous_options.density_std = true;

    auto [models, graph, measurements_data] = prepare_task_and_measurements(
        task_settings, scheme_folder, tags_filename, tags_folder);

    double dt = 60;

    // Пакетный расчет
    auto batch_settings = batch_control_settings_t::get_default_settings();
    transport_batch_data batch_data(graph, batch_settings, dt, measurements_data);

    transport_task_block_tree<qsm_pipe_transport_solver_t> task(std::move(models), std::move(graph), task_settings);

    batch_endogenous_scenario processor(task, batch_data.astro_times);
    oil_transport::transport_batch::perform(processor,
        batch_data.times, batch_data.flow_measurements, batch_data.endogenous_measurements, batch_data.controls);

    auto calculations = processor.get_calculations();
    auto measurements = processor.get_measurements();
    std::string calc_folder = prepare_test_folder("output/calc");
    write_csv_tag_files(calc_folder, batch_data.astro_times, calculations);
    std::string meas_folder = prepare_test_folder("output/measurements");
    write_csv_tag_files(meas_folder, batch_data.astro_times, measurements);
}

TEST(QSMFlowsDensity, DensityHotTuProfilesJson)
{
    using namespace oil_transport;

    std::string scheme_folder = get_schemes_folder() + "hot_tu1/scheme"; 
    std::string tags_filename = scheme_folder + "/tags_data_flows_density_final.json";
    std::string tags_folder = get_schemes_folder() + "hot_tu1/tags_db/flows_recon_and_extended";

    auto task_settings = transport_task_settings::default_values();
    task_settings.structured_transport_solver.rebind_zeroflow_measurements = false;
    task_settings.structured_transport_solver.block_solver.use_simple_block_flow_heuristic = true;
    task_settings.structured_transport_solver.endogenous_options.density_std = true;

    // Получаем данные схемы для создания маппинга ребер
    graph_siso_data siso_data = json_graph_parser_t::parse_transport<qsm_pipe_transport_solver_t::pipe_parameters_type>(
        scheme_folder);
    
    // Создание маппинга идентификаторов объектов из данных схемы
    std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash> 
        edge_id_to_object_name = siso_data.create_edge_binding_to_name();

    auto [models, graph, measurements_data] = prepare_task_and_measurements(
        task_settings, scheme_folder, tags_filename, tags_folder);

    double dt = 60;

    // Пакетный расчет
    auto batch_settings = batch_control_settings_t::get_default_settings();
    transport_batch_data batch_data(graph, batch_settings, dt, measurements_data);

    transport_task_block_tree<qsm_pipe_transport_solver_t> task(std::move(models), std::move(graph), task_settings);

    // Создание процессора с записью профилей в JSON
    std::string json_output_path = prepare_test_folder("output") + "layers_results.json";
    std::ofstream json_output_file(json_output_path, std::ios::trunc);
    if (!json_output_file.is_open()) {
        throw std::runtime_error("Failed to open JSON output file: " + json_output_path);
    }
    
    batch_serializing_result<oil_transport::transport_task_block_tree<qsm_pipe_transport_solver_t>> processor(
        task, batch_data.astro_times, edge_id_to_object_name, json_output_file, 
        quasi_stationary_regime_selection_strategy::EveryHour);

    oil_transport::transport_batch::perform(processor,
        batch_data.times, batch_data.flow_measurements, batch_data.endogenous_measurements, batch_data.controls);


    // JSON-файл с результатами создан по пути json_output_path
}

/// @brief Гидравлический расчет для квазистационарных режимов, выделенных при транспортном расчете
TEST(QSMFlowsDensity, DISABLED_DensityHotTuHydroTransportCalc)
{
    using namespace oil_transport;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;
    using transport_solver = qsm_pipe_transport_solver_t;

    // TODO: файл \testing\oiltransport_schemes\hot_tu1\scheme\valve_data.json поправить
    std::string scheme_folder = get_schemes_folder() + "hot_tu1/scheme";
    std::string tags_filename = scheme_folder + "/tags_data_flows_density_final.json";
    std::string tags_folder = get_schemes_folder() + "hot_tu1/tags_db/FOR_TESTING_ONLY_flows_reduced";

    // Зачитка схемы
    auto [transport_siso_data, hydro_siso_data] =
        json_graph_parser_t::parse_hydro_transport<transport_solver::pipe_parameters_type, hydro_solver::pipe_parameters_type>(
            scheme_folder);

    auto transport_task_settings = transport_task_settings::default_values();
    transport_task_settings.structured_transport_solver.rebind_zeroflow_measurements = false;
    transport_task_settings.structured_transport_solver.block_solver.use_simple_block_flow_heuristic = true;
    transport_task_settings.structured_transport_solver.endogenous_options.density_std = true;

    // 1. Изолированный пакетный транспортный расчет
    // 1.1 Зачитка данных пакетного расчета
    auto [transport_models, transport_graph, measurements_data, controls_data] = prepare_task_and_measurements_and_controls(
        transport_task_settings, scheme_folder, tags_filename, tags_folder);

    double dt = 30;

    auto batch_settings = batch_control_settings_t::get_default_settings();
    transport_batch_data batch_data_transport(transport_graph, batch_settings, dt, measurements_data);

    // 1.3 Создание таска и пакетного процессора
    transport_task_block_tree<qsm_pipe_transport_solver_t> task(std::move(transport_models), std::move(transport_graph), transport_task_settings);
    
    // 1.2 Заполнение буферов недостоверной нефтью
    auto initial_measurements_layer = batch_helpers::build_measurement_layer(batch_data_transport.flow_measurements.front(),
        batch_data_transport.endogenous_measurements.front());
    task.solve(initial_measurements_layer, true);

    // Создание процессора с записью профилей в JSON
    std::string json_endo_result_out_path = prepare_test_folder("output") + "layers_results.json";
    std::ofstream json_endo_out_file(json_endo_result_out_path, std::ios::trunc);
    if (!json_endo_out_file.is_open()) {
        throw std::runtime_error("Failed to open JSON output file: " + json_endo_result_out_path);
    }

    // Создаем маппинг от edge_id к имени объекта из topology.json (включая трубы, sources, lumpeds)
    auto edge_id_to_object_name = transport_siso_data.create_edge_binding_to_name();

    batch_serializing_result<oil_transport::transport_task_block_tree<qsm_pipe_transport_solver_t>> transport_processor(
        task, batch_data_transport.astro_times,
        edge_id_to_object_name,
        json_endo_out_file,
        quasi_stationary_regime_selection_strategy::EveryHour);

    // 1.4 Непосредственный расчет
    oil_transport::transport_batch::perform(transport_processor,
        batch_data_transport.times, batch_data_transport.flow_measurements,
        batch_data_transport.endogenous_measurements, batch_data_transport.controls);

    json_endo_out_file.flush();
    json_endo_out_file.close();

    // 2. Изолированный пакетный гидравлический расчет
    auto hydro_settings = hydro_task_settings::default_values();
    make_pipes_uniform_profile_handled<hydro_solver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    auto [hydro_models, hydro_graph] =
        graph_builder<pde_solvers::iso_nonbaro_pipe_solver_t>::build_hydro(hydro_siso_data);

    hydro_batch_data hydro_batch_data(batch_data_transport, controls_data, hydro_graph);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    // Создание процессора с записью профилей в JSON
    std::string hydro_json_output_path = prepare_test_folder("output") + "hydro_layers_results.json";
    std::ofstream hydro_json_output_file(hydro_json_output_path, std::ios::trunc);
    if (!hydro_json_output_file.is_open()) {
        throw std::runtime_error("Failed to open JSON output file: " + hydro_json_output_path);
    }

    // Поток для чтения результатов эндогенного рассчета
    std::ifstream json_endo_in_file(json_endo_result_out_path);
    if (!json_endo_in_file.is_open()) {
        throw std::runtime_error("Failed to open JSON output file: " + json_endo_result_out_path);
    }

    quasi_stationary_regimes_hydro_processor<hydro_task_t> hydro_processor(
        hydro_task, hydro_batch_data.astro_times,
        edge_id_to_object_name,
        json_endo_in_file,
        hydro_json_output_file);
    
    //oil_transport::hydro_batch::perform(hydro_processor,
    //    hydro_batch_data.times, hydro_batch_data.controls);

    hydro_json_output_file.flush();
    hydro_json_output_file.close();

    json_endo_in_file.close();

}