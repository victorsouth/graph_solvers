#pragma once


namespace graphlib {
;





/// @brief класс, в конструктор которого подается перенумерованный граф в зависимости от известных значений давления
/// Вершины имеют нумерацию с нуля подряд (0,1,2,3,...,m-1). Класс создается после докемпозиции исходного графа на автономные контуры
/// @tparam ModelIncidences Тип инцидентностей (должен быть model_incidences_t с полем model, указывающим на closing_relation_t*)
template<typename ModelIncidences>
class nodal_solver_renumbering_t {
public:
    /// @brief Тип настроек солвера
    using Settings = nodal_solver_settings_t;
private:
    /// @brief new_vertex_to_old[i] = j означает, что вершина i в новом графе соответствует вершине j в старом графе
    std::vector<std::size_t> new_vertex_to_old;
    /// @brief get[i] = j означает, что ребро i в новом графе соответствует ребру j в старом графе
    std::vector<std::size_t> new_edge1_to_old;
    /// @brief в nodal_solver_t передается ССЫЛКА. Поэтому необходимо хранить сам объект графа здесь. (здесь будет храниться перенумерованный граф)
    graphlib::basic_graph_t<ModelIncidences> graph_for_solver;
    /// @brief Солвер МД
    nodal_solver_t<ModelIncidences> solver;
private:
    /// @brief получение вектора перехода от новых вершин к старым
    /// @return Вектор, индексы которого соответсвуют новым номерам вершинам, значения соответствуют старым номерам вершин
    static std::vector<std::size_t> transition_vector_nodes(const graphlib::basic_graph_t<ModelIncidences>& graph) {

        std::size_t nodes_count = graph.get_vertex_count();

        // определить наличие/ остутствие значения давления у ВСЕХ вершин графа
        std::vector<bool> has_pressure(nodes_count);
        for (std::size_t i = 0; i < graph.edges1.size(); ++i) {
            const auto& edge = graph.edges1[i];
            std::size_t old_vertex = edge.get_vertex_for_single_sided_edge();
            has_pressure[old_vertex] = !std::isnan(edge.model->get_known_pressure());
        }

        // Индексы вершин (здесь в порядке)
        std::vector<std::size_t> new_vertex_to_old(nodes_count);
        // заполняем new_vertex_to_old последовательно от 0 до nodes_count - 1.  (new_vertex_to_old[i]=i)
        std::iota(new_vertex_to_old.begin(), new_vertex_to_old.end(), 0);

        // Используем stable, чтобы порядок вершин при перестановках по возможности сохранялся
        // Можно и просто std::partition
        // в результате stable_partition вершины без давления будут в начале, с давлением - в конце
        std::stable_partition(new_vertex_to_old.begin(), new_vertex_to_old.end(),
            [&](std::size_t vertex_index) {
                return has_pressure[vertex_index] == false;
            });

        return new_vertex_to_old;
    }

    /// @brief получение вектора перехода от новых ребер edges1 к старым
    /// @return Вектор, индексы которого соответсвуют новым индексам edges1, значения соответствуют старым индексам edges1
    static std::vector<std::size_t> transition_vector_edges1(const graphlib::basic_graph_t<ModelIncidences>& graph) {
        std::size_t edges1_count = graph.edges1.size();

        // определить наличие/ остутствие значения давления у вершин ребер edges1
        std::vector<bool> has_pressure(edges1_count);
        for (std::size_t i = 0; i < graph.edges1.size(); ++i) {
            const auto& edge = graph.edges1[i];
            has_pressure[i] = !std::isnan(edge.model->get_known_pressure());
        }

        // индексы ребер edges1 (здесь в порядке)
        std::vector<std::size_t> new_edges1_to_old(edges1_count);
        // заполняем new_edges1_to_old последовательно от 0 до edges1_count - 1.  (new_edges1_to_old[i]=i)
        std::iota(new_edges1_to_old.begin(), new_edges1_to_old.end(), 0);

        // Используем stable, чтобы порядок ребер edges1 при перестановках по возможности сохранялся
        // в результате stable_partition ребра без давления будут в начале, с давлением - в конце
        std::stable_partition(new_edges1_to_old.begin(), new_edges1_to_old.end(),
            [&](std::size_t edge_index) {
                return has_pressure[edge_index] == false;
            });

        return new_edges1_to_old;

    }

