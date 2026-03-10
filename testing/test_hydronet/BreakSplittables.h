#include "test_hydronet.h"

namespace break_splittables_test {

/// @brief Фиктивная модель
struct stub_split_model {
    /// @brief В качестве односторонней модели используем эту же модель 
    stub_split_model* get_single_sided_model(bool) { return this; }
};

using test_edge_t = graphlib::model_incidences_t<stub_split_model*>;
using test_graph_t = graphlib::basic_graph_t<test_edge_t>;

}

/// @brief Проверяет наличие исключения при попытке рассечения графа 
/// с непоследовательной нумерацией как графа с последовательной нумерацией
TEST(BreakSplittables, RenumbersVerticesIfNeeded_WhenNonSequentialVertexOrder)
{
    using namespace graphlib;
    using namespace break_splittables_test;

    test_graph_t source_graph;
    stub_split_model splittable_model;

    /// Граф с неупорядоченной нумерацией вершин
    /// [e1_0] ---> (2) ==[e2_0: splittable]==> (15) --[e2_1: instant]--> (24) ---> [e1_1]
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_to(2), nullptr);
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_from(24), nullptr);
    // edge2[0]: 2 -> 15 (splittable)
    source_graph.edges2.emplace_back(
        edge_incidences_t(2, 15), &splittable_model);
    // edge2[1]: 15 -> 24 (instant)
    source_graph.edges2.emplace_back(
        edge_incidences_t(15, 24), nullptr);

    auto model_type_getter = [](size_t edge_index) {
        // Только одно ребро Splittable
        return edge_index == 0
            ? computational_type::Splittable
            : computational_type::Instant;
    };

    // Act расщепления splittable ребер
    using Renumberer = break_and_renumber_t<test_graph_t, test_graph_t, decltype(model_type_getter)>;
    auto [broken_mappings, calculation_order] = Renumberer::break_splittables(
        source_graph, false, model_type_getter, broken_graph_vertex_numbering::renumber_if_needed);

    const size_t subgraph_id = 0;
    const auto& broken_graph = broken_mappings.subgraphs.at(subgraph_id);

    // Assert
    // Splittable edge (2->15) рассекается на два edge1
    // Итого edges1: 2 исходных + 2 от рассечения = 4
    ASSERT_EQ(broken_graph.edges1.size(), 4);
    // Instant edge (15->24) остается как edge2
    ASSERT_EQ(broken_graph.edges2.size(), 1);

    // ascend_map содержит записи для перенумерованых вершин {0, 1, 2} вместо фактических вершин {2, 15, 24},
    std::unordered_set<size_t> actual_vertices = source_graph.get_vertex_indices();
    std::unordered_set<size_t> renumbered_vertices = broken_graph.get_vertex_indices();

    size_t vertex_count = source_graph.get_vertex_count();
    std::unordered_set<size_t> etalon_renumbered_vertices;
    for (int i = 0; i < vertex_count; ++i) {
        etalon_renumbered_vertices.insert(i);
    }

    ASSERT_EQ(renumbered_vertices, etalon_renumbered_vertices);
}

/// @brief Проверяет выполнение перенумерации вершин при рассечении исходного графа с непоследовательной нумерацией 
TEST(BreakSplittables, VerticesAreRenumbered_WhenNonSequentialVertexOrder)
{
    using namespace graphlib;
    using namespace break_splittables_test;

    test_graph_t source_graph;
    stub_split_model splittable_model;

    /// Граф с неупорядоченной нумерацией вершин
    /// [e1_0] ---> (2) ==[e2_0: splittable]==> (15) --[e2_1: instant]--> (24) ---> [e1_1]
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_to(2), nullptr);
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_from(24), nullptr);
    // edge2[0]: 2 -> 15 (splittable)
    source_graph.edges2.emplace_back(
        edge_incidences_t(2, 15), &splittable_model);
    // edge2[1]: 15 -> 24 (instant)
    source_graph.edges2.emplace_back(
        edge_incidences_t(15, 24), nullptr);

    auto model_type_getter = [](size_t edge_index) {
        // Только одно ребро Splittable
        return edge_index == 0
            ? computational_type::Splittable
            : computational_type::Instant;
    };

    // Act - расщепление splittable ребер
    auto [broken_mappings, calculation_order] = break_and_renumber_t<test_graph_t, test_graph_t, decltype(model_type_getter)>::break_splittables(
        source_graph, false, model_type_getter, broken_graph_vertex_numbering::renumber_always);

    const size_t subgraph_id = 0;
    const auto& broken_graph = broken_mappings.subgraphs.at(subgraph_id);

    // Assert
    // Splittable edge (2->15) рассекается на два edge1
    // Итого edges1: 2 исходных + 2 от рассечения = 4
    ASSERT_EQ(broken_graph.edges1.size(), 4);
    // Instant edge (15->24) остается как edge2
    ASSERT_EQ(broken_graph.edges2.size(), 1);

    // ascend_map содержит записи для перенумерованых вершин {0, 1, 2} вместо фактических вершин {2, 15, 24},
    std::unordered_set<size_t> actual_vertices = source_graph.get_vertex_indices();
    std::unordered_set<size_t> renumbered_vertices = broken_graph.get_vertex_indices();

    size_t vertex_count = source_graph.get_vertex_count();
    std::unordered_set<size_t> etalon_renumbered_vertices;
    for (int i = 0; i < vertex_count; ++i) {
        etalon_renumbered_vertices.insert(i);
    }

    ASSERT_EQ(renumbered_vertices, etalon_renumbered_vertices);

    // Проверка биективности вершин исходного и рассеченного графов
    for (size_t vertex : renumbered_vertices) {
        subgraph_binding_t broken_binding(subgraph_id, graph_binding_type::Vertex, vertex);
        EXPECT_EQ(broken_mappings.ascend_map.count(broken_binding), 1)
            << "No ascend binding for vertex " << vertex;
    }

    for (size_t vertex : actual_vertices) {
        graph_binding_t global_binding(graph_binding_type::Vertex, vertex);
        EXPECT_EQ(broken_mappings.descend_map.count(global_binding), 1)
            << "Vertex " << vertex << " missing in descend_map";
    }
}

