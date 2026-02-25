#pragma once

namespace oil_transport {
;

template <typename ModelIncidences,
    typename PipeModel,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
/// @brief Солвер транспортного расчета на дереве блоков с записью расходов в буферы
/// @tparam ModelIncidences Тип инцидентностей модели
/// @tparam PipeModel Тип модели трубы
/// @tparam BoundMeasurement Тип измерений расходов/параметров
/// @tparam EndogenousValue Тип эндогенных значений
/// @tparam Mixer Тип миксера для эндогенов
/// @tparam EndogenousSelector Селектор рассчитываемых эндогеных параметров
class transport_block_solver_buffer_based {
    
    const graphlib::basic_graph_t<ModelIncidences>& transport_graph;
    /// @brief Тот же подграф, что и transport_graph, но с гидравлическими моделями
    /// @brief Селектор рассчитываемых эндогенных свойств
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const graphlib::transport_block_solver_settings& settings;

public:
    /// @brief Тип настроек солвера
    using Settings = graphlib::transport_block_solver_settings;

public:
    /// @brief Инициализация солвера
    /// @param graph Подграф для расчета
    transport_block_solver_buffer_based(const graphlib::basic_graph_t<ModelIncidences>& transport_graph,
        const EndogenousSelector& selector,
        const graphlib::transport_block_solver_settings& settings)
        : transport_graph(transport_graph)
        , selector(selector)
        , settings(settings)
    {

    }

    /// @brief Выполняет шаг расчета транспортного расчете на дереве блооков и обновляет расходы в буферах
    /// @param time_step Шаг по времени
    /// @param flows Измерения расходов
    /// @param endogenous Эндогенные измерения
    /// @param layer_offset Целевой слой буфера для записи результатов
    /// @return Информация об использованных измерениях
    graphlib::transport_problem_info<BoundMeasurement> step(double time_step,
        const std::vector<BoundMeasurement>& flows,
        const std::vector<BoundMeasurement>& endogenous,
        graphlib::layer_offset_t layer_offset)
    {
        std::vector<double> flows1;
        std::vector<double> flows2;

        graphlib::transport_block_solver_template_t<ModelIncidences, PipeModel, BoundMeasurement,
            EndogenousValue, Mixer, EndogenousSelector> solver(transport_graph, selector, settings);

        graphlib::transport_problem_info<BoundMeasurement> result = 
            solver.step(time_step, flows, endogenous, &flows1, &flows2);

        // Запись расходов в буферы моделей
        // TODO: уже есть очень похожий функционал в nodal_solver_buffer_based_t::update_buffers. Обощить
        transport_buffer_helpers::update_flows_in_buffers(transport_graph, layer_offset, flows1, flows1);

        return result;
    }
};

}
