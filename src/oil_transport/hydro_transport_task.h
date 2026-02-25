
namespace oil_transport {
;

/// @brief Способ совмещения транспортной и гидравлической задач
enum class hydro_transport_solution_method
{
    /// @brief Расчет расходов на основе измерений (ЗРР) - и тогда для эндогенного расчета использовать блочный солвер
    FindFlowFromMeasurements,
    /// @brief Расчет расходов из КЗП -  и тогда для эндогенного расчета использовать солвер на полной распространимости расходов
    CalcFlowWithHydraulicSolver
};

/// @brief Параметры квазистационарного (совмещенного) гидравлического расчета
struct hydro_transport_task_settings {
    /// @brief Способ совмещения транспортной и гидравлической задач
    hydro_transport_solution_method solution_method;
    /// @brief Настройки солвера уровня структурированного графа для транспортной задачи
    graphlib::structured_transport_solver_settings_t structured_transport_solver;
    /// @brief Настройки солвера уровня структурированного графа для гидравлики
    graphlib::structured_hydro_solver_settings_t structured_hydro_solver;
    /// @brief Шаг по координате для трубы
    double pipe_coordinate_step{ std::numeric_limits<double>::quiet_NaN() };


    /// @brief Возвращает настройки квазистатонарного гидравлического расчета по умолчанию
    static hydro_transport_task_settings get_default_settings() {
        hydro_transport_task_settings result;
        result.solution_method = hydro_transport_solution_method::FindFlowFromMeasurements;
        result.structured_hydro_solver = graphlib::structured_hydro_solver_settings_t::default_values();
        result.structured_transport_solver = graphlib::structured_transport_solver_settings_t::default_values();
        result.pipe_coordinate_step = 200;
        return result;
    }
};

/// @brief Инициализация эндогенных свойств в буферах на основе измерений.
/// На основании всех измерений вычисляет среднее значение каждого параметра из oil_transport_hydro_values_t.
/// Для параметров без измерений берёт значения из default_values. Затем записывает результат в буферы всех моделей.
/// @param transport_graph Транспортный граф, буферы которого будут инициализированы
/// @param measurements Измерения, которые будут использоаны для инициализации
/// @param layer_offset определяет, в какой слой буфера трубы записывать значения (текущий или предыдущий).
/// @param default_values Источник значений по умолчанию для параметров, измерения которых отсутствуют
/// @brief Инициализирует эндогенные параметры в буферах моделей транспортного графа
/// @tparam PipeSolver Тип солвера трубы
/// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
template <typename PipeSolver, typename LumpedCalcParametersType = oil_transport::endohydro_values_t>
inline void init_endogenous_in_buffers(
    const oil_transport::transport_graph_t& transport_graph,
    const std::vector<graph_quantity_value_t>& measurements,
    graphlib::layer_offset_t layer_offset,
    const oil_transport::endohydro_values_t& default_values = oil_transport::endohydro_values_t{})
{
    // 1. Собираем статистику по измерениям
    hydro_measurements_averager_t averager;
    for (const auto& m : measurements) {
        averager.add_measurement(m);
    }

    // 2. Формируем усреднённые значения: гидравлические + эндогенные
    oil_transport::endohydro_values_t avg_values =
        averager.get_average_hydro_values(default_values);

    // 3. Записываем усреднённые значения в буферы всех моделей
    auto edges = transport_graph.get_edge_binding_list();
    for (const auto& edge_binding : edges) {

        oil_transport::oil_transport_model_t* model =
            transport_graph.get_model_incidences(edge_binding).model;

        void* buffer_pointer = model->get_edge_buffer();

        if (buffer_pointer == nullptr) {
            throw std::runtime_error("Model has null buffer");
        }

        // Проверяем источники и сосредоточенные объекты первыми
        if (auto source_model = dynamic_cast<oil_transport::transport_source_model_t*>(model)) {
            // Для источников/потребителей буфер — одиночный oil_transport_hydro_values_t
            auto* buffer = reinterpret_cast<oil_transport::endohydro_values_t*>(buffer_pointer);
            transport_buffer_helpers::init_single_sided_buffer_from_average(buffer, avg_values);
            continue;
        }
        
        if (auto lumped_model = dynamic_cast<oil_transport::transport_lumped_model_t*>(model)) {
            // Для двусторонних сосредоточенных объектов буфер — массив указателей на два экземпляра
            auto* buffers_for_getter = reinterpret_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(buffer_pointer);
            transport_buffer_helpers::init_two_sided_buffer_from_average(buffers_for_getter, avg_values);
            continue;
        }
        
        // Труба: инициализируем профиль для произвольного солвера
        using buffer_type = pde_solvers::ring_buffer_t<typename PipeSolver::layer_type>;
        auto* pipe_buffer = reinterpret_cast<buffer_type*>(buffer_pointer);
        
        using layer_type = typename PipeSolver::layer_type;
        
        // Инициализируем профили плотности
        transport_buffer_helpers::pipe_buffer_initializer_t<layer_type, oil_transport::endohydro_values_t>::init_density(
            pipe_buffer, layer_offset, avg_values);
        
        // Остальные эндогенные параметры для профилей в трубе пока не поддержаны
        if (averager.has_viscosity() || averager.has_sulfur() || averager.has_improver() || averager.has_temperature()) {
            throw std::runtime_error("Implement wrapper for ring buffer layer of this endogenous param");
        }
    }
}

/// @brief Однослойная (на один временной шаг) расчетная квазистационарная гидравлическая задача на полном графе
/// @tparam PipeSolver Солвер трубы
template <typename PipeSolver, typename LumpedCalcParametersType>
class hydro_transport_task_template {
public:
    /// @brief Тип солвера трубы
    using pipe_solver_type = PipeSolver;
    /// @brief Тип параметров для сосредоточенных объектов
    using lumped_calc_parameters_type = LumpedCalcParametersType;
    /// @brief Базовый тип управления объектами
    using base_control_type = oil_transport::hydraulic_object_control_t;

private:
    /// @brief Транспортный солвер по рассчитанным расходам с чтением расходов из буферов
    using endogenous_solver_buffer_based = full_propagatable_endogenous_solver_buffer_based<
        transport_model_incidences_t, 
        graph_quantity_value_t, 
        pde_solvers::endogenous_values_t, 
        transport_mixer_t, 
        pde_solvers::endogenous_selector_t
    >;

