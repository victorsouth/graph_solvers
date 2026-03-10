#pragma once

namespace hydro_task_test_helpers {


/// @brief Проверяет, что в результатах расчета для источников рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
/// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertSourcesHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const std::set<std::size_t>& edges_index_to_check = std::set<std::size_t>())
{
    // Проверяем буферы источников/потребителей - должны быть расчитаны давления и расходы
    for (const auto& [edge_id, buffer] : result.sources) {

        // Если результаты по ребру не нужно проверять, то переходим к следующему ребру
        if (!edges_index_to_check.empty() && !edges_index_to_check.contains(edge_id))
            continue;

        ASSERT_TRUE(std::isfinite(buffer->vol_flow));
        ASSERT_TRUE(std::isfinite(buffer->pressure));
    }
}

/// @brief Проверяет, что в результатах расчета для источников рассчитаны эндогенные параметры согласно настройкам
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param result Результаты расчета
/// @param endogenous_options Селектор эндогенных параметров, указывающий какие параметры должны быть рассчитаны
/// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertSourcesEndoResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const pde_solvers::endogenous_selector_t& endogenous_options,
    const std::set<std::size_t>& edges_index_to_check = std::set<std::size_t>())
{
    // Проверяем буферы источников/потребителей - должны быть расчитаны эндогенные параметры согласно настройкам
    for (const auto& [edge_id, buffer] : result.sources) {

        // Если результаты по ребру не нужно проверять, то переходим к следующему ребру
        if (!edges_index_to_check.empty() && !edges_index_to_check.contains(edge_id))
            continue;

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
/// /// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertLumpedEndoResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const pde_solvers::endogenous_selector_t& endogenous_options,
    const std::set<std::size_t>& edges_index_to_check = std::set<std::size_t>())
{
    // Проверяем буферы сосредоточенных объектов - должны быть расчитаны эндогенные параметры согласно настройкам
    for (const auto& [edge_id, buffer_array] : result.lumpeds) {

        // Если результаты по ребру не нужно проверять, то переходим к следующему ребру
        if (!edges_index_to_check.empty() && !edges_index_to_check.contains(edge_id))
            continue;


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
/// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertLumpedHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const std::set<std::size_t>& edges_index_to_check = std::set<std::size_t>())
{
    // Проверяем буферы сосредоточенных объектов - должны быть расчитаны давления и расходы
    for (const auto& [edge_id, buffer_array] : result.lumpeds) {

        // Если результаты по ребру не нужно проверять, то переходим к следующему ребру
        if (!edges_index_to_check.empty() && !edges_index_to_check.contains(edge_id))
            continue;

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
/// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertPipesHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result,
    const std::set<std::size_t>& edges_index_to_check = std::set<std::size_t>())
{
    // Проверяем трубы - должны быть расчитаны давления и расходы
    for (const auto& [pipe_edge_id, pipe_result_ptr] : result.pipes) {

        // Если результаты по ребру не нужно проверять, то переходим к следующему ребру
        if (!edges_index_to_check.empty() && !edges_index_to_check.contains(pipe_edge_id))
            continue;

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

/// @brief Проверяет, что в результатах расчета для всех ПРОТОЧНЫХ объектов рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @param task Таск гидравлического расчета
template <typename PipeSolver>
void AssertFlowsSubgraphsHydroResultsFinite(oil_transport::hydro_task_t& task) {

    oil_transport::hydraulic_graph_t& graph = task.get_graph();
    auto [structured_graph, flow_subgraphs, zeroflow_subgraphs] = oil_transport::split_graph<oil_transport::hydraulic_graph_t>(graph);

    std::set<size_t> flow_sources;
    std::set<size_t> flow_pipes;
    std::set<size_t> flow_lumpeds;

    for (const size_t subgraph_id : flow_subgraphs) {

        const oil_transport::hydraulic_graph_t& subgraph = structured_graph.subgraphs.at(subgraph_id);

        // Получаем номера проточных притоков
        for (size_t edge_id = 0; edge_id < subgraph.edges1.size(); ++edge_id) {
            auto sub_binding = graphlib::subgraph_binding_t(subgraph_id, graphlib::graph_binding_type::Edge1, edge_id);
            auto original_binding = structured_graph.ascend_map.at(sub_binding);
            flow_sources.insert(original_binding.edge);
        }

        // Получаем номера проточных двусторонних ребер
        for (size_t edge_id = 0; edge_id < subgraph.edges2.size(); ++edge_id) {

            auto sub_binding = graphlib::subgraph_binding_t(subgraph_id, graphlib::graph_binding_type::Edge2, edge_id);
            auto original_binding = structured_graph.ascend_map.at(sub_binding);

            oil_transport::hydraulic_model_t* model = graph.get_model_incidences(original_binding).model;

            if (auto transport_pipe_model = dynamic_cast<oil_transport::hydraulic_pipe_model_t<PipeSolver>*>(model))
                // Проточная труба;
                flow_pipes.insert(original_binding.edge);
            else
                // Проточное сосредоточенное ребро
                flow_lumpeds.insert(original_binding.edge);
        }

    }
    const auto& result = task.get_result();

    AssertSourcesHydroResultFinite(result, flow_sources);
    AssertLumpedHydroResultFinite(result, flow_lumpeds);
    AssertPipesHydroResultFinite(result, flow_pipes);

}

/// @brief Проверяет, что в результатах расчета для всех объектов рассчитаны давления и расходы
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
/// @param edges_index_to_check Индексы ребер (из исходного графа), для которых нужно выполнить проверку
template <typename PipeSolver, typename LumpedCalcParametersType>
void AssertAllEdgesHydroResultFinite(
    const oil_transport::task_common_result_t<PipeSolver, LumpedCalcParametersType>& result)
{
    AssertSourcesHydroResultFinite(result);
    AssertLumpedHydroResultFinite(result);
    AssertPipesHydroResultFinite(result);
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

/// @brief Формирует гидравлические контролы для списка ребер.
/// @tparam ControlType Тип контрола (наследник hydraulic_object_control_t)
/// @tparam PropertyType Тип изменяемого свойства контрола
template<typename ControlType, typename PropertyType>
std::vector<ControlType> prepare_controls_for_multiple_edges(
    PropertyType ControlType::* property,
    const PropertyType& new_value,
    const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding,
    const std::vector<std::string>& edge_names)
{
    std::vector<ControlType> controls;
    controls.reserve(edge_names.size());

    auto create_control = [&](const std::string& edge_name) {
        ControlType control;
        control.binding = name_to_binding.at(edge_name);
        control.*property = new_value;
        controls.push_back(control);
        };

    std::for_each(edge_names.begin(), edge_names.end(), create_control);

    return controls;
}

/// @brief Отправляет в гидравлический таск набор типизированных переключений для объектов одного типа.
/// @tparam ControlType Тип контрола (наследник hydraulic_object_control_t)
template<typename ControlType>
void send_multiple_typed_controls(const std::vector<ControlType>& controls,
    oil_transport::hydro_task_t* hydro_task)
{
    std::vector<const oil_transport::hydraulic_object_control_t*> control_ptrs;
    control_ptrs.reserve(controls.size());
    for (const auto& control : controls) {
        control_ptrs.push_back(&control);
    }
    hydro_task->send_controls(control_ptrs);
}

/// @brief Формирует таск гидравлической задачи для гидравлического расчета заданной схемы с помощью МД.
/// Параметры таска по умолчанию, за исключением разрброса для начального приближения давления pressure_deviation
/// @param scheme_name Название тестовой схемы
/// @param default_density Значение плотности, устанавливаемое в буферах всех моделей
template<typename PipeHydroSolver>
oil_transport::hydro_task_t prepare_hydro_task_for_test_calc(oil_transport::graph_siso_data& hydro_siso_data,
    double pressure_deviation,
    double default_density)
{
    using namespace oil_transport;
    using namespace graphlib;
    using namespace hydro_task_test_helpers;

    // Формирование настроек таска
    auto hydro_settings = hydro_task_settings::default_values();
    hydro_settings.structured_solver.nodal_solver.random_pressure_deviation = pressure_deviation;

    // Формирование профилей труб
    make_pipes_uniform_profile_handled<typename PipeHydroSolver::pipe_parameters_type>(
        hydro_settings.pipe_coordinate_step, &hydro_siso_data.hydro_props.second);

    hydro_task_t hydro_task(hydro_siso_data, hydro_settings);

    // Заполнение буферов недостоверной плотностью
    oil_transport::endohydro_values_t default_endogenous;
    default_endogenous.density_std.value = default_density;
    init_endogenous_in_buffers<PipeHydroSolver, oil_transport::endohydro_values_t>(
        hydro_task.get_graph(), {}, graphlib::layer_offset_t::ToPreviousLayer, default_endogenous);

    return std::move(hydro_task);
}

/// @brief Проверяет, что во всех подграфах результата диагностика (argument_history) пустая
inline void AssertResidualsAnalysisEmpty(const graphlib::structured_flow_distribution_result& result) {
    for (const auto& [subgraph_id, subgraph_result] : result.results_by_subgraphs) {
        EXPECT_TRUE(subgraph_result.residuals_analysis.argument_history.empty())
            << "Subgraph " << subgraph_id << " diagnostics (argument_history) should be empty";
    }
}

/// @brief Проверяет, что в каждом подграфе результата диагностика (argument_history) не пустая
inline void AssertResidualsAnalysisNonEmpty(const graphlib::structured_flow_distribution_result& result) {
    for (const auto& [subgraph_id, subgraph_result] : result.results_by_subgraphs) {
        EXPECT_FALSE(subgraph_result.residuals_analysis.argument_history.empty())
            << "Subgraph " << subgraph_id << " diagnostics (argument_history) should be non-empty";
    }
}

/// @brief Проверяет, что во всех подграфах результата расчётные данные (давления, расходы) не пустые
inline void AssertFlowResultNonEmpty(const graphlib::structured_flow_distribution_result& result) {
    for (const auto& [subgraph_id, subgraph_result] : result.results_by_subgraphs) {
        EXPECT_GT(subgraph_result.pressures.size(), 0) << "Subgraph " << subgraph_id << ": pressures should not be empty";
        EXPECT_GT(subgraph_result.flows1.size(), 0) << "Subgraph " << subgraph_id << ": flows1 should not be empty";
        EXPECT_GT(subgraph_result.flows2.size(), 0) << "Subgraph " << subgraph_id << ": flows2 should not be empty";
    }
}

}

