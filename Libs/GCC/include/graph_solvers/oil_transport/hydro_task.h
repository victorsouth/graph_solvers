
namespace oil_transport {
;

/// @brief Настройки таска для изолированного гидравлического расчета
struct hydro_task_settings {
    /// @brief Настройки гидравлического солвера на несвязном графе
    graphlib::structured_hydro_solver_settings_t structured_solver;
    /// @brief Шаг по координате для трубы
    double pipe_coordinate_step{ std::numeric_limits<double>::quiet_NaN() };

    /// @brief Создаёт набор настроек со значениями по умолчанию
    static hydro_task_settings default_values() {
        hydro_task_settings result;
        result.structured_solver = graphlib::structured_hydro_solver_settings_t::default_values();
        result.pipe_coordinate_step = 200;
        return result;
    }
};

/// @brief Однослойная (на один временной шаг) расчетная квазистационарная гидравлическая задача на графе всей схемы
/// @tparam PipeSolver Солвер трубы
template <typename PipeSolver, typename LumpedCalcParametersType>
class hydro_task_template {
public:
    /// @brief Тип солвера трубы
    using pipe_solver_type = PipeSolver;
    /// @brief Тип параметров для сосредоточенных объектов
    using lumped_calc_parameters_type = LumpedCalcParametersType;
    /// @brief Базовый тип управления объектами
    using base_control_type = oil_transport::hydraulic_object_control_t;

private:
    /// @brief Гидравлический солвер на несвязном графе
    using structured_hydro_solver = graphlib::structured_hydro_solver_t<
        graphlib::nodal_solver_buffer_based_t<oil_transport::hydraulic_model_incidences_t>,
        oil_transport::hydraulic_graph_t,
        oil_transport::hydraulic_model_incidences_t,
        hydraulic_pipe_model_t<PipeSolver>
    >;

private:
    /// @brief Контейнер моделей и буферов
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
    /// @brief Гидравлический граф
    oil_transport::hydraulic_graph_t hydro_graph;
    /// @brief Результаты расчета, классифицированные по типам объектов: источники, трубы, остальные объекты 
    task_common_result_t<PipeSolver, LumpedCalcParametersType> result_structure;
    /// @brief Параметры расчетной задачи
    hydro_task_settings settings;
    /// @brief Флаг для выявления первого вызова solve
    bool solve_called_previously = false;

private:
    /// @brief Этот изврат нужен, чтобы не делать unique_ptr для task_common_result_t
    hydro_task_template(
        std::tuple<
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
        oil_transport::hydraulic_graph_t
        >&& models_and_graphs)
        : models(std::get<0>(std::move(models_and_graphs)))
        , hydro_graph(std::get<1>(std::move(models_and_graphs)))
        , result_structure(hydro_graph)
    {
    }

public:

    /// @brief Конструктор гидравлического таска из моделей и графов
    /// @param models Модели графа (транспортные и гидравлические)
    /// @param hydro_graph Гидравлический граф
    /// @param _settings Настройки таска
    hydro_task_template(
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>&& models,
        oil_transport::hydraulic_graph_t&& hydro_graph,
        const hydro_task_settings& _settings)
        : models(std::move(models))
        , hydro_graph(std::move(hydro_graph))
        , settings(_settings)
    {
    }

    /// @brief Вызывает graph_builder на основе siso_data
    /// @param siso_data Данные о графе
    /// @param _settings Настройки таска
    hydro_task_template(const graph_siso_data& siso_data,
        const hydro_task_settings& _settings)
        : hydro_task_template(graph_builder<PipeSolver, LumpedCalcParametersType>::build_hydro(siso_data))
    {
        this->settings = _settings;
    }

public:
    /// @brief Стационарный гидравлический расчет при текущей реологии в буферах моделей
    /// @return Результаты и диагностика по подграфам структурированного графа
    graphlib::structured_flow_distribution_result solve() {
        structured_hydro_solver solver(hydro_graph, settings.structured_solver);
        solver.advance_pipe_buffers();

        auto estimation_type = graphlib::solver_estimation_type_t::FromPreviousLayer;
        if (!solve_called_previously) {
            estimation_type = graphlib::solver_estimation_type_t::RandomInital;
            solve_called_previously = true;
        }
        return solver.solve(estimation_type);
    }

    /// @brief Выполняет статический расчет и формирует исключение
    void solve_handled()
    {
        throw std::runtime_error("Not impl");
    }

    /// @brief Выполняет переключение объекта графа
    /// @param control Управление объектом
    void send_control(const hydraulic_object_control_t& control) {
        auto& edge = hydro_graph.get_edge(control.binding);
        edge.model->send_control(control);
    }

    /// @brief Выполняет несколько переключений объектов графа
    void send_controls(const std::vector<const hydraulic_object_control_t*>& controls) {
        for (const hydraulic_object_control_t* control : controls) {
            if (control != nullptr) {
                send_control(*control);
            }
        }
    }

    /// @brief Возвращает структуру для зачитки результатов расчета
    const task_common_result_t<PipeSolver, LumpedCalcParametersType>& get_result() {
        result_structure.update_pipe_result();
        return result_structure;
    }

    /// @brief Возвращает константную ссылку на гидравлический граф
    const oil_transport::hydraulic_graph_t& get_graph() const {
        return hydro_graph;
    }

    /// @brief Возвращает ссылку на гидравлический граф
    oil_transport::hydraulic_graph_t& get_graph() {
        return hydro_graph;
    }

    /// @brief Возвращает ссылку на коллекцию гидравлических моделей рёбер графа
    /// Сгенерировано в cursor
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>& get_models() {
        return models;
    }

};

/// @brief Специализация таска совмещенного квазистационарного гидравлического расчета
using hydro_task_t = hydro_task_template<
    pde_solvers::iso_nonbaro_pipe_solver_t,
    oil_transport::endohydro_values_t
>;



}