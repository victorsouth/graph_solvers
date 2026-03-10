#pragma once

namespace oil_transport {
;

/// @brief Переключения, сопоставленные временной сетке расчета
using transport_control_batch_data_t = std::unordered_multimap<
    std::size_t,
    std::unique_ptr<oil_transport::transport_object_control_t>
>;

/// @brief Переключения гидравлических объектов, сопоставленные временной сетке расчета
using hydro_control_batch_data_t = std::unordered_multimap<
    std::size_t,
    std::unique_ptr<oil_transport::hydraulic_object_control_t>
>;

inline void append_transport_contol_to_batch(const graphlib::graph_place_binding_t& binding, const size_t modelling_time_index,
    const bool control_value, transport_control_batch_data_t* controls)
{
    if (binding.binding_type == graphlib::graph_binding_type::Edge1) {
        // Переключение для поставщика/потребителя  
        auto control =
            std::make_unique<oil_transport::transport_source_control_t>();
        control->binding = binding;
        control->is_zeroflow = control_value;
        controls->insert(std::make_pair(modelling_time_index, std::move(control)));
    }
    else if (binding.binding_type == graphlib::graph_binding_type::Edge2)
    {
        // Переключение для люмпеда
        auto control =
            std::make_unique<oil_transport::transport_lumped_control_t>();
        control->binding = binding;
        control->is_opened = control_value;
        controls->insert(std::make_pair(modelling_time_index, std::move(control)));
    }
    else {
        throw std::runtime_error("Unknown model type");
    }
}

inline void append_hydro_control_to_batch(const hydraulic_graph_t& graph, const graphlib::graph_place_binding_t& binding, 
    const size_t modelling_time_index, const double control_value, hydro_control_batch_data_t* controls)
{
    const auto& edge = graph.get_model_incidences(binding);
    hydraulic_model_t* model = edge.model;

    if (auto source_model = dynamic_cast<oil_transport::qsm_hydro_source_model_t*>(model)) {
        // Переключение для источника/потребителя
        throw std::runtime_error("Not impl");
    }
    else if (auto resistance_model = dynamic_cast<oil_transport::qsm_hydro_local_resistance_model_t*>(model)) {
        // Переключение для клапана/местного сопротивления
        auto control = std::make_unique<oil_transport::qsm_hydro_local_resistance_control_t>();
        control->binding = binding;
        control->opening = control_value;
        controls->insert(std::make_pair(modelling_time_index, std::move(control)));
    }
    // В будущем здесь можно добавить поддержку насосов и других типов объектов
    // else if (auto pump_model = dynamic_cast<oil_transport::pump_model_t*>(model)) { ... }
    else {
        throw std::runtime_error("Unknown hydraulic model type for control");
    }
}

template <typename TypedTimeSeries>
hydro_control_batch_data_t prepare_batch_control_hydro(
    const hydraulic_graph_t& graph, const graphlib::graph_places_vector_t& control_bindings,
    const TypedTimeSeries& control_timeseries, const std::vector<std::time_t>& astro_times)
{
    hydro_control_batch_data_t all_controls;

    if (control_bindings.size() != control_timeseries.size()) {
        throw std::runtime_error("There is no bijectivity between time series and bindings");
    }

    for (std::size_t object_index = 0; object_index < control_timeseries.size(); ++object_index) {
        const graphlib::graph_place_binding_t& binding = control_bindings[object_index];

        const auto& [times, controls] = control_timeseries[object_index];

        for (std::size_t time_index = 0; time_index < times.size(); ++time_index) {

            // Для попадания в расчетную сетку надо найти индекс последнего момента времени на сетке,
            // не превышающего момент переключения.
            time_t control_astro_time = times[time_index];
            size_t control_model_time_index = std::upper_bound(astro_times.begin(), astro_times.end(), control_astro_time)
                - astro_times.begin() - 1;

            double control_value = controls[time_index];
            append_hydro_control_to_batch(graph, binding, control_model_time_index, control_value, &all_controls);
        }
    }

    return all_controls;
}

/// @brief Формирует данные о переключениях в пакетном формате
/// @param graph Граф, содержащий модели
/// @param control_bindings Привязки переключений на графе
/// @param control_timeseries Временной ряд переключений. 
/// Для каждого ряда должна быть задана привязка
/// @param astro_times Астрономические моменты в расчетной сетке
inline transport_control_batch_data_t prepare_batch_control_transport(
    const transport_graph_t& graph, const graphlib::graph_places_vector_t& control_bindings,
    const transport_control_timeseries_t& control_timeseries, const std::vector<std::time_t>& astro_times)
{
    transport_control_batch_data_t all_controls;

    if (control_bindings.size() != control_timeseries.size()) {
        throw std::runtime_error("There is no bijectivity between time series and bindings");
    }

    for (std::size_t object_index = 0; object_index < control_timeseries.size(); ++object_index) {
        const graphlib::graph_place_binding_t& binding = control_bindings[object_index];

        const auto& [times, controls] = control_timeseries[object_index];

        for (std::size_t time_index = 0; time_index < times.size(); ++time_index) {

            // Для попадания в расчетную сетку надо найти индекс последнего момента времени на сетке,
            // не превышающего момент переключения.
            time_t control_astro_time = times[time_index];
            size_t control_model_time_index = std::upper_bound(astro_times.begin(), astro_times.end(), control_astro_time)
                - astro_times.begin() - 1;

            bool control_value = controls[time_index];
            append_transport_contol_to_batch(binding, control_model_time_index, control_value, &all_controls);
        }
    }

    return all_controls;
}

/// @brief Подготавливает замеры для пакетного расчета
/// @param timeseries Структура векторным временным рядом для графа
/// @param astro_times Временная сетка с астрономическим временем
/// @return Временной ряд векторов измерений
std::vector<std::vector<graph_quantity_value_t>>
inline prepare_batch_measurements(graph_vector_timeseries_t& timeseries,
    const std::vector<std::time_t>& astro_times)
{
    std::vector<std::vector<graph_quantity_value_t>> result;
    for (time_t t : astro_times) {
        result.emplace_back(timeseries(t));
    }

    return result;
}


std::pair<
    std::vector<std::vector<graph_quantity_value_t>>,
    std::vector<std::vector<graph_quantity_value_t>>
> inline prepare_batch_measurements_classified(const graph_vector_timeseries_t& timeseries,
    const std::vector<std::time_t>& astro_times)
{
    std::vector<std::vector<graph_quantity_value_t>> flow_result, endogenous_result;
    for (time_t t : astro_times) {
        auto layer = timeseries(t);
        auto [flow, endogenous] = classify_measurements(layer);
        flow_result.emplace_back(std::move(flow));
        endogenous_result.emplace_back(std::move(endogenous));
    }

    return std::make_pair(std::move(flow_result), std::move(endogenous_result));
}




/// @brief Настройки пакетного расчета
struct batch_control_settings_t
{
    /// @brief Возвращает настройки по умолчанию
    static batch_control_settings_t get_default_settings()
    {
        batch_control_settings_t result;
        return result;
    }
    
