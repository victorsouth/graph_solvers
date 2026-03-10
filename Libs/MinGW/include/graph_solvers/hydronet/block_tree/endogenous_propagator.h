#pragma once

namespace graphlib {
;

/// @tparam ModelIncidences Модель ребра и ее инцидентности
/// @tparam Model Модель ребра
template <typename ModelIncidences, typename PipeModel>
inline bool is_lumped_block(const biconnected_component_t& block, const basic_graph_t<ModelIncidences>& graph)
{
    for (size_t edge : block.edges2)
    {
        auto edge_binding = graph_binding_t(graph_binding_type::Edge2, edge);
        if (dynamic_cast<PipeModel*>(graph.get_model_incidences(edge_binding).model))
            return false;
    }
    return true;
}

inline bool is_fully_propagatable_block(const biconnected_component_t& block, 
    const std::vector<double>& flows2)
{
    for (size_t edge : block.edges2)
    {
        if (!std::isfinite(flows2[edge])) {
            return false;
        }
    }
    return true;
}


template <typename StructuredGraph, typename BoundMeasurement>
inline std::vector<BoundMeasurement> ascend_bindings(
    const StructuredGraph& structured_graph, size_t subgraph_id,
    const std::vector<BoundMeasurement>& completeness
)
{
    std::vector<BoundMeasurement> result = completeness;

    for (BoundMeasurement& measurement : result) {
        measurement.binding = structured_graph.ascend_map.at(
            subgraph_binding_t(subgraph_id, measurement.binding));
    }

    return result;
}

// Forward Declaration
struct transport_block_solver_settings;

/// @brief Распространение измерений эндогенных свойств по ориентированному графу блоков
/// @tparam Mixer Класс, реализующие статические методы prepare_data_for_mixing (формирование структур эндогенных свойств входящих потоков) 
/// и mix (расчет свойств смеси на основе сформированных структур)
/// @tparam EndogenousValue Структура - значение эндогенного свойства
/// @tparam ModelIncidences Инцидентности модели ребра
/// @tparam BoundMeasurement Структура - значение измерения и его привязка к дереву блоков
/// @tparam Model Модель ребра
/// @tparam PipeModel Модель трубы (распрострение эндогенных свойств по трубе-мосту обрабаывается особым образом)
template <typename ModelIncidences,
    typename PipeModel,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector>
class block_endogenous_propagator
{
protected:

    /// @brief Тип отраслеваой модели
    typedef typename ModelIncidences::model_type Model;

    /// @brief Транспортный граф
    const basic_graph_t<ModelIncidences>& graph;
    /// @brief Дерево блоков поверх транспортного графа
    const block_tree_t<ModelIncidences>& block_tree;
    /// @brief Расходы через точки сочленения в привязке к блокам
    const std::vector<std::vector<double>>& cut_flows;
    /// @brief Расходы через поставщики, потребители
    const std::vector<double>& flows1;
    /// @brief Расходы через двусторонние ребра
    const std::vector<double>& flows2;
    /// @brief Настройки расчета
    const transport_block_solver_settings& settings;
    /// @brief Измерения в привязке к элементам дерева блоков
    mutable std::unordered_map<
        block_tree_binding_t, 
        std::vector<BoundMeasurement>, 
        block_tree_binding_hash
    > measurements_by_tree_objects;
    /// @brief Измерения в привязке к элементам графа (для расчета нетривиальных блоков с известными расходами)
    std::unordered_map<
        graph_binding_t, 
        std::vector<BoundMeasurement>, 
        graph_binding_t::hash
    > measurements_by_graph_object;
    /// @brief Эндогенные свойства на коннекторах блоков
    mutable std::vector<std::vector<EndogenousValue>> cut_endogenous;
    /// @brief Эндогенные свойства на точках сочленения (без привязки к блокам)
    mutable std::unordered_map<size_t, EndogenousValue> vertex_endogenous;
    /// @brief Селектор эндогенных параметров для расчета
    EndogenousSelector selector;

    /// @brief Возвращает все измерения по данному объекту графа блоков
    inline const std::vector<BoundMeasurement>& get_endogenous_measurement_by_binding(
        const block_tree_binding_t& binding) const
    {
        static const std::vector<BoundMeasurement> empty_measurements;
        if (has_map_key(measurements_by_tree_objects, binding)) {
            return measurements_by_tree_objects.at(binding);
        }
        else {
            return empty_measurements;
        }
    }

