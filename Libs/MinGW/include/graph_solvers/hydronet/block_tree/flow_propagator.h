#pragma once

namespace graphlib {
;

/// @brief Параметры транспортного солвера
struct transport_block_solver_settings {
    /// @brief Для блока с двумя ребрами и двумя точками сочленения делим расход пополам
    bool use_simple_block_flow_heuristic;

    /// @brief Создаёт набор настроек со значениями по умолчанию
    static transport_block_solver_settings default_values() {
        transport_block_solver_settings result;
        result.use_simple_block_flow_heuristic = false;
        return result;
    }
};

/// @brief Обеспечивает распространение расходов на дереве блоков
/// @tparam ModelIncidences Инцидентности модели ребра
/// @tparam BoundMeasurement Структура - значение измерения и его привязка к графу, поверх которого строится граф блоков
/// @tparam FlowGetter Геттер, возвращающий расход из измерения BoundMeasurement
template <typename ModelIncidences,
    typename BoundMeasurement>
class block_flow_propagator
{
    const transport_block_solver_settings& settings;
    /// @brief Транспортный граф
    const basic_graph_t<ModelIncidences>& graph;
    /// @brief Дерево блоков поверх транспортного графа
    const block_tree_t<ModelIncidences>& block_tree;
    /// @brief Количество ожидаемых посещений разделяющих вершин - 
    /// количество непосещенных поставщиков или коннекторов блоков, входящих в вершину
    mutable std::unordered_map<size_t, int> cut_vertex_remaining_visits;

protected:
    
    /// @brief Возвращает привязки объектов дерева блоков, инцидентных данной точке сочленения И имеющих неизвестные расходы
    /// @param vertex Точка сочленения
    /// @param cut_vertices_incidences Инциденции точек сочленения на дереве блоков (!)
    /// @param flows1 Расходы по ребрам, инцидентным точкам сочленения
    /// @param cut_flows Расходы по точкам сочленения блоков
    /// @return Список привязков на дереве блоков
    inline std::vector<block_tree_binding_t>
        get_elements_with_nan_flows(size_t vertex, const std::unordered_map<size_t, cut_vertex_incidences_t>& cut_vertices_incidences,
            const std::vector<double>& flows1,
            const std::vector<std::vector<double>>& cut_flows)
    {
        std::vector<block_tree_binding_t> result;

        // Инциденции заданной вершины на дереве блоков
        const cut_vertex_incidences_t& incidences = cut_vertices_incidences.at(vertex);

        for (size_t block_index : incidences.blocks) {
            const biconnected_component_t& block = block_tree.blocks[block_index];
            // Расходы для точек сочленения блоков
            size_t cut_vertex = block.get_local_cut_vertex_index(vertex);
            if (std::isnan(cut_flows[block_index][cut_vertex])) {
                result.emplace_back(block_tree_binding_type::Block_cutvertex, block_index, cut_vertex);
            }
        }

        for (size_t edge1 : incidences.edges1) {
            // Расходы для односторонних ребер, инцидентынх точкам сочленения на дереве блоков
            size_t edge_local_index = block_tree.get_local_edge1_index(edge1);
            if (std::isnan(flows1[edge_local_index])) {
                result.emplace_back(block_tree_binding_type::Edge1, edge1);
            }
        }

        return result;
    }

    /// @brief Возвращает индексы точек сочленения  заданного блока, для которых расход неизвестен
    /// ВАЖНО! Возвращаемые индексы соответствуют
    /// @param block ID блока
    /// @param cut_flows Расходы на точках сочленения блоков
    /// @param flows1_internal Расходы по внутренним(!) односторонним ребрам блоков
    inline std::vector<size_t>
        get_local_cutvertex_index_with_nan_flow(size_t block, const std::vector<std::vector<double>>& cut_flows)
    {
        std::vector<size_t> cut_vertices_nan_flow;

        for (size_t vertex_index = 0; vertex_index < block_tree.blocks[block].cut_vertices.size(); ++vertex_index) {
            if (std::isnan(cut_flows[block][vertex_index]))
                cut_vertices_nan_flow.push_back(vertex_index);
        }

        return cut_vertices_nan_flow;
    }

