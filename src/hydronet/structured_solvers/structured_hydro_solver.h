#pragma once

namespace graphlib {
;

/// @brief Настройки солвера уровня структурированного графа для гидравлики
struct structured_hydro_solver_settings_t {
    /// @brief Настройки солвера МД
    // TODO: после реализации МГГ разнести настройки по base и specific
    graphlib::nodal_solver_settings_t nodal_solver;
    
    /// @brief Создаёт набор настроек со значениями по умолчанию
    static structured_hydro_solver_settings_t default_values() {
        structured_hydro_solver_settings_t result;
        result.nodal_solver = nodal_solver_settings_t::default_values();
        return result;
    }

    /// @brief Возвращает настройки для указанного типа солвера подграфа
    /// @tparam SubgraphSolver Тип солвера подграфа (шаблонный тип)
    template <typename SubgraphSolver>
    const solver_settings_t<SubgraphSolver>& get_settings() const {
        using settings_type = solver_settings_t<SubgraphSolver>;
        if constexpr (std::is_same_v<settings_type, nodal_solver_settings_t>) {
            return nodal_solver;
        }
        else {
            throw std::runtime_error("Unsupported SubgraphSolver type. Solver must have Settings type alias.");
        }
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
    /// @param subgraph_solver_settings Настройки солвера подграфа
    /// @param split_graph_func Функция для разделения графа на подграфы
    /// @param is_first_call Флаг первого вызова (для определения типа начального приближения)
    void solve(
        solver_estimation_type_t estimation_type)
    {
        // Разделение графа на подграфы
        auto [structured_graph, flow_subgraphs, zeroflow_subgraphs] = split_graph<Graph>(graph);
                
        const auto& subgraph_solver_settings = settings.get_settings<SubgraphSolver>();

        // Решение для каждого проточного подграфа
        for (const size_t subgraph_id : flow_subgraphs) {
            const Graph& subgraph = structured_graph.subgraphs.at(subgraph_id);
            
            SubgraphSolver solver(subgraph, subgraph_solver_settings);
            
            solver.solve(estimation_type,
                layer_offset_t::ToCurrentLayer, nullptr /* нет диагностики */);
        }
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

