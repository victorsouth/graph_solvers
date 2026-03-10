#pragma once

namespace graphlib {
;

/// @brief Выделение непроточности части на основе алгоритма распространения расхода на дереве блоков
/// По сути является оберткой над алгоритмом распространения расходов. 
/// Там, где можно распространить нулевые расходы, это и будет непроточное часть, остальное - проточная
/// @tparam Graph Тип графа
/// @tparam BoundMeasurement Тип фиктивных измерений расхода для алгоритма распространения
/// @tparam BindingGetter Задает привязку к графу для контейнера с измерениями 
/// (например, set_binding(graph_quantity_value_t* measurement, const graphlib::graph_place_binding_t& binding))
template <typename Graph, 
    typename BoundMeasurement,
    typename BindingSetter
>
class zeroflow_decomposer_t {
public:
    /// @brief Тип моделей, сопоставленных ребрам графа
    typedef typename Graph::model_type ModelIncidences;

    /// @brief Проводит параллельную проточно-непроточную декомпозицию автономных контуров
    /// @param structured_graph Автономные контуры графа
    static std::tuple<
        graphlib::general_structured_graph_t<Graph>,
        std::vector<size_t>,
        std::vector<size_t>
    >
        split(
            bool tadpole_is_zeroflow,
            const graphlib::general_structured_graph_t<Graph>& structured_graph,
            bool (* is_zero_flow_edge)(const ModelIncidences&)
            )
    {

        std::unordered_map<size_t, std::vector<graphlib::subgraph_t>> subgraphs;

        for (const auto& [subgraph_id, subgraph] : structured_graph.subgraphs) {
            // Выделяем дерево блоков в каждом компоненте связности
            block_tree_t<ModelIncidences> block_tree = block_tree_builder_BGL<ModelIncidences>::get_block_tree(subgraph);

            // Выявляем заведомо непроточные односторонние ребра
            std::vector<size_t> zeroflow_edges1 = search_zero_flow_edges1(block_tree, is_zero_flow_edge);

            // Формируем фиктивные измерения Q=0 на заведомо непроточных притоках
            std::vector<BoundMeasurement> dummy_flows = get_flow_measurements_for_zeroflow_edges1(zeroflow_edges1);

            // Алгоритмом распространения расходов выявляем подграф Q=0
            // После алгортима проточной является часть с неопределенным расходом Q=nan
            graphlib::transport_block_solver_settings default_settings;
            graphlib::block_flow_propagator<ModelIncidences, BoundMeasurement> 
                flow_propagator(subgraph, block_tree, default_settings);
            auto [flows1, cut_flows] = flow_propagator.propagate_flow(dummy_flows);

            // Расставляем флаги непроточности по результатам алгоритма распространения
            std::vector<bool> edge1_zeroflow_flag, edge2_zeroflow_flag;
            std::tie(edge1_zeroflow_flag, edge2_zeroflow_flag) = get_zeroflow_flags(block_tree, flows1, cut_flows);

            // Выделяем комопненты связности в проточном и непроточном подграфах
            subgraphs[subgraph_id] = split_zeroflow(subgraph, edge1_zeroflow_flag, edge2_zeroflow_flag);
        }

        graphlib::general_structured_graph_t<Graph> result = structured_graph.split(subgraphs);

        auto [flow_subgraphs, zeroflow_subgraphs] =
            get_subgraphs_flow_relation(tadpole_is_zeroflow, is_zero_flow_edge, result);

        return std::make_tuple(
            std::move(result),
            std::move(flow_subgraphs),
            std::move(zeroflow_subgraphs));

    }

protected:
    /// @brief Проверяет, является ли подграф проточным
    /// @param tadpole_is_zeroflow Флаг, считать ли головастики непроточными
    /// @param is_zero_flow_edge Функция проверки, является ли ребро непроточным
    /// @param subgraph Подграф для проверки
    /// @return true, если подграф проточный, иначе false
    static bool is_nonzeroflow_subgraph(
        bool tadpole_is_zeroflow,
        bool (*is_zero_flow_edge)(const ModelIncidences&),
        const Graph& subgraph
        ) 
    {
        size_t nonzeroflow_sources_count = 0;

        for (size_t index = 0; index < subgraph.edges1.size(); ++index)
        {
            const ModelIncidences& model = subgraph.edges1.at(index);
            // Если ребро само по себе непроточное, то оно точно не попадает в проточный подграф
            if (!is_zero_flow_edge(model)) {
                ++nonzeroflow_sources_count;
                if (tadpole_is_zeroflow) {
                    // если головастики считаются непроточными, то надо хотя бы два nonzeroflow-поставщика
                    if (nonzeroflow_sources_count >= 2) {
                        return true;
                    }
                }
                else {
                    // если головастики считаются проточными, 
                    // то единственные nonzeroflow-поставщик дает проточный подграф
                    return true; 
                }
            }
                
        }
        return false;
    }
    /// @brief Получает отношение проточности для подграфов
    /// @param tadpole_is_zeroflow Флаг, считать ли головастики непроточными
    /// @param is_zero_flow_edge Функция проверки, является ли ребро непроточным
    /// @param structured_graph Структурированный граф
    /// @return Кортеж из вектора идентификаторов проточных подграфов и вектора идентификаторов непроточных подграфов
    static std::tuple<
        std::vector<size_t>,
        std::vector<size_t>
    > get_subgraphs_flow_relation(
        bool tadpole_is_zeroflow,
        bool (*is_zero_flow_edge)(const ModelIncidences&),
        const graphlib::general_structured_graph_t <Graph>& structured_graph)
    {
        std::vector<size_t> flow_subgraphs;
        std::vector<size_t> zeroflow_subgraphs;

        for (auto& [id, subgraph] : structured_graph.subgraphs) {
            if (is_nonzeroflow_subgraph(tadpole_is_zeroflow, is_zero_flow_edge, subgraph)) {
                flow_subgraphs.push_back(id);
            }
            else {
                zeroflow_subgraphs.push_back(id);
            }
        }

        return std::make_tuple(std::move(flow_subgraphs), std::move(zeroflow_subgraphs));

    }

