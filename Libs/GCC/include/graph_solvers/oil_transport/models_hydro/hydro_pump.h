#pragma once

namespace oil_transport {
;

/// @brief Аппроксимация напорной характеристики для расчета обратного полинома
class head_characteristic_approximation_t : public fixed_solvers::ranged_polynom_t<vector<double>> {
private:


    double get_approximation_left_bound(const vector<double>& characteristic)
    {
        if (characteristic.size() != 4)
            throw (L"Perfomance approximation order must be 3");
        auto derivative1 = fixed_solvers::poly_differentiate(characteristic);
        auto derivative2 = fixed_solvers::poly_differentiate(derivative1);

        double a = derivative1[2];
        double b = derivative1[1];
        double c = derivative1[0];
        double D = b * b - 4 * a * c;

        double Q_border; // граница участков характеристики

        if (D < 0) {
            // нет действительных корней, производная нигде не равна нулю, нет экстремумов
            // граница диапазона - Q = 0
            // заднная зависимость для отрицательных расходов нужна, чтобы не было неконтролируемых участков
            Q_border = 0;
        }
        else {
            // граница диапазонов на максимуме, найдем его
            double x1 = (-b - sqrt(D)) / (2 * a);
            double x2 = (-b + sqrt(D)) / (2 * a);

            double x1d2 = fixed_solvers::polyval(derivative2, x1);
            double x2d2 = fixed_solvers::polyval(derivative2, x2);


            if (x1d2 < 0) {
                // x1 максимум
                Q_border = x1;
            }
            else if (x2d2 < 0) {
                // x2 максимум
                Q_border = x2;
            }
            else {
                throw std::runtime_error("Unexpected compressor performance polynom");
            }
        }

        return Q_border;
    }


    vector<fixed_solvers::function_range_t<vector<double>>> build_ranges(
        fixed_solvers::function_range_t<vector<double>> range)
    {
        typedef fixed_solvers::function_range_t<vector<double>> range_type;

        auto create_linear_function = [](double x0, double y0, double slope) ->vector<double> {
            double k = slope;
            double b = (y0 - k * x0);
            vector<double> result{ b, k };
            return result;
            };

        double Q_border = get_approximation_left_bound(range.coefficients);
        range.range_start = std::max(Q_border, range.range_start);

        double Eps_left_border = fixed_solvers::polyval(range.coefficients, range.range_start);
        double Eps_right_border = fixed_solvers::polyval(range.coefficients, range.range_end);

        range_type range_left;
        double k_left = -0.005;
        range_left.coefficients = create_linear_function(range.range_start, Eps_left_border, k_left);
        range_left.range_start = -std::numeric_limits<double>::infinity();
        range_left.range_end = range.range_start;

        range_type range_right;
        range_right.coefficients = create_linear_function(range.range_end, Eps_right_border, k_left / 600);
        range_right.range_start = range.range_end;
        range_right.range_end = std::numeric_limits<double>::infinity();

        vector<range_type> result{ range_left, range, range_right };
        return result;
    }
    
public:
    /// @brief Конструктор из коэффициентов напорной характеристики
    head_characteristic_approximation_t(
        const fixed_solvers::function_range_t<vector<double>>& coefficients)
        : fixed_solvers::ranged_polynom_t<vector<double>>(build_ranges(coefficients))
    {}
    /// @brief Конструктор по-умолчанию
    head_characteristic_approximation_t() = default;
    /// @brief Конструктор копирования
    head_characteristic_approximation_t(const head_characteristic_approximation_t&) = default;
};

/// @brief Напорные характеристики насосного агрегата
struct head_characteristics_t {
    /// @brief Коэффициенты аппроксимации напорной характеристики насоса (расход в м3/с)
    fixed_solvers::function_range_t <std::vector<double>> nominal_head_characteristic;

    /// @brief Мемоизация номинальной напорной характеристики
    mutable head_characteristic_approximation_t memo_nominal_approximation;

