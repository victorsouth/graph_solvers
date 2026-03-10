#pragma once

namespace graphlib {
;
/// @brief Инциденции ребра. Поддерживает ребра 1-го и 2-го типа
struct edge_incidences_t {
    /// @brief Начальная вершина ребра
    size_t from;
    /// @brief Конечная вершина ребра
    size_t to;
    /// @brief Конструктор формирует ребро с заданными начальной и конечной вершинами.
    edge_incidences_t(size_t from, size_t to) 
        : from(from)
        , to(to)
    {
    }
    /// @brief Конструктор формирует ребро только с конечной вершиной.
    /// Для наальной вершины используется std::numeric_limits<size_t>::max()
    edge_incidences_t(size_t to) 
        : from(std::numeric_limits<size_t>::max())
        , to(to)
    {
    }
    /// @brief Конструктор формирует ребро без заданных начальной и конечной вершин.
    /// В качестве значения по умолчанию используется std::numeric_limits<size_t>::max()
    edge_incidences_t() 
        : from(std::numeric_limits<size_t>::max())
        , to(std::numeric_limits<size_t>::max())
    {
    }
    /// @brief Возвращает односторонее ребро-отбор (с начальной вершиной)
    /// @param from Индекс начальной вершины
    static edge_incidences_t single_sided_from(size_t from)
    {
        return edge_incidences_t(from, edge_incidences_t::no_vertex_value());
    }
    /// @brief Возвращает одностороннее ребро-приток (с конечной вершиной)
    /// @param to Индекс конечной вершины 
    static edge_incidences_t single_sided_to(size_t to)
    {
        return edge_incidences_t(to);
    }
    /// @brief Проверяет, является ли ребро односторонним
    bool is_single_sided_edge() const
    {
        return !has_vertex_to() || !has_vertex_from();
    }
    /// @brief Проверяет, является ли ребро двусторонним 
    bool is_double_sided_edge() const
    {
        return has_vertex_to() && has_vertex_from();
    }
    /// @brief Проверяет, задана ли конечная вершина ребра
    bool has_vertex_to() const
    {
        return to != std::numeric_limits<size_t>::max();
    }
    /// @brief Проверяет, задана ли начальная вершина ребра
    bool has_vertex_from() const
    {
        return from != std::numeric_limits<size_t>::max();
    }
    /// @brief Возвращает индекс единственной вершны одностороннего ребра
    /// @throws std::runtime_error Если ребро двустороннее.
    size_t get_vertex_for_single_sided_edge() const
    {
        if (has_vertex_from() && has_vertex_to())
            throw std::runtime_error("Single vertex requested for double sided edge");
        if (has_vertex_from())
            return from;
        else
            return to;
    }

    /// @brief Возвращает ссылку на индекс единственной вершны одностороннего ребра
    /// @throws std::runtime_error Если ребро двустороннее.
    size_t & get_vertex_for_single_sided_edge()
    {
        if (has_vertex_from() && has_vertex_to())
            throw std::runtime_error("Single vertex requested for double sided edge");
        if (has_vertex_from())
            return from;
        else
            return to;
    }

