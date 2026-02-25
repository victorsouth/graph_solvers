#pragma once

namespace oil_transport {
;


template <typename ModelIncidences,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
/// @brief Транспортный солвер на графе полной распространимости с записью результатов в буферы
/// @tparam ModelIncidences Тип инцидентностей модели
/// @tparam BoundMeasurement Тип измерений с привязкой к графу
/// @tparam EndogenousValue Тип эндогенных значений
/// @tparam Mixer Миксер эндогенных свойств
/// @tparam EndogenousSelector Селектор рассчитываемых эндогенов
class full_propagatable_endogenous_solver_buffer_based {
    
    /// @brief Проточный граф без циклов
    const graphlib::basic_graph_t<ModelIncidences>& non_flow_oriented_graph;
    /// @brief Селектор рассчитываемых эндогенных свойств
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const graphlib::transport_graph_solver_settings& settings;
public:
    /// @brief Тип настроек солвера
    using Settings = graphlib::transport_graph_solver_settings;

public:
    /// @brief Инициализация солвера
    /// @param graph Подграф для расчета
    /// @param selector Селектор эндогенных параметров
    /// @param settings Настройки расчета
    full_propagatable_endogenous_solver_buffer_based(const graphlib::basic_graph_t<ModelIncidences>& non_flow_oriented_graph,
        const EndogenousSelector& selector,
        const graphlib::transport_graph_solver_settings& settings = graphlib::transport_graph_solver_settings::default_values())
        : non_flow_oriented_graph(non_flow_oriented_graph)
        , selector(selector)
        , settings(settings)
    {

    }

    /// @brief Выполняет шаг расчета эндогенных свойств. Расходы читаются из буферов
    /// @param time_step Шаг по времени
    /// @param endogenous Измеренные эндогенные величины
    /// @param offset Слой буфера, из которого читаются расходы
    void step(double time_step,
        const std::vector<BoundMeasurement>& endogenous,
        graphlib::solver_estimation_type_t offset)
    {
        // Получаем расходы из буферов
        const auto& [flows1, flows2] = transport_buffer_helpers::get_flows_from_buffers(
            non_flow_oriented_graph, offset);

        // Переориентируем транспортный граф по расходам
        auto [oriented_graph, oriented_flows1, oriented_flows2] =
                graphlib::prepare_flow_relative_graph(non_flow_oriented_graph, flows1, flows2);

        // Решаем задачу распространения эндогенных свойств
        // TODO: Возможно пригодится возвращать информацию об использовании измерений
        graphlib::full_propagatable_endogenous_solver<
            ModelIncidences, BoundMeasurement, EndogenousValue, Mixer, EndogenousSelector>
            solver(oriented_graph, selector, settings);
        solver.step(time_step, oriented_flows1, oriented_flows2, endogenous);
    }
};

}
