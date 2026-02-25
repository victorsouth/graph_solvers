#pragma once

namespace graphlib {
;

template <typename ModelIncidences>
struct basic_graph_t;

/// @brief Подграф в графе типа basic_graph_t
struct subgraph_t {
    /// @brief Индексы ребер 1-го типа
    std::vector<size_t> edges1;
    /// @brief Индексы ребер 2-го типа
    std::vector<size_t> edges2;
    /// @brief Индексы вершин
    mutable std::unordered_set<size_t> vertices;
    /// @brief Граничные ребра 1-го типа, не входящие в подграф
    mutable std::vector<size_t> boundary_edges1;
    /// @brief Граничные ребра 2-го типа, не входящие в подграф
    mutable std::vector<size_t> boundary_edges2;
    /// @brief Проверяет, является ли подграф пустым?
    bool empty() const
    {
        return total_place_count() == 0;
    }
    /// @brief Проверяет, отсутствие граничных ребер у подграфа
    bool no_boundaries() const
    {
        return boundary_edges1.empty() && boundary_edges2.empty();
    }
    /// @brief Возвращает общее количество элементов в подграфе (вершин, ребер 1-го и 2-го типов)
    size_t total_place_count() const
    {
        return edges1.size() + edges2.size() + vertices.size();
    }
    /// @brief Возвращает общее количество ребер (1-го и 2-го типов)
    size_t total_edge_count() const
    {
        return edges1.size() + edges2.size();
    }
    /// @brief Возвращает привязки ко всем элементам подграфа 
    /// @warning Привязки со СКВОЗНОЙ индексацией (из исходного графа) и С УКАЗАНИЕМ индекса подграфа
    /// @param graph_id Индекс подграфа
    std::vector<subgraph_binding_t> get_global_binding_list(size_t graph_id) const
    {
        std::vector<graph_binding_t> graphs = get_global_binding_list();
        std::vector<subgraph_binding_t> result;// (graphs.size());
        std::transform(graphs.begin(), graphs.end(),
            std::back_inserter(result),
            [graph_id](const graph_binding_t& graph_binding) { return subgraph_binding_t(graph_id, graph_binding); }
        );
        return result;
    }

    /// @brief Возвращает привязки ко всем элементам подграфа 
    /// @warning Привязки с ЛОКАЛЬНОЙ индексацией (в пределах подграфа) и С УКАЗАНИЕМ индекса подграфа
    /// @param graph_id Индекс подграфа
    std::vector<subgraph_binding_t> get_local_binding_list(size_t graph_id) const
    {
        std::vector<subgraph_binding_t> result;
        for (size_t edge1 = 0; edge1 < edges1.size(); ++edge1) {
            result.emplace_back(graph_id, graph_binding_type::Edge1, edge1);
        }
        for (size_t edge2 = 0; edge2 < edges2.size(); ++edge2) {
            result.emplace_back(graph_id, graph_binding_type::Edge2, edge2);
        }
        for (size_t vertex = 0; vertex < vertices.size(); ++vertex) {
            result.emplace_back(graph_id, graph_binding_type::Vertex, vertex);
        }
        return result;
    }
    /// @brief Возвращает привязки ко всем элементам подграфа
    /// @warning Привязки со СКВОЗНОЙ индексацией (из исходного графа) БЕЗ УКАЗАНИЯ индекса подграфа (!)
    std::vector<graph_binding_t> get_global_binding_list() const
    {
        std::vector<graph_binding_t> result;
        for (size_t edge1 : edges1) {
            result.emplace_back(graph_binding_type::Edge1, edge1);
        }
        for (size_t edge2 : edges2) {
            result.emplace_back(graph_binding_type::Edge2, edge2);
        }
        for (size_t vertex : vertices) {
            result.emplace_back(graph_binding_type::Vertex, vertex);
        }
        return result;
    }
    /// @brief Получает список граничных вершин подграфа
    /// @details Граничные вершины - это вершины, которые инцидентны ребрам, не входящим в подграф
    /// @tparam ModelIncidences Тип моделей инциденций
    /// @param graph Исходный граф
    /// @return Вектор индексов граничных вершин
    template <typename ModelIncidences>
    const std::vector<size_t> get_boundary_vertices(const basic_graph_t<ModelIncidences>& graph) const;


    /// @brief Создает множество вершин подграфа на основе его ребер
    /// @details Заполняет поле vertices на основе вершин, инцидентных ребрам подграфа
    /// @tparam ModelIncidences Тип моделей инциденций
    /// @param graph Исходный граф
    template <typename ModelIncidences>
    void create_vertices_by_edges(const basic_graph_t<ModelIncidences>& graph) const;

    /// @brief Создает граничные ребра для подграфа
    /// @details Граничные ребра - это ребра исходного графа, которые соединяют вершины подграфа с вершинами вне подграфа
    /// @tparam ModelIncidences Тип моделей инциденций
    /// @param graph Исходный граф
    template <typename ModelIncidences>
    void create_boundary_edges(const basic_graph_t<ModelIncidences>& graph) const;
};


/// @brief Граф с ребрами 1-го типа (односторонние; висячие ребра; ребра с одной вершиной;) и 2-го типа (классические ребра; ребра с двумя вершинами)
/// @tparam ModelIncidences Тип моделей ребер
template <typename ModelIncidences>
struct basic_graph_t {

private:
    /// @brief Инциденции для каждой вершины графа
    vertex_incidences_map_t _vertex_incidences;

public:

    /// @brief Модели данного типа (статика, динамика) с инциденциями
    typedef ModelIncidences model_type;
        
    /// @brief Индекс графа
    size_t id{ 0 };
    
    /// @brief Ребра 1-го типа
    std::vector<ModelIncidences> edges1;
    /// @brief Ребра 2-го типа
    std::vector<ModelIncidences> edges2;

    /// @brief Конструктор копирования для класса basic_graph_t. Используется реализация по умолчанию
    basic_graph_t(const basic_graph_t& rhs) = default;

    /// @brief Конструктор перемещения для класса basic_graph_t
    basic_graph_t(basic_graph_t&& rhs) noexcept
    {
        edges1 = std::move(rhs.edges1);
        edges2 = std::move(rhs.edges2);
        _vertex_incidences = std::move(rhs._vertex_incidences);
        id = rhs.id;
    }

    /// @brief Конструктор по умолчанию. Инициализирует пустой граф.
    basic_graph_t() = default;

    /// @brief Оператор присваивания копированием. Используется реализация по умолчанию.
    basic_graph_t& operator =(const basic_graph_t&) = default;

    /// @brief Виртуальный деструктор. Используется реализация по умолчанию.
    virtual ~basic_graph_t() = default;

    /// @brief Добавляет ребро в граф
    /// @param edge Добавляемое ребро
    /// @warning Тип ребра (1-ый или 2-ый) определяется автоматически
    void add_edge(const ModelIncidences& edge) {
        if (edge.is_single_sided_edge())
            edges1.emplace_back(edge);
        else
            edges2.emplace_back(edge);
    }
    /// @brief Получает ссылку на ребро по привязке
    /// @param binding Привязка к ребру в графе
    /// @return Ссылка на ребро соответствующего типа (Edge1 или Edge2)
    /// @throws std::runtime_error Если привязка не относится к типу ребра
    ModelIncidences& get_edge(const graph_binding_t& binding) {        
        switch (binding.binding_type) {
        case graph_binding_type::Edge1:
            return edges1[binding.edge];
        case graph_binding_type::Edge2:
            return edges2[binding.edge];
        default:
            throw std::runtime_error("Binding must be of edge type");
        }
    }

    /// @brief Проверяет, является ли граф пустым (т.е. не содержит ребер)
    bool empty() const
    {
        return edges1.empty() && edges2.empty();
    }

    /// @brief Возвращает количество элементов в графе (вершин, ребер обоих типов)
    size_t get_object_count() const
    {
        return edges1.size() + edges2.size() + get_vertex_count();
    }

    /// @brief Возвращает количество вершин в графе
    size_t get_vertex_count() const
    {
        return get_vertex_incidences().size();
    }

    /// @brief Возвращает инциденции для вершины с заданным индексом 
    /// @param vertex_index Индекс вершины
    const vertex_incidences_pair_t& get_vertex_incidences(size_t vertex_index) const
    {
        return get_vertex_incidences().at(vertex_index);
    }

    /// @brief Возвращает инциденции для каждой вершины графа
    const vertex_incidences_map_t& get_vertex_incidences() const
    {
        typedef basic_graph_t<ModelIncidences> Graph;
        Graph* self = const_cast<Graph*>(this);

        if (!self->empty() && _vertex_incidences.empty()) {
            self->_vertex_incidences = get_vertex_incidences_map(edges1, edges2);
        }
        return _vertex_incidences;
    }