    /// @brief перенумерация вершин графа в соответствии с new_vertex_to_old
    /// @param new_vertex_to_old вектор перехода от новых вершин к старым
    /// @param graph 
    static void renumber_vertices(const std::vector<std::size_t>& new_vertex_to_old,
        graphlib::basic_graph_t<ModelIncidences>& graph)
    {
        // вектор перехода от старых вершин к новым
        std::vector<std::size_t> old_vertex_to_new;
        old_vertex_to_new.resize(new_vertex_to_old.size());
        for (std::size_t i = 0; i < new_vertex_to_old.size(); ++i) {
            old_vertex_to_new[new_vertex_to_old[i]] = i;
        }
        // перенумеруем ребра edges1
        for (std::size_t i = 0; i < graph.edges1.size(); ++i) {
            std::size_t& old_vertex = graph.edges1[i].get_vertex_for_single_sided_edge();
            std::size_t new_vertex = old_vertex_to_new[old_vertex];
            old_vertex = new_vertex;
        }

        // перенумеруем ребра edges2
        for (std::size_t i = 0; i < graph.edges2.size(); ++i) {
            graph.edges2[i].from = old_vertex_to_new[graph.edges2[i].from];
            graph.edges2[i].to = old_vertex_to_new[graph.edges2[i].to];
        }
    }

    /// @brief перенумерация элементов графа: номера узлов с давлением имеют больший номер, чем узлы без давления;
    /// висячие дуги с давлением в конце списка edges1. 
    /// @param graph 
    /// @return new_graph перенумерованный граф
    graphlib::basic_graph_t<ModelIncidences> renumber_graph(const graphlib::basic_graph_t<ModelIncidences>& graph)
    {
        new_vertex_to_old = transition_vector_nodes(graph);
        new_edge1_to_old = transition_vector_edges1(graph);

        graphlib::basic_graph_t<ModelIncidences> new_graph;
        for (std::size_t old_edge1_index : new_edge1_to_old) {
            new_graph.edges1.push_back(graph.edges1[old_edge1_index]);
        }
        new_graph.edges2 = graph.edges2;

        if (check_renumber_nodes(graph))
            renumber_vertices(new_vertex_to_old, new_graph);

        return new_graph;
    }


    /// @brief функция для проверки необходимости перенумерации вершин графа
    /// @param graph 
    /// @return возвращает True, если необходимо осуществить перенумерацию вершин графа
    bool check_renumber_nodes(const graphlib::basic_graph_t<ModelIncidences>& graph)
    {
        // проходимся по вектору перехода от новых вершин к старым
        // если new_vertex_to_old[i] != i хотя бы для одного i, то необходима перенумерация
        for (std::size_t i = 0; i < new_vertex_to_old.size(); ++i) {
            if (new_vertex_to_old[i] != i)
                return true;
        }
        return false;
    }