    /// @brief Транспортный солвер на дереве блоков с записью расходов в буферы
    using transport_block_solver = transport_block_solver_buffer_based <
        transport_model_incidences_t,
        transport_pipe_model_t<PipeSolver>,
        graph_quantity_value_t,
        pde_solvers::endogenous_values_t,
        transport_mixer_t,
        pde_solvers::endogenous_selector_t
    >;

    /// @brief Гидравлический солвер на несвязном графе
    using structured_hydro_solver = graphlib::structured_hydro_solver_t<
        graphlib::nodal_solver_buffer_based_t<oil_transport::hydraulic_model_incidences_t>,
        oil_transport::hydraulic_graph_t,
        oil_transport::hydraulic_model_incidences_t,
        hydraulic_pipe_model_t<PipeSolver>
    >;

    /// @brief Блочный транспортный солвер на несвязном графе (для расчета расходов из ЗРР)
    using structured_transport_block_solver = graphlib::structured_transport_solver_t<
        transport_block_solver,
        transport_graph_t,
        graph_quantity_value_t,
        pde_solvers::endogenous_selector_t,
        transport_pipe_model_t<PipeSolver>
    >;

    /// @brief Транспортный солвер полной распространимости на несвязном графе (для предрасчитанных расходов из КЗП)
    using structured_transport_solver_endo = graphlib::structured_transport_solver_t<
        endogenous_solver_buffer_based,
        transport_graph_t,
        graph_quantity_value_t,
        pde_solvers::endogenous_selector_t,
        transport_pipe_model_t<PipeSolver>
    >;

private:
    /// @brief Контейнер моделей и буферов
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
    /// @brief Транспортный граф с полной топологией, ссылки на модели из models
    transport_graph_t transport_graph;
    /// @brief Гидравлический граф
    oil_transport::hydraulic_graph_t hydro_graph;
    /// @brief Результаты расчета, классифицированные по типам объектов: источники, трубы, остальные объекты 
    task_common_result_t<PipeSolver, LumpedCalcParametersType> result_structure;
    /// @brief Параметры расчетной задачи
    hydro_transport_task_settings settings;
private:
    /// @brief Этот изврат нужен, чтобы не делать unique_ptr для task_common_result_t
    hydro_transport_task_template(
        std::tuple<
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
        transport_graph_t,
        oil_transport::hydraulic_graph_t
        >&& models_and_graphs)
        : models(std::get<0>(std::move(models_and_graphs)))
        , transport_graph(std::get<1>(std::move(models_and_graphs)))
        , hydro_graph(std::get<2>(std::move(models_and_graphs)))
        , result_structure(hydro_graph)
    { }

public:

