
namespace oil_transport {
;

/// @brief Эффективный диаметр (м) при "закрытом" обратном клапане
constexpr double CHECK_VALVE_CLOSED_EFFECTIVE_DIAMETER = 0.1;
/// @brief Гидравлическое сопротивление при "закрытом" обратном клапане
constexpr double CHECK_VALVE_CLOSED_HYDRAULIC_RESISTANCE = 1e8;

/// @brief Параметры обратного клапана для квазистационарного гидравлического расчета
struct iso_qsm_check_valve_parameters : public oil_transport::model_parameters_t {
    /// @brief Коэффициент местного сопротивления для полностью открытого ОК
    double hydraulic_resistance{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Диаметр сечения
    double diameter{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Условный диаметр на диаметр трубопровода (желаемое положение)
    double opening{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Начальное состояние в статике
    fixed_solvers::nullable_bool_t is_initially_closed_static { fixed_solvers::nullable_bool_t::Undefined };

    iso_qsm_check_valve_parameters() = default;

    /// @brief Конструктор из JSON-данных обратного клапана
    iso_qsm_check_valve_parameters(const oil_transport::check_valve_json_data& json_data) 
    {
        hydraulic_resistance = json_data.hydraulic_resistance;
        diameter = json_data.diameter;
        opening = json_data.opening;
        is_initially_closed_static = json_data.is_initially_closed_static;
    }

};

/// @brief Состояние объекта - обратного клапана
struct iso_qsm_check_valve_state_t {
    /// @brief Текущее положение
    double actual_opening{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Желаемое положение
    double desired_opening{ std::numeric_limits<double>::quiet_NaN() };

    /// @brief Конструктор состояния из параметров
    iso_qsm_check_valve_state_t(const iso_qsm_check_valve_parameters& parameters)
        : actual_opening(parameters.opening)
        , desired_opening(parameters.opening)
    {
    }
};

/// @brief Базовый класс для гидравлических моделей обратного клапана
template <typename ModelParameters>
class iso_qsm_check_valve_model_base_t
    : public hydraulic_model_base_t<ModelParameters, iso_qsm_check_valve_state_t>
{
protected:
    /// @brief Базовый тип - модель гидравлического объекта с параметрами и состоянием
    using base_type = hydraulic_model_base_t<ModelParameters, iso_qsm_check_valve_state_t>;
    using base_type::state;
    using base_type::parameters;

public:
    /// @brief Двустороннее ребро
    static constexpr bool is_single_side() {
        return false;
    }

    /// @brief Конструктор
    iso_qsm_check_valve_model_base_t(const ModelParameters& parameters)
        : base_type(parameters)
    {
    }

    /// @brief Тип модели с точки зрения декомпозиции — обратный клапан всегда Splittable
    virtual graphlib::computational_type model_type() const override 
    {
        if (parameters.is_initially_closed_static == fixed_solvers::nullable_bool_t::True) {
            return graphlib::computational_type::Splittable;
        }
        return graphlib::computational_type::Instant;
    }

    /// @brief Получить модель одной из частей рассеченного ребра (дефолт — исключение; реализация в производном классе)
    virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override 
    {
        throw std::runtime_error("Check valve does not support single-sided model");
    }
};

/// @brief Стационарная изотермическая гидравлическая модель обратного клапана
class iso_qsm_check_valve_model_t
    : public iso_qsm_check_valve_model_base_t<iso_qsm_check_valve_parameters>
{
    /// @brief Базовый тип - модель обратного клапана с базовой функциональностью
    using base_type = iso_qsm_check_valve_model_base_t<iso_qsm_check_valve_parameters>;

protected:
    /// @brief Буфер гидравлических для входа ребра
    oil_transport::endohydro_values_t& buffer_in;
    /// @brief Буфер для выхода ребра
    oil_transport::endohydro_values_t& buffer_out;
    /// @brief Массив указателей на буферы для get_edge_buffer()
    mutable std::array<oil_transport::endohydro_values_t*, 2> buffers_for_getter;

    /// @brief Параметры источника с нулевым расходом при рассечении ребра (по аналогии с oil_transport_lumped_model2_t)
    qsm_hydro_source_parameters zeroflow_source_parameters{ qsm_hydro_source_parameters::get_zeroflow_source(true) };
    /// @brief Модель источника с нулевым расходом для рассеченного ребра (выход обратного клапана)
    qsm_hydro_source_model_t zeroflow_source_model;
    /// @brief Модель потребителя с нулевым расходом для рассеченного ребра (вход обратного клапана)
    qsm_hydro_source_model_t zeroflow_sink_model;

public:
    /// @brief Создает гидравлическую модель обратного клапана
    iso_qsm_check_valve_model_t(std::array<oil_transport::endohydro_values_t, 2>& buffer_data,
        const iso_qsm_check_valve_parameters& parameters)
        : base_type(parameters)
        , buffer_in(buffer_data[0])
        , buffer_out(buffer_data[1])
        , buffers_for_getter{ &buffer_in, &buffer_out }
        , zeroflow_source_model(buffer_in, zeroflow_source_parameters)
        , zeroflow_sink_model(buffer_out, zeroflow_source_parameters)
    {
    }

    /// @brief Получить модель одной из частей рассеченного ребра (вход — потребитель Q=0, выход — поставщик Q=0)
    virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override 
    {
        if (is_end_side == false) {
            return &zeroflow_sink_model;
        }
        return &zeroflow_source_model;
    }

    /// @brief Возвращает буфер модели
    virtual void* get_edge_buffer() const override {
        return const_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(&buffers_for_getter);
    }

    /// @brief Модель двусторонняя, этот метод не имеет смысла
    virtual bool is_zero_flow() const override {
        throw std::logic_error("Must not be called for two-sided object");
    }

    /// @brief Возвращает известное давление для P-притоков
    virtual double get_known_pressure() override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает известный расход для Q-притоков
    virtual double get_known_flow() override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Расход висячей дуги при заданном давлении
    virtual double calculate_flow(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Производная расхода висячей дуги по давлению
    virtual double calculate_flow_derivative(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Вычисляет расход через обратный клапан при заданных давлениях
    virtual double calculate_flow(double pressure_in, double pressure_out) override 
    {
        const double density = buffer_in.density_std.value;

        double flow = calc_flow_for_local_resistance(
            pressure_in, pressure_out, density, parameters.diameter, parameters.hydraulic_resistance);

        return flow;
    }

    /// @brief Обратный клапан в статической модели не поддерживает управление
    virtual void send_control(const hydraulic_object_control_t& control) override {
        throw std::runtime_error("Check valve control is not supported");
    }

    /// @brief Запись значения расхода в буферы модели (двусторонний объект)
    virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        buffer_in.vol_flow = flow;
        buffer_out.vol_flow = flow;
    }

    /// @brief Запись давлений на входе и выходе в буферы модели
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
        buffer_in.pressure = pressure_in;
        buffer_out.pressure = pressure_out;
    }

    /// @brief Для двусторонней модели запись одного давления не определена
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) override {
        return (is_end_side) ? buffer_out.pressure : buffer_in.pressure;
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        return buffer_in.vol_flow;
    }
};

}