    /// @brief Возвращает вершину ребра, противоположную заданной
    /// @param this_side_vertex Индекс заданной вершины
    /// @throws std::runtime_error Если ребро одностороннее
    size_t get_other_side_vertex(size_t this_side_vertex) const
    {
        if (!is_double_sided_edge())
            throw std::runtime_error("Cannot get the other side for one-sided edge");
        if (this_side_vertex == from) {
            return to;
        }
        else if (this_side_vertex == to) {
            return from;
        }
        else {
            throw std::runtime_error("This side vertex not found");
        }
    }
    /// @brief Возвращает инцидентность ребра к заданной вершине
    /// @param vertex Индекс вершины
    /// @return +1, если ребро входит в вершину
    ///         -1, если ребро выходит из вершины
    ///         0, если ребро не инцидентно вершине
    signed char get_vertex_incidence(size_t vertex) const
    {
        if (vertex == std::numeric_limits<size_t>::max())
            return 0;
        else if (vertex == to)
            return +1;
        else if (vertex == from)
            return -1;
        else
            return 0;
    }
    /// @brief Возвращает инцидентность ребра относительно заданного множества вершин.
    /// @tparam VertexSet Тип контейнера, поддерживающего метод `find` (например, `std::unordered_set<size_t>`).
    /// @param vertices Константная ссылка на множество вершин.
    /// @return signed char — значение инцидентности: -1, 0 или +1.
    template <typename VertexSet>
    signed char get_vertex_subset_incidence(const VertexSet& vertices) const
    {
        signed char incidence = 0;
        if (vertices.find(from) != vertices.end()) {
            incidence--;
        }
        if (vertices.find(to) != vertices.end()) {
            incidence++;
        }
        return incidence;
    }
    /// @brief Возвращает значение std::numeric_limits<size_t>::max, соответствующее незаданной вершине ребра
    static size_t no_vertex_value()
    {
        return std::numeric_limits<size_t>::max();
    }
};

/// @brief Возвращает инциденции ребра, поолученные на основе отображения коннекторов в вершины
/// @throws std::logic_error Если ни один из коннекторов не задан(обе вершины отсутствуют).
inline edge_incidences_t get_edge_incidences_from_connectors(const std::unordered_map<int, size_t>& connector_to_vertex)
{
    edge_incidences_t result;
    if (connector_to_vertex.find(1) != connector_to_vertex.end()) {
        result.from = connector_to_vertex.at(1);
    }
    if (connector_to_vertex.find(2) != connector_to_vertex.end()) {
        result.to = connector_to_vertex.at(2);
    }
    if (!result.has_vertex_from() && !result.has_vertex_to())
        throw std::logic_error("regular_object_getter_t must have either connection=1 or connection=2");
    return result;
}

/// @brief Ориентированные инциденции вершины
struct vertex_incidences_t {
    /// @brief Индексы выходящих ребер
    std::vector <size_t> outlet_edges;
    /// @brief Индекс входящих ребер
    std::vector <size_t> inlet_edges;
    /// @brief Возвращает количество инцидентных ребер
    size_t edges_count() const
    {
        return outlet_edges.size() + inlet_edges.size();
    }
    /// @brief Возвращает индексы всех инцидентных ребер (без разделения на входящие и выходящие)
    std::vector <size_t> all_edges() const
    {
        std::vector<size_t> edges(inlet_edges.size() + outlet_edges.size());
        std::merge(inlet_edges.begin(), inlet_edges.end(),
            outlet_edges.begin(), outlet_edges.end(),
            edges.begin());
        return edges;
    }
    /// @brief Возвращает индекс любого ребра — входящего или исходящего.
    /// @throws std::runtime_error Если не задано ни одно ребро
    size_t get_any_edge() const
    {
        if (!outlet_edges.empty())
            return outlet_edges.front();
        if (!inlet_edges.empty())
            return inlet_edges.front();
        throw std::runtime_error("Cannot get 'any' edge: no incidences");
    }

