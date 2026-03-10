
namespace oil_transport {
    ;

    /// @brief Параметры поставщика/потребителя для гидравлического расчета
    struct qsm_hydro_source_parameters : public oil_transport::model_parameters_t {
        /// @brief Объемный расход при номинальных условиях
        double std_volflow{ std::numeric_limits<double>::quiet_NaN() };
        /// @brief Давление
        double pressure{ std::numeric_limits<double>::quiet_NaN() };

        qsm_hydro_source_parameters() = default;
        /// @brief Конструктор из JSON-данных источника
        /// @param json_data JSON-данные источника
        qsm_hydro_source_parameters(const oil_transport::source_json_data& json_data) {
            pressure = json_data.pressure;
            std_volflow = json_data.std_volflow;
            if (json_data.is_initially_zeroflow) {
                pressure = std::numeric_limits<double>::quiet_NaN();
                std_volflow = 0;
            }
        }

        /// @brief Создает параметры источника с нулевым расходом
        /// @param is_consumer Флаг, является ли источник потребителем
        /// @return Параметры источника с нулевым расходом
        static qsm_hydro_source_parameters get_zeroflow_source(bool is_consumer) {
            qsm_hydro_source_parameters result;
            result.std_volflow = 0;
            result.pressure = std::numeric_limits<double>::quiet_NaN();
            return result;
        }
    };

    /// @brief Состояние гидравлического источника/потребителя
    struct qsm_hydro_source_state_t {
        /// @brief Объемный расход при номинальных условиях
        double std_volflow{ std::numeric_limits<double>::quiet_NaN() };
        /// @brief Давление
        double pressure{ std::numeric_limits<double>::quiet_NaN() };

        /// @brief Конструктор состояния из параметров
        qsm_hydro_source_state_t(const qsm_hydro_source_parameters& parameters)
        {
            pressure = parameters.pressure;
            std_volflow = parameters.std_volflow;
        }

        /// @brief Проверяет, является ли источник/потребитель проточным
        bool is_zeroflow() const {
            if (std::isfinite(std_volflow)) {
                // Если задан нулевой расход, ребро непроточное
                constexpr double eps = 1e-12;
                return std::abs(std_volflow) <= eps;
            }
            // В иных случаях ребро считается проточным
            return false;
        }
    };

    /// @brief Управление гидравлическим источником/потребителем
    struct hydraulic_source_control_t : public hydraulic_object_control_t {
        /// @brief Объемный расход при номинальных условиях
        double std_volflow{ std::numeric_limits<double>::quiet_NaN() };
        /// @brief Давление
        double pressure{ std::numeric_limits<double>::quiet_NaN() };
    };


    /// @brief Базовый класс для гидравлических моделей односторонних объектов (источники/потребители)
    /// @tparam ModelParameters Тип параметров модели
    template <typename ModelParameters>
    class qsm_hydro_source_model_base_t
        : public hydraulic_model_base_t<ModelParameters, qsm_hydro_source_state_t>
    {
    protected:
        /// @brief Тип базового класса
        using base_type = hydraulic_model_base_t<ModelParameters, qsm_hydro_source_state_t>;
        using base_type::state;
        using base_type::parameters;

    public:
        /// @brief Поставщику и потребителю соответствуют односторонние ребра
        static constexpr bool is_single_side() {
            return true;
        }

        /// @brief Конструктор
        qsm_hydro_source_model_base_t(const ModelParameters& parameters)
            : base_type(parameters)
        {
        }

        /// @brief Для односторонних моделей всегда Instant
        virtual graphlib::computational_type model_type() const override {
            return graphlib::computational_type::Instant;
        }

        /// @brief Не имеет смысла для поставщика, может быть вызван только по ошибке, кидает исключения
        virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) override {
            throw std::runtime_error("Source cannot have single-sided model");
        }

        /// @brief Проверяет, имеет ли источник строго нулевой расход
        virtual bool is_zero_flow() const override {
            return state.is_zeroflow();
        }

    };

    /// @brief Гидравлическая модель поставщика/потребителя
    class qsm_hydro_source_model_t
        : public qsm_hydro_source_model_base_t<qsm_hydro_source_parameters>
    {
        using base_type = qsm_hydro_source_model_base_t<qsm_hydro_source_parameters>;

    protected:
        /// @brief Буфер значений гидравлического расчета
        oil_transport::endohydro_values_t& buffer_data;

    public:
        /// @brief Конструктор модели источника/потребителя
        /// @param buffer_data Буфер гидравлических значений
        /// @param parameters Параметры источника
        qsm_hydro_source_model_t(oil_transport::endohydro_values_t& buffer_data,
            const qsm_hydro_source_parameters& parameters)
            : base_type(parameters)
            , buffer_data(buffer_data)
        {
        }

        /// @brief Возвращает буфер модели
        virtual void* get_edge_buffer() const override {
            return &buffer_data;
        }

        /// @brief Изменяет состояние объекта
        virtual void send_control(const hydraulic_object_control_t& control) override {
            if (auto source_control = dynamic_cast<const hydraulic_source_control_t*>(&control)) {
                state.pressure = source_control->pressure;
                state.std_volflow = source_control->std_volflow;
            }
            else {
                throw std::runtime_error("Wrong control type");
            }
        }

        /// @brief Возвращает известное давление для P-притоков. 
        /// Если объект не является P-притоком, возвращает NaN
        virtual double get_known_pressure() override {
            return state.pressure;
        }
        /// @brief Возвращает известный расход для Q-притоков. 
        /// Если объект не является Q-притоком, возвращает NaN
        /// Тип расхода (объемный, массовый) не регламентируется и решается на уровне реализации замыкающих соотношений
        /// Нужно, чтобы все замыкающие соотношения подразумевали один и тот же вид расхода
        virtual double get_known_flow() override {
            return state.std_volflow;
        }
        /// @brief Расход висячей дуги при заданном давлении
        virtual double calculate_flow(double pressure) override {
            throw std::runtime_error("not supported");
        }
        /// @brief Производная расход висячей дуги по давлению
        virtual double calculate_flow_derivative(double pressure) override {
            throw std::runtime_error("not supported");
        }

        /// @brief Не поддерживает расчет расхода по давлениям, т.к. одностороннее ребро
        virtual double calculate_flow(double pressure_in, double pressure_out) override {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Не поддерживает расчет производной расхода по давлениям, т.к. одностороннее ребро
        virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) override {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Запись значения расхода в буфер модели
        virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
            buffer_data.vol_flow = flow;
        }

        /// @brief Для односторонней модели запись двух давлений не определена
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Запись давления в буфер модели (односторонний объект)
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
            buffer_data.pressure = pressure;
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            return buffer_data.pressure;
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        /// @param incidence +1 - выход ребра. -1 - вход ребра.
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) override {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Возвращает рассчитанный ранее объемный расход через дугу
        /// @details Берется объемный расход из буфера гидравлической модели источника/потребителя
        virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            return buffer_data.vol_flow;
        }

    };

};
