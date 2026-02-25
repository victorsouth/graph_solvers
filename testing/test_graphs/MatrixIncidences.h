#pragma once

#include "test_graphs.h"


/// @brief Проверяет способность отрезать колонки/строки матриц A1
TEST(IncidenceMatrix, ReducesMatrix1) {
    // Arrange
    graphlib::basic_graph_t<graphlib::edge_incidences_t> graph;
    graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(0)); // приток в узел 0: 1 м³/с 
    graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(2));

    // Act
    std::size_t remove_nodes = 1; // и столько же выкинем висячих дуг
    auto A1r = graph.get_reduced_incidences1(remove_nodes, remove_nodes);

    // Assert
    ASSERT_EQ(A1r.cols(), graph.edges1.size() - remove_nodes);
    ASSERT_EQ(A1r.rows(), graph.get_vertex_count() - remove_nodes);
}

TEST(IncidenceMatrix, SplitMatrix1) {
    // Arrange
    graphlib::basic_graph_t<graphlib::edge_incidences_t> graph;
    graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(0));
    graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(2));
    graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(3));

    // Act
    std::size_t remove_nodes = 2; // и столько же выкинем висячих дуг
    auto [A1r_sparse, A1p_sparse] = graph.split_reduced_incidences1(remove_nodes);
    Eigen::MatrixXd A1r = A1r_sparse;
    Eigen::MatrixXd A1p = A1p_sparse;

    // Assert
    ASSERT_EQ(A1r.cols(), graph.edges1.size() - remove_nodes);
    ASSERT_EQ(A1r.rows(), graph.get_vertex_count() - remove_nodes);

    ASSERT_EQ(A1p.cols(), remove_nodes);
    ASSERT_EQ(A1p.rows(), remove_nodes);

}


/// @brief Проверяет способность отрезать колонки/строки матрицы A2
TEST(IncidenceMatrix, ReducesMatrix2) {
    // Arrange
    graphlib::basic_graph_t<graphlib::edge_incidences_t> graph;
    graph.edges2.emplace_back(graphlib::edge_incidences_t(0, 1)); // ребро 0-1: труба
    graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 2)); // ребро 1-2: расширение

    // Act
    std::size_t remove_nodes = 1; // и столько же выкинем висячих дуг
    auto A2r = graph.get_reduced_incidences2(remove_nodes);

    // Assert
    ASSERT_EQ(A2r.cols(), graph.edges2.size());
    ASSERT_EQ(A2r.rows(), graph.get_vertex_count() - remove_nodes);
}


/// @brief Проверяет способность отрезать колонки/строки матрицы A2
TEST(IncidenceMatrix, SplitMatrix2) {
    // Arrange
    graphlib::basic_graph_t<graphlib::edge_incidences_t> graph;
    graph.edges2.emplace_back(graphlib::edge_incidences_t(0, 1)); 
    graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 2)); 
    graph.edges2.emplace_back(graphlib::edge_incidences_t(2, 3));

    // Act
    std::size_t remove_nodes = 1; 
    auto [A2r_sparse, A2p_sparse] = graph.split_reduced_incidences2(remove_nodes);
    Eigen::MatrixXd A2r = A2r_sparse;
    Eigen::MatrixXd A2p = A2p_sparse;

    // Assert
    ASSERT_EQ(A2r.cols(), graph.edges2.size());
    ASSERT_EQ(A2r.rows(), graph.get_vertex_count() - remove_nodes);

    ASSERT_EQ(A2p.cols(), graph.edges2.size());
    ASSERT_EQ(A2p.rows(), remove_nodes);

}


