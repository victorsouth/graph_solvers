#pragma once

namespace graphlib {
;

template <typename ModelIncidences>
inline std::tuple<
    basic_graph_t<ModelIncidences>,
    std::vector<double>,
    std::vector<double>
> prepare_flow_relative_graph(
    const basic_graph_t<ModelIncidences>& graph,
    const std::vector<double> flows1,
    const std::vector<double> flows2
)
{
    basic_graph_t<ModelIncidences> oriented_graph = get_flow_relative_graph(graph, flows1, flows2);

    std::vector<double> oriented_flows1;
    std::vector<double> oriented_flows2;

    // После переориентирования ребер графа все расходы становятся положительными
    auto change_flow_sign = [](double flow) {
        return flow < 0 ? -flow : flow;
        };
    std::transform(flows1.begin(), flows1.end(), std::back_inserter(oriented_flows1), change_flow_sign);
    std::transform(flows2.begin(), flows2.end(), std::back_inserter(oriented_flows2), change_flow_sign);

    return std::tuple(
        std::move(oriented_graph),
        std::move(oriented_flows1),
        std::move(oriented_flows2)
    );
}

/// @brief Распространяет расходы с некоторых ребер на все возмонжые
/// @tparam ModelIncidences Тип ребра графа с его инциденциями
/// @tparam BoundMeasurement Тип измерения с привязкой к ребру графа
/// @tparam FlowGetter Геттер, возвращающий расход из измерения BoundMeasurement
template <typename ModelIncidences, 
    typename BoundMeasurement>
class graph_flow_propagator_t
{
    /// @brief Граф
    const basic_graph_t<ModelIncidences>& graph;
    /// @brief Настройки солвера
    const transport_graph_solver_settings& settings;
    /// @brief Флаг посещения односторонних ребер
    std::vector<bool> edge1_visited;
    /// @brief Флаг посещения двусторонних ребер
    std::vector<bool> edge2_visited;
    /// @brief Расходы через односторонние ребра
    std::vector<double> edge1_flow;
    /// @brief Расходы через двусторонние ребра
    std::vector<double> edge2_flow;

public:

    /// @brief Конструктор. Подготвавливает структуры для флагов посещения ребер и расходов в ребрах.
    /// @param graph Транспортный граф
    graph_flow_propagator_t(
        const basic_graph_t<ModelIncidences>& graph,
        const transport_graph_solver_settings& settings)
        : graph(graph)
        , settings(settings)
        , edge1_visited(graph.edges1.size(), false)
        , edge2_visited(graph.edges2.size(), false)
        , edge1_flow(graph.edges1.size(), std::numeric_limits<double>::quiet_NaN())
        , edge2_flow(graph.edges2.size(), std::numeric_limits<double>::quiet_NaN())
    {
    }

    /// @brief Расчет расходов в ребрах графа на основе измерений 
    /// @param graph Граф, по которому распространяются расходы
    /// @param flow_measurements Замеры расхода в рассматриваемом подграфе. Просматриваются в порядке следования
    /// ВАЖНО! Если два замера избыточны, то будет отброшен тот, что идет дальше в этом списке
    /// @return (Рассчитанные расходы по односторонним SISO ребрам; 
    ///          Рассчитанные расходы по двусторонним SISO ребрам; 
    ///          Привязки объектов, на которые не удалось распространить расход; 
    ///          Избыточные замеры, не пригодившиеся при расчете расходов)
    inline std::tuple<
        std::vector<double>,
        std::vector<double>,
        std::vector<graph_binding_t>,
        std::vector<graph_binding_t>
    > propagate(const std::vector<BoundMeasurement>& flow_measurements)
    {
        std::vector<BoundMeasurement> useless_measurements;

        // Обход графа выполняется от вершин, инцидентных ребрам с замерами расхода
        // TODO: в цикле простая логика занимает почти полсотни строк. Облагородить
        for (const BoundMeasurement& flow : flow_measurements) {

            switch (flow.binding_type)
            {
            case graph_binding_type::Edge1: {  // Замер на ребре1
                size_t edge1 = flow.edge;

                // Если замер посещен, то он не используется
                if (edge1_visited[edge1]) {
                    useless_measurements.push_back(flow);
                    continue;
                }
                edge1_visited[edge1] = true;
                edge1_flow[edge1] = flow.data.value;

                _dfs(graph.edges1[edge1].get_vertex_for_single_sided_edge());
                break;
            }
            case graph_binding_type::Edge2: { // Замер на ребре2
                size_t edge2 = flow.edge;

                // Если замер посещен, то он не используется
                if (edge2_visited[edge2]) {
                    useless_measurements.push_back(flow);
                    continue;
                }

                edge2_visited[edge2] = true;
                edge2_flow[edge2] = flow.data.value;

                _dfs(graph.edges2[edge2].from);
                _dfs(graph.edges2[edge2].to);
                break;
            }
            default:
                throw std::logic_error("Strange flow binding");
            }
        }

        std::vector<graph_binding_t> useless_measurements_bindings;
        for (const auto& measurement : useless_measurements) {
            useless_measurements_bindings.push_back(measurement);
        }

        return std::make_tuple(
            std::move(edge1_flow), 
            std::move(edge2_flow), 
            get_not_visited(), 
            std::move(useless_measurements_bindings)
        );

    }


