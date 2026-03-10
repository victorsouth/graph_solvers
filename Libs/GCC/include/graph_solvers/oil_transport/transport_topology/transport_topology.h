#pragma once

namespace oil_transport {
;

/// @brief Реализует декомпозицию графа на автономные контура с последующей проточной декомпозцией.
/// В результаты все разбито на подграфы, про каждый подграф известно, проточный он или непроточный
/// @param graph Исходный граф всей схемы
/// @return 
/// подграфы после проточной декомпозиции;
/// индексы проточных подграфов;
/// непроточные подграфов
template <typename Graph>
inline std::tuple<
    graphlib::general_structured_graph_t<Graph>,
    std::vector<size_t>, 
    std::vector<size_t>>
split_graph(const Graph& graph)
{
    auto get_model_split_type = [&graph](size_t edge_index) {
        if constexpr (std::is_same<Graph, oil_transport::transport_graph_t>::value) {
            const oil_transport_model_t* model = graph.edges2[edge_index].model;
            return model->model_type();
        }
        else if constexpr (std::is_same<Graph, oil_transport::hydraulic_graph_t>::value) {
            const oil_transport::hydraulic_model_t* model = graph.edges2[edge_index].model;
            return model->model_type();
        }
        else {
            static_assert(std::is_same<Graph, void>::value == false,
                "split_graph: unsupported Graph type for get_model_split_type");
        }
    };


    // в транспортном расчете нет смысла головастиков относить к проточной части
    // для гидравлического графа головастики учитываются
    constexpr bool tadpole_is_zeroflow = (std::is_same<Graph, oil_transport::transport_graph_t>::value);


    if constexpr (std::is_same<Graph, oil_transport::transport_graph_t>::value) {
        auto result = graphlib::split_graph<
            Graph,
            graph_quantity_value_t,
            decltype(oil_transport::set_binding)
        >(graph, tadpole_is_zeroflow, get_model_split_type, is_zero_flow_transport_incidences);

        return result;
    }
    else if constexpr (std::is_same<Graph, oil_transport::hydraulic_graph_t>::value) {
        auto result = graphlib::split_graph<
            Graph,
            graph_quantity_value_t,
            decltype(oil_transport::set_binding)
        >(graph, tadpole_is_zeroflow, get_model_split_type, oil_transport::is_zero_flow_hydro_incidences);

        return result;
    }
    else {
        throw std::runtime_error("split_graph: unsupported Graph type for zero-flow checker");
    }
}



/// @brief Делает декомпозицию полного SISO-графа на подграфы и раскидывание измерений по полученным подграфам
/// @warning Измерения с непроточных подграфов перепривязываются на проточные, к которым они примыкают
/// Непроточные подграфы в текущей реализации (фев. 2025) больше не нужны, поэтому информацию про них не возвращаем
/// @return 
/// Декомпозированный SISO граф; 
/// Список индексов проточных частей;
/// Список индексов НЕпроточных частей;
/// Измерения расхода, привязанные к SISO-подграфам
/// Эндогенные измерения, привязанные к SISO-подграфам
template <typename Graph = transport_graph_t>
inline std::tuple<
    graphlib::general_structured_graph_t<Graph>,
    std::vector<size_t>,
    std::vector<size_t>,
    std::unordered_map<size_t, std::vector<graph_quantity_value_t>>,
    std::unordered_map<size_t, std::vector<graph_quantity_value_t>>
> prepare_flow_subgraphs_and_measurements(const Graph& graph,
    const std::vector<graph_quantity_value_t>& flow_measurements,
    const std::vector<graph_quantity_value_t>& endogenous_measurements,
    bool rebind_zeroflow_measurements
)
{
    auto [structured_graph, flow_enabled_subgraphs, zeroflow_subgraphs] = split_graph(graph);

    // Переносим замеры полного SISO-графа в подграфы
    std::unordered_map<std::size_t, std::vector<graph_quantity_value_t>> subgraph_flow_measurements =
        rebind_measurements_to_subgraphs(flow_measurements, structured_graph);
    std::unordered_map<std::size_t, std::vector<graph_quantity_value_t>> subgraph_endogenous_measurements =
        rebind_measurements_to_subgraphs(endogenous_measurements, structured_graph);


    if constexpr (std::is_same<Graph, transport_graph_t>::value) {
        if (rebind_zeroflow_measurements) {
            rebind_measurements_to_flow_subgraphs(structured_graph, flow_enabled_subgraphs, zeroflow_subgraphs,
                &subgraph_endogenous_measurements);
        }
    }

    return std::make_tuple(
        std::move(structured_graph), 
        std::move(flow_enabled_subgraphs), 
        std::move(zeroflow_subgraphs),
        std::move(subgraph_flow_measurements),
        std::move(subgraph_endogenous_measurements)
    );
}

/// @brief Делает стандартную декомпозицию графа на компонентны связности с учетом проточности
/// Но проверяет, что в графе оказался один проточный подграф
/// По сути специфическая обертка над prepare_flow_subgraphs_and_measurements
inline std::tuple<
    transport_graph_t,
    std::vector<graph_quantity_value_t>,
    std::vector<graph_quantity_value_t>
>
prepare_single_subgraph_with_measurements(const transport_graph_t& graph,
    const std::vector<graph_quantity_value_t>& flow_measurements,
    const std::vector<graph_quantity_value_t>& endogenous_measurements)
{
    auto [structured_graph, flow_enabled_subgraphs, zerflow_subgraphs, flow_subgraph_measurements, endogenous_subgraph_measurements] =
        prepare_flow_subgraphs_and_measurements(graph, flow_measurements, endogenous_measurements, true);

    if (flow_enabled_subgraphs.size() != 1)
        throw std::runtime_error("Graph must have exactly 1 flow subgraph");
    //if (subgraph_measurements.size() != 1)
    //    throw std::runtime_error("Graph must have exactly 1 flow subgraph");

    size_t the_only_flow_subgraph_index = flow_enabled_subgraphs.front();
    transport_graph_t& subgraph = structured_graph.subgraphs.at(the_only_flow_subgraph_index);
    std::vector<graph_quantity_value_t>& the_subgraph_flow_measurements = flow_subgraph_measurements.begin()->second;
    std::vector<graph_quantity_value_t>& the_subgraph_endogenous_measurements = endogenous_subgraph_measurements.begin()->second;

    return std::make_tuple(subgraph, the_subgraph_flow_measurements, the_subgraph_endogenous_measurements);
}


}