    /// @brief Возвращает списки инцидентности на основе флага visited
    /// @param select_not_visited Флаг возврата только непосещенных инциденций
    vertex_incidences_t get_visited_incidences(const std::vector<bool>& visited_edge,
        bool select_not_visited = false) const
    {
        vertex_incidences_t result;

        std::copy_if(inlet_edges.begin(), inlet_edges.end(),
            std::back_inserter(result.inlet_edges),
            [&](size_t edge) {
                return visited_edge[edge] ^ select_not_visited;
            });

        std::copy_if(outlet_edges.begin(), outlet_edges.end(),
            std::back_inserter(result.outlet_edges),
            [&](size_t edge) {
                return visited_edge[edge] ^ select_not_visited;
            });

        return result;
    }
    /// @brief Возвращает отфильтрованные инциденции вершины, содержащие только разрешенные ребра
    /// @param edge_subset Множество разрешенных ребер
    vertex_incidences_t get_incidences_for_edge_subset(const std::unordered_set<size_t>& edge_subset) const
    {
        vertex_incidences_t result;

        /*for each (size_t edge in outlet_edges) {*/
        for (size_t edge : outlet_edges) {
            if (edge_subset.find(edge) != edge_subset.end()) {
                result.outlet_edges.push_back(edge);
            }
        }
        /*for each (size_t edge in inlet_edges) {*/
        for (size_t edge : inlet_edges) {
            if (edge_subset.find(edge) != edge_subset.end()) {
                result.inlet_edges.push_back(edge);
            }
        }
        return result;
    }
    /// @brief Проверяет равенство двух инцидентностей вершин
    bool operator==(const vertex_incidences_t& other) const
    {
        if (outlet_edges.size() != other.outlet_edges.size())
            return false;
        if (inlet_edges.size() != other.inlet_edges.size())
            return false;

        std::unordered_set<size_t> other_inlet(other.inlet_edges.begin(), other.inlet_edges.end());
        std::unordered_set<size_t> other_outlet(other.outlet_edges.begin(), other.outlet_edges.end());

        if (set_intersection(outlet_edges, other_outlet).size() != other_outlet.size())
            return false;
        if (set_intersection(inlet_edges, other_inlet).size() != other_inlet.size())
            return false;

        return true;
    }

};

/// @brief Формирует инциденции всех вершин подграфа, заданного списком ребер
/// @tparam EdgeIncidences Тип ребра с инциденциями
/// @param edges Список ребер подграфа
template <typename EdgeIncidences>
inline std::vector<vertex_incidences_t> get_vertex_incidences(const std::vector<EdgeIncidences>& edges)
{
    std::vector<vertex_incidences_t> vertex_incidences;
    size_t vertex_count = 0;
    for (size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
        const EdgeIncidences& edge = edges[edge_index];

        vertex_count = std::max(vertex_count, 1 + std::max(edge.vertex_from, edge.vertex_to));
        vertex_incidences.resize(vertex_count);

        vertex_incidences[edge.vertex_from].outlet_edges.push_back(edge_index);
        vertex_incidences[edge.vertex_to].inlet_edges.push_back(edge_index);
    }
    return vertex_incidences;
}

/// @brief Разделенные инциденции вершины - инциденции с разделением на ребра 1-го типа и 2-го типа
struct vertex_incidences_pair_t : public std::pair < vertex_incidences_t, vertex_incidences_t > {
    /// @brief Проверяет равенство двух инцидентностей
    bool operator==(const vertex_incidences_pair_t& other) const {
        return first == other.first && second == other.second;
    }