    /// @brief Расчет продвижения эндогенных свойств по одностороннему ребру
    inline void edge1_endogenous_calc(
        const block_tree_binding_t& binding, double time_step,
        std::vector<BoundMeasurement>* overwritten_by_measurements
        ) 
    {
        size_t edge = binding.edge;
        bool is_source = graph.edges1[edge].has_vertex_to();

        EndogenousValue* endogenous_input = is_source
            ? nullptr
            : &vertex_endogenous.at(graph.edges1[edge].from); /* Не может быть эндогенных свойств с выхода предыдщуего ребра */

        Model model = graph.get_model_incidences(graph_binding_t(graph_binding_type::Edge1, edge)).model;

        auto block_edge1_measurements = get_endogenous_measurement_by_binding(binding);
        model->propagate_endogenous(time_step, flows1[edge], endogenous_input,
            block_edge1_measurements, overwritten_by_measurements);
    }

    /// @brief Расчет продвижения эндогенных свойству по ребру-мосту
    inline std::vector<EndogenousValue> bridge_endogenous_calc(
        double time_step,
        size_t block_index,
        const std::vector<BoundMeasurement>& block_measurements,
        std::vector<BoundMeasurement>* overwritten_by_measurements)
    {
        const biconnected_component_t& block = block_tree.blocks[block_index];
        const block_connectors_t& block_connectors = block_tree.get_block_connectors()[block_index];
        std::vector<EndogenousValue> block_cut_endogenous(block.cut_vertices.size());


        size_t input_vertex = block_connectors.inlets.front();
        EndogenousValue input_endogenous = vertex_endogenous.at(input_vertex);
        // Поток со свойствами "вытекает" из вершины графа блоков и тут же "втекает" во входной коннектор моста
        size_t local_input_vertex = block.get_local_cut_vertex_index(input_vertex);
        block_cut_endogenous[local_input_vertex] = input_endogenous;

        // Получаем модель ребра
        size_t edge = block.edges2.front();
        Model model = graph.get_model_incidences(
            graph_binding_t(graph_binding_type::Edge2, edge)).model;

        model->propagate_endogenous(
            time_step,
            cut_flows[block_index].front() /* Расходы на коннекторах блока положительны (уже провели переориентирование) и равны*/,
            &input_endogenous,
            block_measurements,
            overwritten_by_measurements);

        // На выход блока забираем эндогенные свойства с выхода ребра (в случае трубы свойства на входе и выходе различны)
        EndogenousValue output_endogenous = model->get_endogenous_output(cut_flows[block_index].front());

        size_t output_vertex = block_connectors.outlets.front();
        size_t local_output_vertex = block.get_local_cut_vertex_index(output_vertex);

        block_cut_endogenous[local_output_vertex] = output_endogenous;

        return block_cut_endogenous;
    }

    /// @brief Расчет блока с неизвестным расходами его ребер - считаем как вершину
    inline std::vector<EndogenousValue> block_as_vertex_endogenous_calc(
        size_t block_index,
        const std::vector<BoundMeasurement>& block_measurements,
        std::vector<BoundMeasurement>* overwritten_by_measurements) const
    {
        const biconnected_component_t& block = block_tree.blocks[block_index];
        const block_connectors_t& block_connectors = block_tree.get_block_connectors()[block_index];
        std::vector<EndogenousValue> block_cut_endogenous(block.cut_vertices.size());

        // Копируем из "точек сочленения" во "входные коннекторы"
        for (size_t input_vertex : block_connectors.inlets) {
            size_t local_input_vertex = block.get_local_cut_vertex_index(input_vertex);
            block_cut_endogenous[local_input_vertex] = vertex_endogenous.at(input_vertex);
        }

        auto [input_flows, input_endogenous] = Mixer::prepare_data_for_mixing_in_block(
            block_tree, block_index, flows1, cut_flows, vertex_endogenous);
        EndogenousValue calculated_endogenous = Mixer::mix(
            selector, input_flows, input_endogenous,
            block_measurements, overwritten_by_measurements);

        // Записываем свойства смеси в выходные коннекторы блока
        for (size_t cut_vertex : block_connectors.outlets) {
            size_t local_output_vertex = block.get_local_cut_vertex_index(cut_vertex);
            block_cut_endogenous[local_output_vertex] = calculated_endogenous;
        }
        return block_cut_endogenous;
    }

