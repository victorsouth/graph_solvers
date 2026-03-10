#pragma once

namespace graphlib {
;
/// @brief Forward declaration типа оценки начального приближения для узлового солвера
/// @details Полное определение находится в nodal_solver_buffer_based.h
enum class solver_estimation_type_t;
/// @brief Forward declaration типа смещения слоя буфера для записи результатов расчета
/// @details Полное определение находится в nodal_solver_buffer_based.h
enum class layer_offset_t;

/// @brief Результат решения задачи потокораспределения: векторы расходов по рёбрам и давлений по вершинам графа
struct flow_distribution_result_t {
    /// @brief Расходы по односторонним (висячим) рёбрам графа в порядке их индексов в контейнере `edges1`
    Eigen::VectorXd flows1;
    /// @brief Расходы по двусторонним рёбрам графа в порядке их индексов в контейнере `edges2`
    Eigen::VectorXd flows2;
    /// @brief Давления во всех вершинах графа в порядке их индексов
    Eigen::VectorXd pressures;
    /// @brief Результат численного метода
    fixed_solver_result_t<-1> numerical_result;
    /// @brief Диагностическая информация численного метода
    fixed_solver_result_analysis_t<-1> residuals_analysis;
};

/// @brief Замыкающее соотношение МД
class closing_relation_t {
public:
    /// @brief Возвращает известное давление для P-притоков. 
    /// Если объект не является P-притоком, возвращает NaN
    virtual double get_known_pressure() = 0;
    /// @brief Возвращает известный расход для Q-притоков. 
    /// Если объект не является Q-притоком, возвращает NaN
    /// Тип расхода (объемный, массовый) не регламентируется и решается на уровне реализации замыкающих соотношений
    /// Нужно, чтобы все замыкающие соотношения подразумевали один и тот же вид расхода
    virtual double get_known_flow() = 0;

    /// @brief Расход висячей дуги при заданном давлении
    virtual double calculate_flow(double pressure) = 0;
    /// @brief Производная расход висячей дуги по давлению
    virtual double calculate_flow_derivative(double pressure) = 0;

    /// @brief Расход дуги при заданных давлениях на входе и выходе
    /// @param pressure_in Давление на входе дуги
    /// @param pressure_out Давление на выходе дуги
    /// @return Расход через дугу
    virtual double calculate_flow(double pressure_in, double pressure_out) = 0;
    /// @brief Производные расхода дуги по давлениям на входе и выходе
    /// @param pressure_in Давление на входе дуги
    /// @param pressure_out Давление на выходе дуги
    /// @return Массив из двух элементов: производная по pressure_in и производная по pressure_out
    virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) = 0;

    /// @brief Запись значения расхода в модель
    virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) = 0;

    /// @brief Запись давления в модель
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) = 0;

    /// @brief Запись давления в модель
    virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) = 0;
    
    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) = 0;

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    /// @param incidence +1 - выход ребра. -1 - вход ребра.
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) = 0;

    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Тип расхода (объемный, массовый) должен соответствовать договоренностям на уровне реализации моделей.
    /// В текущей реализации под расходом понимается объемный расход из буфера гидравлической модели.
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) = 0;
};

/// @brief Замыкающее соотношение МД для висячих дуг
class closing_relation1 : public closing_relation_t {
public:
    /// @brief Расход через давления определен только для двусторонней дуги
    virtual double calculate_flow(double pressure_in, double pressure_out) override {
        throw std::runtime_error("Must not be called for edge type I");
    }
    /// @brief Расход через давления определен только для двусторонней дуги
    virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) {
        throw std::runtime_error("Must not be called for edge type I");
    }
    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    /// @param incidence +1 - выход ребра. -1 - вход ребра.
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) override {
        throw std::runtime_error("Must not be called for edge type I");
    }
};


/// @brief Замыкающее соотношение МД для двусторонних дуг
class closing_relation2 : public closing_relation_t {
public:
    /// @brief Известное давление может быть определено только на висячем ребре
    virtual double get_known_pressure() override {
        throw std::runtime_error("Must not be called for edge type II");
    }
    /// @brief Известный расход может быть определен только на висячем ребре
    virtual double get_known_flow() override {
        throw std::runtime_error("Must not be called for edge type II");
    }
    /// @brief Для двустороннего ребра расход нельзя определить по единственному давлению
    virtual double calculate_flow(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Для двустороннего ребра расход нельзя определить по единственному давлению
    virtual double calculate_flow_derivative(double pressure) override {
        throw std::runtime_error("Must not be called for edge type II");
    }

    /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
    virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
        throw std::runtime_error("Must not be called for edge type II");
    }
};

