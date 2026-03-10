#pragma once

namespace graphlib {
;

/// @brief Вспомогательный шаблон для static_assert в шаблонных функциях (GCC совместимость)
template <typename T>
struct always_false : std::false_type {};

/// @brief Настройки солвера транспортной задачи на несвязном графе
struct structured_transport_solver_settings_t {
    /// @brief Настройки солвера транспортной задачи на несвязном графе с использованием дерева блоков
    transport_block_solver_settings block_solver;
    /// @brief Настройки солвера транспортной задачи на подграфе полной распространимости
    transport_graph_solver_settings graph_solver;
    /// @brief Опции выбора рассчитываемых эндогенных параметров
    pde_solvers::endogenous_selector_t endogenous_options;
    /// @brief Переносить ли измерения из непроточной части в проточную
    bool rebind_zeroflow_measurements{ true };
    
    /// @brief Создаёт набор настроек со значениями по умолчанию
    static structured_transport_solver_settings_t default_values() {
        structured_transport_solver_settings_t result;
        result.rebind_zeroflow_measurements = true;
        result.block_solver = transport_block_solver_settings::default_values();
        result.graph_solver = transport_graph_solver_settings::default_values();
        result.endogenous_options = pde_solvers::endogenous_selector_t{};
        return result;
    }

    /// @brief Возвращает настройки для указанного типа солвера подграфа
    /// @tparam SubgraphSolver Тип солвера подграфа (шаблонный тип)
    template <typename SubgraphSolver>
    const solver_settings_t<SubgraphSolver>& get_settings() const {
        using settings_type = solver_settings_t<SubgraphSolver>;
        if constexpr (std::is_same_v<settings_type, transport_block_solver_settings>) {
            return block_solver;
        }
        else if constexpr (std::is_same_v<settings_type, transport_graph_solver_settings>) {
            return graph_solver;
        }
        else {
            throw std::runtime_error("Unsupported SubgraphSolver type. Solver must have Settings type alias.");
        }
    }
};

/// @brief Солвер транспортной задачи на несвязном графе
/// @tparam SubgraphSolver Тип солвера для подграфов (например, transport_block_solver_template_t или transport_graph_solver_template_t)
/// @tparam Graph Тип графа
/// @tparam BoundMeasurement Тип измерений
/// @tparam EndogenousSelector Тип селектора эндогенных параметров
/// @tparam PipeModelType Тип модели трубы
template <
    typename SubgraphSolver,
    typename Graph,
    typename BoundMeasurement,
    typename EndogenousSelector,
    typename PipeModelType
>
class structured_transport_solver_t {
private:
    const Graph& graph;
    const EndogenousSelector& selector;
    const structured_transport_solver_settings_t& settings;    
public:
    /// @brief Конструктор
    structured_transport_solver_t(
        const Graph& graph,
        const EndogenousSelector& selector,
        const structured_transport_solver_settings_t& settings)
        : graph(graph)
        , selector(selector)
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
    
    /// @brief Решает транспортную задачу на структурированном графе
    /// @param prepare_flow_subgraphs_func Функция для разделения графа на подграфы и распределения измерений
    /// /// @param use_flows_from_buffers Использовать расходы из буферов вместо измерений
    /// @return Информация о полноте и использовании измерений
    transport_problem_info<BoundMeasurement> solve(
        const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous_measurements,
        bool use_flows_from_buffers = false)
    {
        constexpr double time_step = std::numeric_limits<double>::infinity();
        return step(time_step, flow_measurements, endogenous_measurements, use_flows_from_buffers);
    }
    