    /// @brief Рассчитывает дисбаланс расходов в вершине дерева блоков
    /// Расчет на основе матбаланса для разделяющей вершины: 
    /// сумма расходов по всем точкам сочленения блоков и по односторонним ребрам равна нулю 
    /// @param vertex Вершина на дереве блоков
    /// @param cut_vertices_map Инциденции точек сочленения на дереве блоков (не в исходном графе!)
    /// @param flows1 Расходы по односторнним ребрам
    /// @param cut_flows Расходы по точкам сочленения
    /// @return Дисбаланс
    inline double calc_disbalance_for_vertex(size_t vertex, const std::unordered_map<size_t, cut_vertex_incidences_t>& cut_vertices_map,
        const std::vector<double>& flows1,
        const std::vector<std::vector<double>>& cut_flows)
    {
        // Расход всех известных потоков с учетом их ориентации
        double cut_vertex_flow_disbalance = 0;

        // Получаем инцидентные ребра для текущей точки сочленения
        const auto& cut_vertex_incidences = cut_vertices_map.at(vertex);

        // Вклад односторнних ребер в дисбаланс расходов на точке сочленения
        for (const auto& edge : cut_vertex_incidences.edges1) {
            size_t edge_local_index = block_tree.get_local_edge1_index(edge);
            double flow_edge1 = flows1[edge_local_index];
            int flow_sign = graph.edges1[edge].has_vertex_from() ? -1 : +1;
            if (!std::isnan(flow_edge1))
                cut_vertex_flow_disbalance += flow_edge1 * flow_sign;
        }

		// Вклады блоков в дисбаланс расходов на точке сочленения
		for (const auto& block_index : cut_vertex_incidences.blocks) {
			const biconnected_component_t& block = block_tree.blocks[block_index];
			size_t cut_vertex = block.get_local_cut_vertex_index(vertex);
			int flow_sign = block.cut_is_sink[cut_vertex] ? -1 : +1;
			double cut_flow = cut_flows[block_index][cut_vertex];
			if (!std::isnan(cut_flow))
				cut_vertex_flow_disbalance += cut_flow * flow_sign;
		}
        // Расход неизвестного потока равен дебалансу с обратным знаком:
        // Если дебаланс > 0, есть избыток вещества. Неизвестный поток должен быть оттоком
        // Если дебаланс < 0, есть недостаток вещества. Неизвестный поток должен быть притоком
        return cut_vertex_flow_disbalance;
    }

    /// @brief Рассчитывает дисбаланс расходов по точкам сочленения блока
    /// @param block Блок
    /// @param cut_vertices_map Инциденции точек сочленения на дереве блоков
    /// @param cut_flows Расходы по точкам сочленения
    /// @return Дисбаланс
    inline double calc_disbalance_for_block(size_t block, const std::unordered_map<size_t, cut_vertex_incidences_t>& cut_vertices_map,
        const std::vector<std::vector<double>>& cut_flows)
    {
        double flow_disbalance = 0;

        const std::vector<size_t>& cut_vertices = block_tree.blocks[block].cut_vertices;

        // Будет выполнен только один из циклов - либо по точкам сочленения, либо по внутренним односторонним ребрам
        for (size_t cut_vertex_index = 0; cut_vertex_index < cut_vertices.size(); ++cut_vertex_index) {
            if (std::isnan(cut_flows[block][cut_vertex_index]))
                continue;
            int flow_sign = block_tree.blocks[block].cut_is_sink[cut_vertex_index] ? +1 : -1;
            flow_disbalance += cut_flows[block][cut_vertex_index] * flow_sign;
        }

        return flow_disbalance;
    }

    /// @brief Определяет расход для точек сочленения блоков и односторонних ребер с незаданным расходом 
    /// @param cut_vertex_flow_disbalance Дисбаланс расхода на вершине дерева блоков
    /// @param unknown_flow_binding Привязка неизвестного расхода на дереве блоков
    /// @param ptr_flows1 Расходы по односторонним ребрам на дереве блоков
    /// @param ptr_cut_flows Расходы по точкам сочленения блоков
    inline void propagate_disbalance_over_vertex_incidences(double cut_vertex_flow_disbalance, const block_tree_binding_t& unknown_flow_binding,
        std::vector<double>* ptr_flows1,
        std::vector<std::vector<double>>* ptr_cut_flows)
    {
        std::vector<std::vector<double>>& cut_flows = *ptr_cut_flows;
        std::vector<double>& flows1 = *ptr_flows1;
        
        // Распространяем дисбаланс
        switch (unknown_flow_binding.binding_type) {
        case block_tree_binding_type::Edge1:
        {
            // По одностороннему ребру с учетом ориентации на графе
            size_t edge = unknown_flow_binding.edge;
            int flow_sign = graph.edges1[edge].has_vertex_from() ? +1 : -1;
            size_t edge_local_index = block_tree.get_local_edge1_index(edge);
            flows1[edge_local_index] = flow_sign * cut_vertex_flow_disbalance;
            break;
        }   
        case block_tree_binding_type::Block_cutvertex:
        {
            // На точку сочленения блока
            auto [block_index, cut_vertex_local_index] = unknown_flow_binding.block_cutvertex;
            const biconnected_component_t& block = block_tree.blocks[block_index];
            int cut_direction = block.cut_is_sink[cut_vertex_local_index] ? +1 : -1;
            cut_flows[block_index][cut_vertex_local_index] = cut_direction * cut_vertex_flow_disbalance;
            break;
        }
        default:
            throw std::logic_error("Wrong binding_type");
        }
    }