    /// @brief Использовать эвристику начальных состояний. 
    /// Начальное состояние, заданное пользователем, обрабатывается как виртуальное переключение. 
    /// Т.е. начало интервала моделирования определяется только по измерениям
    bool use_initial_state_as_control{ true };
    /// @brief Использовать эвристику конечных состояний.
    /// Предполагается, что после последнего известного переключения по каждому из объектов отсутствовали другие переключения. 
    /// Т.е. конец интервала моделирования определяется только по измерениям.
    bool extrapolate_last_control{ true };
};

/// @brief Генератор сетки модельного времени
class time_grid_generator
{

public:

    /// @brief Возвращает сетку модельного времени, начиная с нулевого момента времени
    /// TODO: скрыть. Не давать пользователю сломать расчет некорректными интервалами времени
    static std::tuple<
        std::vector<double>,
        std::vector<std::time_t>
    > prepare_time_grid(double step, time_t time_start, time_t time_end)
    {
        time_t duration = (time_end - time_start);

        // Считаем количество точек в сетке
        size_t dots_count = static_cast<size_t>(ceil(duration / step) + 0.00001);

        std::vector<double> times(dots_count);
        std::vector<std::time_t> astronomic_times(dots_count);

        for (size_t i = 0; i < dots_count; i++) {
            times[i] = step * i;
            astronomic_times[i] = static_cast<std::time_t>(times[i] + 0.5) + time_start;
        }
        return std::make_tuple(
            std::move(times),
            std::move(astronomic_times)
        );
    }

    /// @brief Возвращает сетку модельного времени, построенную на интервале времени, определяемом настрокой расчета
    /// Предполагается, что временные ряды переключений не заданы
    static std::tuple<
        std::vector<double>,
        std::vector<std::time_t>
    > prepare_time_grid(const batch_control_settings_t& batch_settings, double step,
        const vector_timeseries_t<double>& measurements)
    {
        if (not batch_settings.use_initial_state_as_control) {
            throw std::runtime_error("Trying to prepare time grid without control timeseries with wrong task option");
        }
        // Интервал для моделирования определяется только наличием исходных данных измерений
        time_t start_date = measurements.get_start_date();
        time_t end_date = measurements.get_end_date();
        return prepare_time_grid(step, start_date, end_date);
    }


    /// @brief Возвращает сетку модельного времени. Интервал времени определяется на основе временных рядов. 
    /// Опция time_grid_options в настройках определяет эвристики, используемые при работе с временными рядами переключений
    static std::tuple<
        std::vector<double>,
        std::vector<std::time_t>
    > prepare_time_grid(const batch_control_settings_t& batch_settings, double step,
        const vector_timeseries_t<double>& measurements, const transport_control_timeseries_t& controls)
    {
        time_t control_start_date;
        time_t control_end_date;
        std::tie(control_start_date, control_end_date) = vector_timeseries_t<bool>::get_timeseries_period(controls);

        // Определение начала временной сетки модельноо времени
        time_t start_date;
        if (batch_settings.use_initial_state_as_control) {
            // Момент начала моделирования определяется только временными рядами измерений
            start_date = measurements.get_start_date();
        }
        else {
            start_date = std::max(measurements.get_start_date(), control_start_date);
        }

        time_t end_date;
        if (batch_settings.extrapolate_last_control) {
            // Экстраполируем последнее известное переключение до окончания временных рядов измерений
            end_date = measurements.get_end_date();
        }
        else {
            // Моделирование от момента t1, когда по всем объектам известно переключение, до момента последнего измерения
            end_date = std::min(measurements.get_end_date(), control_end_date);

        }

        if (start_date >= end_date)
            throw std::runtime_error("Wrong time intervals. Check timeseries and time grid options");

        return prepare_time_grid(step, start_date, end_date);
    }
};


/// @brief Вспомогательные функции для пакетных расчетов
struct batch_helpers {
    /// @brief Собирает единый слой измерений из измерений расходов и эндогенов
    static std::vector<graph_quantity_value_t> build_measurement_layer(
        const std::vector<graph_quantity_value_t>& flow_batch,
        const std::vector<graph_quantity_value_t>& endogenous_batch
    )
    {
        std::vector<graph_quantity_value_t> result;
        result.reserve(flow_batch.size() + endogenous_batch.size());
        result.insert(result.end(), flow_batch.begin(), flow_batch.end());
        result.insert(result.end(), endogenous_batch.begin(), endogenous_batch.end());
        return result;
    }    
    
