#pragma once

namespace graphlib {
;

/// @brief Информация о полноте измерений расхода и эндогенов
struct transport_problem_completeness_info {
    /// @brief Идентификатор подграфа
    /// TODO: Зачем здесь subraph_id? Он явно передается в метод ascend_bindings
    std::size_t subgraph_id;
    /// @brief Объекты, к которым привязаны не задействованные расходы
    std::vector<graphlib::graph_binding_t> useless_flows;
    /// @brief Объекты, для которых не удалось рассчитать расход
    std::vector<graphlib::graph_binding_t> lacking_flows;
    /// @brief Объекты, для которых нужны эндогенные измерения, но их нет
    std::vector<graphlib::graph_binding_t> lacking_endogenious;

    /// @brief Проверяет, является ли информация о полноте пустой
    /// @return true, если все списки (бесполезные расходы, 
    /// недостающие расходы, недостающие эндогены) пусты, иначе false
    bool empty() const {
        return useless_flows.empty() && 
               lacking_flows.empty() &&
               lacking_endogenious.empty();
    }

    /// @brief Возвращает информацию о полноте для объектов подграфа с заданным subgraph_id
    template <typename StructuredGraph>
    transport_problem_completeness_info ascend_bindings(
        const StructuredGraph& structured_graph, size_t subgraph_id) const 
    {
        transport_problem_completeness_info subgraph_completness;
        subgraph_completness.useless_flows = 
            structured_graph.ascend_graph_bindings(subgraph_id, useless_flows);
        subgraph_completness.lacking_flows = 
            structured_graph.ascend_graph_bindings(subgraph_id, lacking_flows);
        subgraph_completness.lacking_endogenious = 
            structured_graph.ascend_graph_bindings(subgraph_id, lacking_endogenious);
        
        return subgraph_completness;
    }
};

template <typename StructuredGraph>
inline transport_problem_completeness_info ascend_bindings(
    const StructuredGraph& structured_graph, size_t subgraph_id, 
    const transport_problem_completeness_info& completeness
) 
{
    return completeness.ascend_bindings(structured_graph, subgraph_id);
}

/// @brief Статус измерений для транспортной задачи
/// @tparam BoundMeasurement Тип привязанного измерения
template <typename BoundMeasurement>
struct transport_problem_measurement_status {
    /// @brief Статус по измерениям расхода (использован/не использован)
    std::vector<BoundMeasurement> flows;
    /// @brief Статус по эндогенным измерениям 
    /// (заменил достоверное/заменил недостоверное/не использовался)
    std::vector<BoundMeasurement> endogenous;
};

/// @brief Информация о полноте и флагах активности измерений
/// @tparam BoundMeasurement 
template <typename BoundMeasurement>
struct transport_problem_info {
    /// @brief Флаги активность измерений
    transport_problem_measurement_status<BoundMeasurement> measurement_status;
    /// @brief Полнота измерений в графе
    transport_problem_completeness_info completeness;

    /// @brief Возвращает информацию о полноте для объектов подграфа с заданным subgraph_id
    template <typename StructuredGraph>
    transport_problem_info<BoundMeasurement> ascend_bindings(
        const StructuredGraph& structured_graph, size_t subgraph_id) const
    {
        transport_problem_info<BoundMeasurement> result;

        result.measurement_status.endogenous = 
            graphlib::ascend_graph_bindings<StructuredGraph, BoundMeasurement>(
                structured_graph, subgraph_id, measurement_status.endogenous);

        // Измерения расхода из подграфа
        result.measurement_status.flows =
            graphlib::ascend_graph_bindings<StructuredGraph, BoundMeasurement>(
                structured_graph, subgraph_id, measurement_status.flows);

        // Информация о нехватке эндогенов и расходов, избыточных расходах
        result.completeness =
            graphlib::ascend_bindings<StructuredGraph>(
                structured_graph, subgraph_id, completeness);

        return result;
    }

    /// @brief Дополняет сведения о полноте и статусах измерений переданными. Обычно используется при агрегации
    /// статусов измерений в подграфах некоторого графа
    /// @param info_to_append Сведения о полноте и статусе измерений в подграфе
    void append(const transport_problem_info<BoundMeasurement>& info_to_append) {

        // Эндогенные измерения из подграфа
        graphlib::append_vector(measurement_status.endogenous, info_to_append.measurement_status.endogenous);
        // Измерения расхода из подграфа
        graphlib::append_vector(measurement_status.flows, info_to_append.measurement_status.flows);
        // Информация о нехватке эндогенных свойств
        graphlib::append_vector(completeness.lacking_endogenious, info_to_append.completeness.lacking_endogenious);
        // Информация о нехватке расходов
        graphlib::append_vector(completeness.lacking_flows, info_to_append.completeness.lacking_flows);
        // Информация об избыточных расходах
        graphlib::append_vector(completeness.useless_flows, info_to_append.completeness.useless_flows);
    }