    /// @brief Конструктор транспортного таска из моделей и графов
    /// @param models Модели графа (транспортные и гидравлические)
    /// @param transport_graph Транспортный граф
    /// @param hydro_graph Гидравлический граф
    /// @param _settings Настройки транспортной задачи
    hydro_transport_task_template(
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>&& models,
        transport_graph_t&& transport_graph,
        oil_transport::hydraulic_graph_t&& hydro_graph,
        const hydro_transport_task_settings& _settings)
        : models(std::move(models))
        , transport_graph(std::move(transport_graph))
        , hydro_graph(std::move(hydro_graph))
        , settings(_settings)
    { }

    /// @brief Вызывает graph_builder на основе siso_data
    hydro_transport_task_template(const graph_siso_data& siso_data,
        const hydro_transport_task_settings& _settings)
        : hydro_transport_task_template(graph_builder<
            PipeSolver, LumpedCalcParametersType>::build_hydro_transport(siso_data, 
                _settings.structured_transport_solver.endogenous_options))
    {
        this->settings = _settings;
    }
private:

    /// @brief Расчет на один шаг с использованием измерений расходов (ЗРР)
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step_flow_propagation(
        double time_step,
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements)
    {
        // Транспортный шаг
        structured_transport_block_solver transport_solver(transport_graph, 
            settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver);
        // Сдвиг буферов труб. Т.к. буферы единые, то сдвиг для structured_hydro_solver не нужен
        transport_solver.advance_pipe_buffers();
        graphlib::transport_problem_info<graph_quantity_value_t> graph_result =
            transport_solver.step(time_step, flow_measurements, endogenous_measurements);

        // Гидравлический шаг
        structured_hydro_solver hydro_solver(hydro_graph, settings.structured_hydro_solver);    
        hydro_solver.solve(graphlib::solver_estimation_type_t::FromPreviousLayer);

        return graph_result;
    }

    /// @brief Расчет на один шаг с расчетом расходов из КЗП (вычисление расходов)
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step_flow_calculation(
        double time_step,
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements)
    {
        // Фиктивное совмещение декомпозированных задач. Транспортный солвер на рассчитанных из КЗП расходах

        // Гидравлический шаг
        structured_hydro_solver solver_hydro(hydro_graph, settings.structured_hydro_solver);
        // Сдвиг буферов труб. Т.к. буферы единые, то сдвиг для structured_transport_solver не нужен
        solver_hydro.advance_pipe_buffers();
        solver_hydro.solve(graphlib::solver_estimation_type_t::FromPreviousLayer);

        // Транспортный шаг
        structured_transport_solver_endo solver_transport(transport_graph, 
            settings.structured_transport_solver.endogenous_options, settings.structured_transport_solver);
        
        graphlib::transport_problem_info<graph_quantity_value_t> transport_result = 
            solver_transport.step(time_step, flow_measurements, endogenous_measurements);
            
        graphlib::transport_problem_info<graph_quantity_value_t> graph_result;
        graph_result.append(transport_result);


        return graph_result;
    }

public:
    /// @brief Сбрасывает код достоверности всех объектов
    /// @details Используется после анализа статического кода достоверности
    void reset_confidence() {
        models.reset_confidence();
    }
    /// @brief Статический транспортный расчет для анализа полноты измерений
    /// @param measurements Вектор измерений (расходы и эндогены смешаны)
    /// @param confidence_reset Флаг сброса кода достоверности после решения
    /// @return Информация о полноте и использовании измерений
    graphlib::transport_problem_info<graph_quantity_value_t> solve(
        const std::vector<graph_quantity_value_t>& measurements, bool confidence_reset)
    {
        auto [flow, endogenous] = classify_measurements(measurements);
        return solve(flow, endogenous, confidence_reset);
    }

