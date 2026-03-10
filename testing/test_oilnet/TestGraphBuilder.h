#include "test_oilnet.h"

/// @brief Тест проверяет корректность построения графов для комбинированной задачи гидравлика+транспортный
/// Проверяется, что задвижка правильно представляется в обоих графах
TEST(GraphBuilder, HandlesHydroTransport) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - подготовка схемы
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    auto settings = transport_task_settings::default_values();

    // Act - Формируем граф с моделями
    auto [models_with_buffers, transport_graph, hydro_graph] =
        load_hydro_transport_scheme<>(scheme_folder, "", settings);

    // Assert - проверки типов данных полученных моделей
    // Задвижка в транспортном графе представлена сосредоточенным объектом
    auto model_type_info_transport = &typeid(*models_with_buffers.transport_models.second.front());
    auto expected_transport = &typeid (transport_lumped_model_t);
    ASSERT_EQ(model_type_info_transport, expected_transport);

    // Задвижка в гидравлическом графе представлена местным сопротивлением
    auto model_type_info_hydro = &typeid(*models_with_buffers.hydro_models.second.front());
    auto expected_hydro = &typeid (oil_transport::qsm_hydro_local_resistance_model_t);
    ASSERT_EQ(model_type_info_hydro, expected_hydro);
}

/// @brief Тест проверяет корректность построения графа для транспортного расчета
/// Проверяется, что задвижка правильно представляется в транспортном графе
TEST(GraphBuilder, HandlesTransportOnly) {
    using namespace graphlib;
    using namespace oil_transport;

    // Arrange
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    auto settings = transport_task_settings::default_values();

    // Act
    auto [models_with_buffers, transport_graph] =
        load_transport_scheme<>(scheme_folder, "", settings);

    // Assert - Задвижка в транспортном графе представлена сосредоточенным объектом
    auto model_type_info = &typeid(*models_with_buffers.transport_models.second.front());
    auto expected = &typeid (transport_lumped_model_t);
    ASSERT_EQ(model_type_info, expected);
}

/// @brief Тест проверяет корректность построения графа для гидравлического расчета
/// Проверяется, что задвижка правильно представляется в гидравлическом графе
TEST(GraphBuilder, HandlesHydroOnly) {
    using namespace graphlib;
    using namespace oil_transport;

    // Arrange
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    auto settings = hydro_task_settings::default_values();

    // Act
    auto [hydro_models, hydro_graph] =
        load_hydro_scheme<>(scheme_folder, "", settings);

    // Assert - Задвижка в гидравлическом графе представлена местным сопротивлением
    auto model_type_info = &typeid(*hydro_models.hydro_models.second.front());
    auto expected = &typeid (oil_transport::qsm_hydro_local_resistance_model_t);
    ASSERT_EQ(model_type_info, expected);
}


