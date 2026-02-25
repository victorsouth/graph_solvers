#include "test_oilnet.h"

#include "oil_hydro.h"

TEST(HydroPipe, DISABLED_Develop) {

    using namespace graphlib;
    using namespace oil_transport;

    // Arrange
    std::string scheme_folder = get_schemes_folder() + "hydro_pipe_test/";

    graph_json_data data = json_graph_parser_t::load_json_data(scheme_folder, "", qsm_problem_type::Hydraulic);
    auto hparams = data.get_hydro_object_parameters<qsm_pipe_transport_parameters_t>();

    graph_siso_data siso_data =
        json_graph_parser_t::prepare_hydro_siso_graph<qsm_pipe_transport_parameters_t>(hparams, data);

    auto settings = transport_task_settings::get_default_settings();

    // Act
    auto [hydro_models, hydro_graph] = graph_builder<qsm_pipe_transport_solver_t>::build_hydro(
        siso_data, settings);

}