    /// @brief Из временного ряда переключений возвращает переключения
    /// для текущего момента времени
    template <typename BaseModelControlType>
    static auto get_step_controls(
        const std::unordered_multimap<std::size_t,
        std::unique_ptr<BaseModelControlType>>& controls,
        size_t step_index)
    {
        std::vector<const BaseModelControlType*> step_controls;
        auto range = controls.equal_range(step_index);
        for (auto it = range.first; it != range.second; ++it) {
            const auto& control = it->second;
            step_controls.push_back(control.get());
        }
        return step_controls;
    }
};

/// @brief Пакетный гидравлический расчет
struct hydro_batch {
public:
    /// @brief Пакетный расчет в режиме с заданными замерами 
    /// (расходы и эндогены смешаны) и переключениями
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const hydro_control_batch_data_t& controls
    )
    {
        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        data_processor(time_step, step_index,
            batch_helpers::get_step_controls(controls, step_index));

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index,
                batch_helpers::get_step_controls(controls, step_index));
        }

    };
};


/// @brief Пакетный транспортный расчет
struct transport_batch {
public:
    /// @brief Пакетный расчет в режиме с заданными замерами 
    /// (расходы и эндогены смешаны) и переключениями
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<std::vector<graph_quantity_value_t>>& measurements_batch,
        const std::unordered_multimap<std::size_t,
        std::unique_ptr<transport_object_control_t>>& controls =
        std::unordered_multimap<std::size_t,
        std::unique_ptr<transport_object_control_t>>()
    )
    {
        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        data_processor(time_step, step_index, measurements_batch[step_index],
            batch_helpers::get_step_controls(controls, step_index));

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, measurements_batch[step_index],
                batch_helpers::get_step_controls(controls, step_index));
        }

    };

    /// @brief Пакетный расчет в режиме с заданными замерами 
    /// (классифицированными на расходы и эндогены) и переключениями
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<std::vector<graph_quantity_value_t>>& flow_batch,
        const std::vector<std::vector<graph_quantity_value_t>>& endogenous_batch,
        const std::unordered_multimap<std::size_t,
        std::unique_ptr<transport_object_control_t>>& controls =
        std::unordered_multimap<std::size_t,
        std::unique_ptr<transport_object_control_t>>()
    )
    {
        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        auto layer = batch_helpers::build_measurement_layer(flow_batch[step_index], endogenous_batch[step_index]);
        data_processor(time_step, step_index, layer,
            batch_helpers::get_step_controls(controls, step_index));

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            auto layer = batch_helpers::build_measurement_layer(flow_batch[step_index], endogenous_batch[step_index]);
            data_processor(time_step, step_index, layer,
                batch_helpers::get_step_controls(controls, step_index));
        }

    }

    /// @brief Пакетный расчет в режиме с заданными замерами (без переключений)
    /// Один слой измерений на весь расчет
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<graph_quantity_value_t>& flow_batch,
        const std::vector<graph_quantity_value_t>& endogenous_batch
    )
    {
        std::unique_ptr<transport_object_control_t> controls;

        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        auto layer = batch_helpers::build_measurement_layer(flow_batch, endogenous_batch);
        data_processor(time_step, step_index, layer, controls);

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, layer, controls);
        }
    }

    /// @brief Пакетный расчет в режиме с заданными замерами
    /// Один слой измерений на всё время расчета
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<graph_quantity_value_t>& measurements
    )
    {
        std::vector<const transport_object_control_t*> dummy_controls;

        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        data_processor(time_step, step_index, measurements, dummy_controls);

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, measurements, dummy_controls);
        }

    }

};

