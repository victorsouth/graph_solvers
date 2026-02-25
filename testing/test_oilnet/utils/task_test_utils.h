#pragma once

namespace hydro_task_test_helpers {


/// @brief Проверяет, что в результатах расчета для источников рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertSourcesHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result)
{
    // Проверяем буферы источников/потребителей - должны быть расчитаны давления и расходы
    for (const auto& [edge_id, buffer] : result.sources) {
        ASSERT_TRUE(std::isfinite(buffer->vol_flow));
        ASSERT_TRUE(std::isfinite(buffer->pressure));
    }
}

/// @brief Проверяет, что в результатах расчета для источников рассчитаны эндогенные параметры согласно настройкам
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
/// @param endogenous_options Селектор эндогенных параметров, указывающий какие параметры должны быть рассчитаны
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertSourcesEndoResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const pde_solvers::endogenous_selector_t& endogenous_options)
{
    // Проверяем буферы источников/потребителей - должны быть расчитаны эндогенные параметры согласно настройкам
    for (const auto& [edge_id, buffer] : result.sources) {
        if (endogenous_options.density_std) {
            ASSERT_TRUE(std::isfinite(buffer->density_std.value));
        }
        if (endogenous_options.viscosity_working) {
            ASSERT_TRUE(std::isfinite(buffer->viscosity_working.value));
        }
        if (endogenous_options.sulfur) {
            ASSERT_TRUE(std::isfinite(buffer->sulfur.value));
        }
        if (endogenous_options.temperature) {
            ASSERT_TRUE(std::isfinite(buffer->temperature.value));
        }
        if (endogenous_options.improver) {
            ASSERT_TRUE(std::isfinite(buffer->improver.value));
        }
        if (endogenous_options.viscosity0) {
            ASSERT_TRUE(std::isfinite(buffer->viscosity0.value));
        }
        if (endogenous_options.viscosity20) {
            ASSERT_TRUE(std::isfinite(buffer->viscosity20.value));
        }
        if (endogenous_options.viscosity50) {
            ASSERT_TRUE(std::isfinite(buffer->viscosity50.value));
        }
    }
}

/// @brief Проверяет, что в результатах расчета для сосредоточенных объектов рассчитаны эндогенные параметры согласно настройкам
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
/// @param endogenous_options Селектор эндогенных параметров, указывающий какие параметры должны быть рассчитаны
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertLumpedEndoResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const pde_solvers::endogenous_selector_t& endogenous_options)
{
    // Проверяем буферы сосредоточенных объектов - должны быть расчитаны эндогенные параметры согласно настройкам
    for (const auto& [edge_id, buffer_array] : result.lumpeds) {
        for (size_t i = 0; i < 2; ++i) {
            auto* buffer = buffer_array[i];
            if (endogenous_options.density_std) {
                ASSERT_TRUE(std::isfinite(buffer->density_std.value));
            }
            if (endogenous_options.viscosity_working) {
                ASSERT_TRUE(std::isfinite(buffer->viscosity_working.value));
            }
            if (endogenous_options.sulfur) {
                ASSERT_TRUE(std::isfinite(buffer->sulfur.value));
            }
            if (endogenous_options.temperature) {
                ASSERT_TRUE(std::isfinite(buffer->temperature.value));
            }
            if (endogenous_options.improver) {
                ASSERT_TRUE(std::isfinite(buffer->improver.value));
            }
            if (endogenous_options.viscosity0) {
                ASSERT_TRUE(std::isfinite(buffer->viscosity0.value));
            }
            if (endogenous_options.viscosity20) {
                ASSERT_TRUE(std::isfinite(buffer->viscosity20.value));
            }
            if (endogenous_options.viscosity50) {
                ASSERT_TRUE(std::isfinite(buffer->viscosity50.value));
            }
        }
    }
}

