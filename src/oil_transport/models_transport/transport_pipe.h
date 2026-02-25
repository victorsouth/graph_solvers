

namespace oil_transport {
;

/// @brief Параметры трубы для квазистационарного транспортного расчета
struct qsm_pipe_transport_parameters_t {
    /// @brief Профиль трубы
    pde_solvers::pipe_profile_t profile;
    /// @brief диаметр
    double diameter{std::numeric_limits<double>::quiet_NaN()};
    /// @brief Коэффициент теплообмена с окружающей средой
    double ambient_heat_transfer{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Температура окружающей среды
    double ambient_temperature{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Объекто со значениями по умолчанию
    static qsm_pipe_transport_parameters_t default_values() {
        qsm_pipe_transport_parameters_t result;
        double length = 5000;
        double dx = 200;
        result.profile.coordinates = 
            pde_solvers::pipe_profile_uniform::generate_uniform_grid(0.0, length, dx);
        result.diameter = 1;
        result.ambient_heat_transfer = 1.25; // задачник Лурье, задача 147, стр. 99

        return result;
    }
    qsm_pipe_transport_parameters_t() = default;
    /// @brief Конструктор из JSON-данных трубы
    /// @param json_data JSON-данные трубы
    qsm_pipe_transport_parameters_t(const pde_solvers::pipe_json_data& json_data)
    {
        *this = qsm_pipe_transport_parameters_t::default_values();
        profile.coordinates = { json_data.x_start, json_data.x_end };
        diameter = json_data.diameter;
    }
    /// @brief Преобразовывает профиль под равномерный шаг по координате
    void make_uniform_profile(double desired_dx) {
        profile.coordinates = pde_solvers::pipe_profile_uniform::generate_uniform_grid(
            profile.coordinates.front(), profile.get_length(), desired_dx
        );
    }
};


/// @brief Параметры трубы для транспортного расчета
template <typename SolverPipeParameters>
struct transport_pipe_parameters_t 
    : public model_parameters_t
    , public SolverPipeParameters
{
    transport_pipe_parameters_t() = default;
    /// @brief Конструктор из базовых параметров трубы
    /// @param base_pipe_parameters Базовые параметры трубы для солвера
    transport_pipe_parameters_t(const SolverPipeParameters& base_pipe_parameters)
        : SolverPipeParameters(base_pipe_parameters)
    {

    }
};



/// @brief Результат по трубе
template <typename PipeSolver>
struct pipe_common_result_t {
    /// @brief Профиль координат
    const std::vector<double>* coordinates{ nullptr };
    /// @brief Текущий расчетный слой
    const typename PipeSolver::layer_type* layer{ nullptr };
};

/// @brief Солвер для уравнения адвекции
class qsm_pipe_transport_solver_t : public pde_solvers::pipe_solver_transport_interface_t {
public:
    /// @brief Тип слоя для расчета эндогенных параметров
    using layer_type = pde_solvers::pipe_endogenous_calc_layer_t;
    /// @brief Тип буфера для хранения слоев
    using buffer_type = pde_solvers::ring_buffer_t<pde_solvers::pipe_endogenous_calc_layer_t>;
    /// @brief Тип параметров трубы
    using pipe_parameters_type = qsm_pipe_transport_parameters_t;

private:
    /// @brief Параметры трубы
    const qsm_pipe_transport_parameters_t& pipe;
    /// @brief Буфер с профилями свойств
    pde_solvers::ring_buffer_t<pde_solvers::pipe_endogenous_calc_layer_t>& buffer;
    /// @brief Селектор эндогенных параметров - какие профили считать не надо
    pde_solvers::endogenous_selector_t endogenous_selector;
public:
    /// @brief Создает солвер, использующий переданный буффер для транспортного расчета трубы с переданными свойствами
    /// @param pipe Параметры трубы
    /// @param buffer Буфер
    qsm_pipe_transport_solver_t(
        const qsm_pipe_transport_parameters_t& pipe,
        pde_solvers::ring_buffer_t<pde_solvers::pipe_endogenous_calc_layer_t>& buffer,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
        : pipe(pipe)
        , buffer(buffer)
        , endogenous_selector(endogenous_selector)
    {
    }
    /// @brief Геттер для текущего слоя  
    const pde_solvers::pipe_endogenous_calc_layer_t& get_current_layer() const {
        return buffer.current();
    }

public:
    /// @brief Расчет адвекции эндогенных свойств в трубе (конечный шаг по времени)
    virtual void transport_step(double dt, double volumetric_flow, const pde_solvers::endogenous_values_t& boundaries) override
    {
        if (!std::isfinite(dt)) {
            throw std::runtime_error("transport_step: dt must be finite; use transport_solve for steady-state (infinite dt)");
        }
        buffer.current().std_volumetric_flow = volumetric_flow;

        auto step_advection = [this](double dt, double volumetric_flow,
            const pde_solvers::endogenous_confident_value_t& boundary_value,
            auto value_getter, auto confidence_getter)
        {
            pde_solvers::step_advection(dt, volumetric_flow, boundary_value,
                pipe.diameter, pipe.profile.coordinates, buffer,
                value_getter, confidence_getter);
        };

        if (endogenous_selector.density_std) {
            step_advection(dt, volumetric_flow, boundaries.density_std,
                pde_solvers::pipe_endogenous_calc_layer_t::get_density_std_wrapper,
                pde_solvers::pipe_endogenous_calc_layer_t::get_density_std_confidence_wrapper);
        }
        if (endogenous_selector.viscosity_working) {
            step_advection(dt, volumetric_flow, boundaries.viscosity_working,
                pde_solvers::pipe_endogenous_calc_layer_t::get_viscosity_working_wrapper,
                pde_solvers::pipe_endogenous_calc_layer_t::get_viscosity_working_confidence_wrapper
            );
        }
        if (endogenous_selector.sulfur || endogenous_selector.temperature || endogenous_selector.improver ||
            endogenous_selector.viscosity0 || endogenous_selector.viscosity20 || endogenous_selector.viscosity50)
        {
            throw std::runtime_error("Endogenous parameter mixing is not implemented");
        }
    }

    /// @brief Транспортное решение при бесконечном dt (заполнение трубы граничными значениями)
    virtual void transport_solve(double volumetric_flow, const pde_solvers::endogenous_values_t& boundaries) override
    {
        buffer.current().std_volumetric_flow = volumetric_flow;

        if (endogenous_selector.density_std) {
            pde_solvers::solve_advection(boundaries.density_std, buffer, 
                pde_solvers::pipe_endogenous_calc_layer_t::get_density_std_wrapper,
                pde_solvers::pipe_endogenous_calc_layer_t::get_density_std_confidence_wrapper);
        }
        if (endogenous_selector.viscosity_working) {
            pde_solvers::solve_advection(boundaries.viscosity_working, buffer, 
                pde_solvers::pipe_endogenous_calc_layer_t::get_viscosity_working_wrapper,
                pde_solvers::pipe_endogenous_calc_layer_t::get_viscosity_working_confidence_wrapper
            );
        }
        if (endogenous_selector.sulfur || endogenous_selector.temperature || endogenous_selector.improver ||
            endogenous_selector.viscosity0 || endogenous_selector.viscosity20 || endogenous_selector.viscosity50)
        {
            throw std::runtime_error("Endogenous parameter mixing is not implemented");
        }
    }

    /// @brief Получает эндогенные значения на выходе трубы
    /// @param volumetric_flow Объемный расход - по знаку определяется направление (выход при flow >= 0)
    /// @return Эндогенные значения на выходе
    pde_solvers::endogenous_values_t get_endogenous_output(double volumetric_flow) const override
    {
        pde_solvers::endogenous_values_t result;
        const pde_solvers::pipe_endogenous_calc_layer_t& current_layer = buffer.current();

        auto boundary_if_selected = [&](const pde_solvers::confident_layer_t& layer, bool use_parameter)
            -> pde_solvers::endogenous_confident_value_t {
            if (!use_parameter) {
                return { std::numeric_limits<double>::quiet_NaN(), false };
            }
            return layer.get_boundary_value(volumetric_flow);
        };

        result.density_std = boundary_if_selected(current_layer.density_std, endogenous_selector.density_std);
        result.viscosity_working = boundary_if_selected(current_layer.viscosity_working, endogenous_selector.viscosity_working);
        result.sulfur = boundary_if_selected(current_layer.sulfur, endogenous_selector.sulfur);
        result.improver = boundary_if_selected(current_layer.improver, endogenous_selector.improver);
        result.temperature = boundary_if_selected(current_layer.temperature, endogenous_selector.temperature);
        result.viscosity0 = boundary_if_selected(current_layer.viscosity0, endogenous_selector.viscosity0);
        result.viscosity20 = boundary_if_selected(current_layer.viscosity20, endogenous_selector.viscosity20);
        result.viscosity50 = boundary_if_selected(current_layer.viscosity50, endogenous_selector.viscosity50);

        if (endogenous_selector.sulfur || endogenous_selector.temperature || endogenous_selector.improver ||
            endogenous_selector.viscosity0 || endogenous_selector.viscosity20 || endogenous_selector.viscosity50)
        {
            throw std::runtime_error("Endogenous parameter pipe propagation is not implemented");
        }

        return result;
    }

};



/// @brief Транспортная модель трубы, сильно шаблонируется расчетным солвером трубы
template <typename PipeSolver = qsm_pipe_transport_solver_t>
class transport_pipe_model_t
    : public oil_transport_model_base_t<typename PipeSolver::pipe_parameters_type, transport_model_state_t>
{
    using base_model_type = oil_transport_model_base_t<typename PipeSolver::pipe_parameters_type, transport_model_state_t>;
    using base_model_type::endogenous_selector;
    using base_model_type::parameters;
protected:
    /// @brief Буфер с профилями эндогенных свойств
    typename PipeSolver::buffer_type& buffer_data;
    /// @brief Буфер для хранения результата:
    /// координаты - берем из исходных данных
    /// текущий слой - берем из буфера
    pipe_common_result_t<PipeSolver> pipe_result;

    /// @brief Параметры источника с нулевым расходом, образующегося при рассечении ребра
    transport_source_parameters_t zeroflow_source_parameters{ transport_source_parameters_t::get_zeroflow_source(true) };
    /// @brief Буферы данных для односторонних ребер, образующихся при рассечении ребра
    std::array<transport_values, 2> zeroflow_sink_buffer_data;
    /// @brief Модель источника с нулевым расходом, образующегося при рассечении ребра
    transport_source_model_t zeroflow_source_model;
    /// @brief Модель потребителя с нулевым расходом, образующегося при рассечении ребра
    transport_source_model_t zeroflow_sink_model;
public:
    /// @brief Труба НЕ является односторонним ребром
    static constexpr bool is_single_side() {
        return false;
    }
    /// @brief Модель двусторонняя, этот метод не имеет смысла
    bool is_zero_flow() const override {
        throw std::logic_error("Must not be called for two-sided object");
    }

    /// @brief Труба пока не рассекается 
    /// (есть идея для рисерчей сделать ее рассекаемой, чтобы моделировать остановку например лупингов, без моделирования задвижек)
    virtual graphlib::computational_type model_type() const override {
        return graphlib::computational_type::Instant;
    }
    /// @brief Возвращает буфер модели
    virtual void* get_edge_buffer() const override {
        return &buffer_data;
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
    /// @brief Создает расчетную транспортную модель трубы
    /// @param buffer Буфер
    /// @param parameters Параметры трубы
    transport_pipe_model_t(
        typename PipeSolver::buffer_type& buffer_data,
        const typename PipeSolver::pipe_parameters_type& _parameters,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
        : base_model_type(_parameters, endogenous_selector)
        , buffer_data(buffer_data)
        , zeroflow_source_model(zeroflow_sink_buffer_data.front(), zeroflow_source_parameters, endogenous_selector)
        , zeroflow_sink_model(zeroflow_sink_buffer_data.back(), zeroflow_source_parameters, endogenous_selector)
    {
        pipe_result.coordinates = &this->parameters.profile.get_coordinates();
    }
    /// @brief Переход на следующий рассчетный слой буфера
    void buffer_advance() {
        buffer_data.advance(+1);
    }

    /// @brief Расчет продвижения эндогенных свойств по ребру транспортного графа
    /// @param time_step Шаг по времени
    /// @param volumetric_flow Расход 
    /// @param endogenous_input_pointer Свойства с предыдущего ребра графа
    /// @param endogenous_measurements Измерения для рассматриваемого ребра
    virtual void propagate_endogenous(
        double time_step,
        double volumetric_flow,
        const pde_solvers::endogenous_values_t* endogenous_input_pointer,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        std::vector<graph_quantity_value_t>* overriden_with_measurements) override
    {
        if (endogenous_input_pointer == nullptr)
            throw std::runtime_error("Pipe endogenious input is unexpectedly null");

        pde_solvers::endogenous_values_t endogenous_input = *endogenous_input_pointer;
        if (!endogenous_measurements.empty())
            throw std::runtime_error("Unexpected measurements for pipe");

        PipeSolver solver(parameters, buffer_data, endogenous_selector);
        if (std::isfinite(time_step)) {
            solver.transport_step(time_step, volumetric_flow, endogenous_input);
        } else {
            solver.transport_solve(volumetric_flow, endogenous_input);
        }
    }
public:
    /// @brief Возвращает эндогенные свойства на том конце трубы, который фактически является выходом
    /// @param volumetric_flow Расход - по знаку определяется направление течения
    /// @return Эндогенные свойства
    virtual pde_solvers::endogenous_values_t get_endogenous_output(double volumetric_flow) const override
    {
        PipeSolver solver(parameters, buffer_data, endogenous_selector);
        return solver.get_endogenous_output(volumetric_flow);
    }

    /// @brief Выдача результатов транспортного расчета по трубе
    /// Возвращает ссылку на поле класса, ее адрес не будет меняться
    /// @return Текущий слой в буфере
    const pipe_common_result_t<PipeSolver>& get_result() {
        pipe_result.layer = &buffer_data.current();
        return pipe_result;
    }

    /// @brief Запись значения расхода в буфер модели
    virtual void update_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        if (layer_offset == graphlib::layer_offset_t::ToCurrentLayer) {
            buffer_data.current().std_volumetric_flow = flow;
        }
        else {
            buffer_data.previous().std_volumetric_flow = flow;
        }
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера гидравлической модели источника/потребителя
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        if (estimation_type == graphlib::solver_estimation_type_t::FromCurrentLayer) {
            return buffer_data.current().std_volumetric_flow;
        }
        else if (estimation_type == graphlib::solver_estimation_type_t::FromPreviousLayer) {
            return buffer_data.previous().std_volumetric_flow;
        }
        throw std::runtime_error("Invalid estimation type for pipe");
    }
};




}