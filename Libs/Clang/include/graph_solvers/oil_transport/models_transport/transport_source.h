
namespace oil_transport {
;


/// @brief Параметры поставщика/потребителя для транспортного расчета
struct transport_source_parameters_t : public model_parameters_t
{
    /// @brief Некоторая начальное ориентировочная реология (если есть)
    pde_solvers::oil_parameters_t oil;
    /// @brief Некоторая начальноая ориентировочная температура (если есть)
    double desired_temperature{ 300 };
    /// @brief Флаг можно использовать при учете знака расхода при записи результатов
    bool is_consumer{ false };
    /// @brief Начальное состояние
    bool is_initially_zeroflow{ false };

    /// @brief Формирует структуру параметров для источника с нулевым расходом
    /// @param is_consumer Флаг, является ли источник потребителем
    static transport_source_parameters_t get_zeroflow_source(bool is_consumer) {
        transport_source_parameters_t result;
        result.is_initially_zeroflow = true;
        result.is_consumer = is_consumer;
        return result;
    }

    transport_source_parameters_t() = default;
    /// @brief Конструктор из JSON-данных источника
    /// @param json_parameters JSON-данные источника
    transport_source_parameters_t(const source_json_data& json_parameters)
    {
        is_initially_zeroflow = json_parameters.is_initially_zeroflow;
        oil.density.nominal_density = json_parameters.density;
    }

};

/// @brief Состояние притока/отбора
struct transport_source_state_t  {
    /// @brief Текущее состояние
    bool is_zeroflow;
    /// @brief Ориентировочная реология, которую можно менять во время расчета
    pde_solvers::oil_parameters_t oil;
    /// @brief Ориентировочная температура, которую возможно изменить во время расчета
    double temperature{ std::numeric_limits < double >::quiet_NaN() };

