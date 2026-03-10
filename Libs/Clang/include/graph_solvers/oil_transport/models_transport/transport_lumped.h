

namespace oil_transport {
;

/// @brief Параметры универсального сосредоточенного объекта
struct transport_lumped_parameters_t : public model_parameters_t
{
    /// @brief Начальное положение откр/закр
    bool is_initially_opened{ true };

    transport_lumped_parameters_t() = default;
    /// @brief Конструктор из JSON-данных сосредоточенного объекта
    /// @param json_parameters JSON-данные сосредоточенного объекта
    transport_lumped_parameters_t(const lumped_json_data& json_parameters) {
        is_initially_opened = json_parameters.is_initially_opened;
    }

};

/// @brief Состояние сосредоточенного объекта
struct transport_lumped_state_t {
    /// @brief Текущее положение откр/закр
    bool is_opened;
    /// @brief Инициализация состояния в начале моделирования - берем начальное положение откр/закр
    transport_lumped_state_t(const transport_lumped_parameters_t& parameters)
    {
        is_opened = parameters.is_initially_opened;
    }
    /// @brief Пока заглушка, чтобы вызывалось из объектов, из неканонической транспортной онтологии. 
    /// Грохнуть после удаление тех объектов
    template <typename T>
    transport_lumped_state_t(const T& parameters)
    {
        throw std::runtime_error("Please, implement me");
    }


};

/// @brief Управление задвижкой
struct transport_lumped_control_t : public transport_object_control_t {
    /// @brief Новое положение откр/закр
    bool is_opened;
};



/// @brief Базовая реализация сосредоточенной двусторонней модели 
/// TODO: В будущем перенести сюда все
template <typename ModelParameters, typename ModelState = transport_lumped_state_t>
class oil_transport_lumped_model2_t
    : public oil_transport_model_base_t<ModelParameters, ModelState>
{
protected:
    /// @brief Эндогенные свойства на входе
    transport_values& buffer_in;
    /// @brief Эндогенные свойства на выходе
    transport_values& buffer_out;
    /// @brief Указатели на те же объекты, на которые ссылаются buffer_in и buffer_out
    /// Используется для получения буферов через get_edge_buffer()
    mutable std::array<transport_values*, 2> buffers_for_getter;
protected:
    /// @brief Параметры источника с нулевым расходом, 
    /// образующегося при рассечении ребра
    transport_source_parameters_t zeroflow_source_parameters{ transport_source_parameters_t::get_zeroflow_source(true) };
    /// @brief Модель источника с нулевым расходом для рассеченного ребра
    transport_source_model_t zeroflow_source_model;
    /// @brief Модель потребителя с нулевым расходом для рассеченного ребра
    transport_source_model_t zeroflow_sink_model;
public:
    /// @brief Двустороннее ребро
    static constexpr bool is_single_side() {
        return false;
    }
    /// @brief Модель двусторонняя, этот метод не имеет смысла
    virtual bool is_zero_flow() const {
        throw std::logic_error("Must not be called for two-sided object");
    }
    /// @brief Создает транспортную модель расходомера с заданным буфером и параметрами модели
    /// @param buffer Буфер
    /// @param parameters Параметры объекта
    oil_transport_lumped_model2_t(transport_values& buffer_in, transport_values& buffer_out,
        const ModelParameters& parameters, const pde_solvers::endogenous_selector_t& endogenous_selector)
        : oil_transport_model_base_t<ModelParameters, ModelState>(parameters, endogenous_selector)
        , buffer_in(buffer_in)
        , buffer_out(buffer_out)
        , buffers_for_getter{ &buffer_in, &buffer_out }
        , zeroflow_source_model(buffer_in, zeroflow_source_parameters, endogenous_selector)
        , zeroflow_sink_model(buffer_out, zeroflow_source_parameters, endogenous_selector)
    {
    }
    /// @brief Возвращает буфер модели
    /// Используется только для зачитки результата. Результаты расчетов будем переосмыслять
    /// Возвращает указатель на buffers_for_getter, который содержит указатели на те же объекты,
    /// на которые ссылаются buffer_in и buffer_out
    virtual void* get_edge_buffer() const override {
        return &buffers_for_getter;
    }

    /// @brief Рассекается при закрытии
    virtual graphlib::computational_type model_type() const override {
        if (this->state.is_opened) {
            return graphlib::computational_type::Instant;
        }
        else {
            return graphlib::computational_type::Splittable;
        }
    }
    /// @brief Не имеет смысла для поставщика, может быть вызван только по ошибке, кидает исключения
    virtual oil_transport_model_t* get_single_sided_model(bool is_end_side) override {
        if (is_end_side == false) {
            // вход задвижки - это потребитель Q=0
            return &zeroflow_sink_model; 
        }
        else {
            // выход задвижки - это поставщик Q=0
            return &zeroflow_source_model;
        }
    }

    /// @brief Распространяет эндогенные свойства на вход и выход ребра
    /// @param time_step Временной шаг
    /// @param volumetric_flow Расход для определения начала и конца ребра
    /// @param endogenous_input Эндогенные свойства с предшествующего ребра
    /// @param endogenous_measurements Измерения, из которых могут быть переопределены расчетные значения эндогенных свойств
    virtual void propagate_endogenous(
        double time_step,
        double volumetric_flow,
        const pde_solvers::endogenous_values_t* endogenous_input,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        std::vector<graph_quantity_value_t>* overwritten_with_measurements) override
    {
        pde_solvers::endogenous_values_t calculated_endogenous;
        if (endogenous_input != nullptr) { 
            calculated_endogenous = *endogenous_input;
        }

        // Переопределяем рассчитанные эндогенные свойства значениями из измерений
        overwrite_endogenous_values_from_measurements(
            endogenous_measurements, overwritten_with_measurements,
            &calculated_endogenous);

        // Сосредоточнный нерассекаемый объект, эндогенные свойства на входе и выходе равны
        // Обновляем только эндогенные свойства в буфере
        static_cast<pde_solvers::endogenous_values_t&>(buffer_in) = calculated_endogenous;
        static_cast<pde_solvers::endogenous_values_t&>(buffer_out) = calculated_endogenous;
    }

    /// @brief Возвращает эндогенные свойства на выходе ребра
    /// @param volumetric_flow - расход через ребро для определения направления течения
    /// @return Эндогенные свойства
    virtual pde_solvers::endogenous_values_t get_endogenous_output(double volumetric_flow) const override
    {
        // Эндогенные свойства на обоих концах ребра
        if (volumetric_flow >= 0)
            return (buffer_out);
        else
            return (buffer_in);
    }

    /// @brief Запись значения расхода в буфер модели
    virtual void update_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        // Для двусторонней модели устанавливаем расход в оба буфера
        buffer_in.vol_flow = flow;
        buffer_out.vol_flow = flow;
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера гидравлической модели источника/потребителя
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        // Для двусторонней модели расход должен быть одинаковым на входе и выходе
        // Возвращаем расход из выходного буфера
        return buffer_out.vol_flow;
    }
};




/// @brief Транспортная модель универсального сосредоточенного объекта
class transport_lumped_model_t
    : public oil_transport_lumped_model2_t<transport_lumped_parameters_t>
{
public:
    /// @brief Создает транспортную модель универсального сосредоточенного объекта
    transport_lumped_model_t(transport_values& buffer_in, transport_values& buffer_out,
        const transport_lumped_parameters_t& parameters,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
        : oil_transport_lumped_model2_t(buffer_in, buffer_out, parameters, endogenous_selector)
    {
    }
    virtual void send_control(const transport_object_control_t& control) override {

        if (auto lumped_control = dynamic_cast<const transport_lumped_control_t*>(&control)) {
            this->state.is_opened = lumped_control->is_opened;
        }
        else {
            throw std::runtime_error("Please, implement me");
        }
    }

};




}