#include "test_oilnet.h"

/// @brief Проверка возможности чтения неориентированного измерения на ребре  
TEST(BoundsParser, HandlesEdgeMeasurementUnsided) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - Фиктивная топология
    graph_binding_t dummy_binding(graphlib::graph_binding_type::Edge2, 1234);
    
    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummyEdge", dummy_binding}
    };

    std::string bounds_json_str = R"({
      "transport_measurements": [
        {
          "edge": "DummyEdge",
          "type": "vol_flow_std",
          "value": 0.9
        }
      ]
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, name_to_binding);

    // Assert - Результат парсинга равен ожидаемому
    graph_quantity_value_t expected_measurement(dummy_binding, measurement_type::vol_flow_std, 0.9);
    auto parsed_measurement = bounds.measurements.front();
    ASSERT_EQ(parsed_measurement, expected_measurement);
}

/// @brief Проверка возможности чтения ориентированного измерения на ребре
TEST(BoundsParser, HandlesEdgeMeasurementSided) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - Фиктивная топология
    graph_place_binding_t dummy_binding(graphlib::graph_binding_type::Edge2, 1234, graphlib::graph_edge_side_t::Outlet);

    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummyEdge", dummy_binding}
    };

    std::string bounds_json_str = R"({
      "transport_measurements": [
        {
          "edge": "DummyEdge",
          "type": "density_std_out",
          "value": 800
        }
      ]
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, name_to_binding);

    // Assert - Результат парсинга равен ожидаемому
    graph_quantity_value_t expected_measurement(dummy_binding, measurement_type::density_std, 800);

    auto parsed_measurement = bounds.measurements.front();
    ASSERT_EQ(parsed_measurement, expected_measurement);
}

/// @brief Проверка возможности чтения измерения в вершине
TEST(BoundsParser, HandlesVertexMeasurement) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - Фиктивная топология
    graph_binding_t dummy_binding(graphlib::graph_binding_type::Vertex, 1234);

    std::string bounds_json_str = R"({
      "transport_measurements": [
        {
          "vertex": "1234",
          "type": "density_std_out",
          "value": 800
        }
      ]
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);

    std::unordered_map<std::string, graphlib::graph_binding_t> dummy_name_to_binding;
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, dummy_name_to_binding);

    // Assert - Результат парсинга равен ожидаемому
    graph_quantity_value_t expected_measurement(dummy_binding, measurement_type::density_std, 800);

    auto parsed_measurement = bounds.measurements.front();
    ASSERT_EQ(parsed_measurement, expected_measurement);
}

/// @brief Проверка чтения переключений по притокам 
TEST(BoundsParser, HandlesControlSource) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Arrange - Фиктивная топология
    graph_binding_t dummy_binding(graphlib::graph_binding_type::Edge1, 1234);

    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummySource", dummy_binding}
    };

    std::string bounds_json_str = R"({
      "controls": {
        "sources": {
          "DummySource": {
            "pressure": 110000.0,
            "std_vol_flow": 1.0,
            "density_std_out": 850.0,
            "viscosity_working": 5.0,
            "temperature": 20.0
          }
        }
      }
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, name_to_binding);

    // Assert - Результат парсинга равен ожидаемому
    auto h_control = dynamic_cast<hydraulic_source_control_t*>(bounds.hydraulic_controls.front().get());
    auto t_control = dynamic_cast<transport_source_control_t*>(bounds.transport_controls.front().get());

    ASSERT_EQ(h_control->binding, dummy_binding);
    ASSERT_EQ(h_control->pressure, 110000.0);
    ASSERT_EQ(h_control->std_volflow, 1.0);

    ASSERT_EQ(t_control->binding, dummy_binding);
    ASSERT_EQ(t_control->density_std, 850.0);
    ASSERT_EQ(t_control->viscosity_working, 5.0);
    ASSERT_EQ(t_control->temperature, 20.0);
}

/// @brief Проверка чтения переключений по задвижкам
TEST(BoundsParser, HandlesControlValve) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Arrange - Фиктивная топология
    graph_binding_t dummy_binding(graphlib::graph_binding_type::Edge2, 1234);

    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummyValve", dummy_binding}
    };

    std::string bounds_json_str = R"({
      "controls": {
        "valves": {
          "DummyValve": {
            "opening": 0.5
          }
        }
      }
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, name_to_binding);

    // Assert - получены ожидаемые контролы
    auto h_control = dynamic_cast<qsm_hydro_local_resistance_control_t*>(bounds.hydraulic_controls.front().get());
    auto t_control = dynamic_cast<transport_lumped_control_t*>(bounds.transport_controls.front().get());

    ASSERT_EQ(h_control->binding, dummy_binding);
    ASSERT_EQ(h_control->opening, 0.5);

    ASSERT_EQ(t_control->binding, dummy_binding);
    ASSERT_EQ(t_control->is_opened, true);
}