    /// @brief Проверяет, является ли структура пустой
    /// @return true, если информация о полноте пуста, иначе false
    bool empty() const {
        return completeness.empty();
    }

};

// Forward Declaration
struct transport_graph_solver_settings;

/// @brief Эндогенный расчет графа с полной распространимостью
/// Расчет состоит в обходе вершин в порядке топологической сортировки и
/// @tparam ModelIncidences Базовый тип отраслевых моделей и их инциденций
/// @tparam BoundMeasurement Тип измерения с привязкой к графу basic_graph_t
/// @tparam EndogenousValue Тип стркутуры для хранения эндогенных свойств
/// @tparam Mixer Тип для расчета свойств смеси
/// @tparam EndogenousSelector Тип селектора, задающего перечень рассчитываемых параметров
template <
    typename ModelIncidences,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
class graph_endogenous_propagator_t
{  
    /// @brief Тип отраслеваой модели
    typedef typename ModelIncidences::model_type Model;
    /// @brief Ориентированный по расходам граф
    const basic_graph_t<ModelIncidences>& oriented_graph;
    /// @brief Селектор эндогенных параметров для расчета
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const transport_graph_solver_settings& settings;
private:
    static const std::vector<BoundMeasurement>& get_endogenous_measurement_by_binding(
        const std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, 
            graph_binding_t::hash>& measurements_by_tree_objects,
        const graph_binding_t& binding) 
    {
        static const std::vector<BoundMeasurement> empty_measurements;
        if (has_map_key(measurements_by_tree_objects, binding)) {
            return measurements_by_tree_objects.at(binding);
        }
        else {
            return empty_measurements;
        }
    }
public:
    /// @brief Распространяет измерения эндогенных свойств по источникам. Вызывать ТОЛЬКО в начальный момент времени
    /// В основном алгоритме распространения измерений не будет возможности вызывать propagate_endogenous у ребер 1, ВХОДЯЩИХ в вершину
    /// @param oriented_edges1 Односторонние ребра графа
    /// @param measurements_by_object Измерения в привязке к инциденциям графа
    /// @param flows1 Расходы по односторонним ребрам
    static void initial_propagate_endogenous_for_sources(const std::vector<ModelIncidences>& oriented_edges1,
        const std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash>& measurements_by_object,
        const std::vector<double>& flows1)
    {
        double time_step = 0; // фиктивное значение шага по времени для вызова propagate_endogenous

        for (size_t edge1 = 0; edge1 < oriented_edges1.size(); ++edge1)
        {
            if (oriented_edges1[edge1].has_vertex_to() == false)
                continue; // это выходящее ребро, оно посчитается алгоритмом

            auto model = oriented_edges1[edge1].model;
            graph_binding_t binding(graph_binding_type::Edge1, edge1);
            auto edge_measurments = 
                endogenous_measurement_for_incidence<BoundMeasurement>(
                    binding, measurements_by_object);
            model->propagate_endogenous(time_step, flows1[edge1], nullptr, 
                edge_measurments, nullptr);
        }
    }

    /// @brief Конструктор. Для удобства инстанцирования шаблонного класса в пользовательском коде
    /// @param oriented_graph Ориентированный по расходам граф, обладающий полной распространимостью
    /// @param selector Селектор эндогенных параметров
    /// @param settings Настройки расчета
    graph_endogenous_propagator_t(const basic_graph_t<ModelIncidences>& oriented_graph,
        const EndogenousSelector& selector,
        const transport_graph_solver_settings& settings)
        : oriented_graph(oriented_graph)
        , selector(selector)
        , settings(settings)
    {
    }

    /// @brief Используется для расчета full-propagate блока
    void propagate(double time_step,
        const std::vector<double>& flows1, const std::vector<double>& flows2,
        const std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash>& measurements_by_object,
        const std::vector<size_t>& vertex_order,
        std::vector<BoundMeasurement>* overwritten_by_measurements
    )
    {
        Mixer mixer;
        for (size_t vertex_index : vertex_order) {

            // 1. Подготовка данных для смешения в вершине и само смешение: смотрим все это со входящих в вершину ребер
            auto [input_flows, input_endogenous, vertex_measurements] =
                mixer.prepare_data_for_mixing(oriented_graph, vertex_index, flows1, flows2,
                    measurements_by_object);

            EndogenousValue vertex_output_endogenous = mixer.mix(
                selector, input_flows, input_endogenous, vertex_measurements,
                overwritten_by_measurements);

            // 2. Результат смешения отправить во все выходящие ребра
            mixer.propagate_mixture(time_step, oriented_graph, vertex_index, flows1, flows2,
                measurements_by_object, vertex_output_endogenous, overwritten_by_measurements);

        }

    }

    /// @brief Распространяет измерения с притоков графа. Далее обходит вершины в порядке топологической сортировки.
    /// В каждой вершине рассчтиывает параметры смеси в вершине на основе энждогенных свойств во входящих дугах.
    /// Далее распространяет свойства смеси в выходящие дуги
    /// @param time_step Временной шаг расчета
    /// @param flows1 Расходы через ребра 1-го типа
    /// @param flows2 Расходы через ребра 2-го типа
    /// @param endogenous_measurements Измерения эндогенных свойств с привязкой к графу
    std::vector<BoundMeasurement> propagate(double time_step,
        const std::vector<double>& flows1, const std::vector<double>& flows2,
        const std::vector<BoundMeasurement>& endogenous_measurements)
    {
        // Для быстрого поиска эндогенных измерений от объектов графа (ребрам и вершинам)
        std::unordered_map<graph_binding_t, std::vector<BoundMeasurement>, graph_binding_t::hash> measurements_by_object
            = get_endogenous_measurements_by_incidences(endogenous_measurements);

        // Чтобы учесть возможное наличие измерений в поставщиках, надо их просчитать. 
        initial_propagate_endogenous_for_sources(oriented_graph.edges1, measurements_by_object, flows1);

        // Готовим порядок обхода - топологическая сортировка в графе, ориентированном по расходам
        std::vector<size_t> vertex_order, edge2_order;
        oriented_graph.topological_sort(&vertex_order, &edge2_order, +1 /*обход от источников*/, false /*не исключать источники*/);

        std::vector<BoundMeasurement> overwritten_by_measurements;
        propagate(time_step, flows1, flows2, measurements_by_object, vertex_order,
            &overwritten_by_measurements);

        return overwritten_by_measurements;
    }
};


}