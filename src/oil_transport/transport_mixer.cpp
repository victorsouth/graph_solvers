#include "../oil_transport.h"

namespace oil_transport {
;


std::vector<pde_solvers::endogenous_values_t> get_edges_output_endogenous(
    const transport_graph_t& graph, 
    const std::vector<graphlib::graph_binding_t>& edges, 
    const std::vector<double>& flows)
{
    std::vector<pde_solvers::endogenous_values_t> endogenous_for_edges;
    for (size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
        oil_transport_model_t* model = graph.get_model_incidences(edges[edge_index]).model;
        endogenous_for_edges.push_back(model->get_endogenous_output(flows[edge_index]));
    }
    return endogenous_for_edges;
}


std::vector<double> flows_for_edges(const std::vector<graphlib::graph_binding_t>& inlet_edges, 
    const std::vector<double>& flows1, const std::vector<double>& flows2)
{
    std::vector<double> result;
    for (graphlib::graph_binding_t edge : inlet_edges)
    {
        if (edge.binding_type == graphlib::graph_binding_type::Edge1)
        {
            result.push_back(flows1[edge.edge]);
        }
        else
        {
            result.push_back(flows2[edge.edge]);
        }
    }
    return result;
}






void mixing_handler_t::propagate_mixture(double time_step, const transport_graph_t& oriented_graph, size_t vertex_index, 
    const std::vector<double>& flows1, const std::vector<double>& flows2, 
    const std::unordered_map<graphlib::graph_binding_t, std::vector<graph_quantity_value_t>, graphlib::graph_binding_t::hash>& measurements_by_object, 
    pde_solvers::endogenous_values_t& vertex_output_endogenous, std::vector<graph_quantity_value_t>* overwritten)
{
    std::vector<graphlib::graph_binding_t> inlet_edges = oriented_graph.get_vertex_incidences(vertex_index).get_inlet_bindings();
    std::vector<graphlib::graph_binding_t> outlet_edges = oriented_graph.get_vertex_incidences(vertex_index).get_outlet_bindings();
    std::vector<double> outlet_flows = flows_for_edges(outlet_edges, flows1, flows2);
    for (size_t edge_index = 0; edge_index < outlet_edges.size(); ++edge_index) {
        double volumetric_flow = outlet_flows[edge_index];
        const graphlib::graph_binding_t& outlet_edge = outlet_edges[edge_index];
        std::vector<graph_quantity_value_t> edge_measurements
            = graphlib::endogenous_measurement_for_incidence<graph_quantity_value_t>(outlet_edge, measurements_by_object);

        oriented_graph.get_model_incidences(outlet_edge).model->propagate_endogenous(
            time_step, volumetric_flow, &vertex_output_endogenous, edge_measurements, overwritten);
    }
}

pde_solvers::endogenous_values_t transport_mixer_t::mix(const pde_solvers::endogenous_selector_t& endogenous_selector, 
    const std::vector<double>& inlet_volumetric_flows, const std::vector<pde_solvers::endogenous_values_t>& inlet_calculated_ednogenous, 
    const std::vector<graph_quantity_value_t>& vertex_endogenous_measurements, 
    std::vector<graph_quantity_value_t>* overwritten_by_measurements)
{
    // Эндогенные свойства для распространения в выходящие ребра                                                          
    pde_solvers::endogenous_values_t vertex_output_endogenous;

    if (endogenous_selector.density_std) {
        vertex_output_endogenous.density_std.value =
            weighted_sum(inlet_volumetric_flows, inlet_calculated_ednogenous,
                [](const pde_solvers::endogenous_values_t& oil) { return oil.density_std.value; });

        vertex_output_endogenous.density_std.confidence =
            logical_and(
                inlet_calculated_ednogenous,
                [](const pde_solvers::endogenous_values_t& oil) { return oil.density_std.confidence; }
            );
    }
    if (endogenous_selector.viscosity_working) {
        // ВАЖНО! Вязкость по формуле Кендалла-Монроэ  
        vertex_output_endogenous.viscosity_working.value =
            std::pow(
                weighted_sum(inlet_volumetric_flows, inlet_calculated_ednogenous,
                    [](const pde_solvers::endogenous_values_t& oil) { return std::cbrt(oil.viscosity_working.value); }),
                3);
        vertex_output_endogenous.viscosity_working.confidence =
            logical_and(
                inlet_calculated_ednogenous,
                [](const pde_solvers::endogenous_values_t& oil) { return oil.viscosity_working.confidence; }
            );
    }

    if (endogenous_selector.sulfur || endogenous_selector.temperature || endogenous_selector.improver ||
        endogenous_selector.viscosity0 || endogenous_selector.viscosity20 || endogenous_selector.viscosity50)
    {
        throw std::runtime_error("Endogenous parameter mixing is not implemented");
    }

    // Если есть замер на вершине, используем измерения эндогенных свойств вместо рассчитанных значений                                                           
    if (!vertex_endogenous_measurements.empty()) {
        overwrite_endogenous_values_from_measurements(vertex_endogenous_measurements,
            overwritten_by_measurements,
            &vertex_output_endogenous);
    }

    return vertex_output_endogenous;
}

}