/// @brief Проверка чтения одного слоя замеров и переключений
TEST(BoundsParser, HandlesSingleLayer) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Arrange - Подготавливаем схему с трубой
    graph_binding_t dummy_binding_edge1(graphlib::graph_binding_type::Edge1, 1234);
    graph_binding_t dummy_binding_edge2(graphlib::graph_binding_type::Edge2, 5678);
    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummySrc", dummy_binding_edge1},
        {"DummyVlv1", dummy_binding_edge2}
    };

    std::string bounds_json_str = R"({
      "transport_measurements": [
        {
          "edge": "DummyVlv1",
          "type": "vol_flow_std",
          "value": 0.9
        },
        {
          "vertex": 0,
          "type": "density_std",
          "value": 850.0
        },
        {
          "edge": "DummySrc",
          "type": "density_std_out",
          "value": 886.0
        }
      ],
      "controls": {
        "valves": {
          "DummyVlv1": {
            "opening": 0.5
          }
        },
        "sources": {
          "DummySrc": {
            "pressure": 110000.0,
            "std_vol_flow": 1.0,
            "density_std_out": 850.0,
            "viscosity_working": 5.0,
            "temperature": 20.0
          }
        }
      }
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_single_layer(
        json_stream, name_to_binding);

    // Assert - Проверка количества измерений и контролов
    size_t expected_measurements_count = 3;
    size_t expected_measurements_on_edges_count = 2;
    size_t expected_measurements_on_vertices_count = 1;
    size_t expected_sided_measurements_count = 1;
    AssertEqMeasurementsCount(bounds.measurements, 
        expected_measurements_count, expected_measurements_on_edges_count, 
        expected_measurements_on_vertices_count, expected_sided_measurements_count);
}

/// @brief Проверка чтения одного нескольких слоев замеров и переключений
TEST(BoundsParser, HandlesMultipleLayers) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Arrange - Фиктивная топология
    graph_binding_t dummy_binding_edge1(graphlib::graph_binding_type::Edge1, 1234);
    graph_binding_t dummy_binding_edge2(graphlib::graph_binding_type::Edge2, 5678);
    graph_binding_t dummy_binding_edge3(graphlib::graph_binding_type::Edge1, 9999);
    
    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding{
        {"DummySrc", dummy_binding_edge1},
        {"DummyVlv1", dummy_binding_edge2},
        {"DummyVlv2", dummy_binding_edge3}
    };

    std::string bounds_json_str = R"({
      "layers": [
        {
          "step": "0",
          "transport_measurements": [
            {
              "edge": "DummyVlv1",
              "type": "vol_flow_std",
              "value": 0.9
            },
            {
              "vertex": 0,
              "type": "density_std",
              "value": 850.0
            }
          ],
          "controls": {
            "valves": {
              "DummyVlv1": {
                "opening": 0.5
              }
            },
            "sources": {
              "DummySrc": {
                "pressure": 110000.0,
                "std_vol_flow": 1.0,
                "density_std_out": 850.0,
                "viscosity_working": 5.0,
                "temperature": 20.0
              }
            }
          }
        },
        {
          "step": "1",
          "transport_measurements": [
            {
              "edge": "DummyVlv1",
              "type": "vol_flow_std",
              "value": 1.0
            },
            {
              "edge": "DummyVlv2",
              "type": "density_std_out",
              "value": 880.0
            },
            {
              "vertex": 1,
              "type": "density_std",
              "value": 860.0
            }
          ],
          "controls": {
            "valves": {
              "DummyVlv2": {
                "opening": 0.8
              }
            }
          }
        }
      ]
    })";

    // Act - Парсим JSON
    std::istringstream json_stream(bounds_json_str);
    auto bounds = bounds_parser_json::parse_multiple_layers(
        json_stream, name_to_binding);

    // Assert - Проверяем количество слоев
    ASSERT_EQ(bounds.size(), 2);
    ASSERT_TRUE(bounds.find(0) != bounds.end());
    ASSERT_TRUE(bounds.find(1) != bounds.end());

    // Assert - Проверка количества измерений и контролов на слоях
    const auto& layer0 = bounds.at(0);
    size_t expected_layer0_measurements_count = 2;
    size_t expected_layer0_edge_measurements = 1;
    size_t expected_layer0_vertex_measurements = 1;
    size_t expected_layer0_sided_measurements = 0;
    AssertEqMeasurementsCount(layer0.measurements,
        expected_layer0_measurements_count, expected_layer0_edge_measurements,
        expected_layer0_vertex_measurements, expected_layer0_sided_measurements);
    
    ASSERT_EQ(layer0.hydraulic_controls.size(), 2);
    ASSERT_EQ(layer0.transport_controls.size(), 2);

    const auto& layer1 = bounds.at(1);
    size_t expected_layer1_measurements_count = 3;
    size_t expected_layer1_edge_measurements = 2;
    size_t expected_layer1_vertex_measurements = 1;
    size_t expected_layer1_sided_measurements = 1;
    AssertEqMeasurementsCount(layer1.measurements,
        expected_layer1_measurements_count, expected_layer1_edge_measurements,
        expected_layer1_vertex_measurements, expected_layer1_sided_measurements);
    
    ASSERT_EQ(layer1.hydraulic_controls.size(), 1);
    ASSERT_EQ(layer1.transport_controls.size(), 1);
}

