#include "test_oilnet.h"

TEST(SettingsParserJson, ParsesAllSettingsFromJson) {
    using namespace oil_transport;

    std::string settings_json_str = R"({
      "solution_method": "CalcFlowWithHydraulicSolver",
      "pipe_coordinate_step": 500.0,
      "structured_transport_solver": {
        "rebind_zeroflow_measurements": false,
        "endogenous_options": {
          "density_std": true,
          "viscosity_working": true,
          "sulfur": false,
          "temperature": false,
          "improver": false,
          "viscosity0": false,
          "viscosity20": false,
          "viscosity50": false
        },
        "block_solver": {
          "use_simple_block_flow_heuristic": true
        },
        "graph_solver": {}
      },
      "structured_hydro_solver": {
        "nodal_solver": {
          "random_pressure_deviation": 100.0,
          "max_iterations": 200,
          "line_search_max_iterations": 150,
          "line_search_function_decrement_factor": 5.0,
          "argument_increment_norm_tolerance": 1e-5
        }
      }
    })";

    std::istringstream json_stream(settings_json_str);
    auto settings = settings_parser_json::parse(json_stream);

    ASSERT_EQ(settings.solution_method, hydro_transport_solution_method::CalcFlowWithHydraulicSolver);
    ASSERT_EQ(settings.pipe_coordinate_step, 500.0);
    
    ASSERT_EQ(settings.structured_transport_solver.rebind_zeroflow_measurements, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.density_std, true);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.viscosity_working, true);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.sulfur, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.temperature, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.improver, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.viscosity0, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.viscosity20, false);
    ASSERT_EQ(settings.structured_transport_solver.endogenous_options.viscosity50, false);
    ASSERT_EQ(settings.structured_transport_solver.block_solver.use_simple_block_flow_heuristic, true);
    
    ASSERT_EQ(settings.structured_hydro_solver.nodal_solver.random_pressure_deviation, 100.0);
    ASSERT_EQ(settings.structured_hydro_solver.nodal_solver.max_iterations, 200);
    ASSERT_EQ(settings.structured_hydro_solver.nodal_solver.line_search_max_iterations, 150);
    ASSERT_EQ(settings.structured_hydro_solver.nodal_solver.line_search_function_decrement_factor, 5.0);
    ASSERT_EQ(settings.structured_hydro_solver.nodal_solver.argument_increment_norm_tolerance, 1e-5);
}

