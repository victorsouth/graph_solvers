#pragma once

#include "test_oilnet.h"

inline oil_transport::graph_siso_data prepare_pump_chain_scheme() {

    using namespace oil_transport;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - схема "Поставщик - насос - потребитель"
    source_json_data src1 = source_json_data::default_values();
    src1.std_volflow = 0.1;

    pump_json_data pump = pump_json_data::default_values();

    source_json_data snk = source_json_data::default_values();
    snk.pressure = 20e5;


    std::string scheme_dir = prepare_test_folder("chain_scheme_test");
    using Generator = chain_scheme_generator_t<source_json_data, pump_json_data, source_json_data>;
    Generator::generate(std::make_tuple(src1, pump, snk), scheme_dir);

    // Зачитка созданной схемы
    auto hydro_siso_data =
        json_graph_parser_t::parse_hydro<hydro_solver::pipe_parameters_type>(
            scheme_dir);
    
    return hydro_siso_data; 
}

TEST(IsoQsmPump, CreatesHeadDifference_WhilePumpIsWorking) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using hydro_solver = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - схема "Поставщик - насос - потребитель"
    auto hydro_siso_data = prepare_pump_chain_scheme();

    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = 50e3;
    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    // Задание начального распределения плотности
    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = 850;
    init_endogenous_in_buffers<hydro_solver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    // Act - гидравлический расчет схемы
    hydro_task.solve();

    // Assert - Pвых > Pвх
    const auto& result = hydro_task.get_result();

    double Pin = result.sources.front().second->pressure;
    double Pout = result.sources.back().second->pressure;
    ASSERT_GE(Pout, Pin);
}