    /// @brief Распространяет расходы с обработкой ошибок
    /// @details Выполняет распространение расходов и проверяет свойство полной распространимости.
    /// Если нет полной распространимости, выбрасывает исключение.
    /// @param flow_measurements Вектор измерений расхода
    /// @return Кортеж из векторов расходов через односторонние и двусторонние ребра, 
    /// списка непосещенных вершин, графа и вектора бесполезных измерений
    /// @throws graph_exception Если не выполнено свойство полной распространимости
    std::tuple<
        std::vector<double>,
        std::vector<double>,
        std::vector<graph_binding_t>,
        basic_graph_t<ModelIncidences>,
        std::vector<BoundMeasurement>
    > propagate_handled(const std::vector<BoundMeasurement>& flow_measurements)
    {
        auto [flows1, flows2, not_visited, useless_measurements] = propagate(flow_measurements);
        if (not_visited.empty() == false) {
            throw graph_exception(graph.get_binding_list(),
                std::wstring(L"Не выполнено свойство полной распространимости. ") +
                L"Невозможно вычислить все расходы в ребрах графа");
        }
        auto [oriented_graph, oriented_flows1, oriented_flows2] = 
            prepare_flow_relative_graph(graph, flows1, flows2);

        return std::make_tuple(
            std::move(oriented_flows1),
            std::move(oriented_flows2),
            std::move(not_visited),
            std::move(oriented_graph),
            std::move(useless_measurements)
        );
    }

protected:

    /// @brief Выполняет обход в глубину по вершинам. 
    /// Если вершине инциденто единственное ребро без расхода, 
    /// расчитывает дисбаланс расхода и приписывает его этому ребру
    /// @param vertex Текущая вершина
    void _dfs(size_t vertex)
    {
        auto [visited_incidences, not_visited_incidences] = get_visited_and_not_visited_incidences(vertex);

        size_t not_visited_edges_count = not_visited_incidences.total_edges_count();
        if (not_visited_edges_count == 0)
            return; // идти больше некуда, завершаем

        if (not_visited_edges_count > 1)
            return; // по расходу ничего сказать нельзя, возможно, надо зайти с другой ветви

        double vertex_flow = calc_disbalance_for_vertex(visited_incidences);

        size_t other_vertex = propagate_disbalance(vertex, vertex_flow, not_visited_incidences);

        if (other_vertex != -1)
            _dfs(other_vertex);
    }

   
    /// @brief Возвращает посещенные и непосещенные инциденции для заданной вершины
    /// @return (посещенные ребра; НЕпосещенные ребра)
    std::pair<vertex_incidences_pair_t, vertex_incidences_pair_t> get_visited_and_not_visited_incidences(size_t vertex)
    {
        vertex_incidences_pair_t vertex_incidences = graph.get_vertex_incidences().at(vertex);

        auto not_visited_incidences = vertex_incidences.get_visited_incidences(edge1_visited, edge2_visited, /* Выбираем непосещенные ребра */ true);
        auto visited_incidences = vertex_incidences.get_visited_incidences(edge1_visited, edge2_visited);

        return(std::make_pair(std::move(visited_incidences), std::move(not_visited_incidences)));
    }