/// @brief Пакетный совмещенный транспортно-гидравлический расчет
struct hydrotransport_batch {
public:
    /// @brief Пакетный расчет в режиме с заданными замерами 
    /// (расходы и эндогены смешаны) и переключениями двух типов
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<std::vector<graph_quantity_value_t>>& measurements_batch,
        const transport_control_batch_data_t& transport_controls,
        const hydro_control_batch_data_t& hydro_controls
    )
    {
        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        data_processor(time_step, step_index, measurements_batch[step_index],
            batch_helpers::get_step_controls(transport_controls, step_index),
            batch_helpers::get_step_controls(hydro_controls, step_index));

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, measurements_batch[step_index],
                batch_helpers::get_step_controls(transport_controls, step_index),
                batch_helpers::get_step_controls(hydro_controls, step_index));
        }
    }

    /// @brief Пакетный расчет в режиме с заданными замерами 
    /// (классифицированными на расходы и эндогены) и переключениями двух типов
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<std::vector<graph_quantity_value_t>>& flow_batch,
        const std::vector<std::vector<graph_quantity_value_t>>& endogenous_batch,
        const transport_control_batch_data_t& transport_controls,
        const hydro_control_batch_data_t& hydro_controls
    )
    {
        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        auto layer = batch_helpers::build_measurement_layer(flow_batch[step_index], endogenous_batch[step_index]);
        data_processor(time_step, step_index, layer,
            batch_helpers::get_step_controls(transport_controls, step_index),
            batch_helpers::get_step_controls(hydro_controls, step_index));

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            auto layer = batch_helpers::build_measurement_layer(flow_batch[step_index], endogenous_batch[step_index]);
            data_processor(time_step, step_index, layer,
                batch_helpers::get_step_controls(transport_controls, step_index),
                batch_helpers::get_step_controls(hydro_controls, step_index));
        }
    }

    /// @brief Пакетный расчет в режиме с заданными замерами (без переключений)
    /// Один слой измерений на весь расчет
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<graph_quantity_value_t>& flow_batch,
        const std::vector<graph_quantity_value_t>& endogenous_batch
    )
    {
        std::vector<const transport_object_control_t*> empty_transport_controls;
        std::vector<const hydraulic_object_control_t*> empty_hydro_controls;

        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        auto layer = batch_helpers::build_measurement_layer(flow_batch, endogenous_batch);
        data_processor(time_step, step_index, layer, empty_transport_controls, empty_hydro_controls);

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, layer, empty_transport_controls, empty_hydro_controls);
        }
    }

    /// @brief Пакетный расчет в режиме с заданными замерами
    /// Один слой измерений на всё время расчета
    template <typename DataProcessor>
    static void perform(
        DataProcessor& data_processor,
        const std::vector<double>& times,
        const std::vector<graph_quantity_value_t>& measurements
    )
    {
        std::vector<const transport_object_control_t*> empty_transport_controls;
        std::vector<const hydraulic_object_control_t*> empty_hydro_controls;

        size_t step_index = 0;
        double time_step = std::numeric_limits<double>::quiet_NaN();
        data_processor(time_step, step_index, measurements, empty_transport_controls, empty_hydro_controls);

        for (step_index = 1; step_index < times.size(); step_index++)
        {
            time_step = times[step_index] - times[step_index - 1];
            data_processor(time_step, step_index, measurements, empty_transport_controls, empty_hydro_controls);
        }
    }
};

/// @brief Данные пакетного транспортного расчета
struct transport_batch_data {

    /// @brief Задает способ определения интервала времени для пакетного расчета на основе временных рядов
    /// измерений и переключений
    batch_control_settings_t batch_settings;

    /// @brief Расчетная сетка
    std::vector<double> times;
    /// @brief Временные метки времени для расчетной сетки 
    std::vector<std::time_t> astro_times;
    /// @brief Измерения расхода: вектор слоев измерений
    std::vector<std::vector<oil_transport::graph_quantity_value_t>> flow_measurements;
    /// @brief Эндогенные измерения: вектор слоев измерений
    std::vector<std::vector<oil_transport::graph_quantity_value_t>> endogenous_measurements;
    /// @brief Управления для некоторых моментов времени 
    std::unordered_multimap<
        std::size_t,
        std::unique_ptr<oil_transport::transport_object_control_t>
    > controls;

    /// @brief Конструктор по умолчанию. Нужен для работы пользовательских конструкторов
    transport_batch_data() = default;

    /// @brief Формирует данные пактеного расчета (в т.ч. сетку модельного времени на основании фиксированного шага dt) 
    /// на основе временных рядов измерений и переключений
    /// @param graph Транспортный граф. Используется для определения типов объектов при формировании переключений
    /// @param task_settings Используется набор настроек определяющих жвристики работы с переключениями
    /// @param dt Временной шаг временной сетки
    /// @param measurements Временные ряды измерений с ориентированными привязками
    /// @param control_timeseries Временные ряды переключений
    /// @param control_bindings Привязки временных рядов переключений
    transport_batch_data(const transport_graph_t& graph,
        const batch_control_settings_t& batch_settings,
        double dt,
        const bound_measurement_tags_data_t& measurements_data,
        const bound_control_tags_data_t& controls_data
    )
    {
        const graph_vector_timeseries_t& measurements_bound_timeseries = measurements_data.timeseries;
        const transport_control_timeseries_t& control_timeseries = controls_data.timeseries;
        const graphlib::graph_places_vector_t& control_bindings = controls_data.bindings;

        // Интерполяция данных на единую временную сетку
        const auto& measurements_timeseries = measurements_bound_timeseries.get_time_series_object();
        std::tie(times, astro_times) = time_grid_generator::prepare_time_grid(batch_settings, dt,
            measurements_timeseries, control_timeseries);

        // Классификация временных рядов по тегам на расходы, эндогены и переключения
        std::tie(flow_measurements, endogenous_measurements) =
            prepare_batch_measurements_classified(measurements_bound_timeseries, astro_times);
        controls = prepare_batch_control_transport(graph, control_bindings, control_timeseries, astro_times);
    }

    /// @brief Формирует данные пактеного расчета (в т.ч. сетку модельного времени) 
    /// на основе временных рядов измерений  (БЕЗ учета переключений)
    /// @param graph Транспортный граф. Используется для определения типов объектов при формировании переключений
    /// @param task_settings Используется набор настроек определяющих жвристики работы с переключениями
    /// @param dt Временной шаг временной сетки
    /// @param measurements Временные ряды измерений с ориентированными привязками
    /// @param control_timeseries Временные ряды переключений
    transport_batch_data(const transport_graph_t& graph,
        const batch_control_settings_t& batch_settings,
        double dt,
        const bound_measurement_tags_data_t& measurements_data
    )
    {
        const graph_vector_timeseries_t& measurements_bound_timeseries = measurements_data.timeseries;

        // Интерполяция данных на единую временную сетку
        const auto& measurements_timeseries = measurements_bound_timeseries.get_time_series_object();
        std::tie(times, astro_times) = time_grid_generator::prepare_time_grid(batch_settings, dt,
            measurements_timeseries);

        // Классификация временных рядов по тегам на расходы, эндогены и переключения
        std::tie(flow_measurements, endogenous_measurements) =
            prepare_batch_measurements_classified(measurements_bound_timeseries, astro_times);
    }