    /// @brief Переносит результаты решения узлового солвера из перенумерованного графа в исходную нумерацию и дополняет их заданными Q-притоками
    /// @param nodal_solver_results результаты расчет nodal_solver на сокращенном (без заданных P-притоков) перенумерованном подграфе 
    /// @return давления и расходы для полного графа с исходной нумерацией
    flow_distribution_result_t backward_renumbering(
            const flow_distribution_result_t& nodal_solver_results) const
    {       
        // Сопоставление индексов исходного графа и перенумерованного графа
        const std::vector<std::size_t>& new_edge1_to_old = get_new_edge1_to_old();
        const std::vector<std::size_t>& new_vertex_to_old = get_new_vertex_to_old();

        // Результаты на полном графе с исходной нумерацией 
        flow_distribution_result_t result;
        result.pressures = Eigen::VectorXd (graph_for_solver.get_vertex_count());
        result.flows1 = Eigen::VectorXd (graph_for_solver.edges1.size());
        result.flows2 = nodal_solver_results.flows2; // Нумерация ребер2 не изменяется

        // Перенос рассчитанных притоков из сокращенного перенумерованного графа в исходный
        for (size_t i = 0; i < graph_for_solver.edges1.size(); i++) {
            std::size_t old_index = new_edge1_to_old[i];
            result.flows1[old_index] = nodal_solver_results.flows1(i);
        }

        // Перенос давлений из перенумерованного графа на исходных
        for (size_t i = 0; i < graph_for_solver.get_vertex_count(); ++i) {
            std::size_t old_index = new_vertex_to_old[i];
            result.pressures[old_index] = nodal_solver_results.pressures(i);
        }
        return result;
    }

protected:
    /// @brief ф-ция модификации полей places в graph_exception по таблицам сопоставления new_vertex_to_old и new_edge1_to_old
    /// @param e 
    void modify_places(graphlib::graph_exception& e)
    {
        // т.к. places - это unordered_set, то нельзя изменять элементы на месте, необходимо создать новый набор привязок
        std::unordered_set<graphlib::graph_binding_t, graphlib::graph_binding_t::hash> new_places;
        for (auto& place : e.places) {
            if (place.binding_type == graphlib::graph_binding_type::Vertex) {
                // находим старый индекс вершины
                std::size_t old_index = new_vertex_to_old[place.get_place_id()];
                graphlib::graph_binding_t new_place(graphlib::graph_binding_type::Vertex, old_index);
                new_places.insert(new_place);
            }
            else if (place.binding_type == graphlib::graph_binding_type::Edge1) {
                // находим старый номер ребра edges1
                std::size_t old_index = new_edge1_to_old[place.get_place_id()];
                graphlib::graph_binding_t new_place(graphlib::graph_binding_type::Edge1, old_index);
                new_places.insert(new_place);
            }
            else if (place.binding_type == graphlib::graph_binding_type::Edge2) {
                // для рубер edges2 номера не менялись
                std::size_t old_index = place.get_place_id();
                graphlib::graph_binding_t new_place(graphlib::graph_binding_type::Edge2, old_index);
                new_places.insert(new_place);
            }
        }
        e.places = new_places;
    }
public:
    /// @brief Перенумеровывает вектор начальных приближений давления из исходной нумерации вершин в нумерацию перенумерованного графа.
    /// @param estimation_original_graph Вектор давлений для вершин без P-условий в исходной нумерации.
    /// @return Вектор давлений для соответствующих вершин в перенумерованном графе.
    Eigen::VectorXd renumber_estimation(const Eigen::VectorXd& estimation_original_graph) const {
        Eigen::VectorXd renumbered(graph_for_solver.get_vertex_count());

        for (size_t new_index = 0; new_index < graph_for_solver.get_vertex_count(); ++new_index) {
            auto old_index = static_cast<Eigen::Index>(new_vertex_to_old[new_index]);
            renumbered(new_index) = estimation_original_graph(old_index);
        }

        return renumbered;
    }
    /// @brief конструктор, перенумерующий граф и передающий его в nodal_solver_t
    /// @param graph 
    nodal_solver_renumbering_t(const graphlib::basic_graph_t<ModelIncidences>& graph, const nodal_solver_settings_t& settings)
        : graph_for_solver(renumber_graph(graph))
        , solver(graph_for_solver, settings)
    {
    }

    /// @brief вызов солвера
    /// @param initial_pressures Начальное приближение давлений на вершинах без заданного P (в исходной нумерации)
    /// @param solver_result (опционально) указатель для получения «сырого» результата численного метода
    /// @return Давления и расходы на исходном графе
    flow_distribution_result_t solve(const Eigen::VectorXd& initial_pressures,
                                     fixed_solver_result_t<-1>* solver_result = nullptr)
    {
        try {
            Eigen::VectorXd renumbered_initial_pressures;
            if (initial_pressures.size() != 0)
                renumbered_initial_pressures = renumber_estimation(initial_pressures);

            fixed_solver_result_t<-1> numerical_result_carrier;
            if (solver_result == nullptr) {
                solver_result = &numerical_result_carrier;
            }

            flow_distribution_result_t renumbered_result =
                solver.solve(renumbered_initial_pressures, solver_result);

            // При наличии результатов переносим их на исходный граф
            if (solver_result->result_code == numerical_result_code_t::Converged) {
                return backward_renumbering(renumbered_result);
            }

            return renumbered_result;

        }
        catch (graphlib::graph_exception& e) {
            // модифицируем places по таблице сопоставления new_vertex_to_old
            modify_places(e);
            throw;
        }
    }

    /// @brief геттер вектора перехода от новых вершин к старым (для тестов)
    /// @return вектора перехода от новых вершин к старым
    const std::vector<std::size_t>& get_new_vertex_to_old() const
    {
        return new_vertex_to_old;
    }

    /// @brief геттер вектора перехода от новых ребер edge1 к старым (для тестов)
    /// @return вектора перехода от новых ребер edge1 к старым
    const std::vector<std::size_t>& get_new_edge1_to_old() const
    {
        return new_edge1_to_old;
    }

    /// @brief геттер графа с новой нумерацией (для тестов)
    /// @return перенумерованный граф
    const graphlib::basic_graph_t<ModelIncidences>& get_renumbered_graph() const
    {
        return graph_for_solver;
    }

    /// @brief Возвращает количество вершин с заданным давлением
    size_t count_given_pressure_veritces() const {
        return solver.get_given_pressure_size();
    }

};

}
