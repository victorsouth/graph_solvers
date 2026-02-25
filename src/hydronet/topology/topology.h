


namespace graphlib {
;

/// @brief 
/// @tparam SourceGraph 
/// @tparam SplittedGraph 
/// @tparam ModelInfo 
/// @param source_graph 
/// @param remove_null_edges 
/// @param model_info Функция, определяющая тип ребра (instant/splittable)
/// Если модель ребра = nullptr, функция не вызывается
/// @return 
template <typename SourceGraph, typename SplittedGraph, typename ModelInfo>
inline std::pair<
    general_structured_graph_t<SplittedGraph>, 
    std::vector<edge_incidences_t>
> break_splittables2(const SourceGraph& source_graph, bool remove_null_edges,
    ModelInfo model_computational_type)
{
    std::pair<general_structured_graph_t<SplittedGraph>, std::vector<edge_incidences_t>> result;
    general_structured_graph_t<SplittedGraph>& split = result.first;
    std::vector<edge_incidences_t>& calculation_order = result.second;

    split.ascend_map.reserve(source_graph.get_object_count());

    SplittedGraph split_graph;
    split_graph.id = 0;

    for (size_t edge_index = 0; edge_index < source_graph.edges1.size(); ++edge_index) {
        const auto& edge1 = source_graph.edges1[edge_index];
        if (edge1.has_vertex_from()) {
            split_graph.edges1.emplace_back(
                edge_incidences_t::single_sided_from(edge1.from),
                edge1.model);
        }
        else {
            split_graph.edges1.emplace_back(
                edge_incidences_t::single_sided_to(edge1.to),
                edge1.model);
        }
        split.ascend_map.emplace(std::piecewise_construct,
            std::forward_as_tuple(split_graph.id, graph_binding_type::Edge1, split_graph.edges1.size() - 1),
            std::forward_as_tuple(graph_binding_type::Edge1, edge_index));
    }

    for (size_t edge_index = 0; edge_index < source_graph.edges2.size(); ++edge_index) {
        const auto& source_edge2 = source_graph.edges2[edge_index];
        if (source_edge2.model == nullptr && remove_null_edges) {
            continue;
        }

        computational_type model_type = source_edge2.model == nullptr
            ? computational_type::Instant // Если model = nullptr, то она трактуется как Instant
            : model_computational_type(edge_index);

        bool is_instant = model_type == computational_type::Instant;
        if (!is_instant) {
            if (model_type == computational_type::SplittableOrdered) {
                calculation_order.emplace_back(
                    source_edge2.from, source_edge2.to
                );
            }

            // Начало рассченного ребра
            split_graph.edges1.emplace_back(
                edge_incidences_t::single_sided_from(source_edge2.from),
                source_edge2.model->get_single_sided_model(false));
            split.ascend_map.emplace(std::piecewise_construct,
                std::forward_as_tuple(split_graph.id, graph_binding_type::Edge1, split_graph.edges1.size() - 1),
                std::forward_as_tuple(graph_binding_type::Edge2, edge_index));

            // Конец рассченного ребра
            split_graph.edges1.emplace_back(
                edge_incidences_t::single_sided_to(source_edge2.to),
                source_edge2.model->get_single_sided_model(true));
            split.ascend_map.emplace(std::piecewise_construct,
                std::forward_as_tuple(split_graph.id, graph_binding_type::Edge1, split_graph.edges1.size() - 1),
                std::forward_as_tuple(graph_binding_type::Edge2, edge_index));
        }
        else {
            split_graph.edges2.emplace_back(
                edge_incidences_t(source_edge2.from, source_edge2.to),
                source_edge2.model);
            split.ascend_map.emplace(std::piecewise_construct,
                std::forward_as_tuple(split_graph.id, graph_binding_type::Edge2, split_graph.edges2.size() - 1),
                std::forward_as_tuple(graph_binding_type::Edge2, edge_index));
        }
    }

    // Индексация вершин остается
    size_t vertex_count = source_graph.get_vertex_count();
    for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
        split.ascend_map.emplace(std::piecewise_construct,
            std::forward_as_tuple(split_graph.id, graph_binding_type::Vertex, vertex),
            std::forward_as_tuple(graph_binding_type::Vertex, vertex));
    }
    get_inverted_index(split.ascend_map, &split.descend_map);

    split.subgraphs.emplace(split_graph.id, std::move(split_graph));

    return result;
}

/// @brief Выделение автономных контуров. Для каждого автономного контура выделяется проточная и непроточная части
/// @param graph SISO граф
/// @return (Подграфы; Идентификаторы проточных подграфов, Идентификаторы непроточных подграфов)
template <
    typename Graph,
    typename BoundMeasurement,
    typename BindingSetter,
    typename ModelSplitTypeGetter,
    typename IsZeroFlowSourceChecker
>
inline std::tuple<
    general_structured_graph_t<Graph>,
    std::vector<size_t>,
    std::vector<size_t>
> split_graph(const Graph& graph,
    bool tadpole_is_zeroflow,
    ModelSplitTypeGetter get_model_split_type,
    IsZeroFlowSourceChecker is_zero_flow_edge)
{
    auto broken = break_splittables2<Graph, Graph>(
        graph, false, get_model_split_type);
    general_structured_graph_t<Graph>& broken_transport_graph = broken.first;

    //return zeroflow_decomposer_t<Graph, BoundMeasurement>::split(
        //broken_transport_graph, is_zero_flow_edge);
    return zeroflow_decomposer_t<
        Graph, BoundMeasurement,
        BindingSetter
    >::split(tadpole_is_zeroflow, broken_transport_graph, is_zero_flow_edge);
}


}