/// @brief Проверка сохранения нумерации вершин при рассечении исходного графа с последовательной нумерацеий
TEST(BreakSplittables, VertexIndicesAreSameInOriginalAndSplitGraph_WhenSequentialVertexOrder)
{
    using namespace graphlib;
    using namespace break_splittables_test;

    test_graph_t source_graph;
    stub_split_model splittable_model;

    // Вершины графа: {0, 1, 2} — упорядоченная нумерация с разрывами
    // Граф с упорядоченной нумерацией вершин
    // [e1_0] ---> (0) ==[e2_0: splittable]==> (1) --[e2_1: instant]--> (2) ---> [e1_1]
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_to(0), nullptr);
    source_graph.edges1.emplace_back(
        edge_incidences_t::single_sided_from(2), nullptr);
    // edge2[0]: 0 -> 1 (splittable)
    source_graph.edges2.emplace_back(
        edge_incidences_t(0, 1), &splittable_model);
    // edge2[1]: 1 -> 2 (instant)
    source_graph.edges2.emplace_back(
        edge_incidences_t(1, 2), nullptr);

    auto model_type_getter = [](size_t edge_index) {
        // Только одно ребро Splittable
        return edge_index == 0
            ? computational_type::Splittable
            : computational_type::Instant;
    };

    // Act - расщепление splittable ребер
    auto [broken_mappings, calculation_order] = break_and_renumber_t<test_graph_t, test_graph_t, decltype(model_type_getter)>::break_splittables(
        source_graph, false, model_type_getter, broken_graph_vertex_numbering::renumber_never);

    const size_t subgraph_id = 0;
    const auto& broken_graph = broken_mappings.subgraphs.at(subgraph_id);

    // Assert
    // Splittable edge (0->1) рассекается на два edge1
    // Итого edges1: 2 исходных + 2 от рассечения = 4
    ASSERT_EQ(broken_graph.edges1.size(), 4);
    // Instant edge (1->2) остается как edge2
    ASSERT_EQ(broken_graph.edges2.size(), 1);

    // Рассеченное ребро ссылается на оригинальные вершины 0 и 1
    EXPECT_EQ(broken_graph.edges1[2].from, 0);
    EXPECT_EQ(broken_graph.edges1[3].to, 1);

    // Instant edge2 сохраняет оригинальную нумерацию
    EXPECT_EQ(broken_graph.edges2[0].from, 1);
    EXPECT_EQ(broken_graph.edges2[0].to, 2);

    // ascend_map содержит записи для ВСЕХ фактических вершин {0, 1, 2}
    std::unordered_set<size_t> actual_vertices = source_graph.get_vertex_indices();
    ASSERT_EQ(actual_vertices.size(), 3);

    for (size_t vertex : actual_vertices) {
        subgraph_binding_t broken_binding(subgraph_id, graph_binding_type::Vertex, vertex);

        if (broken_mappings.ascend_map.count(broken_binding) == 0)
            FAIL() << "No ascend binding for vertex " << vertex;

        graph_binding_t source_binding = broken_mappings.ascend_map.at(broken_binding);

        EXPECT_EQ(source_binding.binding_type, graph_binding_type::Vertex);
        EXPECT_EQ(source_binding.vertex, vertex);
    }

    // descend_map содержит корректные записи для всех вершин
    for (size_t vertex : actual_vertices) {
        graph_binding_t global_binding(graph_binding_type::Vertex, vertex);
        EXPECT_EQ(broken_mappings.descend_map.count(global_binding), 1)
            << "Vertex " << vertex << " missing in descend_map";
    }
}