    /// @brief Возвращает индексы вершин графа
    std::unordered_set<std::size_t> get_vertex_indices() const
    {
        typedef basic_graph_t<ModelIncidences> Graph;
        Graph* self = const_cast<Graph*>(this);
        const vertex_incidences_map_t& incidences = self->get_vertex_incidences();
        auto result = get_map_keys(incidences);
        return result;
    }

    /// @brief Возвращает привязки для всех элементов графа
    std::vector<graph_binding_t> get_binding_list() const
    {
        std::vector<graph_binding_t> result;
        for (size_t edge1 = 0; edge1 < edges1.size(); ++edge1) {
            result.emplace_back(graph_binding_type::Edge1, edge1);
        }
        for (size_t edge2 = 0; edge2 < edges2.size(); ++edge2) {
            result.emplace_back(graph_binding_type::Edge2, edge2);
        }
        for (size_t vertex = 0; vertex < get_vertex_incidences().size(); ++vertex) {
            result.emplace_back(graph_binding_type::Vertex, vertex);
        }
        return result;
    }
    /// @brief Инициализирует заданный std::vector<bool> флагами посещений для всех вершин графа 
    /// @param vertices Инциденции вершин
    /// @param result Указатель на вектор, который будет заполнен
    static void _initialize_vertex_visit_variable(const vertex_incidences_map_t& vertices,
        std::vector<bool>* result)
    {
        *result = std::vector<bool>(vertices.size(), false);
    }

    /// @brief Инициализирует заданный std::unordered_map флагами посещений для всех вершин графа 
    /// @param vertices Инциденции вершин
    /// @param result Указатель на std::unordered_map
    static void _initialize_vertex_visit_variable(const vertex_incidences_map_t& vertices,
        std::unordered_map<size_t, bool>* result)
    {
        for (const auto& vertex : vertices)
        {
            (*result)[vertex.first] = false;
        }
    }

    /// @brief Возвращает инциденции для вершин, ранг которых не меньше заданного
    /// @param minimum_rank минимальный ранг
    vertex_incidences_map_t get_ranked_vertex_incidences(size_t minimum_rank) const
    {
        //typedef std::unordered_map<size_t, std::pair<vertex_incidences_t, vertex_incidences_t>> IncidencesMap;
        const auto& vertex_incidences = get_vertex_incidences();

        vertex_incidences_map_t result;

        // Просмотр всех вершин 
        for (const auto& vertex : vertex_incidences)
        {
            size_t vertex_index = vertex.first;
            const vertex_incidences_pair_t& incidences_for_vertex = vertex.second;

            size_t incident_edges_count = incidences_for_vertex.first.edges_count()
                + incidences_for_vertex.second.edges_count();

            if (incident_edges_count >= minimum_rank)
                result[vertex_index] = incidences_for_vertex;
        }

        return vertex_incidences;
    }

    /// @brief Обход в глубину с расчетом произвольного параметра в вершине
    /// @warning Только для dfs_function, отдельно не вызывается 
    /// @tparam VertexData Тип значений, сопоставленных вершинам
    /// @tparam VertexDataCalc Функтор, вычисляющий значение в вершине 
    /// @param calc Функтор, принимающий вектор индексов вершин и вектор значений в вершинах
    /// @param vertex Индекс текущей вершины
    /// @param _vertex_data Расчетные значения в вершинах
    /// @param _visited Флаги посещенности вершин
    template <typename VertexData, typename VertexDataCalc>
    void _dfs(VertexDataCalc& calc, size_t vertex, std::vector<VertexData>* _vertex_data, std::vector<bool>* _visited) const {
        std::vector<bool>& visited = *_visited;
        std::vector<VertexData>& vertex_data = *_vertex_data;

        visited[vertex] = true;

        const vertex_incidences_pair_t& incidences_for_vertex = get_vertex_incidences(vertex);
        for (size_t edge2 : incidences_for_vertex.second.outlet_edges) {
            size_t next_vertex = edges2[edge2].get_other_side_vertex(vertex);
            if (!visited[next_vertex])
                _dfs(calc, next_vertex, &vertex_data, &visited);
        }

        std::vector <size_t> downmost_vertices;
        for (size_t edge2 : incidences_for_vertex.second.outlet_edges) {
            downmost_vertices.push_back(edges2[edge2].get_other_side_vertex(vertex));
        }
        vertex_data[vertex] = calc(downmost_vertices, vertex_data);
    }

    /// @brief По заданной функции рассчитывает данные, приписанные к вершинам направленного графа
    /// с учетом топологического порядка вершин, заданного направлениями ребер графа
    /// Пример использования: поиск всех вершин в поддереве каждой вершины
    /// @tparam VertexData Тип значений, сопоставленных вершинам
    /// @tparam VertexDataCalc Функтор, вычисляющий значение в вершине 
    /// @param calc Функтор, принимающий вектор индексов вершин и вектор значений в вершинах
    /// @param vertex_data Массив с начальными значениями данных по всем вершинам
    /// @throws std::logic_error Если заданы данные vertex_data не во всех вершинах
    template <typename VertexData, typename VertexDataCalc>
    void dfs_function(VertexDataCalc& calc, std::vector<VertexData>* vertex_data) const {
        const vertex_incidences_map_t& incidences = get_vertex_incidences();

        std::vector<bool> visited(get_vertex_count(), false);
        if (vertex_data->size() != visited.size())
            /*throw std::runtime_error("basic_graph::dfs_function(): wrong 'vertex_data' size");*/
            throw std::logic_error("basic_graph::dfs_function(): wrong 'vertex_data' size");

        for (const auto& vertex : incidences) {
            _dfs(calc, vertex.first, vertex_data, &visited);
        }

    }

    // Из исходного графа строит граф компонент связности
    // Требуется, чтобы граничные ребра компонент связности ВСЕГДА соединяли его с другим компонентом связности

    /// @brief Строит граф компонент связности на основе заданных подграфов.
    /// Каждый подграф из входного списка рассматривается как отдельная компонента связности.
    /// Граф компонент связности содержит рёбра между компонентами, если между ними есть граничные рёбра.
    /// @pre Граничные рёбра подграфов должны соединять их с другими компонентами.Если вершина не найдена
    /// в обратном индексе(subgraph_by_vertex), выбрасывается исключение graph_exception.
    /// @tparam ModelIncidences Тип модели инциденций ребер исходного графа.
    /// @param components Вектор подграфов, каждый из которых представляет собой компоненту связности.
    /// @return basic_graph_t<edge_incidences_t> — граф компонент связности.
    /// @throws graph_exception Если вершина не может быть сопоставлена ни одной компоненте.
    basic_graph_t<edge_incidences_t> get_cohesion_components_graph(const std::vector<subgraph_t>& components) const
    {
        basic_graph_t<edge_incidences_t> cohesion_graph;

        // Построить обратный индекс для вершин граничных ребер
        std::unordered_map<size_t, size_t> subgraph_by_vertex;
        for (size_t subgraph_index = 0; subgraph_index < components.size(); ++subgraph_index)
        {
            const subgraph_t& subgraph = components[subgraph_index];
            for (size_t boundary_edge : subgraph.boundary_edges2) {
                const edge_incidences_t& edge = edges2[boundary_edge];
                if (subgraph.vertices.find(edge.from) != subgraph.vertices.end()) {
                    subgraph_by_vertex[edge.from] = subgraph_index;
                }
                if (subgraph.vertices.find(edge.to) != subgraph.vertices.end()) {
                    subgraph_by_vertex[edge.to] = subgraph_index;
                }
            }
        }

        // Построить граф компонент связности
        auto vertex_to_component = [&](size_t vertex) {
            if (subgraph_by_vertex.find(vertex) == subgraph_by_vertex.end()) {
                throw graph_exception(get_binding_list(),
                    L"Provided subgraph are not cohesion components"
                );
                //throw std::runtime_error("Provided subgraph are not cohesion components");
            }
            return subgraph_by_vertex.at(vertex);
            };

        for (size_t subgraph_index = 0; subgraph_index < components.size(); ++subgraph_index)
        {
            const subgraph_t& subgraph = components[subgraph_index];
            for (size_t boundary_edge : subgraph.boundary_edges2) {
                const edge_incidences_t& edge = edges2[boundary_edge];
                if (subgraph.vertices.find(edge.from) == subgraph.vertices.end()) {
                    // Пропускать граничные ребра, ВХОДЯЩИЕ в данный комп. связности
                    // Эти ребра будут учтены при обработке компонента связности, из которого они ВЫХОДЯТ
                    // (инвариант, что граничные ребра комп. связности могут идти 
                    //  только в другой компонент связности, поддерживается эксепшном в vertex_to_component() )
                    continue;
                }

                cohesion_graph.edges2.emplace_back(
                    vertex_to_component(edge.from),
                    vertex_to_component(edge.to));
            }
        }

        if (cohesion_graph.edges2.empty()) {
            // в случае одного компонента связности
            cohesion_graph._vertex_incidences.emplace(0, vertex_incidences_pair_t());
        }

        return cohesion_graph;
    }

