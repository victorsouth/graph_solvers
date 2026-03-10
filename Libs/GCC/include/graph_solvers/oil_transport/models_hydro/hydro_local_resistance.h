
namespace oil_transport {
;

/// @brief Нижняя граница открытия для расчета эффективного диаметра
constexpr double VALVE_OPENING_BORDER = 1e-3;

/// @brief Проверка, открыт ли клапан
inline bool is_valve_opened(double opening) {
    return opening > VALVE_OPENING_BORDER;
}

/// @brief Расчет расхода через местное сопротивление по известным давлениям
/// @param hydraulic_resistance Коэффициент гидравлического сопротивления
/// @return Объемный расход
inline double calc_flow_for_local_resistance(
    double pressure_in,
    double pressure_out,
    double density,
    double effective_diameter,
    double hydraulic_resistance)
{
    const double dp = pressure_in - pressure_out;
    const double S = pde_solvers::circle_area(effective_diameter);
    const double v = fixed_solvers::ssqrt(2.0 * dp / (hydraulic_resistance * density));
    double flow = v * S;
    return flow;
}

/// @brief Расчет перепада давления через местное сопротивление при известном расходе
/// @param hydraulic_resistance Коэффициент гидравлического сопротивления
/// @return Потери давления (dp = sign(Q) * xi * rho / 2 * (Q/S)^2)
inline double calc_pressure_loss_for_local_resistance(
    double volumetric_flow,
    double density,
    double effective_diameter,
    double hydraulic_resistance)
{
    const double S = pde_solvers::circle_area(effective_diameter);
    const double v = volumetric_flow / S;
    const double dp = (hydraulic_resistance * density * v * v) / 2.0;
    return (volumetric_flow > 0) ? dp : -dp;
}

/// @brief Параметры локального сопротивления для квазистационарного гидравлического расчета
struct qsm_hydro_local_resistance_parameters : public oil_transport::model_parameters_t {
    /// @brief Коэффициент местного сопротивления
    double xi{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Диаметр проходного сечения
    double diameter{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Начальная степень открытия
    double initial_opening{ std::numeric_limits<double>::quiet_NaN() };

    qsm_hydro_local_resistance_parameters() = default;
    /// @brief Конструктор из JSON-данных клапана
    /// @param json_data JSON-данные клапана
    qsm_hydro_local_resistance_parameters(const oil_transport::valve_json_data& json_data) {
        xi = json_data.xi;
        diameter = json_data.diameter;
        initial_opening = json_data.initial_opening;
    }
};


/// @brief Состояние объекта - гидравлического сопротивления
struct qsm_hydro_local_resistance_state_t {
    /// @brief Текущая степень открытия
    double opening { std::numeric_limits<double>::quiet_NaN() };

    /// @brief Является ли задвижка открытой (в т.ч. частично)
    bool is_opened() const {
        return opening > 0;
    }

    /// @brief Инициализация состояния в начале моделирования
    qsm_hydro_local_resistance_state_t(const qsm_hydro_local_resistance_parameters& parameters)
    {
        opening = parameters.initial_opening;
    }
};

/// @brief Управление объектом - гидравлическим сопротивлением
struct qsm_hydro_local_resistance_control_t : public hydraulic_object_control_t {
    /// @brief Новая степень открытия задвижки/заслонки
    double opening{ std::numeric_limits<double>::quiet_NaN() };
};


/// @brief Базовый класс для гидравлических моделей типа местное сопротивление
/// @tparam ModelParameters Тип параметров модели
/// @tparam ClosingRelationType Тип замыкающего соотношения (closing_relation2)
template <typename ModelParameters>
class qsm_hydro_local_resistance_model_base_t
    : public hydraulic_model_base_t<ModelParameters, qsm_hydro_local_resistance_state_t>
{
protected:
    /// @brief Тип базового класса
    using base_type = hydraulic_model_base_t<ModelParameters, qsm_hydro_local_resistance_state_t>;
    using base_type::state;
    using base_type::parameters;

public:
    /// @brief Двустороннее ребро
    static constexpr bool is_single_side() {
        return false;
    }

    /// @brief Конструктор
    qsm_hydro_local_resistance_model_base_t(const ModelParameters& parameters)
        : base_type(parameters)
    {
    }

    /// @brief Тип модели с точки зрения декомпозиции
    virtual graphlib::computational_type model_type() const override {
        if (this->state.is_opened()) {
            return graphlib::computational_type::Instant;
        }
        else {
            return graphlib::computational_type::Splittable;
        }
    }

    /// @brief Получить модель одной из частей рассеченного ребра
    virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override {
        // TODO: Реализовать создание односторонних моделей для закрытых объектов
        throw std::runtime_error("Not implemented yet");
    }
};

/// @brief Гидравлическая модель универсального сосредоточенного объекта
class qsm_hydro_local_resistance_model_t
    : public qsm_hydro_local_resistance_model_base_t<qsm_hydro_local_resistance_parameters>
{
    using base_type = qsm_hydro_local_resistance_model_base_t<qsm_hydro_local_resistance_parameters>;

    protected:
        /// @brief Буфер гидравлических для выхода ребра
        oil_transport::endohydro_values_t& buffer_in;
        /// @brief Буфер для входа ребра
        oil_transport::endohydro_values_t& buffer_out;
        /// @brief Массив указателей на буферы для get_edge_buffer()
        mutable std::array<oil_transport::endohydro_values_t*, 2> buffers_for_getter;
    protected:
        /// @brief Параметры источника с нулевым расходом, 
        /// образующегося при рассечении ребра
        qsm_hydro_source_parameters zeroflow_source_parameters{ qsm_hydro_source_parameters::get_zeroflow_source(true) };
        /// @brief Модель источника с нулевым расходом для рассеченного ребра
        qsm_hydro_source_model_t zeroflow_source_model;
        /// @brief Модель потребителя с нулевым расходом для рассеченного ребра
        qsm_hydro_source_model_t zeroflow_sink_model;

    public:
        // TODO два буфера будут прилетать в констуктор.
        /// @brief Создает гидравлическую модель универсального сосредоточенного объекта
        qsm_hydro_local_resistance_model_t(std::array<oil_transport::endohydro_values_t, 2>& buffer_data,
            const qsm_hydro_local_resistance_parameters& parameters)
            : base_type(parameters)
            , buffer_in(buffer_data[0])
            , buffer_out(buffer_data[1])
            , buffers_for_getter{ &buffer_in, &buffer_out }
            , zeroflow_source_model(buffer_data[0], zeroflow_source_parameters)
            , zeroflow_sink_model(buffer_data[1], zeroflow_source_parameters)
        {
        }

    /// @brief Возвращает буфер модели
    /// Используется только для зачитки результата. Результаты расчетов будем переосмыслять
    /// Возвращает указатель на buffers_for_getter, который содержит указатели на те же объекты,
    /// на которые ссылаются buffer_in и buffer_out
    virtual void* get_edge_buffer() const override {
        return const_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(&buffers_for_getter);
    }


public:

    /// @brief Тип модели с точки зрения декомпозиции
    virtual graphlib::computational_type model_type() const override {
        if (this->state.is_opened()) {
            return graphlib::computational_type::Instant;
        }
        else {
            return graphlib::computational_type::Splittable;
        }
    }

        /// @brief Получить модель одной из частей рассеченного ребра
        virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override {
            if (is_end_side == false) {
                // вход задвижки - это потребитель Q=0
                return &zeroflow_sink_model;
            }
            else {
                // выход задвижки - это поставщик Q=0
                return &zeroflow_source_model;
            }
        }

    /// @brief Модель двусторонняя, этот метод не имеет смысла
    virtual bool is_zero_flow() const override {
        throw std::logic_error("Must not be called for two-sided object");
    }

    /// @brief Возвращает известное давление для P-притоков. 
    /// Если объект не является P-притоком, возвращает NaN
    virtual double get_known_pressure() override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает известный расход для Q-притоков. 
    /// Если объект не является Q-притоком, возвращает NaN
    /// Тип расхода (объемный, массовый) не регламентируется и решается на уровне реализации замыкающих соотношений
    /// Нужно, чтобы все замыкающие соотношения подразумевали один и тот же вид расхода
    virtual double get_known_flow() override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Расход висячей дуги при заданном давлении
    virtual double calculate_flow(double pressure)  override {
        throw std::runtime_error("Must not be called for edge type II");
    }
    /// @brief Производная расход висячей дуги по давлению
    virtual double calculate_flow_derivative(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    // TODO: убрать после добавления базоовго класса для hydro_edge1 и hydro_edge2
    using base_type::calculate_flow_derivative;

    /// @brief Вычисляет расход через локальное сопротивление при заданных давлениях
    /// @param pressure_in Давление на входе
    /// @param pressure_out Давление на выходе
    /// @return Расход через локальное сопротивление
    virtual double calculate_flow(double pressure_in, double pressure_out) override {
        // TODO: density_working нужна здесь
        double density = buffer_in.density_std.value;

        double flow = calc_flow_for_local_resistance(
            pressure_in, pressure_out, density, parameters.diameter, parameters.xi);

        return flow;
    }

    /// @brief Изменяет состояние объекта
    virtual void send_control(const hydraulic_object_control_t& control) override {
        if (auto resistance_control = dynamic_cast<const qsm_hydro_local_resistance_control_t*>(&control)) {
            state.opening = resistance_control->opening;
        }
        else {
            throw std::runtime_error("Wrong control type");
        }
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
    /// @param incidence +1 - выход ребра. -1 - вход ребра.
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) override {
        return (is_end_side) ? buffer_out.pressure : buffer_in.pressure;
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера входа (совпадает с выходом для установившегося режима)
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        return buffer_in.vol_flow;
    }
};

}
