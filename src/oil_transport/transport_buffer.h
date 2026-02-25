#pragma once

namespace oil_transport {
;


/// @brief Буфер для односторонних ребер
template <typename LumpedCalcParametersType>
using single_sided_model_buffer = LumpedCalcParametersType;

/// @brief Буфер с эндогенными свойствами на входе и выходе двустороннего ребра
template <typename LumpedCalcParametersType>
using two_sided_model_buffer = std::array<LumpedCalcParametersType, 2>;


/// @brief Полиморфный данные расчета для моделей разных типов
template <typename PipeLayerType, typename LumpedCalcParametersType>
using oil_transport_buffer_t = std::variant<
    pde_solvers::ring_buffer_t<PipeLayerType>, /* для труб */
    single_sided_model_buffer<LumpedCalcParametersType>, /* для односторонних ребер (поставщики, потребители) */
    two_sided_model_buffer<LumpedCalcParametersType> /* для двусторонних ребер НЕ труб (открытая задвижка, УЗР и др.) */
>;

/// @brief Буферы транспортрных объктов - используются для расчета значений целевых параметров
/// Для двустороннего протяженного ребра (труба) - буфер из двух слоев density_viscosity_layer. Длина определяется профилем.
/// Для двустороннего сосредоточенного ребра - вектор из двух pde_solvers::endogenous_values_t - свойства нефти на входе и на выходе.
/// Для одностороннего ребра - вектор из одного pde_solvers::endogenous_values_t - свойства нефти на входе (выходе)
namespace transport_buffer_helpers {
;

/// @brief Индекс типа std.variant для буфера трубы
inline constexpr size_t edge_buffer_pipe_index = 0;
/// @brief Индекс типа std.variant для буфера односторонней модели
inline constexpr size_t edge_buffer_single_sided_index = 1;
/// @brief Индекс типа std.variant для буфера двусторонней модели НЕ трубы)
inline constexpr size_t edge_buffer_two_sided_index = 2;


/// @brief Создаиние буфера для трубы
/// @param layer_count Количество слоев в буфере
/// @param point_count Размерность одного слоя
template <typename PipeLayerType, typename LumpedCalcParametersType>
inline oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> create_pipe(size_t point_count)
{
    size_t layer_count = 2; // всегда два слоя достаточно
    oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> data(
        std::in_place_index<edge_buffer_pipe_index>, 
        /*дальше идут параметры конструктора ring_buffer<PipeLayerType>*/
        layer_count, point_count);
    return data;
}
/// @brief Констуктор буфера для ребра не-трубы
template <typename PipeLayerType, typename LumpedCalcParametersType>
inline oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> create_single_sided()
{
    oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> data(std::in_place_index<edge_buffer_single_sided_index>);
    return data;
}
/// @brief Констуктор буфера для ребра не-трубы
template <typename PipeLayerType, typename LumpedCalcParametersType>
inline oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> create_two_sided()
{
    oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType> data(std::in_place_index<edge_buffer_two_sided_index>);
    return data;
}
/// @brief Сбрасывает достоверность всех объектов в "ложь"
template <typename PipeLayerType, typename LumpedCalcParametersType>
inline void reset_confidence(oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType>* buffer) {

    if constexpr (!std::is_same<PipeLayerType, pde_solvers::pipe_endogenous_calc_layer_t>::value) {
        throw std::runtime_error("Confidence is unsupported for the layer type");
    }
    else {
        switch (buffer->index()) {
        case edge_buffer_pipe_index: {
            auto& the_buffer = std::get<edge_buffer_pipe_index>(*buffer);
            pde_solvers::reset_confidence(&the_buffer.current());
            break;
        }
        case edge_buffer_single_sided_index: {
            single_sided_model_buffer<LumpedCalcParametersType>& the_buffer =
                std::get<edge_buffer_single_sided_index>(*buffer);
            pde_solvers::reset_confidence(&the_buffer);
            break;
        }
        case edge_buffer_two_sided_index: {
            two_sided_model_buffer<LumpedCalcParametersType>& the_buffer =
                std::get<edge_buffer_two_sided_index>(*buffer);
            pde_solvers::reset_confidence(&the_buffer[0]);
            pde_solvers::reset_confidence(&the_buffer[1]);
            break;
        }
        default:
            throw std::runtime_error("Unknown buffer type");
        }

    }
}

/// @brief Класс для инициализации профилей эндогенных свойств в буфере трубы по усреднённым значениям
/// @tparam PipeLayerType Тип расчетного слоя
/// @tparam EndogenousLike Тип эндогенных свойств
template <typename PipeLayerType, typename EndogenousLike>
class pipe_buffer_initializer_t {
private:
    /// @brief Устанавливает профиль значения в слое
    /// @param value_vector Вектор значений для заполнения
    /// @param confidence_vector Вектор достоверности для заполнения (может быть пустым)
    /// @param avg_value Усреднённое значение
    /// @param avg_confidence Усреднённая достоверность
    static void set_profile(
        std::vector<double>& value_vector,
        std::vector<double>& confidence_vector,
        double avg_value,
        bool avg_confidence)
    {
        std::fill(value_vector.begin(), value_vector.end(), avg_value);
        if (!confidence_vector.empty()) {
            std::fill(confidence_vector.begin(), confidence_vector.end(), 
                avg_confidence ? 1.0 : 0.0);
        }
    }

public:
    /// @brief Инициализирует профиль плотности в буфере
    /// @param pipe_buffer Указатель на буфер трубы
    /// @param layer_offset Смещение слоя (текущий или предыдущий)
    /// @param avg_values Усреднённые значения эндогенных свойств
    static void init_density(
        pde_solvers::ring_buffer_t<PipeLayerType>* pipe_buffer,
        graphlib::layer_offset_t layer_offset,
        const EndogenousLike& avg_values)
    {
        if (std::isfinite(avg_values.density_std.value)) {
            auto& layer =
                (layer_offset == graphlib::layer_offset_t::ToCurrentLayer)
                ? pipe_buffer->current()
                : pipe_buffer->previous();
            set_profile(
                layer.density_std.value,
                layer.density_std.confidence,
                avg_values.density_std.value,
                avg_values.density_std.confidence);
        }
    }