/// @brief Тип дуги графа, с моделью - замыкающим соотношением
using nodal_edge_t = graphlib::model_incidences_t<closing_relation_t*>;

/// @brief Определяет условия формирования диагностики для расчета методом Ньютона-Рафсона
enum class analysis_execution_mode {
    /// @brief Диагностика никогда не формируется
    never,
    /// @brief В случае, если расчет не успешен, повторяется расчет со сбором диагностики
    if_failed,
    /// @brief Диагностика формируется всегда
    always
};

/// @brief Параметры настройки узлового солвера МД
struct nodal_solver_settings_t {
    /// @brief Разброс вокруг среднего давления заданных P-притоков, используемый при случайной инициализации начального приближения
    double random_pressure_deviation{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Максимальное количество итераций основного алгоритма Ньютона-Рафсона
    size_t max_iterations{ 0 };
    /// @brief Максимальное количество итераций линейного поиска
    size_t line_search_max_iterations{ 0 };
    /// @brief Фактор уменьшения функции при линейном поиске
    double line_search_function_decrement_factor{ std::numeric_limits<double>::quiet_NaN() };    
    /// @brief Критерий остановки по норме приращения аргумента
    double argument_increment_norm_tolerance{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Условие сбора диагностики расчета
    analysis_execution_mode analysis_mode{ analysis_execution_mode::never };
    /// @brief Флаги диагностики целевой функции
    fixed_solver_analysis_parameters_t analysis_parameters;
    /// @brief Создаёт набор настроек солвера со значениями по умолчанию
    static nodal_solver_settings_t default_values() {
        nodal_solver_settings_t result;
        result.random_pressure_deviation = 0.0;
        result.max_iterations = 100;
        result.line_search_max_iterations = 100;
        result.line_search_function_decrement_factor = 10.0;
        result.argument_increment_norm_tolerance = 1e-6;
        result.analysis_mode = analysis_execution_mode::never;
        return result;
    }
};

/// @brief Класс расчета потокораспределения методом МД алгоритмом Ньютона-Рафсона
/// @tparam ModelIncidences Тип инцидентностей (должен быть model_incidences_t с полем model, указывающим на closing_relation_t*)
template<typename ModelIncidences>
class nodal_solver_t : public fixed_system_t<-1> {
private:
    using Settings = nodal_solver_settings_t;
private:
    /// @brief объект класса сети
    graphlib::basic_graph_t<ModelIncidences>& graph;
    /// @brief заданное давление в последних висячих ребрах 
    /// (должны соответствовать последним вершинам)
    Eigen::VectorXd given_pressure;
    /// @brief Сокращенная матрица инцидентности для односторонних дуг 
    /// (только строки узлов с неизвестным P)
    Eigen::SparseMatrix<double, Eigen::RowMajor> A1r;
    /// @brief Сокращенная матрица инцидентности для односторонних дуг 
    /// (только строки узлов с заданным P)
    Eigen::SparseMatrix<double, Eigen::RowMajor> A1p;
    /// @brief Сокращенная матрица инцидентности для двусторонних дуг (только строки неизвестных узлов)
    /// (только строки узлов с неизвестным P)
    Eigen::SparseMatrix<double, Eigen::RowMajor> A2r;
    /// @brief Сокращенная матрица инцидентности для двусторонних дуг (только строки неизвестных узлов)
    /// (только строки узлов с неизвестным P)
    Eigen::SparseMatrix<double, Eigen::RowMajor> A2p;
    /// @brief Настройки расчета
    nodal_solver_settings_t settings;
public:

    /// @brief Якобиан системы уравнений
    virtual std::vector<Eigen::Triplet<double>> jacobian_sparse(const Eigen::VectorXd& reduced_pressure) override
    {
        /// Восстанавливаем полный вектор давлений
        Eigen::VectorXd full_pressures = get_full_pressure(reduced_pressure);
        /// Вычисляем количество узлов с неизвестными давлениями (переменными)
        std::size_t variable_nodes = static_cast<std::size_t>(
            full_pressures.size() - given_pressure.size());
        /// Получаем количество ребер типа 2 в графе
        std::size_t edges2_count = graph.edges2.size();

        /// Создаем разреженное представление триплетов 
        /// dpsi/dPu - производная потоков по расчетным давлениям
        std::vector<Eigen::Triplet<double>> d_triplets;
        /// Максимально возможное количество элементов
        d_triplets.reserve(edges2_count * 2);
        /// Выполняем обход по всем ребрам типа 2 (цикл)
        for (std::size_t i = 0; i < edges2_count; ++i) {
            /// ссылка на текущее ребро
            const auto& edge = graph.edges2[i];
            /// индекс начального узла дуги
            std::size_t from = edge.from;
            /// индекс конечного узла дуги
            std::size_t to = edge.to;

            ///давления в начальном и конечном узлах
            double p_from = full_pressures[from];
            double p_to = full_pressures[to];
            ///вычисление производных потока по давлениям на входе и выходе
            auto [d_in, d_out] = edge.model->calculate_flow_derivative(p_from, p_to);

            if (!std::isfinite(d_in) || !std::isfinite(d_out)) {
                throw std::runtime_error("Derivative is nan");
            }

            d_in = ensure_abs_epsilon_value(d_in);
            d_out = ensure_abs_epsilon_value(d_out);        
           
            // Добавление соответствующего значения для начального узла (если там не задано давление)
            if (from < variable_nodes) {
                d_triplets.emplace_back((int)i, (int)from, d_in);
            }
            // Добавление соответствующего значения для конечного узла (если там не задано давление)
            if (to < variable_nodes) {
                d_triplets.emplace_back((int)i, (int)to, d_out);
            }
        }
        ///Создание разреженной матрицы dpsi/dPu размерностью: количество ребер × количество узлов, где не задано давление
        Eigen::SparseMatrix<double, Eigen::RowMajor> dpsi_dPu(
            static_cast<int>(edges2_count),
            static_cast<int>(variable_nodes));
        ///Заполнение матрицы из триплетов
        dpsi_dPu.setFromTriplets(d_triplets.begin(), d_triplets.end());
        /// сжимание матрицы для оптимального хранения
        dpsi_dPu.makeCompressed();

        // J = A2r * dpsi/dPu
        /// вычисление Якобиана
        Eigen::SparseMatrix<double> J = A2r * dpsi_dPu;
        /// перевод Якобиана в формат триплет
        std::vector<Eigen::Triplet<double>> analyt_tripletes = graphlib::get_triplets(J);
        return analyt_tripletes;

        // на всякий случай, для проверки на малых схемах, пока отлаживаемся:
        //auto numeric_tripletes = jacobian_sparse_numeric(x); 
    }

private:

    /// @brief Возвращает абсолютное значение, не меньшее заданного порога epsilon
    static double ensure_abs_epsilon_value(double value, double epsilon = 1e-6) {
        double abs_value = std::abs(value);
        if (abs_value >= epsilon)
            return value;
        return fixed_solvers::pseudo_sgn(value) * epsilon;
    }

    /// @brief функция сбора вектора полных давлений, на вход подается вектор неизвестных давлений (переменные) 
    Eigen::VectorXd get_full_pressure(const Eigen::VectorXd& reduced_pressures) const
    {
        // полный вектор давлений содержит неизвестные + известные давления
        Eigen::Index n = static_cast<Eigen::Index>(graph.get_vertex_count());
        Eigen::Index pressure_nodes = (given_pressure.size());
        // Создается вектор полных давлений размером n (общее количество узлов)
        Eigen::VectorXd full_pressures(n);
        // Заполняем начало вектора full_pressures значениями из reduced_pressures
        // head(n - pressure_nodes) - берем первые (n - pressure_nodes) элементов
        // Это соответствует узлам с неизвестными давлениями
        full_pressures.head(n - pressure_nodes) = reduced_pressures;
        //Заполняем конец вектора full_pressures известными значениями давлений
        // tail(pressure_nodes) - берем последние pressure_nodes элементов
        // given_pressure содержит заданные давления для соответствующих узлов
        full_pressures.tail(pressure_nodes) = given_pressure; // последные вершины имеют заданное давление
        return full_pressures;
    }

    /// @brief функция вычисления вектора расходов через все ребра
    Eigen::VectorXd compute_flows1_pressure(const Eigen::VectorXd& flows2) const
    {
        Eigen::VectorXd flows1_pressures(get_given_pressure_size());

        Eigen::SparseLU<Eigen::SparseMatrix<double> > solver;
        solver.analyzePattern(A1p);
        solver.factorize(A1p);
        if (solver.info() != Eigen::Success) {
            throw std::runtime_error("Cannot compute flows1 of pressure nodes");
        }

        Eigen::VectorXd right_party = -A2p * flows2;
        Eigen::VectorXd result = solver.solve(right_party);
        return result;
    }


    /// @brief функция вычисления вектора расходов через все ребра
    Eigen::VectorXd compute_flows1_reduced(const Eigen::VectorXd& full_pressures) const
    {
        //Определяем количество узлов с заданными давлениями (граничные условия)
        std::size_t pressure_nodes = given_pressure.size();
        //Создается вектор flows1 для расходов через ребра 1-го типа размерностью: общее количество ребер 1-го типа минус количество граничных узлов
        Eigen::VectorXd flows1(graph.edges1.size() - pressure_nodes);

        // Расходы через ребра 1-го типа (постоянные)
        // Проходим по всем ребрам 1-го типа, кроме тех, что связаны с граничными узлами
        for (size_t i = 0; i < graph.edges1.size() - pressure_nodes; i++) {
            //Для каждого ребра 1 - го типа извлекаем заранее известный расход
            flows1(i) = graph.edges1[i].model->get_known_flow();
        }

        return flows1;
    }

    /// @brief функция вычисления вектора расходов через все ребра
    Eigen::VectorXd compute_flows2(const Eigen::VectorXd& full_pressures) const
    {
        //Определяем количество узлов с заданными давлениями (граничные условия)
        std::size_t pressure_nodes = given_pressure.size();
        //Создается вектор flows2 для расходов через ребра 2-го типа размерностью: полное количество ребер 2-го типа
        Eigen::VectorXd flows2(graph.edges2.size());
        // Расходы через ребра 2-го типа (зависящие от перепада давления)
        for (size_t i = 0; i < graph.edges2.size(); i++) {
            //Ссылка на текущее ребро 2-го типа
            const auto& edge = graph.edges2[i];
            //давление в начальном узле ребра из полного вектора давлений
            double p_start = full_pressures[edge.from];
            //давление в конечном узле ребра из полного вектора давлений
            double p_end = full_pressures[edge.to];
            //Вычисляется расход через ребро на основе перепада давления
            flows2(i) = edge.model->calculate_flow(p_start, p_end);
            if (!std::isfinite(flows2(i))) {
                throw graphlib::graph_exception(
                    graphlib::graph_binding_t(graph_binding_type::Edge2, i),
                    L"Nan-значения при расчете невязок замыкающего соотношения для метода МД"
                );
            }
        }
        //Возвращается расходы через ребра 1-го и 2-го типов
        return flows2;
    }
public:
    /// @brief Оценивает начальное распределение давлений по известным P-притокам,
    /// используя разброс из настроек солвера
    Eigen::VectorXd estimation_random_reduced() const
    {
        return estimation_random().head(graph.get_vertex_count() - get_given_pressure_size());
    }

    /// @brief Оценивает начальное распределение давлений по известным P-притокам,
    /// используя разброс из настроек солвера
    Eigen::VectorXd estimation_random() const
    {
        double spread = settings.random_pressure_deviation;
        if (!std::isfinite(spread)) {
            throw std::runtime_error("Random pressure deviation is not set");
        }

        Eigen::Index unknown_count =
            static_cast<Eigen::Index>(graph.get_vertex_count()) - get_given_pressure_size();

        const double avg_predefined_pressure = (given_pressure.size() > 0) ?
            (given_pressure.sum() / static_cast<double>(given_pressure.size())) : 0.0;

        std::mt19937 generator(10u); // Генератор с заданным seed
        std::uniform_real_distribution<> dis(-spread / 2, spread / 2);

        Eigen::VectorXd estimation(unknown_count);
        std::generate(estimation.begin(), estimation.end(), [&]() { return avg_predefined_pressure + dis(generator); });

        return estimation;
    }

    /// @brief расчёт невязок r(P) = A1*ψ1(P) + A2*ψ2(P)
    /// @param reduced_pressures Принимает константную ссылку на вектор давлений
    /// @return Возвращает вектор невязок (размерности n) типа double
    virtual Eigen::VectorXd residuals(const Eigen::VectorXd& reduced_pressures) override
    {
        //Преобразование приведенных давлений в полный вектор давлений во всех узлах системы
        Eigen::VectorXd full_pressures = get_full_pressure(reduced_pressures);
        auto flows1 = compute_flows1_reduced(full_pressures);
        auto flows2 = compute_flows2(full_pressures);

        // вычисляем невязку r(P) = A1*ψ1(P) + A2*ψ2(P) (невязка равна сумме произведений матриц инцидентности на потоки)
        Eigen::VectorXd r = A1r * flows1 + A2r * flows2;

        // ввектор невязок
        return r;
    }
private:
    /// @brief Подготовка параметров солвера Ньютона-Рафсона
    fixed_solver_parameters_t<-1, 0, golden_section_search> prepare_solver_parameters() const {
        //Создание объекта параметров решателя с шаблонными параметрами: -1 - динамический размер, 0 - параметр, golden_section_search - метод линейного поиска
        fixed_solver_parameters_t<-1, 0, golden_section_search> params;

        params.line_search.iteration_count = static_cast<int>(settings.line_search_max_iterations);
        params.line_search.function_decrement_factor = settings.line_search_function_decrement_factor;
        params.iteration_count = static_cast<int>(settings.max_iterations);
        params.argument_increment_norm = settings.argument_increment_norm_tolerance;
        //params.residuals_norm = settings.residuals_norm_tolerance;
        params.analysis = settings.analysis_parameters;

        return params;
    }

    /// @brief Заполняет гидравлические результаты (давления во всех узлах + расходы)
    /// на основе сырых результатов численного метода
    void fill_flow_result_from_numerical(flow_distribution_result_t& result) const {
        const Eigen::VectorXd& reduced_pressure = result.numerical_result.argument;
        result.pressures = get_full_pressure(reduced_pressure);
        result.flows2 = compute_flows2(result.pressures);
        Eigen::VectorXd flows1_reduced = compute_flows1_reduced(result.pressures);
        Eigen::VectorXd flows1_pressure = compute_flows1_pressure(result.flows2);
        result.flows1 = Eigen::VectorXd(graph.edges1.size());
        result.flows1.head(flows1_reduced.size()) = flows1_reduced;
        result.flows1.tail(flows1_pressure.size()) = flows1_pressure;
    }

    /// @brief Статическая функция, возвращающая количество заданных давлений
    /// @param Константная ссылка на вектор граничных элементов edges1 
    /// @return возвращает вектор заданных давлений
    template<typename EdgeType>
    static size_t extract_known_pressures_count(const std::vector<EdgeType>& edges1)
    {
        std::size_t pressure_count = 0;
        //Обратный цикл по вектору edges1 (от конца к началу)
        for (auto it = edges1.rbegin(); it != edges1.rend(); ++it)
        {
            // Получение известного давления из модели текущего граничного элемента
            double known_pressure = it->model->get_known_pressure();
            //Проверка, что давление является конечным числом
            if (std::isfinite(known_pressure)) {
                pressure_count++;
            }
            else {
                //Если встретилось некорректное давление, прерывание цикла
                break;
            }
        }
        return pressure_count;
    }

    /// @brief В диагностической информации сокращенный вектор давлений заменяет на полный вектор
    void transform_analysis_from_reduced_to_full_pressure(fixed_solver_result_analysis_t<-1>* analysis) {
        if (analysis == nullptr)
            return;

        Eigen::Index iteration_count = analysis->argument_history.size();
        for (Eigen::Index i = 0; i < iteration_count; ++i) {
            analysis->argument_history[i] = get_full_pressure(analysis->argument_history[i]);
        }
    }

    /// @brief Выполняет итерационную процедуру численного метода; результат и диагностика пишутся в переданные буферы
    void execute_numerical_method(const Eigen::VectorXd& initial,
        fixed_solver_parameters_t<-1, 0, golden_section_search>& params,
        fixed_solver_result_t<-1>* numerical_result,
        fixed_solver_result_analysis_t<-1>* analysis) 
    {
        fixed_newton_raphson<-1>::solve(*this, initial, params, numerical_result, analysis);
        transform_analysis_from_reduced_to_full_pressure(analysis);
    }

public:
    /// @brief решение системы методом Ньютона-Рафсона
    /// @param initial_pressures Вектор начального приближения давлений в узлах без P-условий
    /// @return Структура с расходами ребер, давлениями в узлах, результатом численного метода и диагностикой
    flow_distribution_result_t solve(const Eigen::VectorXd& initial_pressures)
    {
        fixed_solver_parameters_t<-1, 0, golden_section_search> params 
            = prepare_solver_parameters();

        Eigen::VectorXd initial;
        if (initial_pressures.size() != 0) {
            initial = initial_pressures.head(
                initial_pressures.size() - get_given_pressure_size()
            );
        }
        else {
            initial = estimation_random_reduced();
        }

        flow_distribution_result_t result;
        
        bool analysis_needed = (settings.analysis_mode == analysis_execution_mode::always);
        fixed_solver_result_analysis_t<-1>* residuals_analysis = (analysis_needed)
            ? &result.residuals_analysis
            : nullptr;

        execute_numerical_method(initial, params, &result.numerical_result, residuals_analysis);

        if (result.numerical_result.result_code != numerical_result_code_t::Converged) {
            bool analysis_if_failed = (settings.analysis_mode == analysis_execution_mode::if_failed);
            if (analysis_if_failed) {
                execute_numerical_method(initial, params, &result.numerical_result, &result.residuals_analysis);
            }
            return result;
        }

        fill_flow_result_from_numerical(result);
        return result;
    }

protected:
    /// @brief Статическая функция, возвращающая вектор заданных давлений
    /// @param Константная ссылка на вектор граничных элементов edges1 
    /// @return возвращает вектор заданных давлений
    template<typename EdgeType>
    static Eigen::VectorXd extract_known_pressures(size_t nodes_count, const std::vector<EdgeType>& edges1)
    {
        std::size_t pressure_count = extract_known_pressures_count(edges1);
        std::vector<double> given_pressures(pressure_count);

        // Сдвиг индексов узлов с известным давлением - равен количеству узлов без давления
        size_t shift = nodes_count - pressure_count;
        // Обратный цикл по edges1 с заданным давлением
        for (auto it = edges1.rbegin(); it != edges1.rbegin() + pressure_count; ++it)
        {
            // Получение известного давления
            double known_pressure = it->model->get_known_pressure();
            size_t node = it->get_vertex_for_single_sided_edge();
            given_pressures[node - shift] = known_pressure;
        }

        //Создание Eigen::VectorXd из данных вектора given_pressures
        return Eigen::VectorXd::Map(given_pressures.data(), given_pressures.size());
    }
public:
    /// @brief Возвращает количество вершин с заданным давлением
    Eigen::Index get_given_pressure_size() const {
        return given_pressure.size();
    }
private:
    /// @brief Проверка дуг с двумя давлениями на границах
    void check_no_PP_edges() const {
        std::size_t reduced_vertex_count = 
            graph.get_vertex_count() - get_given_pressure_size();

        for (const auto& edge2 : graph.edges2) {
            if (edge2.from >= reduced_vertex_count &&
                edge2.to >= reduced_vertex_count) 
            {
                // TODO: Реализовать по Введению в МД
                // Перед этим организовать перенумеровку, чтобы такие дуги были в конце
                throw std::runtime_error("PP edges handling is not implemented");
            }
        }


    }
public:
    /// @brief Запоминает исходные данные и инициализирует настройки солвера значениями по умолчанию
    /// @param graph Ссылка на объект графа
    nodal_solver_t(graphlib::basic_graph_t<ModelIncidences>& graph, nodal_solver_settings_t settings)
        : graph(graph)
        , settings(settings)
        , given_pressure(extract_known_pressures(graph.get_vertex_count(), graph.edges1))
    {
        check_no_PP_edges();

        //Получение матриц инцидентности из графа

        std::tie(A1r, A1p) = graph.split_reduced_incidences1(given_pressure.size());
        std::tie(A2r, A2p) = graph.split_reduced_incidences2(given_pressure.size());
    }

};

}