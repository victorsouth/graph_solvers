#pragma once

namespace graphlib {
;

/// @brief Возвращает измерения для заданной привязки в SISO графе
/// @param binding Привязка в SISO графе
/// @param measurements_by_object Измерения для всех привязок SISO графа
/// @return Измерения
template <typename BoundMeasurement>
inline std::vector<BoundMeasurement> endogenous_measurement_for_incidence(const graphlib::graph_binding_t& binding,
    const std::unordered_map<graphlib::graph_binding_t, std::vector<BoundMeasurement>, 
    graphlib::graph_binding_t::hash>& measurements_by_object)
{
    std::vector<BoundMeasurement> measurements;
    if (graphlib::has_map_key(measurements_by_object, binding)) {
        measurements = measurements_by_object.at(binding);
    }
    return measurements;
}

/// @brief Сопоставляет идентификаторам ребер (вершин) графа замеры эндогенных свойств
/// @param graph SISO граф
/// @param measurements Измерения
/// @return (привязка в SISO графе : измерения)
template<typename BoundMeasurement>
inline std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash>
get_endogenous_measurements_by_incidences(const std::vector<BoundMeasurement>& measurements)
{
    std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash> result;
    for (const auto& measurement : measurements)
    {
        result[measurement].push_back(measurement);
    }
    return result;
}



/// @brief Классифицирует вершины подграфов на три категории: 
///     - проточные - вершины проточного подграфа; 
///     - граничные - общие вершины проточного и непроточного подграфов;
///     - непроточные - вершины непроточного подграфа
/// @tparam StructuredGraph Тип графа. Должен поддерживать метод get_subgraph_global_vertices
/// @param structured_graph Граф с выделенными на нем подграфами
/// @param flow_subgraphs Индексы проточных подграфов
/// @param zeroflow_subgraphs Индексы непроточных подграфов
/// @param flow_vertices Указатель для записи индексов проточных вершин по подграфам 
/// @param linked_vertices Указатель для записи индексов граничных вершин по подграфам 
/// @param separated_vertices Указатель для записи индексов непроточных вершин по подграфам  
template <typename StructuredGraph>
inline void get_zeroflow_vertices_by_type(
    const StructuredGraph& structured_graph,
    const std::vector<size_t>& flow_subgraphs,
    const std::vector<size_t>& zeroflow_subgraphs,
    std::unordered_map<size_t, std::unordered_set<size_t>>* flow_vertices,
    std::unordered_map<size_t, std::unordered_set<size_t>>* linked_vertices,
    std::unordered_map<size_t, std::unordered_set<size_t>>* separated_vertices)
{
    std::unordered_set<size_t> flow_vertices_set;
    for (size_t subgraph : flow_subgraphs) {
        flow_vertices->emplace(std::piecewise_construct,
            std::forward_as_tuple(subgraph),
            std::forward_as_tuple(structured_graph.get_subgraph_global_vertices(subgraph))
        );
        flow_vertices_set.insert(flow_vertices->at(subgraph).begin(), flow_vertices->at(subgraph).end());
    }

    for (size_t subgraph : zeroflow_subgraphs) {
        auto vertices = structured_graph.get_subgraph_global_vertices(subgraph);
        auto intersection = set_intersection(vertices, flow_vertices_set);
        if (intersection.empty()) {
            separated_vertices->emplace(std::piecewise_construct,
                std::forward_as_tuple(subgraph),
                std::forward_as_tuple(std::move(vertices))
            );
        }
        else {
            linked_vertices->emplace(std::piecewise_construct,
                std::forward_as_tuple(subgraph),
                std::forward_as_tuple(std::move(vertices))
            );
        }
    }

}


/// @brief Получение Triplet (ненулевые значения)
/// @param Принимает константную ссылку на разреженную матрицу любого типа
/// @return возвращает вектор троек (Triplet(row, col, value)) типа double 
template<typename SparseMatrixType>
inline std::vector<Eigen::Triplet<double>> get_triplets(
    const SparseMatrixType& mat)
{
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(mat.nonZeros());

    // Итерация по ненулевым элементам
    for (int k = 0; k < mat.outerSize(); ++k) {
        for (typename SparseMatrixType::InnerIterator it(mat, k); it; ++it) {
            triplets.emplace_back((int)it.row(), (int)it.col(), it.value());
        }
    }

    return triplets;
}


}