    /// @brief Подготавливает вектор поправок расхода по умолчанию (все равны 1.0)
    /// @return Вектор поправок расхода
    /// @throws std::runtime_error Если измерения расхода не определены
    std::vector<double> prepare_default_flow_corrections() const
    {
        if (flow_measurements.empty())
            throw std::runtime_error("No measurements defined");

        return std::vector<double>(flow_measurements.front().size(), 1.0);
    }

    /// @brief Подготавливает вектор поправок расхода по умолчанию для заданных расходов
    /// @param flows_to_correct Вектор привязок расходов, которые нужно скорректировать
    /// @return Вектор поправок расхода (все равны 1.0)
    /// @throws std::runtime_error Если измерения расхода не определены
    std::vector<double> prepare_default_flow_corrections(
        const std::vector<oil_transport::graph_quantity_binding_t> flows_to_correct) const
    {
        if (flow_measurements.empty())
            throw std::runtime_error("No measurements defined");

        return std::vector<double>(flows_to_correct.size(), 1.0);
    }

    std::vector<std::vector<oil_transport::graph_quantity_value_t>>
    /// @brief Вычисляет скорректированные расходы на основе поправок
    /// @param corrections Вектор поправок расхода
    /// @return Вектор векторов скорректированных значений расхода для каждого временного слоя
        calc_corrected_flows(const std::vector<double>& corrections) const
    {
        std::vector<std::vector<oil_transport::graph_quantity_value_t>> result;
        result.reserve(flow_measurements.size());

        for (const auto& layer : flow_measurements) {
            std::vector<oil_transport::graph_quantity_value_t> corrected_flow_layer;
            corrected_flow_layer.reserve(layer.size());
            for (std::size_t index = 0; index < corrections.size(); ++index) {
                oil_transport::graph_quantity_value_t measurement = layer[index];
                measurement.data.value *= corrections[index];
                corrected_flow_layer.emplace_back(std::move(measurement));
            }
            result.emplace_back(std::move(corrected_flow_layer));
        }
        return result;
    }
    std::vector<std::vector<oil_transport::graph_quantity_value_t>>
    /// @brief Вычисляет скорректированные расходы для заданных привязок на основе поправок
    /// @param flows_to_correct Вектор привязок расходов, которые нужно скорректировать
    /// @param corrections Вектор поправок расхода
    /// @return Вектор векторов скорректированных значений расхода для каждого временного слоя
    /// @throws std::runtime_error Если привязки расходов не найдены в измерениях
        calc_corrected_flows(const std::vector<oil_transport::graph_quantity_binding_t> flows_to_correct,
            const std::vector<double>& corrections) const
    {
        std::vector<std::vector<oil_transport::graph_quantity_value_t>> result;
        result.reserve(flow_measurements.size());

        std::vector<std::size_t> indices;
        for (const auto& flow_binding : flows_to_correct) {
            const auto& layer = flow_measurements.front();

            auto it = std::find(layer.begin(), layer.end(), flow_binding);
            if (it == layer.end()) {
                throw std::runtime_error("Flow bindings doesnt exist");
            }
            indices.push_back(it - layer.begin());
        }

        for (const auto& layer : flow_measurements) {
            std::vector<oil_transport::graph_quantity_value_t> corrected_flow_layer = layer;
            for (std::size_t i = 0; i < indices.size(); ++i) {
                double correction = corrections[i];
                std::size_t index = indices[i];
                corrected_flow_layer[index].data.value *= correction;
            }
            result.emplace_back(std::move(corrected_flow_layer));
        }
        return result;
    }

    /// @brief Создает данные пакетного расчета, полагая измерения постоянными на всем интервале времени
    /// @param time_step Временной шаг
    /// @param step_count Количество шагов расчета
    /// @param flow_measurements Срез измерений расхода
    /// @param endogenous_measurements Срез эндогенных измерений
    static transport_batch_data constant_batch(double time_step, std::size_t step_count,
        const std::vector<oil_transport::graph_quantity_value_t>& flow_measurements,
        const std::vector<oil_transport::graph_quantity_value_t>& endogenous_measurements
    )
    {
        transport_batch_data batch_data;
        std::tie(batch_data.times, batch_data.astro_times) = time_grid_generator::prepare_time_grid(
            time_step, 0, (time_t)time_step * step_count);
        batch_data.flow_measurements =
            std::vector<std::vector<oil_transport::graph_quantity_value_t>>(step_count, flow_measurements);
        batch_data.endogenous_measurements =
            std::vector<std::vector<oil_transport::graph_quantity_value_t>>(step_count, endogenous_measurements);

        return batch_data;
    }

};

/// @brief Данные для пакетного гидравлического расчета.
/// Имеет смысл только при выполнении до этого пакетного транспортного расчета
struct hydro_batch_data {

    /// @brief Задает способ определения интервала времени для пакетного расчета
    const batch_control_settings_t& batch_settings;
    /// @brief Расчетная сетка транспортного расчета
    const std::vector<double>& times;
    /// @brief Временные метки для расчетной сетки транспортного расчета
    const std::vector<std::time_t>& astro_times;

