#pragma once

namespace oil_transport {
;

/// @brief Возвращает эндогенные свойств с выходов нескольких ребер графа (полезно при расчете смешения)
/// @param graph Ориентированный по расходам граф
/// @param edges Перечень SISO ребер
/// @param flows Расходы через ребра из перечня
/// @return Эндогенные свойства
std::vector<pde_solvers::endogenous_values_t> get_edges_output_endogenous(
    const transport_graph_t& graph, 
    const std::vector<graphlib::graph_binding_t>& edges,
    const std::vector<double>& flows);

/// @brief Возвращает расходы для списка заданных ребер. Допускает смешанные списки ребер 1-го и 2-го типов
/// @param inlet_edges Ребра, для которых будут сформированы расходы
/// @param flows1 Расходы по односторонним ребрам
/// @param flows2 Расходы по двусторонним ребрам
/// @return Расходы по требуемым ребрам
std::vector<double> flows_for_edges(
    const std::vector<graphlib::graph_binding_t>& inlet_edges,
    const std::vector<double>& flows1,
    const std::vector<double>& flows2);




/// @brief Обработчик смешения - подготовливает исходные данные и распространяет результат смешения
class mixing_handler_t {
public:
    /// @brief Подготавливает данные для расчета смешения в вершине
    /// @param oriented_graph Ориентированный по расходам граф
    /// @param vertex_index Индекс рассматриваемой вершины
    /// @param flows1 Расходы по односторонним ребрам
    /// @param flows2 Расходы по двусторонним ребрам
    /// @param measurements_by_object Измерения эндогенных свойств, сопоставленные ребрам/вершинам графа
    /// @return (Расходы входящих ребер, 
    ///         Эндогенные свойств входящих ребер, 
    ///         Измерения эндогенных свойств на вершине смешения)
    static std::tuple<
        std::vector<double>, 
        std::vector<pde_solvers::endogenous_values_t>, 
        std::vector<graph_quantity_value_t>
    > prepare_data_for_mixing(const transport_graph_t& oriented_graph, size_t vertex_index,
            const std::vector<double>& flows1, const std::vector<double>& flows2,
        const std::unordered_map<graphlib::graph_binding_t, std::vector<graph_quantity_value_t>, graphlib::graph_binding_t::hash>& measurements_by_object)
    {
        std::vector<graphlib::graph_binding_t> inlet_edges = oriented_graph.get_vertex_incidences(vertex_index).get_inlet_bindings();
        std::vector<double> input_flows = flows_for_edges(inlet_edges, flows1, flows2);
        std::vector<pde_solvers::endogenous_values_t> input_endogenous = get_edges_output_endogenous(oriented_graph, inlet_edges, input_flows);
        std::vector<graph_quantity_value_t> vertex_measurements =
            graphlib::endogenous_measurement_for_incidence<graph_quantity_value_t>(
                graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, vertex_index), measurements_by_object);

        return std::make_tuple(input_flows, input_endogenous, vertex_measurements);
    }


    static std::tuple<std::vector<double>, std::vector<pde_solvers::endogenous_values_t>>
    /// @brief Подготавливает данные для расчета смешивания в блоке
    /// @param block_tree Дерево блоков транспортного графа
    /// @param block_index Индекс блока
    /// @param flows1 Расходы через односторонние ребра
    /// @param cut_flows Расходы в точках сочленения для каждого блока
    /// @param vertex_endogenous Эндогенные значения в вершинах
    /// @return Расходы и эндогенные свойства по входным коннекторам блока
        prepare_data_for_mixing_in_block(
            const transport_block_tree_t& block_tree,
            std::size_t block_index,
            const std::vector<double>& flows1,
            const std::vector<std::vector<double>>& cut_flows,
            const std::unordered_map<size_t, pde_solvers::endogenous_values_t>& vertex_endogenous)
    {
        std::vector<double> input_flows;
        std::vector<pde_solvers::endogenous_values_t> input_endogenous;

        const graphlib::block_connectors_t& block_connectors = block_tree.get_block_connectors()[block_index];

        const graphlib::biconnected_component_t& block = block_tree.blocks[block_index];

        const std::vector<size_t>& vertices_of_incident_block = block.cut_vertices;

        // Входы блока
        for (size_t cut_vertex : block_connectors.inlets) {

            // Индекс точки сочленения в блоке для чтения расхода
            size_t local_index_of_vertex = block.get_local_cut_vertex_index(cut_vertex);

            double flow = cut_flows[block_index][local_index_of_vertex];
            const pde_solvers::endogenous_values_t& endogenous = vertex_endogenous.at(cut_vertex);
            input_endogenous.emplace_back(endogenous);
            input_flows.emplace_back(flow);
        }

        return std::make_tuple(input_flows, input_endogenous);
    }