    /// @brief QH-характеристика для экстраполяции
    std::vector<double> head_characteristic_alternative;
    /// @brief Расход, выше которого надо экстраполировать QH-характеристику (м3/с)
    double switching_volumetric_flow_for_head_characteristic{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Коэффициенты аппроксимации КПД насоса (полином)
    std::vector<double> efficiency_characteristic;
    /// @brief Номинальные обороты (об/мин)
    double nominal_frequency{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Обточка рабочего колеса
    double rotor_wheel_reducing{ std::numeric_limits<double>::quiet_NaN() };

    /// @brief Возвращает аппроксимацию напорной характеристики
    const head_characteristic_approximation_t& get_interpolation(
            double reduced_frequency) const
    {
        if (nominal_head_characteristic.coefficients.empty())
            throw std::runtime_error("no nominal head characteristic found");

        if (!memo_nominal_approximation.ranges.empty())
            // Возвращаем ранее найденную характеристику
            return memo_nominal_approximation;

        memo_nominal_approximation = head_characteristic_approximation_t(nominal_head_characteristic);
        return memo_nominal_approximation;
    }
};


/// @brief Коэффициент местного сопротивления для прямого потока через выключенный насос
constexpr double PUMP_OFF_DIRECT_FLOW_RESISTANCE = 100.0;

/// @brief Параметры насоса для квазистационарного гидравлического расчета
struct iso_qsm_pump_parameters : public oil_transport::model_parameters_t {
    /// @brief Напорные характеристики насоса
    head_characteristics_t head_characteristics;
    /// @brief Начальная относительная частота вращения
    double initial_frequency{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Насос включен
    bool is_initially_started{ true };

    iso_qsm_pump_parameters() = default;
    /// @brief Конструктор из JSON-данных насоса
    /// @param json_data JSON-данные насоса
    iso_qsm_pump_parameters(const oil_transport::pump_json_data& json_data) {
        // TODO: Задание диапазонов (расходы в м3/ч)
        head_characteristics.nominal_head_characteristic.range_start = 0;
        head_characteristics.nominal_head_characteristic.range_end = 6000;
        head_characteristics.nominal_head_characteristic.coefficients = json_data.head_characteristic;
        head_characteristics.head_characteristic_alternative = json_data.head_characteristic_alternative;
        head_characteristics.switching_volumetric_flow_for_head_characteristic = json_data.switching_volumetric_flow_for_head_characteristic;
        head_characteristics.efficiency_characteristic = json_data.efficiency_characteristic;
        head_characteristics.nominal_frequency = json_data.nominal_frequency;
        head_characteristics.rotor_wheel_reducing = json_data.rotor_wheel_reducing;
        initial_frequency = json_data.initial_frequency;
        is_initially_started = json_data.is_initially_started;
    }

    /// @brief Расчет напора при номинальной частоте - по паспортным характеристикам
    /// @param reduced_volflow Приведенный объемный расход (м3/ч)
    /// @return Напор (м)
    double get_head_increment_at_nominal_frequency(double reduced_volflow) const {
        reduced_volflow *= 3600; // перевод в м3/час

        const auto& head_char =
            reduced_volflow < head_characteristics.switching_volumetric_flow_for_head_characteristic * 3600
            ? head_characteristics.nominal_head_characteristic.coefficients
            : head_characteristics.head_characteristic_alternative;

        double dh = 0;

        for (size_t poly_coeff = 0; poly_coeff < head_char.size(); ++poly_coeff) {
            dh += head_char[poly_coeff] * std::pow(reduced_volflow, static_cast<int>(poly_coeff));
        }

        return dh;
    }

    /// @brief Расчет напора при заданной частоте
    /// @param reduced_frequency Относительная частота вращения
    double get_head_increment(double std_volumetric_flow, double reduced_frequency) const {
        double reduced_volflow = std_volumetric_flow / (reduced_frequency * (1.0 - head_characteristics.rotor_wheel_reducing));

        double dh = std::pow(reduced_frequency * (1.0 - head_characteristics.rotor_wheel_reducing), 2.0) *
            get_head_increment_at_nominal_frequency(reduced_volflow);
        return dh;
    }

};

/// @brief Состояние объекта - насоса
struct iso_qsm_pump_state_t {
    /// @brief Насос всегда работает (включен)
    bool is_running{ false };
    /// @brief Текущая относительная частота вращения
    double current_frequency{ std::numeric_limits<double>::quiet_NaN() };

    /// @brief Конструктор состояния
    iso_qsm_pump_state_t(const iso_qsm_pump_parameters& parameters)
        : is_running(parameters.is_initially_started)
        , current_frequency(parameters.initial_frequency)
    {
    }
};

/// @brief Управление объектом - насосом
struct iso_qsm_pump_control_t : public hydraulic_object_control_t {
    /// @brief Состояние насоса (включен/выключен)
    bool is_running{ false };
};

/// @brief Стационарная изотермическая гидравлическая модель насоса
class iso_qsm_pump_model_t
    : public hydraulic_model_base_t<iso_qsm_pump_parameters, iso_qsm_pump_state_t>
{
    using base_type = hydraulic_model_base_t<iso_qsm_pump_parameters, iso_qsm_pump_state_t>;
    using base_type::state;
    using base_type::parameters;

protected:
    /// @brief Буфер гидравлических для входа ребра
    oil_transport::endohydro_values_t& buffer_in;
    /// @brief Буфер для выхода ребра
    oil_transport::endohydro_values_t& buffer_out;
    /// @brief Массив указателей на буферы для get_edge_buffer()
    mutable std::array<oil_transport::endohydro_values_t*, 2> buffers_for_getter;

public:
    /// @brief Создает гидравлическую модель насоса
    iso_qsm_pump_model_t(std::array<oil_transport::endohydro_values_t, 2>& buffer_data,
        const iso_qsm_pump_parameters& parameters)
        : base_type(parameters)
        , buffer_in(buffer_data[0])
        , buffer_out(buffer_data[1])
        , buffers_for_getter{ &buffer_in, &buffer_out }
    {
    }

    /// @brief Возвращает буфер модели
    /// Используется только для зачитки результата. Результаты расчетов будем переосмыслять
    /// Возвращает указатель на buffers_for_getter, который содержит указатели на те же объекты,
    /// на которые ссылаются buffer_in и buffer_out
    virtual void* get_edge_buffer() const override {
        return const_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(&buffers_for_getter);
    }

private:

    /// @brief Рассчитываем расход по перепаду напора
    double get_volflow(double head_increment) {

        if (state.current_frequency != 1.0)
            throw std::runtime_error("Please implement head characteristics for off-nominal frequency");

        auto& approximation = parameters.head_characteristics.get_interpolation(state.current_frequency);
        double result = approximation.get_inv_polynom_value(head_increment);
        result /= 3600; // перевод из м3/ч в м3/с
        return result;

    }

public:

    /// @brief Двустороннее ребро
    static constexpr bool is_single_side() {
        return false;
    }

    /// @brief Тип модели с точки зрения декомпозиции
    virtual graphlib::computational_type model_type() const override {
        return graphlib::computational_type::Instant;
    }

    /// @brief Получить модель одной из частей рассеченного ребра
    virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override {
        throw std::runtime_error("Pump does not support single-sided model");
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
    virtual double calculate_flow(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }
    /// @brief Производная расход висячей дуги по давлению
    virtual double calculate_flow_derivative(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Вычисляет объемный расход через насос при заданных давлениях (изотермическая модель)
    virtual double calculate_flow(double pressure_in, double pressure_out) override {
        if (!state.is_running) {
            
            double working_density = buffer_in.density_std.value; // Используем стандартную плотность как приближение
            constexpr double effective_diameter = 0.1;
            
            double flow = calc_flow_for_local_resistance(pressure_in, pressure_out, 
                buffer_in.density_std.value, effective_diameter, PUMP_OFF_DIRECT_FLOW_RESISTANCE);
            
            // Учитываем направление потока
            if (pressure_in > pressure_out) {
                flow = -flow;
            }
            
            return flow;
        }

        // Включенный насос: используем характеристику насоса
        double working_density = buffer_in.density_std.value; // Используем стандартную плотность как приближение
        
        // Вычисляем напор из перепада давления
        double head = (pressure_out - pressure_in) / (working_density * M_G);
        
        // Находим расход по напору через обратный полином
        double flow20 = get_volflow(head);
        
        return flow20;
    }

    /// @brief Вычисляет производные расхода по давлениям для насоса
    /// @param pressure_in Давление на входе
    /// @param pressure_out Давление на выходе
    /// @return Массив производных [dQ/dP_in, dQ/dP_out]
    virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) override {
        if (!state.is_running) {
            return base_type::calculate_flow_derivative(pressure_in, pressure_out);
        }

        // Включенный насос: используем численный метод для производной
        double working_density = buffer_in.density_std.value;
        double flow = calculate_flow(pressure_in, pressure_out);

        // Даем приращение по расходу относительно текущего
        double eps = 1e-8;

        double dp_calculated2 = M_G * working_density * parameters.get_head_increment(
            (flow + eps), state.current_frequency);
        double dp_calculated1 = M_G * working_density * parameters.get_head_increment(
            (flow - eps), state.current_frequency);
        double ddp = dp_calculated2 - dp_calculated1;

        double derivative = 2.0 * eps / ddp;

        return { -derivative, derivative };
    }

    /// @brief Изменяет состояние объекта
    virtual void send_control(const hydraulic_object_control_t& control) override {
        if (auto pump_control = dynamic_cast<const iso_qsm_pump_control_t*>(&control)) {
            state.is_running = pump_control->is_running;
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