    /// @brief Возвращает общее количество инцидентных ребер обоих типов
    size_t total_edges_count() const {
        return first.edges_count() + second.edges_count();

    }
    /// @brief Возвращает ориентированные привязки для ребер, инцидентных вершине
    std::vector<graph_place_binding_t> get_egde_places_for_vertex() const {
        std::vector<graph_place_binding_t> bindings;
        for (size_t edge_index : second.inlet_edges) {
            bindings.emplace_back(graph_binding_t(graph_binding_type::Edge2, edge_index), graph_edge_side_t::Outlet);
        }
        for (size_t edge_index : second.outlet_edges) {
            bindings.emplace_back(graph_binding_t(graph_binding_type::Edge2, edge_index), graph_edge_side_t::Inlet);
        }
        for (size_t edge_index : first.inlet_edges) {
            bindings.emplace_back(graph_binding_t(graph_binding_type::Edge1, edge_index), graph_edge_side_t::Outlet);
        }
        for (size_t edge_index : first.outlet_edges) {
            bindings.emplace_back(graph_binding_t(graph_binding_type::Edge1, edge_index), graph_edge_side_t::Inlet);
        }
        return bindings;
    }
    /// @brief Возвращает привязки для входящих ребер
    std::vector<graph_binding_t> get_inlet_bindings() const {
        std::vector<graph_binding_t> bindings;
        for (size_t edge_index : second.inlet_edges) {
            bindings.emplace_back(graph_binding_type::Edge2, edge_index);
        }
        for (size_t edge_index : first.inlet_edges) {
            bindings.emplace_back(graph_binding_type::Edge1, edge_index);
        }
        return bindings;
    }
    /// @brief Возвращает привязки для выходящих ребер
    std::vector<graph_binding_t> get_outlet_bindings() const {
        std::vector<graph_binding_t> bindings;
        for (size_t edge_index : second.outlet_edges) {
            bindings.emplace_back(graph_binding_type::Edge2, edge_index);
        }
        for (size_t edge_index : first.outlet_edges) {
            bindings.emplace_back(graph_binding_type::Edge1, edge_index);
        }
        return bindings;
    }

    /// @brief Возвращает списки инцидентности на основе флага visited при обходах графа
    /// @param select_not_visited Предусмотрена возможность возвращать только не посещенные
    vertex_incidences_pair_t get_visited_incidences(
        const std::vector<bool>& edge1_visited, const std::vector<bool>& edge2_visited,
        bool select_not_visited = false) const
    {
        vertex_incidences_pair_t result;
        result.first = first.get_visited_incidences(edge1_visited, select_not_visited);
        result.second = second.get_visited_incidences(edge2_visited, select_not_visited);
        return result;
    }
};

/// @brief Сопоставление между индексами вершин и их разделенными инциденциями
typedef std::unordered_map<size_t, vertex_incidences_pair_t> vertex_incidences_map_t;

/// @brief Формирует разделенные инциденции из списка ребер 1-го типа и спика ребер 2-го типа
/// @tparam EdgeIncidences Тип ребра с его инциденциями
template <typename EdgeIncidences>
inline vertex_incidences_map_t get_vertex_incidences_map(
    const std::vector<EdgeIncidences>& incidences1_list, const std::vector<EdgeIncidences>& incidences2_list)
{
    vertex_incidences_map_t result;
    for (size_t edge1_index = 0; edge1_index < incidences1_list.size(); ++edge1_index) {
        const edge_incidences_t& edge1 = incidences1_list[edge1_index];
        if (edge1.has_vertex_from()) {
            result[edge1.from].first.outlet_edges.push_back(edge1_index);
        }
        if (edge1.has_vertex_to()) {
            result[edge1.to].first.inlet_edges.push_back(edge1_index);
        }
    }
    for (size_t edge2_index = 0; edge2_index < incidences2_list.size(); ++edge2_index) {
        const edge_incidences_t& edge2 = incidences2_list[edge2_index];
        result[edge2.from].second.outlet_edges.push_back(edge2_index);
        result[edge2.to].second.inlet_edges.push_back(edge2_index);
    }
    return result;
}

/// @brief Формирует сопоставление "индекс вершины - инциденции" для всех вершин подграфа, заданного списком ребер
/// @tparam EdgeIncidences Тип ребра с его инциденциями
template <typename EdgeIncidences>
inline std::unordered_map<size_t, vertex_incidences_t> get_vertex_incidences_map(const std::vector<EdgeIncidences>& edges)
{
    std::unordered_map<size_t, vertex_incidences_t> vertex_incidences;
    size_t vertex_count = 0;
    for (size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
        const EdgeIncidences& edge = edges[edge_index];

        vertex_incidences[edge.vertex_from].outlet_edges.push_back(edge_index);
        vertex_incidences[edge.vertex_to].inlet_edges.push_back(edge_index);
    }
    return vertex_incidences;
}


}