    /// @brief Управления для некоторых моментов времени 
    std::unordered_multimap<
        std::size_t,
        std::unique_ptr<oil_transport::hydraulic_object_control_t>
    > controls;

    /// @brief Конструктор по умолчанию. Нужен для работы пользовательских конструкторов
    hydro_batch_data() = default;

    /// @brief Формирует данные пакетного гидравлического(!) расчета на основе данных пакетного транспортного расчтета
    hydro_batch_data(const transport_batch_data& transport_data,
        const bound_control_tags_data_t& hydro_controls_data,
        const hydraulic_graph_t& graph)
        : times(transport_data.times)
        , astro_times(transport_data.astro_times)
        , batch_settings(transport_data.batch_settings)
    {
        controls = prepare_batch_control_hydro(graph,
            hydro_controls_data.bindings, hydro_controls_data.timeseries, astro_times);
    }

};



/// @brief Решает две задачи:
/// 1) Выполняет расчет баланса расходов с последующим ММП-сведением баланса в пакетном режиме
/// 2) Реализует запись результатов пакетного расчета. Отдельные расходы
/// Предполагает, что в графе один компонент связности
class batch_flow_balance_analyzer {
private:
    const transport_graph_t& graph;
    graphlib::redundant_flows_corrector_settings_t redundant_flows_corrector_settings;
private:
    const std::vector<std::time_t> astro_times;
    std::vector<std::vector<graph_quantity_value_t>> flows_batch;
    std::vector<std::vector<graph_quantity_value_t>> flows_corrected;
    std::vector < std::vector<double>> flows_corrections;
    std::vector < std::vector<double>> disbalance;
public:
    /// @brief Конструктор анализатора баланса расходов
    /// @param graph Транспортный граф
    /// @param astro_times Вектор астрономических времен
    batch_flow_balance_analyzer(const transport_graph_t& graph,
        const std::vector<std::time_t>& astro_times,
        const graphlib::redundant_flows_corrector_settings_t& redundant_flows_corrector_settings)
        : graph(graph)
        , redundant_flows_corrector_settings(redundant_flows_corrector_settings)
        , astro_times(astro_times)
        , flows_batch(astro_times.size())
        , flows_corrected(astro_times.size())
        , flows_corrections(astro_times.size())
        , disbalance(astro_times.size())
    {

    }
    /// @brief Оператор вызова для обработки одного временного слоя
    /// @param index Индекс временного слоя
    /// @param flows_layer Вектор значений расхода для текущего слоя
    /// @param controls Вектор переключений объектов
    void operator()(double, std::size_t index,
        const std::vector<graph_quantity_value_t>& flows_layer,
        const std::vector<const transport_object_control_t*>& controls)
    {
        flows_batch[index] = flows_layer;

        std::vector<graph_quantity_value_t> flows_corr;

        std::tie(flows_corrected[index], flows_corrections[index], disbalance[index]) =
            graphlib::analyze_flow_balance<
            transport_model_incidences_t,
            graph_quantity_value_t
            >(graph, flows_layer, redundant_flows_corrector_settings);
    }
private:
    /// @brief Вывод в файл данных о расходах в пакетном расчете 
    void write_result_header(std::ofstream& research_outfile) const {
        std::size_t flows_count = flows_corrected.front().size();
        research_outfile << "time";
        for (std::size_t index = 1; index <= flows_count; ++index) {
            research_outfile << ";Q" << index;
            research_outfile << ";Q" << index << "_MLE";
            research_outfile << ";Q" << index << "_delta";;
        }

        std::size_t disbalance_count = disbalance.front().size();
        for (std::size_t index = 1; index <= disbalance_count; ++index) {
            research_outfile << ";R" << index;
        }
        research_outfile << std::endl;
    }
    /// @brief Вывод в файл данных о расходах в пакетном расчете 
    void write_result_row(std::ofstream& research_outfile, std::size_t time_index) const {

        std::time_t t = astro_times[time_index];

        const auto& flows = flows_batch[time_index];
        const auto& flows_mle = flows_corrected[time_index];
        const auto& flows_correction_values = flows_corrections[time_index];
        const auto& R = disbalance[time_index];

        research_outfile << UnixToString(t);
        for (std::size_t index = 0; index < flows.size(); ++index) {
            research_outfile << ";";
            research_outfile << flows[index].data.value;
            research_outfile << ";";
            research_outfile << flows_mle[index].data.value;
            research_outfile << ";";
            research_outfile << flows_correction_values[index];
        }

        for (std::size_t index = 0; index < R.size(); ++index) {
            research_outfile << ";";
            research_outfile << R[index];
        }
        research_outfile << std::endl;

    }
public:
    /// @brief Записывает результаты исследования в файл
    /// @param research_outfile_name Имя выходного файла
    /// @throws std::runtime_error Если нет данных о скорректированных расходах
    void write_result(const std::string& research_outfile_name) const {
        if (flows_corrected.empty())
            throw std::runtime_error("Nothing to write");

        std::ofstream research_outfile(research_outfile_name);
        write_result_header(research_outfile);
        for (std::size_t index = 0; index < astro_times.size(); ++index) {
            write_result_row(research_outfile, index);
        }
    }