    static std::tuple<std::vector<double>, std::vector<pde_solvers::endogenous_values_t>>
    /// @brief Подготавливает данные для расчета смешивания в вершине
    /// @param block_tree Дерево блоков транспортного графа
    /// @param vertex Индекс вершины
    /// @param flows1 Расходы через односторонние ребра
    /// @param cut_flows Расходы в точках сочленения для каждого блока
    /// @param cut_endogenous Эндогенные значения в точках сочленения для каждого блока
    /// @return Расходы и эндогенные свойства для ребер, входящих в вершину
        prepare_data_for_mixing_in_vertex(
            const transport_block_tree_t& block_tree,
            size_t vertex,
            const std::vector<double>& flows1,
            const std::vector<std::vector<double>>& cut_flows,
            const std::vector<std::vector<pde_solvers::endogenous_values_t>>& cut_endogenous)
    {
        std::vector<double> input_flows;
        std::vector<pde_solvers::endogenous_values_t> input_endogenous;

        const graphlib::cut_vertex_incidences_t& inlets = 
            block_tree.get_origented_cut_vertices(*block_tree.graph).at(vertex).inlets;

        // Поставщики, входящие в вершину
        for (size_t edge1 : inlets.edges1) {
            double flow = flows1[edge1];
            oil_transport_model_t* model = block_tree.graph->get_model_incidences(
                graphlib::graph_binding_t(graphlib::graph_binding_type::Edge1, edge1)).model;
            input_endogenous.emplace_back(model->get_endogenous_output(flow));
            input_flows.emplace_back(flow);
        }

        // Коннекторы блоков, входящие в вершину
        for (size_t block : inlets.blocks)
        {
            const std::vector<size_t>& vertices_of_incident_block = block_tree.blocks[block].cut_vertices;

            size_t local_index_of_vertex = std::distance(
                vertices_of_incident_block.begin(),
                std::find(vertices_of_incident_block.begin(), vertices_of_incident_block.end(), vertex));

            double flow = cut_flows[block][local_index_of_vertex];
            const pde_solvers::endogenous_values_t& endogenous = cut_endogenous[block][local_index_of_vertex];
            input_endogenous.emplace_back(endogenous);
            input_flows.emplace_back(flow);

        }

        return std::make_tuple(input_flows, input_endogenous);

    }




    /// @brief Распространяет эндогенные свойства смеси по ребрам, выходящим из вершины
    /// @param oriented_graph Ориентированный по расходам граф
    /// @param vertex_index Индекс рассматриваемой вершины
    /// @param flows1 Расходы по односторонним ребрам
    /// @param flows2 Расходы по двусторонним ребрам
    /// @param measurements_by_object Измерения эндогенных свойств, сопоставленные ребрам/вершинам графа
    /// @param vertex_output_endogenous Эндогенные свойства смеси
    /// @param time_step Временной шаг
    static void propagate_mixture(
        double time_step,
        const transport_graph_t& oriented_graph, size_t vertex_index,
        const std::vector<double>& flows1, const std::vector<double>& flows2,
        const std::unordered_map<graphlib::graph_binding_t, std::vector<graph_quantity_value_t>, graphlib::graph_binding_t::hash>& measurements_by_object,
        pde_solvers::endogenous_values_t& vertex_output_endogenous,
        std::vector<graph_quantity_value_t>* overwritten
    );
};


inline void fix_small_amounts(std::vector<double>& amounts) {
    for (double& amount : amounts) {
        if (fabs(amount) < 1e-8)
            amount += fixed_solvers::pseudo_sgn(amount) * 1e-8;
    }
}

template <typename AdditiveType>
inline AdditiveType weighted_sum(std::vector<double> weights, const std::vector<AdditiveType>& elements)
{
    if (elements.size() == 1)
        return elements[0];

    if (elements.empty())
        throw std::logic_error("no elements specified");

    double weight_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (weight_sum < 1e-8) {
        // если общая сумма ненулевая, то проблем в виде деления на ноль не будет
        fix_small_amounts(weights);
        weight_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    }

    AdditiveType result = elements[0] * (weights[0] / weight_sum);
    for (size_t index = 1; index < elements.size(); ++index) {
        AdditiveType element = elements[index] * (weights[index] / weight_sum);
        result += element;
    }

    return result;
}


/// @brief Возвращает взвешенную сумму для элементов сложного типа ComplexType,
/// для которого определен геттер Getter, возвращающий аддитивный тип
/// Для элемента должны быть определены операции суммы и умножения на скаляр
template <typename Getter, typename ComplexType>
inline auto weighted_sum(const std::vector<double>& weights,
    const std::vector<ComplexType>& containter,
    const Getter& getter)
    -> decltype(getter(containter.front()))
{
    typedef decltype(getter(containter.front())) GetterResultType;

    if (containter.size() == 1)
        return getter(containter[0]);

    std::vector<GetterResultType> elements(containter.size());
    transform(containter.begin(), containter.end(), elements.begin(), getter);

    return weighted_sum(weights, elements);
}

template <typename Getter, typename ComplexType>
inline bool logical_and(const std::vector<ComplexType>& container, const Getter& getter)
{
    for (const ComplexType& element : container) {
        if (getter(element) == false) {
            return false;
        }
    }
    return true;
}

/// @brief Расчет смешения
class transport_mixer_t : public mixing_handler_t {
public:
    /// @brief Расчет результата смешения в вершине. 
    /// Если есть измерение на вершине, измерение используется в качестве результата расчета. 
    /// Иначе выполняется расчет на основе расходов и свойств входящих потоков
    /// @param inlet_volumetric_flows Расходы по входящим ребрам
    /// @param inlet_calculated_ednogenous Эндогенные свойства по входящим ребрам
    /// @param vertex_endogenous_measurements Измерения на вершине
    /// @return Расчитанные эндогенные свойства в формате распространия между ребрами графа
    static pde_solvers::endogenous_values_t mix(
        const pde_solvers::endogenous_selector_t& endogenous_selector,
        const std::vector<double>& inlet_volumetric_flows,
        const std::vector<pde_solvers::endogenous_values_t>& inlet_calculated_ednogenous,
        const std::vector<graph_quantity_value_t>& vertex_endogenous_measurements,
        std::vector<graph_quantity_value_t>* overwritten_by_measurements
        );

};


}