    /// @brief На основе результатов распространения расходов формирует флаги непроточности для ребер edge1 и edge2
    /// @param block_tree Дерево блоков на графе
    /// @param flows1 Расходы через edge1
    /// @param cut_flows Расходы через коннекторы блоков
    /// @return (флаг непроточности для ребер1; флаг непроточности для ребер2)
    static std::tuple<std::vector<bool>, std::vector<bool>> get_zeroflow_flags(const graphlib::block_tree_t<ModelIncidences>& block_tree,
        const std::vector<double>& flows1, const std::vector<std::vector<double>>& cut_flows)
    {
        // По умолчанию полагаем весь граф непроточным
        // Для ребер, пройденных алгоритмом, далее снимем флаг непроточности
        std::vector<bool> edge1_zeroflow_flags(block_tree.edges1.size(), true);
        std::vector<bool> edge2_zeroflow_flags(block_tree.edge2_to_block.size(), true);

        // Непроточность ребер 1-го типа (всегда вне блоков!)
        for (size_t edge1_index = 0; edge1_index < block_tree.edges1.size(); ++edge1_index)
        {
            // Если расход не определен, значит непроточный алгортим НЕ обошел ребро - оно проточное
            if (std::isnan(flows1[edge1_index])) {
                edge1_zeroflow_flags[edge1_index] = false;
            }
        }

        // Непроточность ребер 2-го типа (всегда в блоке или мосте)
        for (size_t block_index = 0; block_index < block_tree.blocks.size(); ++block_index)
        {
            // Расходы для точек сочленения блоков
            const std::vector<double> block_connectors_flows = cut_flows[block_index];

            // Если расход на коннекторе не определен, коннектор проточный
            // Если у блока хотя бы один проточный коннектор, то обязательно будет и другой. Блок будет проточный
            bool is_block_flow_enabled = std::any_of(block_connectors_flows.begin(), block_connectors_flows.end(),
                [](double flow) { return std::isnan(flow); });

            if (is_block_flow_enabled) {
                const biconnected_component_t& block = block_tree.blocks[block_index];
                for (size_t edge2 : block.edges2) // индексы глобальные - из исходного графа
                {
                    edge2_zeroflow_flags[edge2] = false;
                }
            }
        }

        return std::make_tuple(std::move(edge1_zeroflow_flags), std::move(edge2_zeroflow_flags));

    }

    static std::vector<graphlib::subgraph_t>
    /// @brief Разделяет граф на подграфы по признаку непроточности
    /// @param graph Исходный граф
    /// @param zeroflow_flag_edge1 Вектор флагов непроточности для односторонних ребер
    /// @param zerflow_flag_edge2 Вектор флагов непроточности для двусторонних ребер
    /// @return Вектор подграфов, полученных в результате разделения
        split_zeroflow(const Graph& graph, 
        const std::vector<bool>& zeroflow_flag_edge1, const std::vector<bool>& zerflow_flag_edge2)
    {
        std::vector<graphlib::subgraph_t> result_subgraphs = graph.bfs_search_subgraph(
            [&](size_t index1, const ModelIncidences& model) { return zeroflow_flag_edge1[index1] == true; },
            [&](size_t index2, const ModelIncidences& model) { return zerflow_flag_edge2[index2] == true; });

        std::vector<graphlib::subgraph_t> flow_enabled_subgraphs = graph.bfs_search_subgraph(
            [&](size_t index1, const ModelIncidences& model) { return zeroflow_flag_edge1[index1] == false; },
            [&](size_t index2, const ModelIncidences& model) { return zerflow_flag_edge2[index2] == false; });

         result_subgraphs.insert(result_subgraphs.end(), flow_enabled_subgraphs.begin(), flow_enabled_subgraphs.end());
         return result_subgraphs;
    }

    /// @brief Формирует фиктивные измерения расхода Q=0 на притоках с заданными индексами
    static std::vector<BoundMeasurement> get_flow_measurements_for_zeroflow_edges1(const std::vector<size_t>& zeroflow_edges1)
    {
        std::vector<BoundMeasurement> measurements(zeroflow_edges1.size());
        for (size_t i = 0; i < zeroflow_edges1.size(); ++i)
        {         
            size_t edge1_index = zeroflow_edges1[i];
            
            graphlib::graph_binding_t binding(graph_binding_type::Edge1, edge1_index);
            double dummy_flow_value = 0;
            
            BoundMeasurement flow;
            static_cast<graphlib::graph_place_binding_t&>(flow) = binding;
            flow.data.value = dummy_flow_value;
            measurements[i] = flow;
        }
        return measurements;
    }
    
    /// @brief Выявляет ЗАВЕДОМО непроточные односторонние ребра в дереве блоков
    static std::vector<size_t> search_zero_flow_edges1(const block_tree_t<ModelIncidences>& block_tree,
        bool (*is_zero_flow_edge)(const ModelIncidences&))
    {
        const auto& graph = *block_tree.graph;    
        std::vector<size_t> zero_flow_edge1;
        for (size_t index = 0; index < graph.edges1.size(); ++index)
        {
            const ModelIncidences& model = graph.edges1.at(index);
            // Если ребро само по себе непроточное, то оно точно не попадает в проточный подграф
            if (is_zero_flow_edge(model))
                zero_flow_edge1.push_back(index);
        }
        return zero_flow_edge1;
    }

};




}