    /// @brief Выполняет шаг расчета на структурированном графе
    /// @param use_flows_from_buffers Использовать расходы из буферов вместо измерений
    /// @return Информация о полноте и использовании измерений
    transport_problem_info<BoundMeasurement> step(
        double time_step,
        const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous_measurements,
        bool use_flows_from_buffers = false)
    {
        if (std::isnan(time_step)) {
            throw std::runtime_error("Time step is NaN.");
        }
        
        // Разделение графа на подграфы
        auto [structured_graph, flow_subgraphs, zeroflow_subgraphs, subgraph_flow_measurements, subgraph_endogenous_measurements] =
            prepare_flow_subgraphs_and_measurements<Graph>(graph, flow_measurements, endogenous_measurements,
                settings.rebind_zeroflow_measurements);

        transport_problem_info<BoundMeasurement> graph_result;

        const auto& subgraph_solver_settings = settings.get_settings<SubgraphSolver>();

        // Решение для каждого проточного подграфа
        for (size_t subgraph_id : flow_subgraphs) {
            const Graph& subgraph = structured_graph.subgraphs.at(subgraph_id);
           
            SubgraphSolver solver(subgraph, selector, subgraph_solver_settings);

            const auto& the_subgraph_flow_measurements = 
                graphlib::get_value_or_default(subgraph_flow_measurements, subgraph_id);
            const auto& the_subgraph_endo_measurements = 
                graphlib::get_value_or_default(subgraph_endogenous_measurements, subgraph_id);

            if (!use_flows_from_buffers && the_subgraph_flow_measurements.size() == 0)
                throw std::runtime_error("No flow measurements passed for Flow Subgraph");

            transport_problem_info<BoundMeasurement> subgraph_result = 
                call_solver_step(solver, time_step, 
                    the_subgraph_flow_measurements,
                    the_subgraph_endo_measurements);

            graph_result.append(
                subgraph_result.ascend_bindings(structured_graph, subgraph_id));
        }

        // Обработка непроточных подграфов
        for (size_t subgraph_id : zeroflow_subgraphs) {
            auto it = subgraph_endogenous_measurements.find(subgraph_id);
            if (it == subgraph_endogenous_measurements.end())
                continue; // Нет измерений по подграфу
            
            // Все измерения помечаем неиспользованными
            for (const BoundMeasurement& measurement : it->second)
            {
                BoundMeasurement ascended_measurement = ascend_graph_binding(
                    structured_graph, subgraph_id, measurement);
                ascended_measurement.data.status = measurement_status_t::unused;
                graph_result.measurement_status.endogenous.emplace_back(
                    std::move(ascended_measurement)
                );
            }
        }

        return graph_result;
    }
    
private:
    
    /// @brief Вызывает метод step солвера
    template <typename Solver>
    transport_problem_info<BoundMeasurement> call_solver_step(
        Solver& solver,
        double time_step,
        const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous_measurements) const
    {
        if constexpr (requires { 
            solver.step(time_step, flow_measurements, endogenous_measurements); 
        }) {
            // Стандартный интерфейс: step(time, flow, endogenous) -> transport_problem_info
            return solver.step(time_step, flow_measurements, endogenous_measurements);
        } else if constexpr (requires {
            solver.step(time_step, flow_measurements, endogenous_measurements, graphlib::layer_offset_t::ToCurrentLayer);
        }) {
            // Интерфейс для transport_block_solver_buffer_based: step(time, flow, endogenous, layer_offset) -> transport_problem_info
            return solver.step(time_step, flow_measurements, endogenous_measurements, 
                graphlib::layer_offset_t::ToCurrentLayer);
        } else if constexpr (requires {
            solver.step(time_step, endogenous_measurements, graphlib::solver_estimation_type_t::FromCurrentLayer);
        }) {
            // Интерфейс для endogenous_solver_buffer_based: step(time, endogenous, offset) -> void
            solver.step(time_step, endogenous_measurements, 
                graphlib::solver_estimation_type_t::FromCurrentLayer);
            transport_problem_info<BoundMeasurement> result;
            return result;
        } else {
            // Используем зависимый тип для GCC совместимости
            static_assert(always_false<Solver>::value, "SubgraphSolver must have step method with compatible signature");
        }
    }
};

} // namespace graphlib

