#include "test_oilnet.h"

/// @brief Проверяет корректность зачитки привязок тегов состояния (контролов) из файла controls_data.json
TEST(GraphParser, ParsesControlTagsForEdges2)
{
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    std::string tags_filename = scheme_folder + "/tags_data.json";
    const std::string etalon_control_name = "Vlv1_Status";
    constexpr auto etalon_control_type = control_tag_conversion_type::valve_state_scada;

    // Act
    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            scheme_folder + "", tags_filename);

    // Assert
    ASSERT_EQ(siso_data.controls2.front().name, etalon_control_name);
    ASSERT_EQ(siso_data.controls2.front().type, etalon_control_type);
}

/// @brief Проверяет способность json_graph_parser_t распарсить схемы через JSON-строку
/// Создается простейшая схема поставщик-задвижка-потребитель, повторяющая схему valve_control_test/scheme/
/// Проверяются зачитанные параметры в graph_siso_data
TEST(GraphParser, ParsesFromStringSimpleScheme)
{
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange
    // Топология: поставщик (Src) -> задвижка (Vlv1) -> потребитель (Snk)
    const std::string topology_json = R"({
        "Src": {
            "to": 0
        },
        "Vlv1": {
            "from": 0,
            "to": 1
        },
        "Snk": {
            "from": 1
        }
    })";

    // Параметры объектов: поставщик, задвижка, потребитель
    const std::string objects_json = R"({
        "sources": {
            "Src": {
                "std_volflow": 1.0
            },
            "Snk": {
                "pressure": 1e5
            }
        },
        "valves": {
            "Vlv1": {
                "initial_opening": 100,
                "xi": 0.03,
                "diameter": 0.2
            }
        }
    })";

    // Act
    graph_siso_data siso_data = json_graph_parser_t::parse_hydro_transport_from_strings<qsm_pipe_transport_parameters_t>(
        topology_json, objects_json);

    // Assert
    // Проверка топологии: должны быть 3 объекта
    ASSERT_EQ(siso_data.name_to_binding.size(), 3);
    ASSERT_TRUE(siso_data.name_to_binding.find("Src") != siso_data.name_to_binding.end());
    ASSERT_TRUE(siso_data.name_to_binding.find("Vlv1") != siso_data.name_to_binding.end());
    ASSERT_TRUE(siso_data.name_to_binding.find("Snk") != siso_data.name_to_binding.end());

    // Проверка параметров поставщика (Src) - транспортные
    const auto& src_transport = siso_data.get_properties<transport_source_parameters_t>("Src");
    ASSERT_FALSE(src_transport.is_initially_zeroflow);

    // Проверка параметров поставщика (Src) - гидравлические
    auto& src_hydro = siso_data.get_properties<qsm_hydro_source_parameters>("Src", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(src_hydro.std_volflow, 1.0);

    // Проверка параметров задвижки (Vlv1) - транспортные
    const auto& vlv1_transport = siso_data.get_properties<transport_lumped_parameters_t>("Vlv1");
    ASSERT_TRUE(vlv1_transport.is_initially_opened); // initial_opening = 100 > 0

    // Проверка параметров задвижки (Vlv1) - гидравлические
    auto& vlv1_hydro = siso_data.get_properties<qsm_hydro_local_resistance_parameters>("Vlv1", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(vlv1_hydro.initial_opening, 100.0);
    ASSERT_DOUBLE_EQ(vlv1_hydro.xi, 0.03);
    ASSERT_DOUBLE_EQ(vlv1_hydro.diameter, 0.2);

    // Проверка параметров потребителя (Snk) - гидравлические
    auto& snk_hydro = siso_data.get_properties<qsm_hydro_source_parameters>("Snk", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(snk_hydro.pressure, 1e5);
}