    /// @brief Инициализирует профиль вязкости в буфере
    /// @param pipe_buffer Указатель на буфер трубы
    /// @param layer_offset Смещение слоя (текущий или предыдущий)
    /// @param avg_values Усреднённые значения эндогенных свойств
    static void init_viscosity(
        pde_solvers::ring_buffer_t<PipeLayerType>* pipe_buffer,
        graphlib::layer_offset_t layer_offset,
        const EndogenousLike& avg_values)
    {
        if (std::isfinite(avg_values.viscosity_working.value)) {
            auto& layer =
                (layer_offset == graphlib::layer_offset_t::ToCurrentLayer)
                ? pipe_buffer->current()
                : pipe_buffer->previous();
            set_profile(
                layer.viscosity_working.value,
                layer.viscosity_working.confidence,
                avg_values.viscosity_working.value,
                avg_values.viscosity_working.confidence);
        }
    }
};

/// @brief Заполняет буфер одностороннего ребра усреднёнными значениями
template <typename LumpedCalcParametersType>
inline void init_single_sided_buffer_from_average(
    LumpedCalcParametersType* buffer,
    const LumpedCalcParametersType& avg_values)
{
    *buffer = avg_values;
}

/// @brief Заполняет буфер двустороннего ребра усреднёнными значениями
template <typename LumpedCalcParametersType>
inline void init_two_sided_buffer_from_average(
    std::array<LumpedCalcParametersType*, 2>* buffers_for_getter,
    const LumpedCalcParametersType& avg_values)
{
    *(buffers_for_getter->at(0)) = avg_values;
    *(buffers_for_getter->at(1)) = avg_values;
}

/// @brief Возвращает расходы через ребра 1-го и 2-го типов
/// @tparam HydroTransportModelIncidences Тип моделе с поддержкой get_vol_flow_from_buffer 
/// (транспортные модели, гидравлические модели)
/// @param estimation_type Из какого слоя буфера брать расход
/// @return пара <расходы1; расходы2>
template <typename HydroTransportModelIncidences>
inline std::tuple<std::vector<double>, std::vector<double>> get_flows_from_buffers(
    const graphlib::basic_graph_t<HydroTransportModelIncidences>& graph,
    graphlib::solver_estimation_type_t estimation_type) {

    std::vector<double> flows1(graph.edges1.size());
    std::vector<double> flows2(graph.edges2.size());

    for (size_t i = 0; i < static_cast<size_t>(graph.edges1.size()); ++i) {
        const auto& model = graph.edges1[i].model;
        double vol_flow = model->get_vol_flow_from_buffer(estimation_type);
        flows1[i] = vol_flow;
    }

    for (size_t i = 0; i < static_cast<size_t>(graph.edges2.size()); ++i) {
        const auto& model = graph.edges2[i].model;
        double vol_flow = model->get_vol_flow_from_buffer(estimation_type);
        flows2[i] = vol_flow;
    }

    return std::make_tuple(std::move(flows1), std::move(flows2));
}
    
/// @brief Обновляет значения расходов в буферах моделей  
/// @tparam HydroTransportModelIncidences Тип моделе с поддержкой update_flow (транспортные модели, гидравлические модели)
/// @param graph Граф
/// @param layer_offset Выбор слоя для записи (текущий, предыдущий)
/// @param flow1 Расходы через ребра1
/// @param flow2 Расходы через ребра2
template <typename HydroTransportModelIncidences>
inline void update_flows_in_buffers(
    const graphlib::basic_graph_t<HydroTransportModelIncidences>& graph,
    graphlib::layer_offset_t layer_offset,
    const std::vector<double>& flows1,
    const std::vector<double>& flows2) {

    for (size_t i = 0; i < flows1.size(); i++) {
        graph.edges1[i].model->update_flow(layer_offset, flows1[i]);
    }

    for (size_t i = 0; i < flows2.size(); i++) {
        graph.edges2[i].model->update_flow(layer_offset, flows2[i]);
    }

}
} // namespace transport_buffer_helpers

}