    /// @brief На основе величины дисбаланса расходов по блоку рассчитывает расход на точке сочленения блока
    /// ВАЖНО! Неизвестен может быть только один расход
    /// @param flow_disbalance Дисбаланс
    /// @param block_index Индекс блока
    /// /// @param local_vertex_index Локальный индекс рассматриваемой точки сочленения среди всех точек сочленения блока
    /// @param ptr_cut_flows Расходы по точкам сочленения блоков
    inline void propagate_disbalance_over_block(double flow_disbalance, size_t block_index, size_t local_vertex_index,
        std::vector<std::vector<double>>* ptr_cut_flows) const
    {
        std::vector<std::vector<double>>& cut_flows = *ptr_cut_flows;
        const biconnected_component_t& block = block_tree.blocks[block_index];
        int flow_sign = block.cut_is_sink[local_vertex_index] ? -1 : +1;
        cut_flows[block_index][local_vertex_index] = flow_sign * (flow_disbalance);
    }

    /// @brief Обход дерева блоков для распространения расходов
    /// @param vertex Точка сочленения
    /// @param ptr_visited_vertices Посещенные вершины
    /// @param ptr_cut_flows Расходы на точках сочленения
    /// @param ptr_flows1 Расходы на односторонних ребрах
    inline void dfs(size_t vertex, std::set<size_t>* ptr_visited_vertices, 
        std::vector<std::vector<double>>* ptr_cut_flows,
        std::vector<double>* ptr_flows1
    )
    {
        const std::unordered_map<size_t, cut_vertex_incidences_t>& cut_vertices_map = block_tree.get_cut_vertices(graph);

        std::vector<std::vector<double>>& cut_flows = *ptr_cut_flows;
        std::vector<double>& flows1 = *ptr_flows1;
        std::set<size_t>& visited_connectors = *ptr_visited_vertices;
        
        if (cut_vertex_remaining_visits.at(vertex) == 0)
            return; // еще не все входы точки сочленения cut_vertex обработаны

        cut_vertex_remaining_visits.at(vertex) -= 1;

        visited_connectors.emplace(vertex);

        // Привязки объектов, расходы через которые неизвестны
        std::vector<block_tree_binding_t> unknown_flows = get_elements_with_nan_flows(vertex, cut_vertices_map, flows1, cut_flows);

        if (unknown_flows.size() != 1)
            return; // Либо все расходы известны, либо не сможем определить расход

        // Привязка для неизвестного расхода
        const block_tree_binding_t& binding = unknown_flows.front();
        
        // Ищем единственный (!) неизвестный расход по матбалансу для точки сочленения
        double cut_vertex_flow_disbalance = calc_disbalance_for_vertex(vertex, cut_vertices_map, flows1, cut_flows);
        propagate_disbalance_over_vertex_incidences(cut_vertex_flow_disbalance, binding, &flows1, &cut_flows);

        // Если найден расход поставщика/потребителя, то продолжать обход некуда - 
        // нет ни одного блока, для которого можно было бы посчитать матбаланс
        if (binding.binding_type == block_tree_binding_type::Edge1) {
            return; 
        }

        // Если найден расход через точку сочленения блока и 
        // у блока осталась одна точка сочленения с неизвестным расходом, можем найти этот расход

        size_t block_index = binding.block;
        const biconnected_component_t& block = block_tree.blocks[block_index];

        // Локальные индесы точек сочленения блока с неизвестным расходом
        std::vector<size_t> cut_vertices_with_unknown_flows = get_local_cutvertex_index_with_nan_flow(block_index, cut_flows);

        if (cut_vertices_with_unknown_flows.size() != 1)
            return; // из матбаланса для блока ничего не посчитать

        // Индекс рассчитываемой точки сочленения в списке точек сочленения блока
        size_t local_vertex_index = cut_vertices_with_unknown_flows.front();
        // Индекс точки сочленения как вершины на исходном графе
        size_t cut_vertex = block.cut_vertices[local_vertex_index];

        // Дисбаланс расходов по блоку (с учетом внутренних ребер Edge1)
        double block_flow_disbalance = calc_disbalance_for_block(block_index, cut_vertices_map, cut_flows); 

        // Расчет неизвестного расхода через точку сочленения или односторонее внутренне ребро
        propagate_disbalance_over_block(block_flow_disbalance, block_index,
            local_vertex_index, &cut_flows);

        // Переходим в точку сочленения блока, куда удалось распространить расход
        dfs(cut_vertex, &visited_connectors, &cut_flows, &flows1);

    }

public:
    /// @brief Предподсчитывает количество ожидаемых посещений блоков и поставщиков 
    /// для алгоритма распространения расходов
    block_flow_propagator(const basic_graph_t<ModelIncidences>& graph,
        const block_tree_t<ModelIncidences>& block_tree,
        const transport_block_solver_settings& settings
        )
        : graph(graph)
        , block_tree(block_tree)
        , settings(settings)
    {
        for (const auto& [cut_vertex, cut_vertex_incidences] : block_tree.get_cut_vertices(graph)) {
            // Подсчет количества блоков и ребер Edge1, инцндентных вершинам дерева блоков 
            cut_vertex_remaining_visits[cut_vertex] =
                (int)(cut_vertex_incidences.blocks.size() + cut_vertex_incidences.edges1.size());
        }
    }