    /// @brief Возвращает индексы ребер, соединяющих вершины из заданного списка вершин
    /// @param vertex_subset список индексов вершин
    std::vector<size_t> get_edges_of_vertex_subset(const std::unordered_set<size_t>& vertex_subset)
    {
        const vertex_incidences_map_t& vertex_incidences = get_vertex_incidences();

        std::vector<size_t> edge_subset;
        for (size_t vertex : vertex_subset) {
            // Проверяем только выходящие ребра. Если ребро принадлежит подграфу, то оно входит и в 
            const std::vector<size_t>& outlet_edges = vertex_incidences.at(vertex).second.outlet_edges;
            for (size_t edge2 : outlet_edges) {
                if (vertex_subset.find(edges2[edge2].to) != vertex_subset.end()) {
                    edge_subset.push_back(edge2);
                }
            }
        }
        return edge_subset;
    }

    /// @brief Формирует разреженные матрицы инциденций графа для рёбер 1-го и 2-го типов
    /// Для каждого ребра создаются записи в (i, j, value), где:
    ///     i — индекс вершины,
    ///     j — индекс ребра,
    ///     value — направление ребра относительно вершины (+1 если входит, иначе - 1)
    /// @return [Матрица инициденций ребер 1-го типа; Матрица инициденций ребер 2-го типа]
    std::tuple<
        Eigen::SparseMatrix<double, Eigen::RowMajor>, 
        Eigen::SparseMatrix<double, Eigen::RowMajor>
    > get_incidences_matrices() const
    {
        std::vector<Eigen::Triplet<double>> incidences1_triplets, incidences2_triplets;
        size_t max_vertex = 0;
        for (size_t edge = 0; edge < edges1.size(); ++edge) {
            const auto& edge_incidences = edges1[edge];
            if (edge_incidences.get_vertex_incidence(edge_incidences.from) != 0) {
                incidences1_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.from, (int)edge, -1));
                max_vertex = std::max(max_vertex, edge_incidences.from);
            }
            else {
                incidences1_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.to, (int)edge, +1));
                max_vertex = std::max(max_vertex, edge_incidences.to);
            }
        }
        for (size_t edge = 0; edge < edges2.size(); ++edge) {
            const auto& edge_incidences = edges2[edge];
            incidences2_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.from, (int)edge, -1));
            incidences2_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.to, (int)edge, +1));
            max_vertex = std::max(max_vertex, std::max(edge_incidences.from, edge_incidences.to));
        }
        size_t vertex_count = max_vertex + 1;

        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences1(vertex_count, edges1.size());
        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences2(vertex_count, edges2.size());
        if (!incidences1_triplets.empty())
            incidences1.setFromTriplets(incidences1_triplets.begin(), incidences1_triplets.end());
        if (!incidences2_triplets.empty())
            incidences2.setFromTriplets(incidences2_triplets.begin(), incidences2_triplets.end());

        return std::make_tuple(std::move(incidences1), std::move(incidences2));
    }

    /// @brief Возвращает сокращенную матрицу инцидентности для висячих дуг
    /// @param remove_last_edges1 Сколько последних ребер (колонок матрицы) отбросить
    /// @param remove_last_rows Сколько последних узлов (строк матрицы) отбросить
    /// @return Матрица A1-сокращенная
    Eigen::SparseMatrix<double, Eigen::RowMajor> 
        get_reduced_incidences1(std::size_t remove_last_edges1,
            std::size_t remove_last_rows) const
    {
        std::size_t vertex_count = get_vertex_count();
        std::size_t reduced_vertex_count = vertex_count - remove_last_rows;
        std::size_t reduced_edge1_count = edges1.size() - remove_last_edges1;

        std::vector<Eigen::Triplet<double>> incidences1_triplets;
        for (size_t edge = 0; edge < reduced_edge1_count; ++edge) {
            const auto& edge_incidences = edges1[edge];
            const std::size_t& vertex = edge_incidences.get_vertex_for_single_sided_edge();
            if (vertex < reduced_vertex_count) {
                int incidence = edge_incidences.get_vertex_incidence(vertex);
                incidences1_triplets.emplace_back((int)vertex, (int)edge, incidence);
            }
        }

        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences1(reduced_vertex_count, reduced_edge1_count);
        if (!incidences1_triplets.empty())
            incidences1.setFromTriplets(incidences1_triplets.begin(), incidences1_triplets.end());
        return incidences1;
    }
    /// @brief Готовит матрицы A1r, A1p, имея ввиду, что последние дуги и последние вершины - P-узлы
    /// Матрица A1p возвращается с относительные индексами узлов и дуг!
    /// См. введение в ТГЦ, формализм висясих дуг
    /// @param remove_last_vertices Количество P-вершин и столько же
    /// @param relative_indices Использовать относительные индексы (в A1r и так будет 
    /// @return Матрицы A1r, A1p
    std::tuple<
        Eigen::SparseMatrix<double, Eigen::RowMajor>,
        Eigen::SparseMatrix<double, Eigen::RowMajor>
    > split_reduced_incidences1(std::size_t remove_last_vertices) const
    {
        std::size_t vertex_count = get_vertex_count();
        std::size_t reduced_vertex_count = vertex_count - remove_last_vertices;
        std::size_t reduced_edge1_count = edges1.size() - remove_last_vertices;

        std::vector<Eigen::Triplet<double>> reduced_incidences1;
        std::vector<Eigen::Triplet<double>> remaining_incidences1;
        for (size_t edge = 0; edge < edges1.size(); ++edge) {
            const auto& edge_incidences = edges1[edge];
            const std::size_t& vertex = edge_incidences.get_vertex_for_single_sided_edge();
            int incidence = edge_incidences.get_vertex_incidence(vertex);
            if (vertex < reduced_vertex_count) {
                reduced_incidences1.emplace_back((int)vertex, (int)edge, incidence);
            }
            else {
                remaining_incidences1.emplace_back(
                    (int)(vertex - reduced_vertex_count),
                    (int)(edge - reduced_edge1_count), 
                    incidence);
            }
        }

        Eigen::SparseMatrix<double, Eigen::RowMajor> reduced_matrix1(
            reduced_vertex_count, reduced_edge1_count);
        if (!reduced_incidences1.empty())
            reduced_matrix1.setFromTriplets(reduced_incidences1.begin(), reduced_incidences1.end());

        Eigen::SparseMatrix<double, Eigen::RowMajor> remaining_matrix1(
            remove_last_vertices, remove_last_vertices);
        if (!remaining_incidences1.empty())
            remaining_matrix1.setFromTriplets(remaining_incidences1.begin(), remaining_incidences1.end());

        return { std::move(reduced_matrix1), std::move(remaining_matrix1) };
    }

    /// @brief Возвращает сокращенную матрицу инцидентности для двусторонних дуг
    /// @param remove_last_rows Сколько последних узлов (строк матрицы) отбросить
    /// @return Матрица A2-сокращенная
    Eigen::SparseMatrix<double, Eigen::RowMajor>
        get_reduced_incidences2(std::size_t remove_last_rows) const
    {
        std::size_t vertex_count = get_vertex_count();
        std::size_t reduced_vertex_count = vertex_count - remove_last_rows;

        std::vector<Eigen::Triplet<double>> incidences2_triplets;

        for (size_t edge = 0; edge < edges2.size(); ++edge) {
            const auto& edge_incidences = edges2[edge];
            if (edge_incidences.from < reduced_vertex_count) {
                incidences2_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.from, (int)edge, -1));
            }
            if (edge_incidences.to < reduced_vertex_count) {
                incidences2_triplets.push_back(Eigen::Triplet<double>((int)edge_incidences.to, (int)edge, +1));
            }
        }
        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences2(reduced_vertex_count, edges2.size());
        if (!incidences2_triplets.empty())
            incidences2.setFromTriplets(incidences2_triplets.begin(), incidences2_triplets.end());

        return incidences2;
    }

    /// @brief Готовит матрицы A2r и A2p. Нумерация узлов в A2p относитедная
    /// См. введение в ТГЦ, формализм висясих дуг
    /// @param remove_last_rows Количество P-узлов
    /// @return матрицы A2r и A2p
    std::tuple< 
        Eigen::SparseMatrix<double, Eigen::RowMajor>,
        Eigen::SparseMatrix<double, Eigen::RowMajor>
    > split_reduced_incidences2(std::size_t remove_last_rows) const
    {
        std::size_t vertex_count = get_vertex_count();
        std::size_t reduced_vertex_count = vertex_count - remove_last_rows;

        std::vector<Eigen::Triplet<double>> reduced_incidences2;
        std::vector<Eigen::Triplet<double>> remaining_incidences2;

        for (size_t edge = 0; edge < edges2.size(); ++edge) {
            const auto& edge_incidences = edges2[edge];

            if (edge_incidences.from < reduced_vertex_count) {
                Eigen::Triplet<double> triplet_from((int)edge_incidences.from, (int)edge, -1);
                reduced_incidences2.push_back(triplet_from);
            }
            else {
                Eigen::Triplet<double> triplet_from(
                    (int)(edge_incidences.from - reduced_vertex_count),
                    (int)edge, -1);
                remaining_incidences2.push_back(triplet_from);
            }
            if (edge_incidences.to < reduced_vertex_count) {
                Eigen::Triplet<double> triplet_to((int)edge_incidences.to, (int)edge, +1);
                reduced_incidences2.push_back(triplet_to);
            }
            else {
                Eigen::Triplet<double> triplet_to(
                    (int)(edge_incidences.to - reduced_vertex_count), 
                    (int)edge, +1);
                remaining_incidences2.push_back(triplet_to);
            }
        }
        Eigen::SparseMatrix<double, Eigen::RowMajor> reduced_matrix2(reduced_vertex_count, edges2.size());
        if (!reduced_incidences2.empty())
            reduced_matrix2.setFromTriplets(reduced_incidences2.begin(), reduced_incidences2.end());

        Eigen::SparseMatrix<double, Eigen::RowMajor> remaining_matrix2(remove_last_rows, edges2.size());
        if (!remaining_incidences2.empty())
            remaining_matrix2.setFromTriplets(remaining_incidences2.begin(), remaining_incidences2.end());

        return { std::move(reduced_matrix2), std::move(remaining_matrix2) };
    }


    /// @brief Матрицы инцидентностей 
    /// Оставляем в матрицах только нужное количество дуг и узлов
    std::tuple<
        Eigen::SparseMatrix<double, Eigen::RowMajor>,
        Eigen::SparseMatrix<double, Eigen::RowMajor>
    > get_incidences_matrices(
        std::size_t remove_last_edges1,
        std::size_t remove_last_rows) const
    {
        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences1
            = get_reduced_incidences1(remove_last_edges1, remove_last_rows);
        Eigen::SparseMatrix<double, Eigen::RowMajor> incidences2
            = get_reduced_incidences2(remove_last_rows);
        return std::make_tuple(std::move(incidences1), std::move(incidences2));
    }

    /// @brief Вычисляет ранг (расстояние) от заданной начальной вершины до всех достижимых вершин графа
    /// @param start_vertex Заданная начальная вершина
    /// @param _rank Ранги вершин
    /// @param _precendence Предшествующие вершины
    void find_distance_rank(size_t start_vertex,
        std::unordered_map<size_t, size_t>* _rank, std::unordered_map<size_t, size_t>* _precendence) const
    {
        std::unordered_map<size_t, size_t>& rank = *_rank;
        std::unordered_map<size_t, size_t>& precendence = *_precendence;

        const auto& vertex_incidences = get_vertex_incidences();

        rank[start_vertex] = 0;
        if (precendence.find(start_vertex) != precendence.end()) {
            // Поскольку алгоритм в принципе может ранее дойти до вершины с рангом 0,
            // то для этой вершины будет назначена вершина предшествования.
            // В этих случаях вершины предшествования надо удалять
            precendence.erase(start_vertex);
        }

        std::queue<size_t> bfs_queue;
        bfs_queue.push(start_vertex);
        while (!bfs_queue.empty()) {
            size_t vertex = bfs_queue.front();
            bfs_queue.pop();
            const vertex_incidences_t& incidences_for_vertex = vertex_incidences.at(vertex).second;

            auto incident_edges = incidences_for_vertex.all_edges();
            for (auto edge : incident_edges) {
                size_t other_vertex = edges2[edge].get_other_side_vertex(vertex);
                size_t rank_for_new_vertex = 1 + rank.at(vertex);

                if (rank.find(other_vertex) == rank.end() || rank.at(other_vertex) > rank_for_new_vertex)
                {
                    // если у новой вершины нет ранга (), 
                    // либо ранг уменьшить
                    rank[other_vertex] = rank_for_new_vertex;
                    precendence[other_vertex] = vertex;

                    if (precendence.size() > get_vertex_count()) {
                        throw std::runtime_error("basic_graph_t::find_start_vertex() has path which is longer, than vertex count");
                    }

                    bfs_queue.push(other_vertex);
                }
            }
        }
    }

    /// @brief 
    /// @param start_vertex Заданная начальная вершина
    /// @param _rank Ранги вершин
    /// @param _precendence Предшествующие вершины

    /// @brief Вычисляет ранг (расстояние) от множества начальных вершин до всех достижимых вершин графа
    /// @tparam Container Тип контейнера с индексами начальных вершин
    /// @param sink_vertex_set Индексы начальных вершин
    /// @param rank Ранги вершин
    /// @param precendence Предшествующие вершины
    template <typename Container>
    void find_distance_ranks(const Container& sink_vertex_set,
        std::unordered_map<size_t, size_t>* rank,
        std::unordered_map<size_t, size_t>* precendence) const
    {
        for (const auto& vertex : sink_vertex_set) {
            find_distance_rank(vertex, rank, precendence);
        }
    }

    /// @brief Находит начальную вершину пути, к которому относится указанная вершина, на основе цепочки предшествующих вершин
    /// @param vertex Индекс вершины
    /// @param precendence Сопоставление индекс вершины - индекс предшественника
    size_t find_start_vertex(size_t vertex, const std::unordered_map<size_t, size_t>& precendence) const
    {
        size_t path_length = 0;
        size_t result = vertex;
        while (precendence.find(result) != precendence.end()) {
            result = precendence.at(result);

            if (++path_length > get_vertex_count()) {
                throw std::runtime_error("basic_graph_t::find_start_vertex() has path which is longer, than vertex count");
            }
        }
        return result;
    }



    /// @brief Возвращает индексы ребер, соединяющих вершины из заданного набора вершин
    /// @tparam Set Тип данных набора вершин
    /// @param vertex_subset список индексов вершин
    template <typename Set>
    std::vector<size_t> get_edges2_of_vertex_subset(const Set& vertex_subset) const
    {
        const auto& vertex_incidences = get_vertex_incidences();

        std::vector<size_t> edge_subset;
        for (size_t vertex : vertex_subset) {
            // проверяем только выходящие ребра. Если ребро принадлежит подграфу, то оно входит и в 
            for (size_t edge2 : vertex_incidences.at(vertex).second.outlet_edges)
            {
                if (vertex_subset.find(edges2[edge2].to) != vertex_subset.end()) {
                    edge_subset.push_back(edge2);
                }
            }
        }
        return edge_subset;
    }

    /// @brief Создает граф на основе заданного подграфа
    /// @tparam GraphType Тип создаваемого подграфа
    /// @param subgraph_id Идентификатор создаваемого подграфа
    /// @param subset Подграф
    /// @return Созданный граф
    template <typename GraphType>
    GraphType create_subgraph(size_t subgraph_id, const subgraph_t& subset) const
    {
        
        GraphType result;
        result.id = subgraph_id;
        
        // Копирование моделей
        result.edges1 = filter_elements_by_indices(edges1.begin(), edges1.end(), subset.edges1);
        result.edges2 = filter_elements_by_indices(edges2.begin(), edges2.end(), subset.edges2);

        // Исправление индексации вершин
        auto vertex_inv_index = get_inverted_index_as_unordered_map(subset.vertices);

        for (auto& edge1 : result.edges1) {
            if (edge1.has_vertex_from()) {
                edge1.from = vertex_inv_index.at(edge1.from);
            }
            else {
                edge1.to = vertex_inv_index.at(edge1.to);
            }
        }
        for (auto& edge2 : result.edges2) {
            edge2.from = vertex_inv_index.at(edge2.from);
            edge2.to = vertex_inv_index.at(edge2.to);
        }

        return result;
    }

    /// @brief Поиск подграфа методом обхода в ширину с фильтром на ребра
    /// @warning Вызывать только из bfs_search_subgraph
    /// @tparam FnAllow1,FnAllow2 Прототип селектора FnAllow(size_t, ModelIncidences::model_type*) -> bool
    /// @param start_vertex Начальная вершина
    /// @param vertices Инциденции вершин графа
    /// @param _visited_vertices Флаги посещения вершин
    /// @param allow_object1 Селектор ребер 1-го типа
    /// @param allow_object2 Селектор ребер 2-го типа
    /// @param allow_empty_subgraphs Флаг разрешения на возврат пустых подграфов
    /// @param max_edge_count Ограничение глубины поиска
    /// @return Подграф, найденный в результате обхода графа
    template <typename FnAllow1, typename FnAllow2>
    inline subgraph_t _bfs_search_subgraph_routine(size_t start_vertex,
        const vertex_incidences_map_t& vertices, std::vector<bool>* _visited_vertices,
        FnAllow1& allow_object1, FnAllow2& allow_object2,
        bool allow_empty_subgraphs, size_t max_edge_count) const
    {
        // при вызове функции необходимо проверить
        // что вершина start_vertex не visited

        if (max_edge_count == 0) return subgraph_t();

        auto check_subset_size = [&](const subgraph_t& subset) {
            if (subset.edges1.size() > edges1.size())
                throw std::runtime_error("bfs_search_subgraph(): subset.edges1 cannot be larger than graph.edges1");
            if (subset.edges2.size() > edges2.size())
                throw std::runtime_error("bfs_search_subgraph(): subset.edges2 cannot be larger than graph.edges2");
            };

        std::vector<bool>& visited_vertices = *_visited_vertices;

        subgraph_t graph_subset;
        std::queue <size_t> bfs_queue;
        bfs_queue.push(start_vertex);
        while (!bfs_queue.empty()) {
            check_subset_size(graph_subset);

            size_t vertex = bfs_queue.front();
            bfs_queue.pop();
            if (visited_vertices[vertex])
                continue; // так могло получиться для параллельных ниток

            visited_vertices[vertex] = true;
            graph_subset.vertices.insert(vertex);

            const vertex_incidences_t& vertices1 = vertices.at(vertex).first;
            const vertex_incidences_t& vertices2 = vertices.at(vertex).second;

            std::vector<size_t> incident_edges1 = vertices1.all_edges();
            std::vector<size_t> incident_edges2 = vertices2.all_edges();

            for (size_t edge1 : incident_edges1) {
                if (allow_object1(edge1, edges1[edge1])) {
                    graph_subset.edges1.push_back(edge1);
                    if (graph_subset.total_edge_count() == max_edge_count)
                        return graph_subset;
                }
                else
                    graph_subset.boundary_edges1.push_back(edge1);
            }

            for (size_t edge2 : incident_edges2) {
                size_t next_vertex = edges2[edge2].get_other_side_vertex(vertex);
                if (allow_object2(edge2, edges2[edge2])) {
                    if (!visited_vertices[next_vertex]) {
                        graph_subset.edges2.push_back(edge2);
                        if (graph_subset.total_edge_count() == max_edge_count)
                            return graph_subset;
                        bfs_queue.push(next_vertex);
                    }
                }
                else {
                    graph_subset.boundary_edges2.push_back(edge2);
                }
            }
        }
        if (graph_subset.edges1.empty() && graph_subset.edges2.empty()
            && !allow_empty_subgraphs)
        {
            graph_subset.vertices.clear();
        }
        return graph_subset;
    }

    /// @brief Поиск подграфа методом обхода в ширину с фильтром на ребра
    /// @warning Вызывать только из bfs_search_subgraph
    /// @tparam FnAllow1,FnAllow2 Прототип селектора FnAllow(size_t, ModelIncidences::model_type*) -> bool
    /// @param start_vertex Начальная вершина
    /// @param vertices Инциденции вершин графа
    /// @param _visited_vertices Флаги посещения вершин
    /// @param allow_object1 Селектор ребер 1-го типа
    /// @param allow_object2 Селектор ребер 2-го типа
    /// @param allow_empty_subgraphs Флаг разрешения на возврат пустых подграфов
    /// @param not_allowed_object_is_always_boundary Флаг обработки ребер, граничащих с допустимым подграфом. 
    /// Если true, то недопустимые рёбра автоматически считаются граничными. Иначе проверяется принадлежность их вершин к подграфу.
    /// @param max_edge_count Ограничение глубины поиска
    /// @return Подграф, найденный в результате обхода графа
    template <typename FnAllow1, typename FnAllow2, typename VertexVisitType>
    inline subgraph_t _bfs_search_subgraph_routine(size_t start_vertex,
        const vertex_incidences_map_t& vertices, VertexVisitType* _visited_vertices,
        FnAllow1& allow_object1, FnAllow2& allow_object2,
        bool allow_empty_subgraphs,
        bool not_allowed_object_is_always_boundary,
        size_t max_edge_count) const
    {
        // при вызове функции необходимо проверить
        // что вершина start_vertex не visited

        if (max_edge_count == 0) return subgraph_t();

        auto check_subset_size = [&](const subgraph_t& subset) {
            if (subset.edges1.size() > edges1.size())
                throw std::logic_error("bfs_search_subgraph(): subset.edges1 cannot be larger than graph.edges1");
            if (subset.edges2.size() > edges2.size())
                throw std::logic_error("bfs_search_subgraph(): subset.edges2 cannot be larger than graph.edges2");
            };

        auto& visited_vertices = *_visited_vertices;

        std::vector<size_t> boundary_candidates_edges2;

        subgraph_t graph_subset;
        std::queue <size_t> bfs_queue;
        bfs_queue.push(start_vertex);
        while (!bfs_queue.empty()) {
            check_subset_size(graph_subset);

            size_t vertex = bfs_queue.front();
            bfs_queue.pop();
            if (visited_vertices[vertex])
                continue; // так могло получиться для параллельных ниток

            visited_vertices[vertex] = true;
            graph_subset.vertices.insert(vertex);

            const vertex_incidences_t& vertices1 = vertices.at(vertex).first;
            const vertex_incidences_t& vertices2 = vertices.at(vertex).second;

            std::vector<size_t> incident_edges1 = vertices1.all_edges();
            std::vector<size_t> incident_edges2 = vertices2.all_edges();

            for (size_t edge1 : incident_edges1) {
                if (allow_object1(edge1, edges1[edge1])) {
                    graph_subset.edges1.push_back(edge1);
                    if (graph_subset.total_edge_count() == max_edge_count)
                        return graph_subset;
                }
                else
                    graph_subset.boundary_edges1.push_back(edge1);
            }

            for (size_t edge2 : incident_edges2) {
                size_t next_vertex = edges2[edge2].get_other_side_vertex(vertex);
                if (allow_object2(edge2, edges2[edge2])) {
                    if (!visited_vertices[next_vertex]) {
                        graph_subset.edges2.push_back(edge2);
                        if (graph_subset.total_edge_count() == max_edge_count)
                            return graph_subset;
                        bfs_queue.push(next_vertex);
                    }
                }
                else {
                    // Проверить, что следующая вершина не принадлежит подграфу,
                    // иначе ребро добавляется два раза
                    if (graph_subset.vertices.find(next_vertex) == graph_subset.vertices.end())
                        boundary_candidates_edges2.push_back(edge2);
                }
            }
        }
        if (graph_subset.edges1.empty() && graph_subset.edges2.empty()
            && !allow_empty_subgraphs)
        {
            graph_subset.vertices.clear();
        }

        if (not_allowed_object_is_always_boundary)
        {
            graph_subset.boundary_edges2 = boundary_candidates_edges2;
        }
        else
        {
            for (size_t boundary_candidate_edge2 : boundary_candidates_edges2) {
                bool vertex_from_in_subgraph =
                    graph_subset.vertices.find(edges2[boundary_candidate_edge2].from) != graph_subset.vertices.end();
                bool vertex_to_in_subgraph =
                    graph_subset.vertices.find(edges2[boundary_candidate_edge2].to) != graph_subset.vertices.end();
                if (vertex_from_in_subgraph && vertex_to_in_subgraph)
                {
                    // ребро в действительности не граничное, есть другой путь между двумя ее вершинами
                    graph_subset.edges2.push_back(boundary_candidate_edge2);
                }
                else {
                    graph_subset.boundary_edges2.push_back(boundary_candidate_edge2);
                }

            }
        }


        return graph_subset;
    }


    /// @brief Общая функция поиск подграфа ребер обоих типов
    /// @tparam FnAllow1,FnAllow2 Прототип селектора FnAllow(size_t, ModelIncidences::model_type*) -> bool
    /// @param start_vertex Начальная вершина
    /// @param allow_object1 Селектор ребер 1-го типа
    /// @param allow_object2 Селектор ребер 2-го типа
    /// @param allow_empty_subgraphs Флаг разрешения на возврат пустых подграфов
    /// @param max_edge_count Ограничение глубины поиска
    /// @return Подграф, найденный в результате обхода графа
    template <typename FnAllow1, typename FnAllow2>
    inline std::vector<subgraph_t> bfs_search_subgraph(
        FnAllow1& allow_object1, FnAllow2& allow_object2, bool allow_empty_subgraphs = false,
        size_t max_edge_count = std::numeric_limits<size_t>::max()) const
    {
        std::vector<subgraph_t> objects;

        const vertex_incidences_map_t& vertices = get_vertex_incidences();
        std::vector<bool> visited_vertices(vertices.size(), false);

        for (const auto& vertex : vertices) {
            size_t start_vertex = vertex.first;
            if (!visited_vertices[start_vertex]) {
                subgraph_t subgraph = _bfs_search_subgraph_routine(start_vertex, vertices,
                    &visited_vertices, allow_object1, allow_object2, allow_empty_subgraphs, max_edge_count);
                if (subgraph.total_place_count() != 0) {
                    objects.emplace_back(std::move(subgraph)); // получится ли здесь move-конструктор?
                }
            }
        }

        return objects;
    }

    /// @brief Общая функция поиск подграфа ребер обоих типов методом обхода в ширину
    /// Прототип селекторов FnAllow(size_t, ModelIncidences::model_type*) -> bool
    /// Прототип фильтров FnFilter(size_t, ModelIncidences::model_type*) -> bool
    /// @param allow_empty_subgraphs Флаг разрешения на возврат пустых подграфов
    /// @param max_edge_count Ограничение глубины поиска
    template <typename FnAllow1, typename FnAllow2, typename FnFilter1, typename FnFilter2>
    std::vector<subgraph_t> bfs_search_subgraph(
        FnAllow1& allow_object1, FnAllow2& allow_object2,
        FnFilter1& filter_object1, FnFilter2& filter_object2,
        bool allow_empty_subgraphs, size_t max_edge_count
    ) const
    {
        std::vector<subgraph_t> allowed_result =
            bfs_search_subgraph(allow_object1, allow_object2, allow_empty_subgraphs, max_edge_count);

        std::vector<subgraph_t> filtered_result;

        for (subgraph_t& subset : allowed_result) {
            subgraph_t filtered_subset;
            filtered_subset.boundary_edges1 = std::move(subset.boundary_edges1);
            filtered_subset.boundary_edges2 = std::move(subset.boundary_edges2);

            std::copy_if(subset.edges1.begin(), subset.edges1.end(),
                std::back_inserter(filtered_subset.edges1),
                [&](size_t edge1) { return filter_object1(edge1, edges1[edge1]); }
            );

            std::copy_if(subset.edges2.begin(), subset.edges2.end(),
                std::back_inserter(filtered_subset.edges2),
                [&](size_t edge2) { return filter_object2(edge2, edges2[edge2]); }
            );

            for (size_t edge1 : filtered_subset.edges1) {
                filtered_subset.vertices.insert(edges1[edge1].get_vertex_for_single_sided_edge());
            }
            for (size_t edge2 : filtered_subset.edges2) {
                filtered_subset.vertices.insert(edges2[edge2].from);
                filtered_subset.vertices.insert(edges2[edge2].to);
            }

            if (allow_empty_subgraphs || !filtered_subset.empty())
                filtered_result.emplace_back(std::move(filtered_subset));
        }
        return filtered_result;
    }

    // @brief Общая функция поиск подграфа ребер обоих типов
    /// Прототип селекторов FnAllow(size_t, ModelIncidences::model_type*) -> bool
    /// @tparam FnAllow1 
    /// @tparam FnAllow2 
    /// @tparam VertexVisitType  std::vector<bool> для последовательной нумерации вершин
    ///                          std::unordered_map<size_t, bool> - для произвольной
    /// @param allow_object1 Селектор ребер типа 1
    /// @param allow_object2 Селектор ребер типа 1
    /// @param not_allowed_object_is_always_boundary Флаг обработки ребер, граничащих с допустимым подграфом. 
    /// Если true, то недопустимые рёбра автоматически считаются граничными. Иначе проверяется принадлежность их вершин к подграфу.
    /// @param max_edge_count Количество объектов в подграфе (используется селектором при идентификации)
    /// @return Список подграфов
    template <typename FnAllow1, typename FnAllow2, typename VertexVisitType = std::vector<bool>>
    std::vector<subgraph_t> bfs_search_subgraph(
        const FnAllow1& allow_object1, const FnAllow2& allow_object2, bool allow_empty_subgraphs = false,
        bool not_allowed_object_is_always_boundary = true,
        size_t max_edge_count = std::numeric_limits<size_t>::max()) const
    {
        std::vector<subgraph_t> objects;

        const vertex_incidences_map_t& vertices = get_vertex_incidences();
        VertexVisitType visited_vertices;
        _initialize_vertex_visit_variable(vertices, &visited_vertices);

        for (const auto& vertex : vertices) {
            size_t start_vertex = vertex.first;
            if (!visited_vertices[start_vertex]) {
                subgraph_t subgraph = _bfs_search_subgraph_routine(start_vertex, vertices,
                    &visited_vertices, allow_object1, allow_object2, allow_empty_subgraphs,
                    not_allowed_object_is_always_boundary, max_edge_count);
                if (subgraph.total_place_count() != 0) {
                    objects.emplace_back(std::move(subgraph)); // получится ли здесь move-конструктор?
                }
            }
        }

        return objects;
    }

    /// @brief Возвращает модель по привязке к ребру графа
    const ModelIncidences& get_model_incidences(const graph_binding_t& binding) const
    {
        if (binding.binding_type == graph_binding_type::Edge1) {
            return edges1[binding.edge];
        }
        else if (binding.binding_type == graph_binding_type::Edge2) {
            return edges2[binding.edge];
        }
        else
            throw "get_edge_model() binding must be Edge1 or Edge2";
    }

    /// @brief Возвращает верщины, инцидентные ребрам с заданными индексами
    /// @param edges1_indices Индексы односторонних ребер
    /// @param edges2_indices Индексы двусторонних ребер
    template <typename IndexList>
    std::unordered_set<size_t> get_incident_vertices_for_edges(const IndexList& edges1_indices, const IndexList& edges2_indices) const
    {
        std::unordered_set<size_t> result;
        for (size_t edge1 : edges1_indices) {
            result.insert(edges1[edge1].get_vertex_for_single_sided_edge());
        }
        for (size_t edge2 : edges2_indices) {
            result.insert(edges2[edge2].from);
            result.insert(edges2[edge2].to);
        }
        return result;
    }

    /// @brief Возвращает привязки ребер графа (без вершин!)
    std::vector<graph_binding_t> get_edge_binding_list() const
    {
        std::vector<graph_binding_t> result;
        for (size_t edge1 = 0; edge1 < edges1.size(); ++edge1) {
            result.emplace_back(graph_binding_type::Edge1, edge1);
        }
        for (size_t edge2 = 0; edge2 < edges2.size(); ++edge2) {
            result.emplace_back(graph_binding_type::Edge2, edge2);
        }
        return result;
    }

    /// @brief Возвращает все вершины-притоки графа (вершины без входящих ребер)
    template <typename ResultContainer>
    ResultContainer get_source_vertices() const
    {
        const auto& vertex_incidences = get_vertex_incidences();
        ResultContainer result;
        for (const auto& vertex : vertex_incidences) {
            size_t vertex_index = vertex.first;
            const vertex_incidences_pair_t& incidences_for_vertex = vertex.second;

            if (incidences_for_vertex.second.inlet_edges.empty())
                result.insert(result.end(), vertex_index);
        }
        return result;
    }

    /// @brief Возвращает все вершины-стоки графа (вершины без выходящих ребер)
    template <typename ResultContainer>
    ResultContainer get_sink_vertices() const
    {
        const auto& vertex_incidences = get_vertex_incidences();
        ResultContainer result;
        for (const auto& vertex : vertex_incidences) {
            size_t vertex_index = vertex.first;
            const vertex_incidences_pair_t& incidences_for_vertex = vertex.second;
            if (incidences_for_vertex.second.outlet_edges.empty())
                result.insert(result.end(), vertex_index);
        }
        return result;
    }

    /// @brief Топологическая сортировка
    /// @param p_vertex_sequence топологическая последовательность вершин
    /// @param edge2_sequence топологическая последовательность двусторонних ребер
    /// @param direction направление обхода: +1 от истоков, -1 от стоков
    /// @param exclude_sources не учитывать вершины, от которых начинается сортировка
    /// @param force_grey_vertices продолжать обход серых вершин (т.е. открытых, но не завершенных)
    /// @param _start_vertices вершины для начала обхода графа
    void topological_sort(
        std::vector<size_t>* p_vertex_sequence, std::vector<size_t>* edge2_sequence = nullptr,
        int direction = +1,
        bool exclude_sources = false,
        bool force_grey_vertices = false,
        const std::vector<size_t>& _start_vertices = std::vector<size_t>()) const
    {
        const auto& vertex_incidences = get_vertex_incidences();

        std::unordered_set<size_t> start_vertices;
        if (_start_vertices.empty()) {
            if (direction > 0)
                start_vertices = get_source_vertices<std::unordered_set<size_t>>();
            else
                start_vertices = get_sink_vertices<std::unordered_set<size_t>>();
        }
        else {
            start_vertices = std::unordered_set<size_t>(_start_vertices.begin(), _start_vertices.end());
        }

        std::deque<size_t> bfs_queue(start_vertices.begin(), start_vertices.end());


        std::vector<size_t> edge_order, vertex_sequence;
        std::vector<bool> visited_edges(edges2.size());

        auto vertex_has_visited_inlet_edges = [&](size_t vertex) -> bool {
            for (auto edge2 : vertex_incidences.at(vertex).second.inlet_edges) {
                if (!visited_edges[edge2])
                    return false;
            }
            return true;
            };

        auto vertex_has_visited_outlet_edges = [&](size_t vertex) -> bool {
            for (auto edge2 : vertex_incidences.at(vertex).second.outlet_edges) {
                if (!visited_edges[edge2])
                    return false;
            }
            return true;
            };

        std::unordered_set<size_t> grey_vertices;

        do {
            while (!bfs_queue.empty()) {
                size_t vertex = bfs_queue.front();
                if (force_grey_vertices) {
                    grey_vertices.erase(vertex);
                }

                if (!exclude_sources || start_vertices.find(vertex) == start_vertices.end()) {
                    // добавляем вершину в путь, 
                    // -если можно добавлять начальные вершины, 
                    // -либо если это не начальные вершина
                    vertex_sequence.push_back(vertex);
                }
                bfs_queue.pop_front();

                const auto& incident_edges = direction > 0
                    ? vertex_incidences.at(vertex).second.outlet_edges
                    : vertex_incidences.at(vertex).second.inlet_edges;
                for (auto edge2 : incident_edges) {
                    visited_edges[edge2] = true;
                    edge_order.push_back(edge2);

                    auto other_side_vertex = edges2[edge2].get_other_side_vertex(vertex);
                    if (direction > 0) {
                        if (vertex_has_visited_inlet_edges(other_side_vertex)) {
                            bfs_queue.push_back(other_side_vertex);
                        }
                        else if (force_grey_vertices) {
                            grey_vertices.insert(other_side_vertex);
                        }
                    }
                    else {
                        if (vertex_has_visited_outlet_edges(other_side_vertex)) {
                            bfs_queue.push_back(other_side_vertex);
                        }
                        else if (force_grey_vertices) {
                            grey_vertices.insert(other_side_vertex);
                        }
                    }
                }
            }

            if (!grey_vertices.empty())
                bfs_queue = std::deque<size_t>(grey_vertices.begin(), grey_vertices.end());
        } while (!bfs_queue.empty());

        if (p_vertex_sequence) {
            *p_vertex_sequence = std::move(vertex_sequence);
        }
        if (edge2_sequence) {
            *edge2_sequence = std::move(edge_order);
        }
    }

    /// @brief Создает обратный индекс вершин по циклам (блокам)
    /// @param cycles Вектор циклов (подграфов)
    /// @return Отображение индекса вершины на множество индексов циклов, содержащих эту вершину
    static std::unordered_map<size_t, std::unordered_set<size_t>> get_block_vertex_inverted_index(
        const std::vector<subgraph_t>& cycles)
    {
        std::unordered_map<size_t, std::unordered_set<size_t>> cycle_inverted_index; // обратный индекс цикла по вершинам
        for (size_t cycle_index = 0; cycle_index < cycles.size(); ++cycle_index) {
            const std::unordered_set<size_t>& cycle_vertices = cycles[cycle_index].vertices;
            for (auto cycle_vertex : cycle_vertices)
            {
                cycle_inverted_index[cycle_vertex].insert(cycle_index);
            }
        }
        return cycle_inverted_index;
    }

    /// @brief Вычисляет матрицу вложенности циклов
    /// @details Для любого цикла определяется отношение вложенности со всеми остальными циклами.
    /// Циклы считаются вложенными, если они имеют хотя бы две общих вершины (по сути, одно ребро).
    /// Матрица симметрическая - отношение вложенности определяется без учета того, кто в кого вложен.
    /// @param cycles Вектор циклов (подграфов)
    /// @return Вектор множеств индексов вложенных циклов для каждого цикла
    std::vector<std::unordered_set<size_t>> cycle_nesting_matrix(const std::vector<subgraph_t>& cycles) const
    {
        // Циклы считаются вложенными, если они имеют хотя бы две общих вершины (по сути, одно ребро)

        // обратный индекс цикла по вершинам
        std::unordered_map<size_t, std::unordered_set<size_t>> vertex2cycle = get_block_vertex_inverted_index(cycles);
        std::vector<std::unordered_set<size_t>> nesting_matrix(cycles.size());

        for (size_t cycle_index = 0; cycle_index < cycles.size(); ++cycle_index) {
            // для всех циклов найдем количество вершин, содержащихся и в данном цикле
            // ключ - цикл
            // значение - количество вершин цикла-ключа в текущем цикле
            std::unordered_map<size_t, size_t> cycle_counter;

            // пройдем по всем вершинам данного цикла и посмотрим, в какие циклы она еще входит
            // просуммируем общее количество вхождений вершин из cycle во все другие циклы 
            const std::unordered_set<size_t>& cycle_vertices = cycles[cycle_index].vertices;
            for (size_t cycle_vertex : cycle_vertices)
            {
                const std::unordered_set<size_t>& cycles_with_this_vertex = vertex2cycle.at(cycle_vertex);
                for (size_t cycle_for_vertex : cycles_with_this_vertex)
                {
                    cycle_counter[cycle_for_vertex]++;
                }
            }

            for (const auto& cycle_repetition : cycle_counter)
            {
                size_t other_cycle_index = cycle_repetition.first;
                size_t common_vertices_with_other_cycle = cycle_repetition.second;
                if (other_cycle_index == cycle_index)
                    continue;
                if (common_vertices_with_other_cycle >= 2)
                    nesting_matrix[cycle_index].insert(other_cycle_index);
            }
        }
        return nesting_matrix;
    }

    /// @brief Находит цепочки вложенных циклов на основе матрицы вложенности
    /// @details Использует обход в ширину (BFS) для группировки связанных циклов в цепочки
    /// @param nesting_matrix Матрица вложенности циклов
    /// @return Вектор списков индексов циклов, образующих цепочки вложенности
    static std::vector<std::list<size_t>> get_nested_chains(const std::vector<std::unordered_set<size_t>>& nesting_matrix) {
        using namespace std;
        std::vector<std::list<size_t>> cycles;
        size_t cycle_count = nesting_matrix.size();
        std::vector<bool> visited_cycles(cycle_count, false);
        for (size_t cycle = 0; cycle < cycle_count; ++cycle) {
            if (visited_cycles[cycle])
                continue;
            list<size_t> chain;
            deque<size_t> bfs_queue;
            bfs_queue.push_back(cycle);
            while (!bfs_queue.empty()) {
                size_t current_cycle = bfs_queue.front();
                bfs_queue.pop_front();
                if (visited_cycles[current_cycle])
                    continue;

                visited_cycles[current_cycle] = true;
                chain.push_back(current_cycle);
                for (unordered_set<size_t>::const_iterator next_cycle = nesting_matrix[current_cycle].begin();
                    next_cycle != nesting_matrix[current_cycle].end(); ++next_cycle)
                {
                    if (!visited_cycles[*next_cycle])
                        bfs_queue.push_back(*next_cycle);
                }
            }
            cycles.push_back(chain);
        }
        return cycles;
    }

    /// @brief Внутренняя функция для поиска циклов в графе
    /// @details Рекурсивно обходит граф, используя поиск в глубину, и находит все циклы
    /// @param _visited_vertices Указатель на вектор флагов посещенных вершин
    /// @param _visited_edges2 Указатель на вектор флагов посещенных ребер типа 2
    /// @param vertex_path Указатель на список вершин текущего пути
    /// @param edge_path Указатель на список ребер текущего пути
    /// @param cycles Указатель на вектор найденных циклов (подграфов)
    void _find_cycles(
        std::vector<bool>* _visited_vertices, std::vector<bool>* _visited_edges2,
        std::list<size_t>* vertex_path, std::list<size_t>* edge_path,
        std::vector<subgraph_t>* cycles) const
    {
        using namespace std;
        vector<bool>& visited_vertices = *_visited_vertices;
        vector<bool>& visited_edges2 = *_visited_edges2;

        size_t vertex = vertex_path->back();
        visited_vertices[vertex] = true;

        const auto& incidences_for_vertex = get_vertex_incidences(vertex);

        auto incident_edges = incidences_for_vertex.second.all_edges();

        for (size_t edge : incident_edges)
        {
            if (visited_edges2[edge])
                continue;
            visited_edges2[edge] = true;

            size_t next_vertex = edges2[edge].get_other_side_vertex(vertex);
            if (visited_vertices[next_vertex]) {
                // цикл
                subgraph_t cycle;
                //cycle.vertex_path.push_back(next_vertex);
                cycle.edges2.push_back(edge);

                list<size_t>::reverse_iterator edge_it = edge_path->rbegin();
                list<size_t>::reverse_iterator vertex_it = vertex_path->rbegin();

                for (; *vertex_it != next_vertex; ++vertex_it, ++edge_it) {
                    cycle.edges2.push_back(*edge_it);
                    //cycle.vertex_path.push_back(*vertex_it);
                }
                //cycle.vertices = unordered_set<size_t>(cycle.vertex_path.begin(), cycle.vertex_path.end());
                cycles->push_back(cycle);
            }
            else {
                // не цикл
                vertex_path->push_back(next_vertex);
                edge_path->push_back(edge);
                _find_cycles(_visited_vertices, _visited_edges2, vertex_path, edge_path, cycles);
                vertex_path->pop_back();
                edge_path->pop_back();
            }
        }
    }

    /// @brief Ищет все циклы в графе, считая его двунаправленным
    /// @param create_boundary_edges Флаг создания граничных ребер для найденных циклов
    /// @return Вектор подграфов, представляющих найденные циклы
    std::vector<subgraph_t> find_cycles(bool create_boundary_edges = true) const
    {
        using namespace std;
        vector<subgraph_t> cycles;
        if (edges2.empty())
            return cycles;

        vector<bool> visited_vertices(get_vertex_count(), false);
        vector<bool> visited_edges2(edges2.size(), false);

        for (size_t vertex = 0; vertex < visited_vertices.size(); ++vertex) {
            if (visited_vertices[vertex] == false) {
                list<size_t> vertex_path, edge_path;
                vertex_path.push_back(vertex); // можно начать с любой вершины, в данном случае - с нулевой (доказать!)
                _find_cycles(&visited_vertices, &visited_edges2, &vertex_path, &edge_path, &cycles);
            }
        }

        if (create_boundary_edges) {
            for (auto& cycle : cycles) {
                cycle.create_boundary_edges(*this);
            }
        }

        return cycles;
    }




    /// @brief Находит двусвязные компоненты графа
    /// @details Двусвязные компоненты получаются путем объединения вложенных циклов в цепочки
    /// @param create_boundary_edges Флаг создания граничных ребер для найденных компонент
    /// @return Вектор подграфов, представляющих двусвязные компоненты
    std::vector<subgraph_t> get_biconnected_components(bool create_boundary_edges = true) const
    {
        std::vector<subgraph_t> cycles = find_cycles(false/* не конструируем граничные ребра*/);

        std::vector<std::unordered_set<size_t>> nesting_matrix = cycle_nesting_matrix(cycles);
        std::vector<std::list<size_t>> nested_chains = get_nested_chains(nesting_matrix);

        std::vector<subgraph_t> merged_cycles(nested_chains.size());

        for (size_t merged_cycle = 0; merged_cycle < merged_cycles.size(); ++merged_cycle)
        {
            std::unordered_set<size_t> merged_cycle_vertices;
            // ребра1 по соглашению к циклам не относятся, игнорируем их
            // ребра2 по определению смежных циклов могут повторяться, поэтому std::unordered_set
            std::unordered_set<size_t> merged_cycle_edges2;

            const std::list<size_t>& chain = nested_chains[merged_cycle];
            for (size_t cycle_index : chain)
            {
                merged_cycle_edges2.insert(cycles[cycle_index].edges2.begin(), cycles[cycle_index].edges2.end());
                merged_cycle_vertices.insert(cycles[cycle_index].vertices.begin(), cycles[cycle_index].vertices.end());
            }

            merged_cycles[merged_cycle].edges2 = std::vector<size_t>(merged_cycle_edges2.begin(), merged_cycle_edges2.end());
            merged_cycles[merged_cycle].vertices = merged_cycle_vertices;
        }

        if (create_boundary_edges) {
            for (auto& cycle : merged_cycles) {
                cycle.create_boundary_edges(*this);
            }
        }
        return merged_cycles;
    }

};

