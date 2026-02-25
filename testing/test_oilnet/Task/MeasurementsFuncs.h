#include "test_oilnet.h"

/// @brief Создает измерение одного эндогенного параметра с заданным значением и привязкой по умолчанию
inline oil_transport::graph_quantity_value_t make_dummy_bound_measurement(oil_transport::measurement_type type, double value) {
    oil_transport::graph_quantity_value_t measurement;
    measurement.type = type;
    measurement.data.value = value;
    return measurement;
}

/// @brief Проверка возможности усреднения эндогенных параметров из замеров
TEST(MeasurementsAverager, ComputesAveragesBetweenInputs) {
    using namespace oil_transport;

    // Arrange: два измерения с разными значениями по всем параметрам
    graph_quantity_value_t m1;
    graph_quantity_value_t m2;

    std::vector<graph_quantity_value_t> measurements1{
        make_dummy_bound_measurement(measurement_type::density_std,           800.0),
        make_dummy_bound_measurement(measurement_type::viscosity_working,         5.0),
        make_dummy_bound_measurement(measurement_type::sulfur,            0.5),
        make_dummy_bound_measurement(measurement_type::mass_flow_improver,1.0),
        make_dummy_bound_measurement(measurement_type::vol_flow_improver, 2.0),
        make_dummy_bound_measurement(measurement_type::temperature,       280.0),
    };

    std::vector<graph_quantity_value_t> measurements2{
        make_dummy_bound_measurement(measurement_type::density_working,           900.0),
        make_dummy_bound_measurement(measurement_type::viscosity_working,         10.0),
        make_dummy_bound_measurement(measurement_type::sulfur,            1.0),
        make_dummy_bound_measurement(measurement_type::mass_flow_improver,3.0),
        make_dummy_bound_measurement(measurement_type::vol_flow_improver, 4.0),
        make_dummy_bound_measurement(measurement_type::temperature,       320.0),
    };
        
    measurements1.insert(measurements1.end(), 
        measurements2.begin(), measurements2.end());

    hydro_measurements_averager_t averager;

    for (const auto& m : measurements1) {
        averager.add_measurement(m);
    }

    // Act: получаем средние значения эндогенных параметров из измерений
    endohydro_values_t avg = averager.get_average_hydro_values();

    // Assert: средние значения лежат между исходными
    ASSERT_BETWEEN(avg.density_std.value,           800.0, 900.0);
    ASSERT_BETWEEN(avg.viscosity_working.value,         5.0,   10.0);
    ASSERT_BETWEEN(avg.sulfur.value,            0.5,   1.0);
    ASSERT_BETWEEN(avg.improver.value,          1.0,   3.0);
    ASSERT_BETWEEN(avg.temperature.value,       280.0, 320.0);

    // Эндогенные параметры должны быть недостоверными
    EXPECT_FALSE(avg.density_std.confidence);
    EXPECT_FALSE(avg.viscosity_working.confidence);
    EXPECT_FALSE(avg.sulfur.confidence);
    EXPECT_FALSE(avg.improver.confidence);
    EXPECT_FALSE(avg.temperature.confidence);
}