    /// @brief Определяет расходы на односторонних ребрах и на точках сочленения блока по измерениям расхода
    /// @param flows Измерения расхода 
    /// @return [расходы через односторонние ребра;
    ///         расходы через точки сочленения блоков]
    std::pair<std::vector<double>, std::vector<std::vector<double>>> propagate_flow(const std::vector<BoundMeasurement>& flows)
    {
        auto [flows1, cut_flows] = prepare_flow_struct(block_tree);
        
        // Сопоставляем замеры расхода на ребрах графа расходам на элементах дерева блоков
        std::vector<size_t> cut_vertex_to_visit = copy_measurements_to_flow_struct(graph, block_tree, flows, &cut_flows, &flows1);

        const std::unordered_map<size_t, cut_vertex_incidences_t>& cut_vertices_map = block_tree.get_cut_vertices(graph);
        std::set<size_t> visited_connectors;
        
        
        // Начинаем обход с каждой точки сочленения
        for (size_t vertex : cut_vertex_to_visit) {
            dfs(vertex, &visited_connectors, &cut_flows, &flows1);
        }

        return std::make_pair(std::move(flows1), std::move(cut_flows));
    }
    /// @brief Распространяет расходы с точек сочленения на внутренние ребра блоков
    /// ЕСЛИ указана опция настроек, обрабатывает частный случай блока с двумя ребрами и двумя точками сочленения\
    /// @param cut_flows Расходы на точках сочленения
    /// @return Расходы на ребрах Edge2
    std::vector<double> propagate_flow_to_edges(const std::vector<std::vector<double>>& cut_flows) const
    {
        std::vector<double> edge2_flow(graph.edges2.size(), std::numeric_limits<double>::quiet_NaN());

        // Распространение расхода с точки сочленения возможно только для мостов
        for (size_t block = 0; block < cut_flows.size(); ++block) {
            const std::vector<size_t>& block_edges = block_tree.blocks[block].edges2;
            if (block_edges.size() == 1) {
                // У моста всегда две сочленения с равными расходами
                size_t edge_id = block_edges.front();
                edge2_flow[edge_id] = cut_flows[block].front();
            }
        }
        return edge2_flow;
    }

    /// @brief Выполняет локальный гидравлический расчет для блоков. ОБРАБАТЫВАЕТ ТОЛЬКО СЛУЧАЙ БЛОКА ИЗ ДВУХ РЕБЕР
    /// @details Вычисляет расходы через ребра блоков на основе расходов в точках сочленения
    /// @param flow_measurement_sample Образец измерения расхода для создания псевдоизмерений
    /// @param cut_flows Расходы в точках сочленения для каждого блока
    /// @param edge2_flow Указатель на вектор, в который будут записаны расходы через двусторонние ребра
    /// @return Вектор псевдоизмерений расхода для блоков
    std::vector<BoundMeasurement> block_local_hydraulic_calc(const BoundMeasurement& flow_measurement_sample,
        const std::vector<std::vector<double>>& cut_flows,
        std::vector<double>* edge2_flow
        ) const 
    {
        BoundMeasurement dummy = flow_measurement_sample;

        std::vector<BoundMeasurement> flow_pseudomeasurements;
        if (settings.use_simple_block_flow_heuristic) {
            for (size_t block_index = 0; block_index < cut_flows.size(); ++block_index) {
                const biconnected_component_t& block = block_tree.blocks[block_index];
                if (block.edges2.size() == 2 && block.cut_vertices.size() == 2) {
                    double flow = 0.5 * cut_flows[block_index].front();
                    // У моста всегда две сочленения с равными расходами
                    for (std::size_t edge2 : block.edges2) {
                        (*edge2_flow)[edge2] = flow;
                        dummy = graph_binding_t(graph_binding_type::Edge2, edge2);
                        dummy.data.value = flow;
                        flow_pseudomeasurements.push_back(dummy);
                    }
                }
            }
        }
        return flow_pseudomeasurements;
    }

protected:

    /// @brief Подготовка структур для расходов на заданном графе блоков
    /// @return [расходы по односторонним ребрам; расходы по точкам сочленения блоков]
    static std::tuple<std::vector<double>, std::vector<std::vector<double>>>
        prepare_flow_struct(const block_tree_t<ModelIncidences>& block_tree)
    {
        std::vector<double> flows1(block_tree.edges1.size(), std::numeric_limits<double>::quiet_NaN());
        std::vector<std::vector<double>> flows_blocks;
        flows_blocks.reserve(block_tree.blocks.size());
        for (int i = 0; i < block_tree.blocks.size(); ++i) {
            std::vector<double> cut_flows(block_tree.blocks[i].cut_vertices.size(), std::numeric_limits<double>::quiet_NaN());
            flows_blocks.emplace_back(std::move(cut_flows));
        }

        return std::make_tuple(std::move(flows1), std::move(flows_blocks));
    }

    /// @brief Переносит расходы из измерений на точки сочленения блоков
    /// Расходы не распространяются! 
    /// @param graph 
    /// @param block_tree 
    /// @param flows 
    /// @param ptr_cut_flows Расходы на точках сочленения всех блоков дерева
    /// @param ptr_flows1 
    /// @return 
    static std::vector<size_t> copy_measurements_to_flow_struct(
        const basic_graph_t<ModelIncidences>& graph,
        const block_tree_t<ModelIncidences>& block_tree,
        const std::vector<BoundMeasurement>& flows,
        std::vector<std::vector<double>>* ptr_cut_flows,
        std::vector<double>* ptr_flows1
    )
    {
        std::vector<std::vector<double>>& cut_flows = *ptr_cut_flows;
        std::vector<double>& flows1 = *ptr_flows1;

        std::vector<size_t> cut_vertices_with_flow;

        for (const auto& flow_measurement : flows) {
            switch (flow_measurement.binding_type)
            {
            case graph_binding_type::Edge2:
            {
                size_t edge2_index = flow_measurement.edge;
                size_t block_index =
                    block_tree.edge2_to_block.at(edge2_index);
                if (block_tree.blocks[block_index].edges2.size() != 1)
                { // Пока обрабатываем только мосты
                    throw std::runtime_error("Unexpected flow measurement block configuration");
                }

                const auto& block = block_tree.blocks[block_index];

                for (size_t index = 0; index < block.cut_vertices.size(); ++index) {
                    size_t vertex = block.cut_vertices[index];
                    cut_vertices_with_flow.push_back(vertex);
                    int cut_direction = block.cut_is_sink[index] ? -1 : +1;
                    int edge_incidence = graph.edges2[edge2_index].get_vertex_incidence(vertex);
                    int flow_sign = edge_incidence * cut_direction;
                    cut_flows[block_index][index] = flow_sign * flow_measurement.data.value;
                }
                break;
            }
            case graph_binding_type::Edge1:
            {
                // Одностороннее ребро ВНЕ БЛОКА будет честно учтено в flows1
                size_t edge1_index = flow_measurement.edge;

                const auto& edge1 = graph.edges1[edge1_index];

                // Индекс на дереве блоков
                size_t edge1_local_index = block_tree.get_local_edge1_index(edge1_index);
                flows1[edge1_local_index] = flow_measurement.data.value;

                size_t vertex = edge1.get_vertex_for_single_sided_edge();
                cut_vertices_with_flow.push_back(vertex); 

                // (это похоже не надо): Приписываем знак согласно ориентации поставщика/потребителя на схеме
                /*size_t vertex = edge1.get_vertex_for_single_sided_edge();
                int flow_sign = edge1.get_vertex_incidence(vertex);
                flows1[edge1_local_index] = flow_sign * FlowGetter{}(flow_measurement);
                cut_vertices_with_flow.push_back(vertex);*/

                break;
            }

            default:
                throw std::runtime_error("Unexpected binding type for flow measurement");

            }
        }
        
        return cut_vertices_with_flow;
    }
};


}