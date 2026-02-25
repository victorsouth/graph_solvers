#include "test_oilnet.h"

/// @brief Проверка возможности записи нескольких слоев расчетных результатов
TEST(ResultSerialization, HandlesMultipleLayersWithRegimeSelectionStrategy) {
    using namespace oil_transport;
    using namespace graphlib;

    using pipe_solver_type = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Подготавливаем схему с трубой и измерения
    constexpr double density_measure = 886.0;
    constexpr double dummy_density = 700.0;
    auto [settings, hydro_siso_data, measurements] = hydro_task_test_helpers::prepare_pipe_scheme_with_measurements(
        oil_transport::hydro_transport_solution_method::FindFlowFromMeasurements,
        1000,
        110e3,
        density_measure,
        dummy_density);

    // Создаем маппинг от edge_id к имени объекта из topology.json
    auto edge_id_to_object_name = hydro_siso_data.create_edge_binding_to_name();

    // Создаем задачу
    hydro_transport_task_t task(hydro_siso_data, settings);

    // Создание потокового буфера для записи JSON
    std::stringstream json_stream;

    double time_step = 30.0;
    
    size_t expected_regimes = 1; // ожидаемое количество режимов при стратегии "Режим каждый час"
    const size_t N = static_cast<size_t>(3600.0 / time_step) + expected_regimes; // Час расчета + 1 шаг
    
    auto [times, astro_times] = time_grid_generator::prepare_time_grid(time_step, 0, (std::time_t)(time_step * N));

    batch_serializing_result<hydro_transport_task_t> processor(
        task,
        astro_times,
        edge_id_to_object_name,
        json_stream,
        quasi_stationary_regime_selection_strategy::EveryHour);

    oil_transport::hydrotransport_batch::perform(processor, times, measurements);

    // Assert - проверяем, что JSON содержит expected_regimes слоев
    size_t layers_count = step_results_json_reader::get_layers_count(json_stream);    
    ASSERT_EQ(layers_count, expected_regimes);

    // TODO: нужна содержательная проверка. Парсер результатов. 
    // Проверка, что есть шаги с достоверностью и шаги с недостоверностью
}