    /// @brief Статический транспортный расчет для анализа полноты измерений
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @param confidence_reset Флаг сброса кода достоверности после решения
    /// @return Информация о полноте и использовании измерений
    graphlib::transport_problem_info<graph_quantity_value_t> solve(
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        bool confidence_reset)
    {
        // Инициализация буферов эндогенными свойствами
        init_endogenous_in_buffers<PipeSolver, LumpedCalcParametersType>(
            transport_graph, endogenous_measurements, graphlib::layer_offset_t::ToCurrentLayer);

        init_endogenous_in_buffers<PipeSolver, LumpedCalcParametersType>(
            transport_graph, endogenous_measurements, graphlib::layer_offset_t::ToPreviousLayer);

        // Используем structured_hydro_solver для решения гидравлической задачи
        structured_hydro_solver solver(hydro_graph, settings.structured_hydro_solver);
        
        solver.solve(graphlib::solver_estimation_type_t::RandomInital);

        // TODO: Контроль полноты измерений - реализовать, когда вопрос будет теоретически проработан
        graphlib::transport_problem_info<graph_quantity_value_t> graph_result;
        return graph_result;
    
    }
    /// @brief Выполняет статический расчет и формирует исключение при отсутствии полноты измерений
    /// @param measurements Вектор измерений (расходы + эндогенные свойства)
    void solve_handled(
        const std::vector<graph_quantity_value_t>& measurements)
    {
        throw std::runtime_error("Not impl");
    }


    /// @brief Выполняет переключение объекта транспортного графа
    void send_control_transport(const transport_object_control_t& control) {
        auto& edge = transport_graph.get_edge(control.binding);
        edge.model->send_control(control);
    }

    /// @brief Выполняет переключение объекта гидравлического графа
    void send_control_hydro(const hydraulic_object_control_t& control) {
        auto& edge = hydro_graph.get_edge(control.binding);
        edge.model->send_control(control);
    }

    /// @brief Выполняет несколько переключений объектов графа
    void send_controls(const std::vector<const transport_object_control_t*>& transport_controls,
        const std::vector<const hydraulic_object_control_t*>& hydro_controls) 
    {
        for (const transport_object_control_t* control : transport_controls) {
            if (control != nullptr) {
                send_control_transport(*control);
            }
        }

        for (const hydraulic_object_control_t* control : hydro_controls) {
            if (control != nullptr) {
                send_control_hydro(*control);
            }
        }
    }

    /// @brief Расчет на один шаг с автоматической классификацией измерений
    /// @param time_step Временной шаг
    /// @param measurements Вектор измерений (расходы и эндогены смешаны)
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step(
        double time_step,
        const std::vector<graph_quantity_value_t>& measurements)
    {
        auto [flow, endogenous] = classify_measurements(measurements);
        return step(time_step, flow, endogenous);
    }

    /// @brief Расчет на один шаг на основе измерений расходов и измерений эндогенных свойств
    /// @param time_step Временной шаг
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @return Информация о полноте и использовании измерений для данного шага
    graphlib::transport_problem_info<graph_quantity_value_t> step(double time_step,
        const std::vector<graph_quantity_value_t>& flow_measurements,
        const std::vector<graph_quantity_value_t>& endogenous_measurements)
    {
        if (settings.solution_method == hydro_transport_solution_method::FindFlowFromMeasurements) {
            return step_flow_propagation(time_step, flow_measurements, endogenous_measurements);
        }
        else {
            return step_flow_calculation(time_step, flow_measurements, endogenous_measurements);
        }
    }

    /// @brief Возвращает структуру для зачитки результатов расчета
    const task_common_result_t<PipeSolver, LumpedCalcParametersType>& get_result() {
        result_structure.update_pipe_result();
        return result_structure;
    }

    /// @brief Возвращает ссылку на транспортный граф
    const transport_graph_t& get_transport_graph() const {
        return transport_graph;
    }

    /// @brief Возвращает ссылку на гидравлический граф
    const hydraulic_graph_t& get_hydro_graph() const {
        return hydro_graph;
    }
};

/// @brief Специализация таска совмещенного квазистационарного расчета
using hydro_transport_task_t = hydro_transport_task_template<
    pde_solvers::iso_nonbaro_pipe_solver_t,
    oil_transport::endohydro_values_t
>;

}