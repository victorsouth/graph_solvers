#pragma once

namespace graphlib {
;

/// @brief Настройки солвера уровня структурированного графа для гидравлики
struct structured_hydro_solver_settings_t {
    /// @brief Настройки солвера МД
    // TODO: после реализации МГГ разнести настройки по base и specific
    graphlib::nodal_solver_settings_t nodal_solver;
    /// @brief Условие сбора диагностики расчета по подграфам
    analysis_execution_mode analysis_mode{ analysis_execution_mode::never };
    /// @brief Флаги диагностики целевой функции
    fixed_solver_analysis_parameters_t analysis_parameters;
    /// @brief Создаёт набор настроек со значениями по умолчанию
    static structured_hydro_solver_settings_t default_values() {
        structured_hydro_solver_settings_t result;
        result.nodal_solver = nodal_solver_settings_t::default_values();
        result.analysis_mode = analysis_execution_mode::never;
        return result;
    }

    /// @brief Возвращает настройки для указанного типа солвера подграфа
    /// @tparam SubgraphSolver Тип солвера подграфа (шаблонный тип)
    template <typename SubgraphSolver>
    const solver_settings_t<SubgraphSolver>& get_settings() const {
        using settings_type = solver_settings_t<SubgraphSolver>;
        if constexpr (std::is_same_v<settings_type, nodal_solver_settings_t>) {
            return static_cast<const settings_type&>(nodal_solver);
        }
        else {
            throw std::runtime_error("Unsupported SubgraphSolver type. Solver must have Settings type alias.");
        }
    }

    /// @brief Возвращает настройки диагностики со всеми активными флагами отладки
    static const fixed_solver_analysis_parameters_t get_enabled_analysis_settings() {
        fixed_solver_analysis_parameters_t analysis_parameters;
        analysis_parameters.argument_history = true;
        analysis_parameters.line_search_explore = true;
        analysis_parameters.objective_function_history = true;
        analysis_parameters.steps = true;
        return analysis_parameters;
    }
};

/// @brief Результат решения задачи потокораспределения на структурированном графе: 
/// результаты по подграфам и привязки рёбер по подграфам
struct structured_flow_distribution_result {
    /// @brief Результаты расчёта по каждому проточному подграфу (потокораспределение, численный результат, диагностика)
    std::unordered_map<std::size_t, flow_distribution_result_t> results_by_subgraphs;
    /// @brief По каждому индексу подграфа — список привязок его рёбер к исходному графу
    std::unordered_map<std::size_t, std::vector<graph_binding_t>> edges_by_subgraph;

    /// @brief Конструктор: заполняет привязки ребер по подграфам
    structured_flow_distribution_result(const descend_map_t& descend_map)
        : edges_by_subgraph(get_edges_by_subgraph(descend_map))
    {}

    structured_flow_distribution_result() = default;

    /// @brief Формирует сопоставление "индекс подграфа — вектор привязок (graph_binding_t) его рёбер к исходному графу"
    static std::unordered_map<std::size_t, std::vector<graph_binding_t>> get_edges_by_subgraph(const descend_map_t& descend_map) {
        std::unordered_map<std::size_t, std::vector<graph_binding_t>> result;
        for (const auto& [global_binding, sub_binding] : descend_map) {
            if (global_binding.binding_type == graph_binding_type::Edge1 ||
                global_binding.binding_type == graph_binding_type::Edge2) {
                result[sub_binding.subgraph_index].push_back(global_binding);
            }
        }
        return result;
    }

    /// @brief Формирует сопоставление "индекс подграфа — имена рёбер этого подграфа"
    std::unordered_map<std::size_t, std::vector<std::string>> get_edge_names_by_subgraph(
        const std::unordered_map<graph_binding_t, std::string, graph_binding_t::hash>& edge_binding_to_name)
    {
        std::unordered_map<std::size_t, std::vector<std::string>> result;
        for (const auto& [subgraph_id, bindings] : edges_by_subgraph) {
            auto& names = result[subgraph_id];
            names.reserve(bindings.size());
            for (const auto& binding : bindings) {
                auto it = edge_binding_to_name.find(binding);
                if (it != edge_binding_to_name.end())
                    names.push_back(it->second);
            }
        }
        return result;
    }
};

/// @brief Солвер гидравлической задачи на несвязном графе
/// @tparam SubgraphSolver Тип солвера для подграфов (например, nodal_solver_buffer_based_t)
/// @tparam Graph Тип графа
/// @tparam ModelIncidences Тип инцидентностей модели
/// @tparam PipeModelType Тип модели трубы
template <
    typename SubgraphSolver,
    typename Graph,
    typename ModelIncidences,
    typename PipeModelType
>
class structured_hydro_solver_t {
private:
    const Graph& graph;
    const structured_hydro_solver_settings_t& settings;
    
public:
    /// @brief Конструктор
    /// @param graph Граф для расчета
    /// @param settings Настройки солвера
    structured_hydro_solver_t(
        const Graph& graph,
        const structured_hydro_solver_settings_t& settings)
        : graph(graph)
        , settings(settings)
    {}

    /// @brief Продвижение буферов на следующий слой.
    void advance_pipe_buffers()
    {
        for (const auto& edge2 : graph.edges2) {
            if (auto casted_model = dynamic_cast<PipeModelType*>(edge2.model))
                casted_model->buffer_advance();
        }
    }
    
    /// @brief Решает гидравлическую задачу на структурированном графе
    /// @param estimation_type Способ формирования начального приближения
    /// @return Давления и расходы по подграфам, диагностика расчета, сопоставление подграфов с исходным графом
    structured_flow_distribution_result solve(solver_estimation_type_t estimation_type)
    {
        auto [structured_graph, flow_subgraphs, zeroflow_subgraphs] = split_graph<Graph>(graph);

        structured_flow_distribution_result full_graph_result(structured_graph.descend_map);

        auto subgraph_solver_settings = settings.get_settings<SubgraphSolver>();
        subgraph_solver_settings.analysis_mode = settings.analysis_mode;
        subgraph_solver_settings.analysis_parameters = settings.analysis_parameters;

        // Решение для каждого проточного подграфа
        for (const size_t subgraph_id : flow_subgraphs) {
            const Graph& subgraph = structured_graph.subgraphs.at(subgraph_id);
            SubgraphSolver solver(subgraph, subgraph_solver_settings);
            full_graph_result.results_by_subgraphs[subgraph_id] = 
                solver.solve(estimation_type, layer_offset_t::ToCurrentLayer);
        }
        return full_graph_result;
    }
    
    /// @brief Создает солвер для подграфа
    // TODO: нужен ли этот метод?
    template <typename SubgraphSolverSettings>
    SubgraphSolver create_subgraph_solver(
        const Graph& subgraph,
        const SubgraphSolverSettings& subgraph_solver_settings) const
    {
        return SubgraphSolver(subgraph, subgraph_solver_settings);
    }
};

} // namespace graphlib

