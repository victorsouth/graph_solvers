
namespace oil_transport {
;

/// @brief Параметры трубы для гидравлического расчета
/// @tparam SolverPipeProperties Тип свойств трубы для солвера (например, condensate_pipe_properties_t)
template <typename SolverPipeProperties>
struct hydraulic_pipe_parameters_t
    : public oil_transport::model_parameters_t
    , public SolverPipeProperties
{
    hydraulic_pipe_parameters_t() = default;
    /// @brief Конструктор из свойств трубы для солвера
    /// @param pipe_properties Свойства трубы для солвера
    hydraulic_pipe_parameters_t(const SolverPipeProperties& pipe_properties)
        : SolverPipeProperties(pipe_properties)
    {
    }

};

/// @brief Гидравлическая модель трубы
/// @tparam PipeSolver Тип гидравлического солвера трубы (должен реализовывать pipe_solver_interface_t)
template <typename PipeSolver>
class hydraulic_pipe_model_t
    : public hydraulic_model_base_t<hydraulic_pipe_parameters_t<typename PipeSolver::pipe_parameters_type>, hydraulic_model_state_t>
{
    // TODO: убрать public
public:
    /// @brief Тип базовой модели
    using base_model_type = hydraulic_model_base_t<hydraulic_pipe_parameters_t<typename PipeSolver::pipe_parameters_type>, hydraulic_model_state_t>;
    /// @brief Тип параметров
    using base_model_type::parameters;

private:

    /// @brief Буфер с профилями эндогенных и гидравлических свойств
    typename PipeSolver::buffer_type& buffer_data;

    /// @brief Буфер для хранения результата:
    /// координаты - берем из исходных данных
    /// текущий слой - берем из буфера
    pipe_common_result_t<PipeSolver> pipe_result;

    // TODO: Криво. Переделать в будущем
    qsm_hydro_source_parameters zeroflow_source_parameters{ qsm_hydro_source_parameters::get_zeroflow_source(true) };
    std::array<oil_transport::endohydro_values_t, 2> zeroflow_sink_buffer_data;
    qsm_hydro_source_model_t zeroflow_source_model;
    qsm_hydro_source_model_t zeroflow_sink_model;



public:
    /// @brief Двустороннее ребро
    static constexpr bool is_single_side() {
        return false;
    }

    /// @brief Модель двусторонняя, этот метод не имеет смысла
    virtual bool is_zero_flow() const override {
        throw std::logic_error("Must not be called for two-sided object");
    }

    /// @brief Не имеет смысла для поставщика, может быть вызван только по ошибке, кидает исключения
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

    /// @brief Конструктор
    hydraulic_pipe_model_t(
        typename PipeSolver::buffer_type& buffer_data,
        const hydraulic_pipe_parameters_t<typename PipeSolver::pipe_parameters_type>& _parameters)
        : base_model_type(_parameters)
        , buffer_data(buffer_data)
        , zeroflow_source_model(zeroflow_sink_buffer_data.front(), zeroflow_source_parameters)
        , zeroflow_sink_model(zeroflow_sink_buffer_data.back(), zeroflow_source_parameters)
    {
        // Получаем координаты из свойств трубы
        const auto& pipe_props = static_cast<const typename PipeSolver::pipe_parameters_type&>(this->parameters);
        pipe_result.coordinates = &pipe_props.profile.coordinates;
    }

    /// @brief Перемещает буфер на следующий слой
    void buffer_advance() {
        buffer_data.advance(+1);
    }



    /// @brief Для труб всегда Instant
    virtual graphlib::computational_type model_type() const override {
        return graphlib::computational_type::Instant;
    }

    /// @brief Возвращает буфер модели
    virtual void* get_edge_buffer() const override {
        return &buffer_data;
    }

    /// @brief Выдача результатов гидравлического расчета по трубе
    /// Возвращает ссылку на поле класса, ее адрес не будет меняться
    /// @return Текущий слой в буфере
    const pipe_common_result_t<PipeSolver>& get_result() {
        pipe_result.layer = &buffer_data.current();
        return pipe_result;
    }

    /// @brief Трубы обычно не имеют управления
    virtual void send_control(const hydraulic_object_control_t& control) override {
        throw std::runtime_error("Pipe control is not supported");
    }

    /// @brief Запись значения расхода в буферы модели (двусторонний объект)
    virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
        auto& layer = (layer_offset == graphlib::layer_offset_t::ToCurrentLayer)
            ? buffer_data.current()
            : buffer_data.previous();
        layer.std_volumetric_flow = flow;
    }

    /// @brief Запись давлений на входе и выходе в буферы модели
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
        // Выбираем слой в зависимости от layer_offset
        auto& layer = (layer_offset == graphlib::layer_offset_t::ToCurrentLayer) 
            ? buffer_data.current() 
            : buffer_data.previous();
        
        // Записываем давление на входе в начало профиля
        layer.pressure.front() = pressure_in;
        // Записываем давление на выходе в конец профиля
        layer.pressure.back() = pressure_out;
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
    /// @param estimation_type Тип оценки (текущий или предыдущий слой)
    /// @param is_end_side true - выход ребра, false - вход ребра
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) override {
        auto& layer = (estimation_type == graphlib::solver_estimation_type_t::FromCurrentLayer) 
            ? buffer_data.current() 
            : buffer_data.previous();
        
        return is_end_side ? layer.pressure.back() : layer.pressure.front();
    }

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        auto& layer = (estimation_type == graphlib::solver_estimation_type_t::FromCurrentLayer) 
            ? buffer_data.current() 
            : buffer_data.previous();
        
        return layer.std_volumetric_flow;
    }
    
    /// @brief Расход дуги при заданных давлениях на входе и выходе
    virtual double calculate_flow(double pressure_in, double pressure_out) override {
        // Используем свойства трубы напрямую из parameters
        const auto& pipe_props = static_cast<const typename PipeSolver::pipe_parameters_type&>(parameters);
        PipeSolver solver(pipe_props, buffer_data);
        double volumetric_flow = solver.hydro_solve_PP(pressure_in, pressure_out);        
        return volumetric_flow;
    }
    
    /// @brief Производные расхода дуги по давлениям на входе и выходе
    /// @param pressure_in Давление на входе дуги
    /// @param pressure_out Давление на выходе дуги
    /// @return Массив из двух элементов: производная по pressure_in и производная по pressure_out
    virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) override {
        const auto& pipe_props = static_cast<const typename PipeSolver::pipe_parameters_type&>(parameters);
        PipeSolver solver(pipe_props, buffer_data);
        return solver.hydro_solve_PP_jacobian(pressure_in, pressure_out);
    }

    /// @brief Возвращает известное давление для P-притоков. 
        /// Если объект не является P-притоком, возвращает NaN
    virtual double get_known_pressure() override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает известный расход для Q-притоков. 
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
    
};

}
