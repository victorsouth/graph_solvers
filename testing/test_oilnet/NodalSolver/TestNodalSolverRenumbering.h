#include "test_oilnet.h"

/// @brief Класс для модульного тестирования перенумерации графа и переназначения привязок в `graph_exception`. 
class nodal_solver_renumbering_t_Test : public graphlib::nodal_solver_renumbering_t<graphlib::nodal_edge_t>
{
public:
    /// @brief Вызывает защищённый метод `modify_places` базового класса для перенумерации привязок в исключении графа.
    /// @param e Исключение графа, в котором требуется перенумеровать множество `places`.
    void test_modify_places(graphlib::graph_exception& e)
    {
        modify_places(e);
    }

    /// @brief Конструктор тестового решателя, выполняющего перенумерацию вершин и рёбер по заданному графу и настройкам.
    /// @param graph Исходный граф с моделями, подлежащий перенумерации.
    /// @param settings Настройки узлового солвера, используемые внутренним `nodal_solver_renumbering_t`.
    nodal_solver_renumbering_t_Test(const graphlib::basic_graph_t<graphlib::nodal_edge_t>& graph,
        const graphlib::nodal_solver_settings_t& settings)
        : graphlib::nodal_solver_renumbering_t<graphlib::nodal_edge_t>(graph, settings)
    {
    }
};

/// @brief Проверка способности работы, когда граф изначально задан с притоками давления в конце списка
TEST(GraphRenumbering, HandlesNoRenumbering)
{
    //Тест 1. Подается изначально правильный граф. Ничего не должно меняться.
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);

    // Act
    auto settings = nodal_solver_settings_t::default_values();
    nodal_solver_renumbering_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    // Assert
    const graphlib::basic_graph_t<nodal_edge_t>& renumbered_graph = solver.get_renumbered_graph();
    const std::vector<std::size_t>& new_old_vertex = solver.get_new_vertex_to_old();
    const std::vector<std::size_t>& new_old_edge = solver.get_new_edge1_to_old();

    // Проверка вектора перехода новых вершин к старым (нумерация не должна поменяться, т.е. new_old_vertex[i] = i)
    for (size_t i = 0; i < new_old_vertex.size(); ++i) {
        ASSERT_EQ(i, new_old_vertex[i]);
    }

    // Проверка вектора перехода новых ребер к старым (нумерация не должна поменяться, т.е. new_old_edge[i] = i)
    for (size_t i = 0; i < new_old_edge.size(); ++i) {
        ASSERT_EQ(i, new_old_edge[i]);
    }

    // Проверка неизменности элементов графа
    for (size_t i = 0; i < renumbered_graph.edges1.size(); ++i) {
        ASSERT_EQ(scheme.graph.edges1[i].get_vertex_for_single_sided_edge(),
            renumbered_graph.edges1[i].get_vertex_for_single_sided_edge());
    }

    for (size_t i = 0; i < renumbered_graph.edges2.size(); ++i) {
        ASSERT_EQ(scheme.graph.edges2[i].from, renumbered_graph.edges2[i].from);
        ASSERT_EQ(scheme.graph.edges2[i].to, renumbered_graph.edges2[i].to);
    }

}

/// @brief проверка работы, когда подается граф с неправильной нумерацией вершин (ребра в порядке)
TEST(GraphRenumbering, RenumbersNodes)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties, false);

    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(2), &scheme.src_model);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(0), &scheme.sink_model);

    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(2, 1), &scheme.resist1_model);
    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 0), &scheme.resist2_model);

    // Act
    auto settings = nodal_solver_settings_t::default_values();
    nodal_solver_renumbering_t<graphlib::nodal_edge_t> solver1(scheme.graph, settings);

    // Assert
    const graphlib::basic_graph_t<nodal_edge_t>& renumbered_graph1 = solver1.get_renumbered_graph();
    const std::vector<std::size_t>& new_old_vertex1 = solver1.get_new_vertex_to_old();
    const std::vector<std::size_t>& new_old_edge1 = solver1.get_new_edge1_to_old();

    // Проверка вектора перехода новых вершин к старым
    ASSERT_EQ(1, new_old_vertex1[0]);
    ASSERT_EQ(2, new_old_vertex1[1]);
    ASSERT_EQ(0, new_old_vertex1[2]);

    // Проверка вектора перехода новых ребер к старым
    for (size_t i = 0; i < new_old_edge1.size(); ++i) {  // расположение ребер не меняется
        ASSERT_EQ(i, new_old_edge1[i]);
    }

    // Проверка изменений в элементах графа
    ASSERT_EQ(1, renumbered_graph1.edges2[0].from);
    ASSERT_EQ(0, renumbered_graph1.edges2[0].to);
    ASSERT_EQ(0, renumbered_graph1.edges2[1].from);
    ASSERT_EQ(2, renumbered_graph1.edges2[1].to);
}

/// @brief проверка работы, когда подается граф с неправильным расположением ребер (вершины в порядке)
TEST(GraphRenumbering, RenumbersEdges1)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties, false);

    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(2), &scheme.sink_model); //pressure
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(0), &scheme.src_model); // приток в узел 0: 1 м³/с 

    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(0, 1), &scheme.resist1_model); // ребро 0-1: труба
    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 2), &scheme.resist2_model); // ребро 1-2: расширение

    // Act
    auto settings = nodal_solver_settings_t::default_values();
    nodal_solver_renumbering_t<graphlib::nodal_edge_t> solver1(scheme.graph, settings);

    // Assert
    const graphlib::basic_graph_t<nodal_edge_t>& renumbered_graph1 = solver1.get_renumbered_graph();
    const std::vector<std::size_t>& new_old_vertex1 = solver1.get_new_vertex_to_old();
    const std::vector<std::size_t>& new_old_edge1 = solver1.get_new_edge1_to_old();

    // Проверка вектора перехода новых вершин к старым
    for (size_t i = 0; i < new_old_vertex1.size(); i++) {
        ASSERT_EQ(i, new_old_vertex1[i]);
    }

    // Проверка вектора перехода новых ребер к старым
    ASSERT_EQ(1, new_old_edge1[0]);
    ASSERT_EQ(0, new_old_edge1[1]);

    // Проверка изменений в элементах графа
    ASSERT_EQ(0, renumbered_graph1.edges2[0].from);
    ASSERT_EQ(1, renumbered_graph1.edges2[0].to);
    ASSERT_EQ(1, renumbered_graph1.edges2[1].from);
    ASSERT_EQ(2, renumbered_graph1.edges2[1].to);
}