    /// @brief Переносит эндогенные измерения на точки сочленения блоков
    /// Эндогенные свойства не распространяются! 
    static std::unordered_map<
        block_tree_binding_t, 
        std::vector<BoundMeasurement>, 
        block_tree_binding_hash
    > bind_measurements_to_block_tree(
        const basic_graph_t<ModelIncidences>& graph,
        const block_tree_t<ModelIncidences>& block_tree,
        const std::vector<BoundMeasurement>& endogenous)
    {
        std::unordered_map<block_tree_binding_t, std::vector<BoundMeasurement>, block_tree_binding_hash> measurements_by_tree_objects;

        // Для быстрого поиска эндогенных измерений от объектов графа (ребрам и вершинам)
        std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash> measurements_by_object
            = get_endogenous_measurements_by_incidences(endogenous);

        const auto& cut_vertices_map = block_tree.get_cut_vertices(graph);

        for (const auto& [graph_binding, measurements] : measurements_by_object) {
            switch (graph_binding.binding_type) {
            case graph_binding_type::Edge2: {


                size_t edge2_index = graph_binding.edge;
                size_t block_index =
                    block_tree.edge2_to_block.at(edge2_index);
                const auto& block = block_tree.blocks[block_index];

                // Если двустороннее ребро - мост, то перепривязываем измерение к блоку
                if (block.edges2.size() == 1)
                {
                    // Измерение переносим на выходной коннектор блока
                    block_tree_binding_t binding(block_tree_binding_type::Block, block_index);
                    // Если уже есть измерения по такому binding, то к нему будут добавлены измерения с текущей вершины
                    measurements_by_tree_objects[binding].insert(measurements_by_tree_objects[binding].end(),
                        measurements.begin(), measurements.end());
                }

                // Если ребро - внутренее для нетривиального блока, эндогенное измерение использовать в общем случае нельзя
                // TODO: здесь должно начаться формирование useless эндогенных измерений на графе блоков               

                break;
            }

            case graph_binding_type::Vertex: {
                size_t vertex_index = graph_binding.vertex;
                block_tree_binding_t binding(block_tree_binding_type::Vertex, vertex_index);
                measurements_by_tree_objects[binding].insert(measurements_by_tree_objects[binding].end(),
                    measurements.begin(), measurements.end());
                break;
            }
            default: // С поставщика/потребителя переносим измерение на ближайшую вершину (которая всегда будет точкой сочленения)
            {
                block_tree_binding_t binding(block_tree_binding_type::Edge1, graph_binding.edge);

                measurements_by_tree_objects[binding].insert(
                    measurements_by_tree_objects[binding].end(),
                    measurements.begin(), measurements.end());

                // Перенос на ближайшую вершину. Отказались от этого
                /*size_t edge1_index = graph_binding.edge;
                size_t vertex_index = graph.edges1[edge1_index].has_vertex_from()
                    ? graph.edges1[edge1_index].vertex_from
                    : graph.edges1[edge1_index].vertex_to;
                block_tree_binding_t binding(block_tree_binding_type::Vertex, vertex_index);
                measurements_by_tree_objects[binding].insert(measurements_by_tree_objects[binding].end(),
                    measurements.begin(), measurements.end());*/
            }

            }
        }

        return measurements_by_tree_objects;
    }

    /// @brief Возвращает незаполненную структуру для эндогенных свойств на точках сочленения блоков
    /// @param block_tree Дерево блоков поверх транспортного графа
    /// @return Эндогенные свойства на точках сочленения блоков
    inline std::tuple<
        std::unordered_map<size_t, EndogenousValue>,
        std::vector<std::vector<EndogenousValue>>
    >
        prepare_endogenous_structs(const block_tree_t<ModelIncidences>& block_tree)
    {
        std::vector<std::vector<EndogenousValue>> cut_endogenous;
        std::unordered_map<size_t, EndogenousValue> vertex_endogenous;

        // Эндогенные свойства на точках сочленения относительно блоков
        cut_endogenous.reserve(block_tree.blocks.size());
        for (int i = 0; i < block_tree.blocks.size(); ++i) {
            std::vector<EndogenousValue> endogenous_on_block_cut_vertices(block_tree.blocks[i].cut_vertices.size());
            cut_endogenous.emplace_back(std::move(endogenous_on_block_cut_vertices));
        }

        // Эндогенные свойства на вершинах (для расчета смешения в вершине)
        const std::unordered_map<size_t, cut_vertex_incidences_t>& vertex_map = block_tree.get_cut_vertices(graph);
        vertex_endogenous.reserve(vertex_map.size());
        for (const auto& vertex_incidences_pair : vertex_map) {
            size_t vertex = vertex_incidences_pair.first;
            vertex_endogenous.emplace(vertex, EndogenousValue());
        }


        return std::make_tuple(std::move(vertex_endogenous), std::move(cut_endogenous));
    }

public:

    /// @brief Определяет эндогенные свойства на односторонних ребрах и на точках сочленения блока на основе измерений
    /// @param flows Измерения расхода 
    /// @return Эндогенные свойства на точках сочленения
    std::vector<BoundMeasurement> propagate_endogenous(double time_step)
    {
        // Порядок обхода дерева блоков
        std::vector<block_tree_binding_t> block_tree_topological_order = 
            topological_sort(graph, block_tree);

        std::vector<BoundMeasurement> overwritten_by_measurements;

        const std::unordered_map<size_t, cut_vertex_oriented_incidences_t>& incidences_map = 
            block_tree.get_origented_cut_vertices(graph);

        // Эндогенный расчет объектов графа блоков в топологическом порядке
        for (const block_tree_binding_t& binding : block_tree_topological_order) {

            switch (binding.binding_type) {

            // Односторонее висячее ребро - объект дерева блоков
            // Результат расчета остается в самом висячем ребре, это аналог cut_endogenous для висячих ребер
            // Результат потом подхватывается при расчете точки сочленения, см. Vertex, prepare_data_for_mixing_in_vertex
            case block_tree_binding_type::Edge1: {
                edge1_endogenous_calc(binding, time_step, &overwritten_by_measurements);
                break;
            }

            // Это точка сочленения блоков (хоть и название сбивает с толку). 
            // В нее попадаем, если все предшествующие блоки (и висячие односторонние ребра) полностью посчитаны
            case block_tree_binding_type::Vertex:  {
                // Cобираем эндогены со всех входящих коннекторов, смешиваем, приписываем вершине
                std::size_t vertex_index = binding.vertex;

                // Подготовка данных для расчета смешения - собираем эндогены со всех входящих коннекторов
                auto [input_flows, input_endogenous] = Mixer::prepare_data_for_mixing_in_vertex(
                    block_tree, vertex_index, flows1, cut_flows, cut_endogenous);
                const auto& vertex_measurements = get_endogenous_measurement_by_binding(binding);

                // Пишем результа смешение в ТОЧКУ СОЧЛЕНЕНИЯ дерева блоков
                vertex_endogenous[vertex_index] = Mixer::mix(selector, input_flows, input_endogenous, 
                    vertex_measurements, &overwritten_by_measurements);

                break; // переходим на следующий элемент в топологическом порядке
            }
            // В блок попадаем, если все ВХОДНЫЕ КОННЕКТОРЫ уже посчитаны
            // Результат пишем в ВЫХОДНЫЕ КОННЕКТОРЫ блока
            case block_tree_binding_type::Block: {
                size_t block_index = binding.block;
                const biconnected_component_t& block = block_tree.blocks[block_index];
                // Эндогенный расчет блока: мост, сосредоточнный нетривиальный блок, нетривиальный блок труб

                const auto& block_measurements = get_endogenous_measurement_by_binding(binding);

                bool is_bridge = block.edges2.size() == 1;
                bool is_lumped = is_lumped_block<ModelIncidences, PipeModel>(block, graph);

                if (is_bridge) {
                    // если мост, то можем рассчитать эндогенные свойства в ребре
                    cut_endogenous[block_index] = bridge_endogenous_calc(
                        time_step, binding.block, block_measurements, &overwritten_by_measurements);
                }
                else if (is_lumped) {
                    // Для нетривиальных блоков расчет эквивалентен расчет смешения на вершине
                    // Подготовка данных для расчета смешения в блоке с несколькими входами
                    cut_endogenous[block_index] = block_as_vertex_endogenous_calc(
                        binding.block, block_measurements, &overwritten_by_measurements);
                }
                else {
                    throw std::logic_error("Nontrivial block with pipes");
                }

                break;
            }
            }
        }
        return overwritten_by_measurements;
    }

     /// @brief Подготавливает структуры для эндогенных свойств в вершинах и точках сочленения блоков
    /// @param flows1 Расходы на ребрах типа edge1
    /// @param cut_flows Расходы через точки сочленения блоков
    block_endogenous_propagator(const basic_graph_t<ModelIncidences>& graph,
        const block_tree_t<ModelIncidences>& block_tree, 
        const EndogenousSelector& selector, 
        const std::vector<BoundMeasurement>& endogenous_measurements,
        const std::vector<double>& flows1,
        const std::vector<double>& flows2,
        const std::vector<std::vector<double>>& cut_flows,
        const transport_block_solver_settings& settings)
        : graph(graph), block_tree(block_tree)
        , flows1(flows1)
        , flows2(flows2)
        , cut_flows(cut_flows)
        , selector(selector)
        , settings(settings)
    {
        std::tie(vertex_endogenous, cut_endogenous) = prepare_endogenous_structs(block_tree);
        measurements_by_tree_objects =
            bind_measurements_to_block_tree(graph, block_tree, endogenous_measurements);

        measurements_by_graph_object = get_endogenous_measurements_by_incidences(endogenous_measurements);
    }
};
}