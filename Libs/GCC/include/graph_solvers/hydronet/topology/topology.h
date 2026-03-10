


namespace graphlib {
;

// Forward declaration
template <typename Graph, typename BoundMeasurement, typename BindingSetter>
class zeroflow_decomposer_t;

/// @brief Способ нумерации вершин в рассеченном графе
enum class broken_graph_vertex_numbering {
    /// @brief Сохраняется нумерация из исходного графа. 
    /// Предполагается, что в исходном графе гарантирована последовательная нумерация, с нуля, без пропусков вершин.
    /// Фактическая последовательность нумерации НЕ проверяется.
    renumber_never,
    /// @brief Выполняется перенумерация вершин в порядке 0, 1, 2 и т.д. 
    /// Предполагается, что в исходном графе нумерация заведомо непоследовательная.
    /// Фактическая последовательность нумерации НЕ проверяется.
    renumber_always,
    /// @brief Проверяется последовательность исходной нумерации. Если последовательная - сохраняется нумерация из исходного графа
    /// Если непоследовательная, выполняется перенумерация.
    renumber_if_needed
};

/// @brief Выполняет рассечение графа по рассекаемым (splittable) ребрам. 
/// @tparam SourceGraph Тип исходного графа
/// @tparam BrokenGraph Тип рассеченного графа
/// @tparam ModelInfo Тип метода для опеделения вида ребра - splittable/instant
template <typename SourceGraph, typename BrokenGraph, typename ModelInfo>
class break_and_renumber_t {
    /// @brief Тип инциденций ребра исходного графа (элемент edges1/edges2)
    using edge_incidences_type = typename SourceGraph::model_type;
private:
    /// @brief Переносит вершины из исходного графа в рассеченный БЕЗ изменения индексации вершин
    static void copy_vertices_to_broken(const SourceGraph& source_graph, 
        general_structured_graph_t<BrokenGraph>& broken_mappings, const size_t broken_graph_id = 0) 
    {
        size_t source_vertex_count = source_graph.get_vertex_count();
        for (size_t vertex = 0; vertex < source_vertex_count; ++vertex) 
        {
            subgraph_binding_t broken_binding(broken_graph_id, graph_binding_type::Vertex, vertex);
            graph_binding_t source_binding(graph_binding_type::Vertex, vertex);

            broken_mappings.ascend_map.emplace(broken_binding, source_binding);
            broken_mappings.descend_map.emplace(source_binding, broken_binding);
        }
    }
    /// @brief Формирует вершины рассеченного графа путем перенумеровки вершин исходного графа
    /// По соглашению broken_graph_id всегда равен нулю при формировании рассеченного графа
    static void reindex_and_copy_vertices_to_broken(const SourceGraph& source_graph, 
        general_structured_graph_t<BrokenGraph>& broken_mappings, const size_t broken_graph_id = 0)
    {
        // Перенумерация вершин - формирует сопоставление "broken -> origin"
        const std::unordered_set<size_t> source_vertices = source_graph.get_vertex_indices();
        size_t broken_vertex_index = 0;
        for (const size_t source_vertex_index : source_vertices) {
            subgraph_binding_t broken_binding(broken_graph_id, graph_binding_type::Vertex, broken_vertex_index);
            graph_binding_t source_binding(graph_binding_type::Vertex, source_vertex_index);

            broken_mappings.ascend_map.emplace(broken_binding, source_binding);
            broken_mappings.descend_map.emplace(source_binding, broken_binding);

            broken_vertex_index++;
        }
    }
    /// @brief Возвращает индекс вершины в рассеченном графе по индексу из исходного графа
    static size_t get_broken_vertex_index(size_t source_vertex_index, const  general_structured_graph_t<BrokenGraph>& broken_mappings, 
        const broken_graph_vertex_numbering renumbering_policy, size_t broken_graph_id = 0)
    {
        if (renumbering_policy == broken_graph_vertex_numbering::renumber_never) {
            // Если используется политика сохранения нумерации из исходного графа, возвращает переданный индекс
            return source_vertex_index;
        }

        // Иначе возвращается индекс перенумерованной вершины
        auto [begin, end] = broken_mappings.descend_map.equal_range(
            graph_binding_t(graph_binding_type::Vertex, source_vertex_index));
        size_t broken_vertex_index = begin->second.vertex;
        return broken_vertex_index;
    }
    /// @brief Добавляет ребро edge1 в граф broken_graph и обновляет информацию о сопоставлении с исходным графом
    static void add_edge1_to_broken_graph(const edge_incidences_type& source_edge1, size_t source_edge_index, BrokenGraph& broken_graph,
        general_structured_graph_t<BrokenGraph>& broken_mappings, const broken_graph_vertex_numbering renumbering_policy)
    {
        if (source_edge1.has_vertex_from()) {
            size_t source_vertex_from = get_broken_vertex_index(source_edge1.from, broken_mappings, renumbering_policy);
            edge_incidences_type broken_edge1(edge_incidences_t::single_sided_from(source_vertex_from),
                source_edge1.model);
            broken_graph.edges1.emplace_back(broken_edge1);
        }
        else {
            size_t source_vertex_to = get_broken_vertex_index(source_edge1.to, broken_mappings, renumbering_policy);
            edge_incidences_type broken_edge1(edge_incidences_t::single_sided_to(source_vertex_to),
                source_edge1.model);
            broken_graph.edges1.emplace_back(broken_edge1);
        }

        subgraph_binding_t broken_binding(broken_graph.id, graph_binding_type::Edge1, broken_graph.edges1.size() - 1);
        graph_binding_t source_binding(graph_binding_type::Edge1, source_edge_index);

        broken_mappings.ascend_map.emplace(broken_binding, source_binding);
        broken_mappings.descend_map.emplace(source_binding, broken_binding);
    }
    /// @brief Добавляет нерассекаемое двустороннее ребро в broken_graph и 
    /// обновляет информацию о сопоставлении с исходным графом
    static void add_instant_edge2_to_broken_graph(const edge_incidences_type& source_edge2, size_t source_edge_index, BrokenGraph& broken_graph,
        general_structured_graph_t<BrokenGraph>& broken_mappings, const broken_graph_vertex_numbering renumbering_policy)
    {
        size_t source_vertex_from = get_broken_vertex_index(source_edge2.from, broken_mappings, renumbering_policy);
        size_t source_vertex_to = get_broken_vertex_index(source_edge2.to, broken_mappings, renumbering_policy);

        // Нерассекаемое ребро2 переносится без изменений
        edge_incidences_type broken_graph_edge2(edge_incidences_t(source_vertex_from, source_vertex_to), source_edge2.model);
        broken_graph.edges2.emplace_back(broken_graph_edge2);

        subgraph_binding_t broken_binding(broken_graph.id, graph_binding_type::Edge2, broken_graph.edges2.size() - 1);
        graph_binding_t source_binding(graph_binding_type::Edge2, source_edge_index);

        broken_mappings.ascend_map.emplace(broken_binding, source_binding);
        broken_mappings.descend_map.emplace(source_binding, broken_binding);
    }
    /// @brief Добавляет рассеченное двустороннее ребров в broken_graph и
    /// обновляет информацию о сопоставлении с исходным графом
    static void add_splittable_edge2_to_broken_graph(const edge_incidences_type& source_edge2, size_t source_edge_index, BrokenGraph& broken_graph,
        general_structured_graph_t<BrokenGraph>& broken_mappings, const broken_graph_vertex_numbering renumbering_policy)
    {
        size_t source_vertex_from = get_broken_vertex_index(source_edge2.from, broken_mappings, renumbering_policy);
        size_t source_vertex_to = get_broken_vertex_index(source_edge2.to, broken_mappings, renumbering_policy);

        // Начало рассченного ребра
        edge_incidences_type broken_edge1_from(edge_incidences_t::single_sided_from(source_vertex_from),
            source_edge2.model->get_single_sided_model(false));
        broken_graph.edges1.emplace_back(broken_edge1_from);

        subgraph_binding_t broken_binding_from(broken_graph.id, graph_binding_type::Edge1, broken_graph.edges1.size() - 1);
        graph_binding_t source_binding_from(graph_binding_type::Edge2, source_edge_index);

        broken_mappings.ascend_map.emplace(broken_binding_from, source_binding_from);
        broken_mappings.descend_map.emplace(source_binding_from, broken_binding_from);

        // Конец рассченного ребра
        edge_incidences_type broken_edge1_to(edge_incidences_t::single_sided_to(source_vertex_to),
            source_edge2.model->get_single_sided_model(true));
        broken_graph.edges1.emplace_back(broken_edge1_to);

        subgraph_binding_t broken_binding_to(broken_graph.id, graph_binding_type::Edge1, broken_graph.edges1.size() - 1);
        graph_binding_t source_binding_to(graph_binding_type::Edge2, source_edge_index);

        broken_mappings.ascend_map.emplace(broken_binding_to, source_binding_to);
        broken_mappings.descend_map.emplace(source_binding_to, broken_binding_to);
    }
    /// @brief Переносит односторонние ребра из source_graph в broken_graph
    /// @param broken_mappings Сопоставление ребер и вершин рассеченного графа и исходного графа
    static void copy_edges1_to_broken_graph(const SourceGraph& source_graph,
        BrokenGraph& broken_graph, general_structured_graph_t<BrokenGraph>& broken_mappings,
        const broken_graph_vertex_numbering renumbering_policy) 
    {
        for (size_t edge_index = 0; edge_index < source_graph.edges1.size(); ++edge_index) {
            const auto& edge1 = source_graph.edges1[edge_index];
            add_edge1_to_broken_graph(edge1, edge_index, broken_graph, broken_mappings, renumbering_policy);
        }
    }
    /// @brief Переносит двусторонние ребра из source_graph в broken_graph. Нерассекаемые ребра переносятся без изменений.
    /// Рассекаемые ребра заменяются на два ребра1
    /// @param remove_null_edges Игнорировать ли ребра, которым не сопоставлена модель
    /// @param model_computational_type Метод для получения типа ребра модели - рассекаемая или нерассекаемая
    static void copy_edges2_to_broken_graph(const SourceGraph& source_graph,
        BrokenGraph& broken_graph, general_structured_graph_t<BrokenGraph>& broken_mappings,
        std::vector<edge_incidences_t>& calculation_order,
        bool remove_null_edges,
        ModelInfo model_computational_type, 
        const broken_graph_vertex_numbering renumbering_policy)
    {

        for (size_t edge_index = 0; edge_index < source_graph.edges2.size(); ++edge_index) {

            const auto& source_edge2 = source_graph.edges2[edge_index];

            if (source_edge2.model == nullptr && remove_null_edges)
                continue;

            computational_type model_type = source_edge2.model == nullptr
                ? computational_type::Instant // Если model = nullptr, то она трактуется как Instant
                : model_computational_type(edge_index);

            bool is_instant = model_type == computational_type::Instant;
            if (!is_instant) {
                if (model_type == computational_type::SplittableOrdered) 
                {
                    size_t source_vertex_from = get_broken_vertex_index(source_edge2.from, broken_mappings, renumbering_policy);
                    size_t source_vertex_to = get_broken_vertex_index(source_edge2.to, broken_mappings, renumbering_policy);
                    calculation_order.emplace_back(source_vertex_from, source_vertex_to);
                }
                add_splittable_edge2_to_broken_graph(source_edge2, edge_index, broken_graph, broken_mappings, renumbering_policy);
            }
            else {
                add_instant_edge2_to_broken_graph(source_edge2, edge_index, broken_graph, broken_mappings, renumbering_policy);
            }
        }
    }
    /// @brief Проверяет, используется ли последовательная нумерация вершин в исходном графе 
    /// (имеют ли индексы вершины от 0 до N без пропусков)
    static bool are_sequential_verices(const SourceGraph& source_graph) {

        if (source_graph.empty())
            return true;

        const std::unordered_set<size_t> source_vertices = source_graph.get_vertex_indices();
        size_t source_vertex_count = source_graph.get_vertex_count();
        size_t max_vertex = source_graph.get_max_vertex_index();
        return max_vertex + 1 == source_vertex_count;
        
        //// Формируем ожидаемую последовательность вершин - от 0 до source_vertex_count - 1 без пропусков
        //std::vector<size_t> temp_vertex_indices(source_vertex_count);
        //std::iota(temp_vertex_indices.begin(), temp_vertex_indices.end(), 0);
        //
        //// Превращаем vector в set для сравнения с исходным графом
        //std::unordered_set<size_t> correct_source_vertices(temp_vertex_indices.begin(), temp_vertex_indices.end());
        //bool are_equal = correct_source_vertices == source_vertices;
        //return are_equal;
    }
public:
    /// @brief Формирует граф, в котором рассекаемые (splittable) ребра2 заменены парами односторонних ребер1.
    /// Нерассекаемые (instant) ребра2 и все ребра1 переносятся без изменений.
    /// @param source_graph Исходный граф
    /// @param remove_null_edges Если true, ребра2 с model == nullptr пропускаются
    /// @param model_computational_type Функция, определяющая тип ребра (instant/splittable).
    /// @return {Структурированный граф с единственным подграфом; 
    ///          Порядок обхода для SplittableOrdered ребер}
    static std::pair <
        general_structured_graph_t<BrokenGraph>,
        std::vector<edge_incidences_t>
    > break_splittables(const SourceGraph& source_graph, bool remove_null_edges,
        ModelInfo model_computational_type, broken_graph_vertex_numbering renumbering_policy)
    {
        general_structured_graph_t<BrokenGraph> broken_mappings;
        std::vector<edge_incidences_t> calculation_order;
        broken_mappings.ascend_map.reserve(source_graph.get_object_count());

        if (renumbering_policy == broken_graph_vertex_numbering::renumber_if_needed) {
            // Проверка на последовательность исходной нумерации
            bool sequential_order = are_sequential_verices(source_graph);
            renumbering_policy = (sequential_order)
                ? broken_graph_vertex_numbering::renumber_never /* Индексация вершин уже последовательная */
                : broken_graph_vertex_numbering::renumber_always /* Требуется перенумерация */;
        }       

        // Перенос вершин в рассеченный граф
        if (renumbering_policy == broken_graph_vertex_numbering::renumber_always)
            reindex_and_copy_vertices_to_broken(source_graph, broken_mappings);
        else
            copy_vertices_to_broken(source_graph, broken_mappings);

        BrokenGraph broken_graph;
        broken_graph.id = 0; // по соглашению при рассечении устанавливается id = 0

        copy_edges1_to_broken_graph(source_graph, broken_graph, broken_mappings, renumbering_policy);

        copy_edges2_to_broken_graph(source_graph, broken_graph, broken_mappings,
            calculation_order, remove_null_edges, model_computational_type, renumbering_policy);

        broken_mappings.subgraphs.emplace(broken_graph.id, std::move(broken_graph));

        return std::make_pair(std::move(broken_mappings), std::move(calculation_order));
    }
};

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
    // TODO: добавить возможность выбора способа обработки вершин при расщеплении
    auto broken = break_and_renumber_t<Graph, Graph, ModelSplitTypeGetter>::break_splittables(
        graph, false, get_model_split_type, broken_graph_vertex_numbering::renumber_always);
    general_structured_graph_t<Graph>& broken_transport_graph = broken.first;

    //return zeroflow_decomposer_t<Graph, BoundMeasurement>::split(
        //broken_transport_graph, is_zero_flow_edge);
    return zeroflow_decomposer_t<
        Graph, BoundMeasurement,
        BindingSetter
    >::split(tadpole_is_zeroflow, broken_transport_graph, is_zero_flow_edge);
}


}