/// @brief проверка работы, когда подается граф с неправильными нумерацией вершин и расположением ребер
TEST(GraphRenumbering, RenumbersNodesAndEdges1)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties, false);

    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(0), &scheme.sink_model);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(2), &scheme.src_model); // приток в узел 0: 1 м³/с 

    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(2, 1), &scheme.resist1_model); // ребро 0-1: труба
    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 0), &scheme.resist2_model); // ребро 1-2: расширение

    // Act
    auto settings = nodal_solver_settings_t::default_values();
    nodal_solver_renumbering_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    // Assert
    const graphlib::basic_graph_t<nodal_edge_t>& renumbered_graph2 = solver.get_renumbered_graph();
    const std::vector<std::size_t>& new_old_vertex2 = solver.get_new_vertex_to_old();
    const std::vector<std::size_t>& new_old_edge2 = solver.get_new_edge1_to_old();

    // Проверка вектора перехода новых вершин к старым
    ASSERT_EQ(1, new_old_vertex2[0]);
    ASSERT_EQ(2, new_old_vertex2[1]);
    ASSERT_EQ(0, new_old_vertex2[2]);

    // Проверка вектора перехода новых ребер к старым
    ASSERT_EQ(1, new_old_edge2[0]);
    ASSERT_EQ(0, new_old_edge2[1]);

    // Проверка изменений в элементах графа
    ASSERT_EQ(1, renumbered_graph2.edges2[0].from);
    ASSERT_EQ(0, renumbered_graph2.edges2[0].to);
    ASSERT_EQ(0, renumbered_graph2.edges2[1].from);
    ASSERT_EQ(2, renumbered_graph2.edges2[1].to);
}

/// @brief проверка работы перенумерации places в graph_exception
TEST(GraphRenumbering, MapsOldPlaces)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;
    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties, false);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(0), &scheme.sink_model);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(2), &scheme.src_model);

    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(2, 1), &scheme.resist1_model);
    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 0), &scheme.resist2_model);
    // создаю graph_exception со старыми номерами ребер и вершин
    graph_exception e(L"Test exception");
    e.places.insert(graph_binding_t(graph_binding_type::Vertex, 0)); // новая вершина 0
    e.places.insert(graph_binding_t(graph_binding_type::Edge1, 1));  // новое ребро 1
    e.places.insert(graph_binding_t(graph_binding_type::Edge2, 3));  // новое ребро 3 (такого ребра нет, но смысл - проверить неизменность)

    // Act
    auto settings = nodal_solver_settings_t::default_values();
    nodal_solver_renumbering_t_Test solver(scheme.graph, settings);
    solver.test_modify_places(e);

    // Assert
    for (const auto& place : e.places) {
        if (place.binding_type == graph_binding_type::Vertex) {
            // новая вершина 0 соответствует старой вершине 1
            ASSERT_TRUE(place.get_place_id() == 1);
        }
        if (place.binding_type == graph_binding_type::Edge1) {
            // новое ребро 1 соответствует старому ребру 0
            ASSERT_TRUE(place.get_place_id() == 0);
        }
        if (place.binding_type == graph_binding_type::Edge2) {
            // ребра Edge2 не перенумеровываются; новое ребро3 соответствует старому ребру 3
            ASSERT_EQ(place.get_place_id(), 3);
        }
    }
}

/// @brief Проверка корректного перенумерования результатов nodal_solver с сокращенного перенумерованного графа на исходный
TEST(GraphRenumbering, RenumbersResults)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();

    simple_hydraulic_scheme scheme(model_properties);
    auto settings = nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_renumbering_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    Eigen::VectorXd estimation(3);
    estimation << 1.17e5, 1.15e5, 1e5;

    // Act
    graphlib::flow_distribution_result_t result = solver.solve(estimation);

    // Assert
    ASSERT_EQ(scheme.graph.edges1.size(), result.flows1.size())
        << "Рассчитанные расходы на притоках не соответствуют полному графу";

    constexpr size_t known_pressure_vertex_index = 2;
    ASSERT_EQ(result.pressures[known_pressure_vertex_index], scheme.sink_model.get_known_pressure())
        << "Давление для P-притока не равно заданному";

    constexpr size_t known_flow_edge1_index = 0;
    ASSERT_EQ(result.flows1[known_flow_edge1_index], scheme.src_model.get_known_flow())
        << "Расход для Q-притока не равен заданному";

    auto is_zero = [](double x) { return std::abs(x) <= 1e-9; };
    bool has_undefined_flows1 = std::any_of(result.flows1.begin(), result.flows1.end(), is_zero);
    bool has_undefined_flows2 = std::any_of(result.flows2.begin(), result.flows2.end(), is_zero);

    ASSERT_FALSE(has_undefined_flows1 || has_undefined_flows2)
        << "Присутствуют ребра, расходы по котором потеряны при обратной перенумеровке";
}