/// @brief Обобщенная функции ориентирования ребер графа на основании расходов. 
/// Если расход через ребро оказался отрицательным, направление ребра инвертируется
/// @tparam ModelIncidences Тип ребра графа
/// @tparam GetFlow1 Тип функционального объекта для получения расхода через одностороннее ребро
/// @tparam GetFlow2 Тип функционального объекта для получения расхода через двустороннее ребро
/// @param graph Граф
/// @param get_flow1 Функциональный объект для получения расхода через одностороннее ребро
/// @param get_flow2 Функциональный объект для получения расхода через двустороннее ребро
/// @return Граф с переориентированными ребрами
template <typename ModelIncidences, typename GetFlow1, typename GetFlow2>
inline basic_graph_t<ModelIncidences> get_flow_relative_graph(
    const basic_graph_t<ModelIncidences>& graph,
    GetFlow1 get_flow1, GetFlow2 get_flow2)
{
    basic_graph_t<ModelIncidences> result;

    const auto& edges1 = graph.edges1;
    const auto& edges2 = graph.edges2;

    for (size_t edge1_index = 0; edge1_index < edges1.size(); ++edge1_index) {
        double flow = get_flow1(edge1_index);
        ModelIncidences edge1 = edges1[edge1_index];
        if (flow < 0)
            std::swap(edge1.from, edge1.to);
        result.edges1.push_back(edge1);
    }

    for (size_t edge2_index = 0; edge2_index < edges2.size(); ++edge2_index) {
        double flow = get_flow2(edge2_index);
        ModelIncidences edge2 = edges2[edge2_index];
        if (flow < 0)
            std::swap(edge2.from, edge2.to);
        result.edges2.push_back(edge2);
    }

    return result;
}
/// @brief Ориентирование ребер графа на основании расходов, заданных в виде векторов
/// @tparam ModelIncidences Тип ребра графа
/// @param graph Граф
/// @param flows1 Расходы через односторонние ребра
/// @param flows2 Расходы через двусторонние ребра
/// @return Граф с переориентированными ребрами
template <typename ModelIncidences>
inline basic_graph_t<ModelIncidences> get_flow_relative_graph(
    const basic_graph_t<ModelIncidences>& graph,
    const std::vector<double>& flows1, const std::vector<double>& flows2)
{
    return get_flow_relative_graph(graph,
        [&](size_t index) { return flows1[index]; },
        [&](size_t index) { return flows2[index]; }
    );
}