    /// @brief Записывает скорректированные расходы в файлы
    /// @param corrected_flows_folder Папка для записи файлов
    /// @param measurement_tags Вектор тегов измерений
    /// @throws std::runtime_error Если нет данных для записи
    void write_corrected_flows(const std::string& corrected_flows_folder,
        const std::vector<tag_info_t>& measurement_tags) const
    {
        if (flows_corrected.empty())
            throw std::runtime_error("Nothing to write");

        if (measurement_tags.size() != flows_corrected.front().size())
            throw std::runtime_error("Measurement count mismatch");

        for (size_t tag_index = 0; tag_index < measurement_tags.size(); ++tag_index)
        {
            const std::string& tag_name = measurement_tags[tag_index].name;
            std::string tag_filename = corrected_flows_folder + tag_name;
            auto flow_getter = [&](size_t time_index, time_t time) {
                return flows_corrected[time_index][tag_index].data.value;
                };
            write_csv_tag_file(tag_filename, astro_times, flow_getter);
        }


    }
};


/// @brief Совокупность значений для каждого измерения 
/// (без явного указания временных меток)
using quantity_timeseries_t = std::unordered_map<
    oil_transport::graph_quantity_binding_t,
    std::vector<measurement_untyped_data_t>,
    oil_transport::graph_quantity_binding_t::hash
>;

/// @brief Совокупность индексов моментов времени для каждого измерения
/// (Ключ: объект графа + тип физической величины; 
/// Значение: Индексы моментов времени)
using quantity_timeseries_indices_t = std::unordered_map<
    oil_transport::graph_quantity_binding_t,
    std::vector<std::size_t>,
    oil_transport::graph_quantity_binding_t::hash
>;

/// @brief Возвращает общее количество измерений по всем объектам
/// @param indices Временные ряды измерений по объектам графа
inline std::size_t get_total_timeseries_size(
    const quantity_timeseries_indices_t& indices)
{
    std::size_t count = 0;
    for (const auto& [binding, indices_for_binding] : indices) {
        count += indices_for_binding.size();
    }
    return count;
}

/// @brief Базовая расчетная задача. На вход пол
template <typename PipeSolver>
class batch_endogenous_scenario {
private:
    oil_transport::transport_task_block_tree<PipeSolver>& task;
private:
    const std::vector<std::time_t> astro_times;
    quantity_timeseries_t measurements;
    quantity_timeseries_t calculations;
public:
    /// @brief Конструктор
    /// @param task Расчетная задача
    /// @param astro_times Модельная сетка в абсолютном времени
    batch_endogenous_scenario(
        oil_transport::transport_task_block_tree<PipeSolver>& task,
        const std::vector<std::time_t>& astro_times)
        : task(task)
        , astro_times(astro_times)
    {

    }
    /// @brief Выполняет расчет слоя, запоминает измерения и 
    /// @param time_step 
    /// @param index 
    /// @param measurements_layer Срез измерения для данного момента времени
    /// @param controls Управление объектами для данного момента времени
    void operator()(double time_step, std::size_t index,
        const std::vector<oil_transport::graph_quantity_value_t>& measurements_layer,
        const std::vector<const oil_transport::transport_object_control_t*>& controls
        )
    {
        // Применяем управление для данного шага
        for (const oil_transport::transport_object_control_t* control : controls) {
            if (control != nullptr) {
                task.send_control(*control);
            }
        }

        auto [flows, endo] = oil_transport::classify_measurements(measurements_layer);

        if (index == 0) {
            task.solve(flows, endo, true); // достоверность обнуляется или нет?

            measurements.clear();
            calculations.clear();
        }

        double dt = static_cast<double>(astro_times[1] - astro_times[0]);
        auto step_result = task.step(dt, flows, endo);

        // Временные ряды по всем измерениям
        for (const auto& m : endo) {
            measurements[m].push_back(m.data);
        }

        // Временные ряды флагов активности по всем эндогенным измерениям
        for (const auto& c : step_result.measurement_status.endogenous) {
            calculations[c].push_back(c.data);
        }

    }
public:
    /// @brief Возвращает временные ряды флагов активности по всем эндогенным измерениям, 
    /// использованным к настоящему моменту при расчете
    const quantity_timeseries_t& get_calculations() const {
        return calculations;
    }

    /// @brief Возвращает временные ряды по всем измерениям, 
    /// использованным к настоящему моменту при расчете
    const quantity_timeseries_t& get_measurements() const {
        return measurements;
    }
public:
    // 
    //

    /// @brief Для каждого измерения возвращает индексы только тех моментов времени,
    /// когда расчетный результат в этом объекте является достоверным (т.е. измерение переопределяет достоверный расчет)
    quantity_timeseries_indices_t get_confident_indices() const {
        quantity_timeseries_indices_t result;
        for (const auto& [binding, timeseries] : calculations)
        {
            std::vector<std::size_t> confident_indices;
            for (std::size_t index = 0; index < timeseries.size(); ++index)
            {
                const measurement_untyped_data_t& value = timeseries[index];
                bool is_confident =
                    value.status == measurement_status_t::confident_calculation;
                if (is_confident) {
                    confident_indices.push_back(index);
                }
            }
            if (!confident_indices.empty()) {
                result[binding] = std::move(confident_indices);
            }
        }
        return result;
    }

private:
    /// @brief Возвращает значения измерений в моменты времени,
    /// когда расчетные результаты являются достоверными
    /// @param confident_indices Индексы моментов достоверности по измерениям
    /// @param data Значения измерений
    static quantity_timeseries_t get_confident_timeseries(
        const quantity_timeseries_indices_t& confident_indices,
        const quantity_timeseries_t& data)
    {
        quantity_timeseries_t result;
        for (const auto& [binding, indices] : confident_indices) {
            const std::vector<measurement_untyped_data_t>& timeseries
                = data.at(binding);
            std::vector<measurement_untyped_data_t>& confident_timeseries
                = result[binding];

            for (std::size_t index : indices) {
                confident_timeseries.emplace_back(timeseries[index]);
            }
        }
        return result;
    }

public:

    /// @brief Получает достоверные расчетные значения из временных рядов
    /// @param confident_flags Индексы достоверных значений
    /// @return Временной ряд достоверных расчетных значений
    quantity_timeseries_t get_confident_calculations(
        const quantity_timeseries_indices_t& confident_flags) const
    {
        return get_confident_timeseries(confident_flags, calculations);
    }
    /// @brief Получает достоверные измеренные значения из временных рядов
    /// @param confident_flags Индексы достоверных значений
    /// @return Временной ряд достоверных измеренний
    quantity_timeseries_t get_confident_measurements(
        const quantity_timeseries_indices_t& confident_flags) const
    {
        return get_confident_timeseries(confident_flags, measurements);
    }
    /// @brief Получает достоверные векторы расчетных и измеренных значений
    /// @param confident_indices Индексы достоверных значений
    /// @return Пара векторов: первый - расчетные значения, второй - измерения
    std::pair<Eigen::VectorXd, Eigen::VectorXd> get_confident_vectors(
        const quantity_timeseries_indices_t& confident_indices) const
    {
        std::size_t count = get_total_timeseries_size(confident_indices);
        Eigen::VectorXd calc_vector(count);
        Eigen::VectorXd meas_vector(count);

        std::size_t offset = 0;

        auto confident_measurements = get_confident_measurements(confident_indices);
        auto confident_calculations = get_confident_calculations(confident_indices);

        for (const auto& [binding, calc_timeseries] : confident_calculations) {
            const auto& meas_timeseries = confident_measurements.at(binding);

            std::transform(meas_timeseries.begin(), meas_timeseries.end(),
                meas_vector.data() + offset,
                [](const measurement_untyped_data_t& data) { return data.value; }
            );
            std::transform(calc_timeseries.begin(), calc_timeseries.end(),
                calc_vector.data() + offset,
                [](const measurement_untyped_data_t& data) { return data.value; }
            );
        }

        return std::make_pair(std::move(calc_vector), std::move(meas_vector));
    }
};

/// @brief Формирует переключения для притока на основе известного временного ряда расходов
/// @param batch_measurments Временной ряд расхода
/// @param flow_epsilon Порог расхода, ниже которого приток считается отключенным
/// @return { момент времени -> переключения }
inline std::unordered_multimap<
    std::size_t,
    std::unique_ptr<oil_transport::transport_object_control_t>
> prepare_source_controls_from_flows(
    const std::vector<std::vector<oil_transport::graph_quantity_value_t>>& batch_measurments,
    double flow_epsilon = 1e-4
)
{
    using graphlib::graph_binding_t;

    std::unordered_multimap<
        std::size_t,
        std::unique_ptr<oil_transport::transport_object_control_t>
    > controls;

    std::unordered_map<graph_binding_t, bool, graph_binding_t::hash> is_zeroflow_source;

    for (std::size_t time_index = 0; time_index < batch_measurments.size(); ++time_index) {
        for (const oil_transport::graph_quantity_value_t& measurement
            : batch_measurments[time_index])
        {
            if (measurement.binding_type != graphlib::graph_binding_type::Edge1)
                continue;
            if (measurement.type != oil_transport::measurement_type::vol_flow_std) {
                continue;
            }
            bool zeroflow_flag_wasnt_initialized =
                is_zeroflow_source.find(measurement) == is_zeroflow_source.end();

            bool is_zeroflow = std::abs(measurement.data.value) < flow_epsilon;
            bool was_zeroflow = is_zeroflow_source[measurement];
            if (zeroflow_flag_wasnt_initialized || is_zeroflow != was_zeroflow)
            {
                auto control =
                    std::make_unique<oil_transport::transport_source_control_t>();
                control->binding = measurement;
                control->is_zeroflow
                    = is_zeroflow_source[measurement]
                    = is_zeroflow;
                controls.insert(std::make_pair(time_index, std::move(control)));
            }
        }
    }
    return controls;
}

/// @brief Записывает временные ряды в файл
/// @param test_folder Директория для записи
/// @param astro_times Метки времени в астрономическом времени
/// @param calculations Временные ряды
inline void write_csv_tag_files(const std::string& test_folder,
    const std::vector<std::time_t>& astro_times,
    const std::unordered_map<
    oil_transport::graph_quantity_binding_t,
    std::vector<measurement_untyped_data_t>,
    oil_transport::graph_quantity_binding_t::hash
    >& calculations)
{
    for (const auto& [quantity, timeseries] : calculations) {
        std::stringstream filename;

        filename << test_folder + "/";

        switch (quantity.binding_type) {
        case graphlib::graph_binding_type::Edge1:
            filename << "edge1=" << quantity.edge;
            break;
        case graphlib::graph_binding_type::Edge2:
            filename << "edge2=" << quantity.edge;
            break;
        case graphlib::graph_binding_type::Vertex:
            filename << "vertex=" << quantity.vertex;
            break;
        default:
            throw std::runtime_error("not impl");
        }
        filename << " type=" << (int)quantity.type;

        write_csv_tag_file(filename.str(), astro_times, timeseries);
    }
}


}
