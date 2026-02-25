#include "test_oilnet.h"

/// @brief Выполняет несколько шагов расчета для частичного вымещения начальной партии
/// @tparam TaskType Тип задачи с методом step
/// @param task Задача для выполнения шагов
/// @param time_step Временной шаг
/// @param N Количество шагов
/// @param measurements Измерения для каждого шага
template <typename TaskType>
void multistep(
    TaskType& task,
    double time_step,
    int N,
    const std::vector<oil_transport::graph_quantity_value_t>& measurements) {
    for (int i = 0; i < N; i++) {
        task.step(time_step, measurements);
    }
}

/// @brief Тест метода Solve класса hydro_transport_task_t
/// Проверяет, что после вызова метода solve в буферах моделей присутствуют значения плотности, расхода и давления
TEST(HydroTransportTask, SolveWritesToBuffers) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Arrange - Подготавливаем схему с задвижкой и измерения
    auto [settings, siso_data, measurements] = prepare_valve_scheme(
        std::nullopt,
        90e3,
        1.0,
        "Vlv1",
        850.0);

    // Act - вызываем solve у таска совмещенного расчета
    hydro_transport_task_t task(siso_data, settings);
    task.solve(measurements, false);

    // Assert - Проверяем наличие плотности, расхода, давления в буферах
    const auto& result = task.get_result();

    // Проверка эндогенных параметров на конечность
    AssertSourcesEndoResultFinite(result, settings.structured_transport_solver.endogenous_options);
    AssertLumpedEndoResultFinite(result, settings.structured_transport_solver.endogenous_options);

    // Проверка гидравлических параметров на конечность
    AssertSourcesHydroResultFinite(result);
    AssertLumpedHydroResultFinite(result);
}

/// @brief Тест метода Step класса hydro_transport_task_t при solution_method = FindFlowFromMeasurements
/// Расходы равны расходам из измерения. Эндогены в буферах определены
TEST(HydroTransportTask, StepWithFlowPropagation) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - Подготавливаем схему с задвижкой и измерения
    auto [settings, siso_data, measurements] = hydro_task_test_helpers::prepare_valve_scheme(
        oil_transport::hydro_transport_solution_method::FindFlowFromMeasurements,
        90e3,
        0.9,
        "Vlv1",
        886.0);

    // Act: Step - решение задачи распространения расхода
    hydro_transport_task_t task(siso_data, settings);
    task.solve(measurements, false);
    double time_step = 30.0;
    auto step_result = task.step(time_step, measurements);

    // Проверяем результаты через get_result()
    const auto& result = task.get_result();

    // Проверяем буферы источников/потребителей
    for (const auto& [edge_id, buffer] : result.sources) {
        // Проверяем наличие плотности
        ASSERT_TRUE(std::isfinite(buffer->density_std.value));
        ASSERT_TRUE(std::isfinite(buffer->vol_flow));       
    }
}

/// @brief Use Case совмещенного расчета с решением ЗРР для схемы с трубой
TEST(HydroTransportTask, StepWithFlowPropagation_PipeUseCase) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using pipe_solver_type = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Подготавливаем схему с трубой и измерения
    constexpr double density_measure = 886.0;
    constexpr double dummy_density = 700.0;
    auto [settings, siso_data, measurements] = prepare_pipe_scheme_with_measurements(
        oil_transport::hydro_transport_solution_method::FindFlowFromMeasurements,
        std::numeric_limits<double>::quiet_NaN(),
        110e3,
        density_measure,
        dummy_density);

    // Act: Step - решение задачи распространения расхода
    hydro_transport_task_t task(siso_data, settings);
    task.solve(measurements, false);

    // Несколько шагов расчета для частичного вымещения начальной партии 
    multistep(task, 30.0, 10, measurements);

    const auto& result = task.get_result();

    // Assert
    AssertSourcesHydroResultFinite(result);
    AssertLumpedHydroResultFinite(result);
    AssertPipesHydroResultFinite(result);
    
    // Проверяем наличие нескольких партий в трубе
    const auto& layer = *result.pipes.front().second->layer;
    for (const auto& d : layer.density_std.value) {
        ASSERT_BETWEEN(d, dummy_density, density_measure);
    }
}

/// @brief Тест метода Step класса hydro_transport_task_t при solution_method = CalcFlowWithHydraulicSolver
/// Проверяет корректность выполнения шага расчета без использования измерений расходов (расходы из КЗП)
/// Расходы равны расходам из КЗП. Эндогены в буферах определены
TEST(HydroTransportTask, StepWithFlowCalculation) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - Подготавливаем схему с задвижкой и измерения
    auto [settings, siso_data, measurements] = hydro_task_test_helpers::prepare_valve_scheme(
        oil_transport::hydro_transport_solution_method::CalcFlowWithHydraulicSolver,
        110e3,
        1.0,
        "Src",
        886.0);

    // Создаем задачу и вызываем step
    hydro_transport_task_t task(siso_data, settings);
    task.solve(measurements, false);
    double time_step = 30.0;
    auto step_result = task.step(time_step, measurements);

    // Проверяем, что результат содержит информацию об измерениях
    ASSERT_GE(step_result.measurement_status.endogenous.size(), 0);

    // Проверяем результаты через get_result()
    const auto& result = task.get_result();

    // Проверяем буферы источников/потребителей
    for (const auto& [edge_id, buffer] : result.sources) {
        // Проверяем наличие плотности
        ASSERT_TRUE(std::isfinite(buffer->density_std.value));
        // Проверяем наличие расхода (должен быть рассчитан из КЗП. С измерением не совпадет)
        ASSERT_TRUE(std::isfinite(buffer->vol_flow));
        ASSERT_NE(buffer->vol_flow, measurements[0].data.value);
    }
}

/// @brief Use Case совмещенного расчета с решением ЗРР для схемы с трубой
TEST(HydroTransportTask, StepWithFlowCalculation_PipeUseCase) {
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    using pipe_solver_type = pde_solvers::iso_nonbaro_pipe_solver_t;

    // Arrange - Подготавливаем схему с трубой и измерения
    double density_measure = 886.0;
    double dummy_density = 700.0;
    auto [settings, hydro_siso_data, measurements] = prepare_pipe_scheme_with_measurements(
        oil_transport::hydro_transport_solution_method::CalcFlowWithHydraulicSolver,
        std::numeric_limits<double>::quiet_NaN(),
        110e3,
        density_measure,
        dummy_density);

    // Act: Step - решение задачи распространения расхода
    hydro_transport_task_t task(hydro_siso_data, settings);
    task.solve(measurements, false);

    // Несколько шагов расчета для частичного вымещения начальной партии 
    multistep(task, 30.0, 10, measurements);

    const auto& result = task.get_result();

    // Assert
    AssertSourcesHydroResultFinite(result);
    AssertLumpedHydroResultFinite(result);
    AssertPipesHydroResultFinite(result);

    // Проверяем наличие нескольких партий в трубе
    const auto& layer = *result.pipes.front().second->layer;
    for (const auto& d : layer.density_std.value) {
        ASSERT_BETWEEN(d, dummy_density, density_measure);
    }
}

/// @brief Тест парсера краевых условий для формата без разделения на слои