    /// @brief Рассчитывает дисбаланс по индексам ребер, входящих в вершину и и выходищх из нее
    /// @param visited_incidences Входящие и выходящие ребра
    /// @return Дисбаланс
    inline double calc_disbalance_for_vertex(const vertex_incidences_pair_t& visited_incidences)
    {
        double vertex_flow = 0;
        for (size_t edge1 : visited_incidences.first.inlet_edges) {
            vertex_flow += edge1_flow[edge1];
        }
        for (size_t edge1 : visited_incidences.first.outlet_edges) {
            vertex_flow -= edge1_flow[edge1];
        }
        for (size_t edge2 : visited_incidences.second.inlet_edges) {
            vertex_flow += edge2_flow[edge2];
        }
        for (size_t edge2 : visited_incidences.second.outlet_edges) {
            vertex_flow -= edge2_flow[edge2];
        }
        return vertex_flow;
    }

    /// @brief Приписывает ЕДИНСТВЕННОМУ (!) непосещенному ребру графа, инцидентному вершине, расход, компенсирующий дисбаланс в вершине
    /// ВАЖНО: единственность непосещенного ребра должна обеспечиваться пользователем
    /// @param vertex Индекс вершины графа
    /// @param vertex_flow Дисбаланс в вершине
    /// @param not_visited_incidences Не посещенные ребра, инцидентые вершине
    /// @return Если расход оказался приписан двустороннему ребру, возвращает его противоположную вершину для дальнейшего обхода графа
    /// Иначе возвращает -1
    inline size_t propagate_disbalance(size_t vertex, double vertex_flow,
        const vertex_incidences_pair_t& not_visited_incidences)
    {

        // Реально сработает только одна итерация только у одного из циклов
        // Неизвестный расход в ребре должен компенсировать дисбаланс расходов. 
        for (size_t edge1 : not_visited_incidences.first.inlet_edges) {
            // Если ребро входит в вершину и дисбаланс отрицательный, то расход должен быть равен дисбалансу со знаком минус
            // Если ребро входит в вершину и дисбаланс положительный, то расход должен быть равен дисбалансу
            edge1_flow[edge1] = -vertex_flow;
            edge1_visited[edge1] = true;
        }
        for (size_t edge1 : not_visited_incidences.first.outlet_edges) {
            // Если ребро выходит из вершины и дисбаланс отрицательный, то расход должен быть равен дисбалансу
            // Если ребро выходит из вершины и дисбаланс положительный, то расход должен быть равен дисбалансу со знаком минус
            edge1_flow[edge1] = vertex_flow;
            edge1_visited[edge1] = true;
        }
        for (size_t edge2 : not_visited_incidences.second.inlet_edges) {
            // см. логику для edge1.first.inlet_edges
            edge2_flow[edge2] = -vertex_flow;
            edge2_visited[edge2] = true;
            size_t other_vertex = graph.edges2[edge2].get_other_side_vertex(vertex);
            return other_vertex;
        }
        for (size_t edge2 : not_visited_incidences.second.outlet_edges) {
            // см. логику для edge1.first.outlet_edges
            edge2_flow[edge2] = vertex_flow;
            edge2_visited[edge2] = true;
            size_t other_vertex = graph.edges2[edge2].get_other_side_vertex(vertex);
            return other_vertex;
        }
        return -1;
    }

    /// @brief Проверяет наличие в графе непосещенных ребер
    inline std::vector<graph_binding_t> get_not_visited()
    {
        std::vector<graph_binding_t> result;

        for (size_t edge1 = 0; edge1 < edge1_visited.size(); ++edge1) {
            if (edge1_visited[edge1] == false) {
                result.emplace_back(graph_binding_type::Edge1, edge1);
            }
        }
        for (size_t edge2 = 0; edge2 < edge2_visited.size(); ++edge2) { 
            if (edge2_visited[edge2] == false) {
                result.emplace_back(graph_binding_type::Edge2, edge2);
            }
        }

        return result;
    }

};

}