template<typename ModelIncidences>
inline const std::vector<size_t> subgraph_t::get_boundary_vertices(const basic_graph_t<ModelIncidences>& graph) const
{
    create_vertices_by_edges(graph);

    std::vector<size_t> _boundary_vertices; // перенести в мемоизацию?
    if (_boundary_vertices.empty()) {
        std::unordered_set<size_t> result;
        for (size_t edge1 : boundary_edges1)
        {
            result.insert(graph.edges1[edge1].get_vertex_for_single_sided_edge());
        }

        for (size_t edge2 : boundary_edges2)
        {
            if (vertices.find(graph.edges2[edge2].from) != vertices.end()) {
                result.insert(graph.edges2[edge2].from);
            }
            else {
                result.insert(graph.edges2[edge2].to);
            }
        }
        _boundary_vertices = std::vector<size_t>(result.begin(), result.end());
    }
    return _boundary_vertices;
}

template<typename ModelIncidences>
inline void subgraph_t::create_vertices_by_edges(const basic_graph_t<ModelIncidences>& graph) const
{
    if (!vertices.empty())
        return;

    vertices.clear();
    for (auto edge1 : edges1) {
        vertices.insert(graph.edges1[edge1].get_vertex_for_single_sided_edge());
    }
    for (auto edge2 : edges2) {
        vertices.insert(graph.edges2[edge2].from);
        vertices.insert(graph.edges2[edge2].to);
    }
}

template<typename ModelIncidences>
inline void subgraph_t::create_boundary_edges(const basic_graph_t<ModelIncidences>& graph)  const
{
    using namespace std;
    create_vertices_by_edges(graph);

    // Находит ребра, инцидентные ровно одной вершине подграфа
    // Такие ребра - граничные для данного подграфа
    boundary_edges1.clear();
    boundary_edges2.clear();

    for (size_t vertex : vertices) {
        const vertex_incidences_pair_t& incidences = graph.get_vertex_incidences(vertex);
        vector<size_t> incident_edges1 = incidences.first.all_edges();
        boundary_edges1.insert(boundary_edges1.end(), incident_edges1.begin(), incident_edges1.end());

        vector<size_t> incident_edges2 = incidences.second.all_edges();
        for (size_t edge2 : incident_edges2) {
            // Текущая вершина подграфа (vertex) принадлежит данном ребру
            // Если другая вершина ребра не принадлежит подграфу, значит это ребро граничное

            size_t other_vertex = graph.edges2[edge2].get_other_side_vertex(vertex);
            if (vertices.find(other_vertex) == vertices.end()) {
                // другая вершина не в подграфе, ребро граничное
                boundary_edges2.push_back(edge2);
            }
        }
    }
}

}