#pragma once

#include "test_graphs.h"


TEST(BlockTreeTopologicalSort, DISABLED_BlockTopologicalSort)
{
    using namespace graphlib;

    basic_graph_t<edge_incidences_t> graph;
    graph.edges2 = { {0, 1}, {1, 2}, {1,2} };

    block_tree_t block_tree = block_tree_builder_t<edge_incidences_t>::get_block_tree(graph);
    biconnected_component_t block = block_tree.blocks[0];
    ASSERT_EQ(block.edges2.size(), 2);

    auto vertices = topological_sort_block(graph, block);


    ASSERT_EQ(vertices.size(), 2);
    ASSERT_EQ(vertices[0], 1);
    ASSERT_EQ(vertices[1], 2);
}

/// @brief Если топология орграфа такова, что до некоторых ребер нельзя добраться из поставщиков, эти ребра не попадут в топологический порядок
/// Транспортный расчет такого оргарафа невозможен. Ожидаем исключение
TEST(BlockTreeTopologicalSort, ThrowsExceptionIfNotAllGraphElementsSorted)
{
    
    //scheme_simple_chain_with_pipe test_scheme;

    //auto [graph, siso_measurements] = test_scheme.prepare_single_flow_subgraph_with_measurements(std::vector<id_type>{});
    //block_tree_t block_tree = graphlib::block_tree_builder_BGL<transport_model_incidences_t>::get_block_tree(graph);

    //// Меняем направление у трубы-моста
    //std::vector<bool>& cutvertices_orientation = block_tree.blocks.back().cut_is_sink;
    //std::transform(cutvertices_orientation.begin(), cutvertices_orientation.end(),
    //    cutvertices_orientation.begin(),
    //    [](bool is_sink) { return !is_sink; });

    //EXPECT_ANY_THROW(topological_sort(graph, block_tree, true 
    ////разрешаем обход только от точек сочленения, от висячих узлов не разрешаем
    //));

}

/// @brief Тест способности алгоритма топологической сортировки обрабатывать точку сочленения ранга >=3
TEST(BlockTreeTopologicalSort, IsConsistentWithExpertKnoledge_TwoBlocksScheme)
{

    /*

    // Тестовая схема без измерений
    scheme_two_blocks test_scheme;
    std::vector<id_type> measurements;

    auto [graph, siso_measurements] = test_scheme.prepare_single_flow_subgraph_with_measurements(measurements);
    block_tree_t block_tree = graphlib::block_tree_builder_BGL<transport_model_incidences_t>::get_block_tree(graph);

    // Индекс блока-треугольника (см. схему) с двумя входами
    const size_t first_nontrivial_block_index = 10;
    // Индекс блока-треугольника с двумя выходами
    const size_t second_nontrivial_block_index = 5;

    // Задаем выходы для нетривиальных блоков
    block_tree.blocks[first_nontrivial_block_index].cut_is_sink.back() = false;
    block_tree.blocks[second_nontrivial_block_index].cut_is_sink[1] = false;
    block_tree.blocks[second_nontrivial_block_index].cut_is_sink.back() = false;

    std::vector<graphlib::block_tree_binding_t> sorted_elements = topological_sort(graph, block_tree);

    ASSERT_TRUE(test_scheme.check_topological_order_consistency(sorted_elements, block_tree, measurements));
    */

}