    /// @brief Конструктор состояния из параметров
    /// @param parameters Параметры источника
    transport_source_state_t(const transport_source_parameters_t& parameters)
    {
        is_zeroflow = parameters.is_initially_zeroflow;
        oil = parameters.oil;
    }
};

/// @brief Управление притоком
struct transport_source_control_t : public transport_object_control_t {
    /// @brief Новое состояние
    bool is_zeroflow{false};
    /// @brief Новая ориентировочная плотность на притоке
    double density_std{ std::numeric_limits < double >::quiet_NaN() };
    /// @brief Новая ориентировочная вязкость на притоке
    double viscosity_working{ std::numeric_limits < double >::quiet_NaN() };
    /// @brief Новая ориентировочная температура на притоке
    double temperature{ std::numeric_limits < double >::quiet_NaN() };
};


/// @brief Транспортная модель поставщика/потребителя
class transport_source_model_t
    : public oil_transport_model_base_t<transport_source_parameters_t, transport_source_state_t>
{
    using oil_transport_model_base_t<transport_source_parameters_t, transport_source_state_t>::state;
protected:
    /// @brief Эндогенные свойства на выходе (для поставщика) или входе (для потребителя) одностороннего ребра
    transport_values& buffer_data;
public:
    /// @brief Поставщику и потребителю соответствуют односторонние ребра
    static constexpr bool is_single_side() {
        return true;
    }
    /// @brief Используется для инициализации zeroflow поставщиков на буфере задвижки - buffer_data
    /// Вызывает конструктор базовой модели, позволяющий не указывать буфер
    transport_source_model_t(transport_values& buffer_data,
        const transport_source_parameters_t& parameters,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
        : oil_transport_model_base_t(parameters, endogenous_selector)
        , buffer_data(buffer_data)
    { }

    /// @brief Возвращает буфер модели
    virtual void* get_edge_buffer() const override {
        return &buffer_data;
    }


    /// @brief Создает транспортную модель поставщика/потребителя с заданным буфером и параметрами
    /// @param buffer Буфер
    /// @param parameters Параметры поставщика/потребителя
    //transport_source_model_t(oil_transport_buffer_t& buffer, 
    //    const transport_source_parameters_t& parameters,
    //    const pde_solvers::endogenous_selector_t& endogenous_selector)
    //    : oil_transport_model_base_t(buffer, parameters, endogenous_selector)
    //    , buffer_data(std::get<transport_buffer_helpers::edge_buffer_single_sided_index>(buffer))
    //{ }

    virtual bool is_zero_flow() const override {
        return state.is_zeroflow;
    }
    /// @brief Для односторонних моделей всегда Instant
    virtual graphlib::computational_type model_type() const override {
        return graphlib::computational_type::Instant;
    }
    /// @brief Не имеет смысла для поставщика, может быть вызван только по ошибке, кидает исключения
    virtual oil_transport_model_t* get_single_sided_model(bool is_end_side) override {
        throw std::runtime_error("Source cannot have single-sided model");
    }

    /// @brief Распостранение эндогенных свойств на вход (для поставщика) 
    /// или выход (для потребителя) одностороннего ребра
    /// @param time_step Временной шаг
    /// @param volumetric_flow Расход для определения начала и конца ребра
    /// @param endogenous_input Эндогенные свойства с предшествующего ребра
    /// @param endogenous_measurements Измерения, из которых могут быть переопределены расчетные значения эндогенных свойств
    virtual void propagate_endogenous(
        double time_step,
        double volumetric_flow,
        const pde_solvers::endogenous_values_t* endogenous_input,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        std::vector<graph_quantity_value_t>* overriden_with_measurements) override
    {
        pde_solvers::endogenous_values_t calculated_endogenous;
        if (endogenous_input == nullptr) {
            // работа в режиме поставщика, 
            // заданные параметры считаем недостоверными - это значения, средние по больнице
            if (endogenous_selector.density_std) {
                calculated_endogenous.density_std.value = state.oil.density.nominal_density;
                calculated_endogenous.density_std.confidence = false;
            }
            if (endogenous_selector.viscosity_working) {
                calculated_endogenous.viscosity_working.value = state.oil.viscosity.nominal_viscosity;
                calculated_endogenous.viscosity_working.confidence = false;
            }
        }
        else {
            // Если есть рассчитанные эндогенные с предыдущего ребра
            // работа в режиме потребителя
            // Приходить могут как достоверные, так и недостоверные значения
            // копируем значения параметров и их достоверность
            calculated_endogenous = *endogenous_input;
        }

        if (endogenous_selector.sulfur || endogenous_selector.temperature || endogenous_selector.improver ||
            endogenous_selector.viscosity0 || endogenous_selector.viscosity20 || endogenous_selector.viscosity50)
        {
            throw std::runtime_error("Endogenous parameter propagation is not implemented");
        }

        // Переопределяем рассчитанные эндогенные свойства значениями из измерений
        overwrite_endogenous_values_from_measurements(
            endogenous_measurements, overriden_with_measurements, &calculated_endogenous);

        // Обновляем только эндогенные свойства в буфере
        static_cast<pde_solvers::endogenous_values_t&>(buffer_data) = calculated_endogenous;
    }

    /// @brief Возвращает эндогенные свойств на единственном конце ребра
    /// @param volumetric_flow - Расход (не используется, т.к. ребро одностороннее)
    virtual pde_solvers::endogenous_values_t get_endogenous_output(double volumetric_flow) const override
    {
        return buffer_data;
    }
    /// @brief Обработка transport_source_control_t
    virtual void send_control(const transport_object_control_t& control) override {
        if (auto source_control = dynamic_cast<const transport_source_control_t*>(&control)) {
            state.is_zeroflow = source_control->is_zeroflow;
            state.oil.density.nominal_density = source_control->density_std;
            state.oil.viscosity.nominal_viscosity = source_control->viscosity_working;
        }
    }

    /// @brief Запись значения расхода в буфер модели
    virtual void update_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        buffer_data.vol_flow = flow;
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера гидравлической модели источника/потребителя
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        return buffer_data.vol_flow;
    }

};


}