/// @brief Проверяет, что в результатах расчета для сосредоточенных объектов рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertLumpedHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result)
{
    // Проверяем буферы сосредоточенных объектов - должны быть расчитаны давления и расходы
    for (const auto& [edge_id, buffer_array] : result.lumpeds) {
        for (size_t i = 0; i < 2; ++i) {
            auto* buffer = buffer_array[i];
            ASSERT_TRUE(std::isfinite(buffer->vol_flow));
            ASSERT_TRUE(std::isfinite(buffer->pressure));
        }
    }
}

/// @brief Проверяет, что в результатах расчета для труб рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertPipesHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result)
{
    // Проверяем трубы - должны быть расчитаны давления и расходы
    for (const auto& [pipe_edge_id, pipe_result_ptr] : result.pipes) {
        ASSERT_NE(pipe_result_ptr, nullptr) << "Pipe result should not be null";
        ASSERT_NE(pipe_result_ptr->layer, nullptr) << "Pipe layer should not be null";

        const auto& layer = *pipe_result_ptr->layer;

        // Проверяем наличие расхода
        ASSERT_TRUE(std::isfinite(layer.std_volumetric_flow)) << "Volumetric flow should be calculated";

        // Проверяем наличие давления
        ASSERT_FALSE(layer.pressure.empty()) << "Pressure profile should not be empty";
        for (const auto& p : layer.pressure) {
            ASSERT_TRUE(std::isfinite(p)) << "All pressure values should be finite";
        }
    }
}

/// @brief Проверяет количество измерений и их распределение по типам привязок
inline void AssertEqMeasurementsCount(
    const std::vector<oil_transport::graph_quantity_value_t>& measurements,
    size_t expected_measurements_count,
    size_t expected_edge_measurements,
    size_t expected_vertex_measurements,
    size_t expected_sided_measurements_count)
{
    ASSERT_EQ(measurements.size(), expected_measurements_count);
    
    auto edge_measurements_count = std::count_if(measurements.begin(), measurements.end(),
        [](const auto& m) { return (m.binding_type == graphlib::graph_binding_type::Edge1) || 
        (m.binding_type == graphlib::graph_binding_type::Edge2); });
    auto vertex_measurements_count = std::count_if(measurements.begin(), measurements.end(),
        [](const auto& m) { return m.binding_type == graphlib::graph_binding_type::Vertex; });
    auto sided_measurements_count = std::count_if(measurements.begin(), measurements.end(),
        [](const auto& m) { return m.is_sided; });
    ASSERT_EQ(edge_measurements_count, expected_edge_measurements);
    ASSERT_EQ(vertex_measurements_count, expected_vertex_measurements);
    ASSERT_EQ(sided_measurements_count, expected_sided_measurements_count);
}

/// @brief Проверяет наличие контрола определенного типа в коллекции контролов
/// @tparam BaseControlType Базовый тип контрола (hydraulic_object_control_t или transport_object_control_t)
template<typename ControlType, typename BaseControlType>
void AssertHasTypedControl(
    const std::unordered_multimap<std::size_t, std::unique_ptr<BaseControlType>>& controls)
{
    bool has_control = false;
    for (const auto& [step, control] : controls) {
        if (dynamic_cast<ControlType*>(control.get())) {
            has_control = true;
            break;
        }
    }
    ASSERT_TRUE(has_control) << "Expected control type not found";
}

/// @brief Проверяет наличие контрола определенного типа в векторе контролов
/// @tparam BaseControlType Базовый тип контрола (hydraulic_object_control_t или transport_object_control_t)
template<typename ControlType, typename BaseControlType>
void AssertHasTypedControl(
    const std::vector<std::unique_ptr<BaseControlType>>& controls)
{
    bool has_control = false;
    for (const auto& control : controls) {
        if (dynamic_cast<ControlType*>(control.get())) {
            has_control = true;
            break;
        }
    }
    ASSERT_TRUE(has_control) << "Expected control type not found";
}

}

