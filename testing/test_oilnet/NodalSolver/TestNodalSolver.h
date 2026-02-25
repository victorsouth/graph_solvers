#include "test_oilnet.h"

/// @brief Вспомогательный класс для тестирования формирования случайного начального приближения для узлового солвера МД
template <typename ModelIncidences>
class nodal_solver_Test : public graphlib::nodal_solver_t<ModelIncidences>
{
public:
    /// @brief Создаёт тестовый солвер на заданном графе с указанными настройками узлового решателя
    /// @param graph Граф, для которого выполняются тестовые расчёты
    /// @param settings Настройки узлового солвера, используемые в тесте
    nodal_solver_Test(graphlib::basic_graph_t<ModelIncidences>& graph, graphlib::nodal_solver_settings_t settings)
        : graphlib::nodal_solver_t<ModelIncidences>(graph, settings)
    {
    }

    /// @brief Вызывает защищённый метод `estimate_random` базового класса для получения тестового начального приближения давлений
    Eigen::VectorXd test_estimate_random() {
        return this->estimation_random_reduced();
    }

    /// @brief Предоставляет доступ к защищённому статическому методу `extract_known_pressures` базового класса
    using graphlib::nodal_solver_t<ModelIncidences>::extract_known_pressures;
};

/// @brief Солвер МД, предназначенный для сравнения аналитического и численного расчета якобиана
class nodal_solver_numerical_jacobian : public graphlib::nodal_solver_t<graphlib::nodal_edge_t> {
public:
    /// @brief Просто вызываем такой же конструктор в предке
    nodal_solver_numerical_jacobian(graphlib::basic_graph_t<graphlib::nodal_edge_t>& graph)
        : nodal_solver_t(graph, graphlib::nodal_solver_settings_t::default_values())
    {
    }
    /// @brief Этом метод вызываем, чтобы сравнить результат с jacobian_sparse
    std::vector<Eigen::Triplet<double>> test_numeric_jacobian(const Eigen::VectorXd& reduced_pressure) {
        return this->jacobian_sparse_numeric(reduced_pressure);
    }
};

/// @brief тест простой схемы
TEST(NodalSolver, Handles2Edges)
{
    using namespace oil_hydro_simple;
    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();

    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    Eigen::VectorXd estimation(3);
    /*estimation << 31.2e5, 1.15e5;*/
    estimation << 1.17e5, 1.15e5, 1e5;

    // Act
    graphlib::flow_distribution_result_t result = solver.solve(estimation);

    // Assert
    // Проверка давлений в узлах
    ASSERT_NEAR(result.pressures[0], 100800, 1);
    ASSERT_NEAR(result.pressures[1], 100300, 1);
    ASSERT_NEAR(result.pressures[2], 100000, 1);

    // Проверка расходов через элементы
    for (int i = 0; i < result.flows1.size(); i++) {
        EXPECT_NEAR(result.flows1[i], model_properties.src.std_volflow, 1e-6);
    }
    for (int i = 0; i < result.flows2.size(); i++) {
        EXPECT_NEAR(result.flows2[i], model_properties.src.std_volflow, 1e-6);
    }

    // Проверка монотонного уменьшения давления
    for (int i = 0; i < result.pressures.size() - 1; i++) {
        EXPECT_GT(result.pressures[i], result.pressures[i + 1]);
    }
}

/// @brief Тест на корректность размерности Якобиана
TEST(NodalSolver, HasJacobianCorrectDimensions)
{
    using namespace oil_hydro_simple;
    // Arrange - подготовка графа и решателя
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);
    Eigen::VectorXd estimation(2);
    estimation << 31.2e5, 1.15e5; // подумать над начальным приближением

    // Act - вычисление якобиана
    auto jacobian = solver.jacobian_sparse(estimation);//Вычисление разреженной матрицы Якобиана для текущего приближения
    Eigen::Matrix2d jac_dense = Eigen::Matrix2d::Zero();// Инициализация матрицы 0
    for (const auto& triplet : jacobian) {
        jac_dense(triplet.row(), triplet.col()) += triplet.value();// переход от разреженной матрицы в полную
    }

    // Assert - проверка размерности
    EXPECT_EQ(jac_dense.rows(), jac_dense.cols()); // проверяем что матрица квадратная
    EXPECT_EQ(jac_dense.rows(), estimation.size()); // и что размер соответствует оценке


}

/// @brief Проверка структуры разреженной матрицы 
TEST(NodalSolver, HasJacobianCorrectSparsityPattern)
{
    using namespace oil_hydro_simple;
    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    const int system_dimension = 2; // размерность для данной конфигурации графа

    Eigen::VectorXd estimation(2);
    estimation << 31.2e5, 1.15e5;

    // Act - получение якобиана и подсчет ненулевых элементов
    auto jacobian = solver.jacobian_sparse(estimation);
    int non_zero_count = 0;

    for (const auto& triplet : jacobian) {
        non_zero_count++;
        // Assert - проверка корректности индексов
        // Универсальная проверка границ индексов
        EXPECT_GE(triplet.row(), 0);
        EXPECT_LT(triplet.row(), system_dimension);
        EXPECT_GE(triplet.col(), 0);
        EXPECT_LT(triplet.col(), system_dimension);

        // Дополнительная проверка: значение не должно быть NaN или бесконечностью
        EXPECT_FALSE(std::isnan(triplet.value()));
        EXPECT_FALSE(std::isinf(triplet.value()));
    }

    // Assert - проверка количества ненулевых элементов
    // Проверка, что матрица не пустая
    EXPECT_GT(non_zero_count, 0);

    // Универсальная проверка максимального количества ненулевых элементов
    const int max_possible_non_zeros = system_dimension * system_dimension;
    EXPECT_LE(non_zero_count, max_possible_non_zeros);

}

/// @brief проверка невырожденности Якобиана
TEST(NodalSolver, HasNonSingularJacobian)
{
    using namespace oil_hydro_simple;
    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);
    Eigen::VectorXd estimation(2);
    estimation << 31.2e5, 1.15e5;

    // Act - вычисление определителя
    auto jacobian = solver.jacobian_sparse(estimation);
    Eigen::Matrix2d jac_dense = Eigen::Matrix2d::Zero();
    for (const auto& triplet : jacobian) {
        jac_dense(triplet.row(), triplet.col()) += triplet.value();
    }
    double determinant = jac_dense.determinant();//Вычисление определителя плотной матрицы

    // Assert - проверка невырожденности
    EXPECT_GT(std::abs(determinant), 1e-10);
}

/// @brief  Непрерывность Якобиана (малые изменения в входных данных приводят к малым изменениям в Якобиане) 
TEST(NodalSolver, HasContinuousJacobian)
{
    using namespace oil_hydro_simple;
    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);
    Eigen::VectorXd estimation(2);
    estimation << 31.2e5, 1.15e5;

    // Act - вычисление якобианов для близких значений
    auto jacobian1 = solver.jacobian_sparse(estimation);
    Eigen::VectorXd estimation2 = estimation * 1.1; // Небольшое изменение (увеличение на 10%)
    auto jacobian2 = solver.jacobian_sparse(estimation2);//Вычисление Якобиана для измененного вектора

    Eigen::Matrix2d jac_dense1 = Eigen::Matrix2d::Zero();
    Eigen::Matrix2d jac_dense2 = Eigen::Matrix2d::Zero();

    for (const auto& triplet : jacobian1) {
        jac_dense1(triplet.row(), triplet.col()) += triplet.value();
    }
    for (const auto& triplet : jacobian2) {
        jac_dense2(triplet.row(), triplet.col()) += triplet.value();
    }
    //Вычисление нормы разности между двумя матрицами Якобиана
    double jac_diff = (jac_dense1 - jac_dense2).norm();

    // Assert - проверка непрерывности (малое изменение при малом возмущении)
    EXPECT_LT(jac_diff, 1e6);
}

/// @brief Тест проверяет, что после решения системы матрица Якобиана остается хорошо обусловленной 
TEST(NodalSolver, HasWellConditionedFinalJacobian)
{
    using namespace oil_hydro_simple;
    // Arrange - решение системы для получения финального состояния
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);
    Eigen::VectorXd estimation(3);
    estimation << 31.2e5, 1.15e5, 1e5;

    // Act - решение системы и вычисление финального якобиана
    auto result = solver.solve(estimation); // Решение системы уравнений - получение давлений и потоков
    Eigen::VectorXd solution_pressures = result.pressures.head(2);

    auto final_jacobian = solver.jacobian_sparse(solution_pressures);//Вычисление матрицы Якобиана для найденного решения
    Eigen::SparseMatrix<double> jacobian_matrix(2, 2);
    jacobian_matrix.setFromTriplets(final_jacobian.begin(), final_jacobian.end());
    Eigen::Matrix2d final_jac_dense = jacobian_matrix;

    double final_determinant = final_jac_dense.determinant();//Вычисление определителя финальной матрицы Якобиана

    // Assert - проверка что якобиан в решении хорошо обусловлен
    EXPECT_GT(std::abs(final_determinant), 1e-10);
}

/// @brief Проверяет, что Якобиан вычисляется корректно при нулевых значениях переменных
TEST(NodalSolver, HandlesJacobianAtZeroPressure)
{
    using namespace oil_hydro_simple;
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    Eigen::VectorXd zero_estimation(2);
    zero_estimation << 0.0, 0.0;  // Нулевые давления во всех узлах

    auto jacobian_zero = solver.jacobian_sparse(zero_estimation);// Получение матрицы Якобиана для нулевого вектора

    Eigen::Matrix2d jac_dense_zero = Eigen::Matrix2d::Zero();

    for (const auto& triplet : jacobian_zero) {
        jac_dense_zero(triplet.row(), triplet.col()) += triplet.value();
    }

    double zero_determinant = jac_dense_zero.determinant();// Получение матрицы Якобиана для нулевого вектора

    // Проверка наличия ненулевых элементов
    int non_zero_count = 0;
    double max_abs_value = 0.0;
    for (const auto& triplet : jacobian_zero) {
        non_zero_count++;
        max_abs_value = std::max(max_abs_value, std::abs(triplet.value()));
    }

    // Проверка, что матрица не пустая (должны быть ненулевые элементы)
    EXPECT_GT(non_zero_count, 0);

    // Проверка, что значения в матрице конечны (не NaN и не бесконечность)
    EXPECT_TRUE(std::isfinite(max_abs_value));

    // Проверка, что матрица не содержит аномально больших значений
    EXPECT_LT(max_abs_value, 1e15);

    // Проверка корректности индексов всех ненулевых элементов
    for (const auto& triplet : jacobian_zero) {
        EXPECT_GE(triplet.row(), 0);
        EXPECT_LT(triplet.row(), zero_estimation.size());
        EXPECT_GE(triplet.col(), 0);
        EXPECT_LT(triplet.col(), zero_estimation.size());
    }

    // Дополнительная проверка: сравнение с Якобианом в малой окрестности нуля
    Eigen::VectorXd small_estimation(2);
    small_estimation << 1e-10, 1e-10;  // Очень малые значения

    auto jacobian_small = solver.jacobian_sparse(small_estimation);
    Eigen::Matrix2d jac_dense_small = Eigen::Matrix2d::Zero();
    for (const auto& triplet : jacobian_small) {
        jac_dense_small(triplet.row(), triplet.col()) += triplet.value();
    }

    // Проверка непрерывности: Якобианы в нуле и в малой окрестности не должны сильно отличаться
    double diff_norm = (jac_dense_zero - jac_dense_small).norm();
    EXPECT_LT(diff_norm, 1e-5);
}

/// @brief Сравнение "боевого" аналитического якобиана с численным
TEST(NodalSolver, MatchesNumericalJacobian) {
    using namespace oil_hydro_simple;
    // Arrange - подготавливаем схему с тестовыми параметрами
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph, settings);

    // Создаем два солвера - для аналитического и численного расчета
    graphlib::nodal_solver_t<graphlib::nodal_edge_t> analytical_solver(scheme.graph, settings);
    nodal_solver_numerical_jacobian numerical_solver(scheme.graph);

    // Начальное приближение давлений в узлах
    Eigen::VectorXd estimation(2);
    estimation << 31.2e5, 1.15e5;  // Давления в узлах 1 и 2

    // Act - вычисляем якобианы обоими методами
    auto analytical_jacobian = analytical_solver.jacobian_sparse(estimation);
    auto numerical_jacobian = numerical_solver.test_numeric_jacobian(estimation);

    // Convert to dense matrices for comparison
    Eigen::Matrix2d analytical_dense = Eigen::Matrix2d::Zero();
    Eigen::Matrix2d numerical_dense = Eigen::Matrix2d::Zero();

    for (const auto& triplet : analytical_jacobian) {
        analytical_dense(triplet.row(), triplet.col()) += triplet.value();
    }

    for (const auto& triplet : numerical_jacobian) {
        numerical_dense(triplet.row(), triplet.col()) += triplet.value();
    }

    // Assert - сравниваем результаты
    double absolute_error = (analytical_dense - numerical_dense).norm();
    double relative_error = absolute_error / analytical_dense.norm();

    // Проверяем, что методы дают близкие результаты
    EXPECT_LT(absolute_error, 1e-6) << "Абсолютная ошибка между аналитическим и численным Якобианом велика";
    EXPECT_LT(relative_error, 1e-4) << "Относительная ошибка между аналитическим и численным Якобианом велика";

    // Проверяем совпадение структуры разреженности
    EXPECT_EQ(analytical_jacobian.size(), numerical_jacobian.size())
        << "Якобианы имеют разную структуру разреженности";

    // Проверяем, что ненулевые элементы в одинаковых позициях
    std::map<std::pair<int, int>, double> analytical_elements;
    std::map<std::pair<int, int>, double> numerical_elements;

    for (const auto& triplet : analytical_jacobian) {
        analytical_elements[{triplet.row(), triplet.col()}] = triplet.value();
    }

    for (const auto& triplet : numerical_jacobian) {
        numerical_elements[{triplet.row(), triplet.col()}] = triplet.value();
    }

    EXPECT_EQ(analytical_elements.size(), numerical_elements.size());

    // Проверяем совпадение значений для каждой позиции
    for (const auto& [position, analytical_value] : analytical_elements) {
        //Поиск элемента с такой же позицией в численном якобиане
        auto numerical_it = numerical_elements.find(position);
        //Проверяем, что элемент найден в численном якобиане
        EXPECT_NE(numerical_it, numerical_elements.end())
            << "Не найден элемент на позиции ("
            << position.first << ", " << position.second << ")";

        if (numerical_it != numerical_elements.end()) {
            //Вычисление абсолютной разницы между аналитическим и численным значениями
            double element_error = std::abs(analytical_value - numerical_it->second);
            EXPECT_LT(element_error, 1e-6)
                << "Большая разница на позиции (" << position.first << ", " << position.second << "): "
                << "аналитическое = " << analytical_value << ", численное = " << numerical_it->second;
        }
    }
}

/// @brief // ПОКА НЕ РАБОТАЕТ !Тест для проверки корректности работы решателя при малых перепадах давления
//TEST(NodalSolver, SolverWorksWithSmallPressureDrops)
//{
//    // Arrange
//    simple_hydraulic_scheme_properties model_properties;
//    model_properties.src.std_volflow = 0.00;
//    model_properties.sink.pressure = 1e5;
//    model_properties.resist1.resistance = 1000.0;
//    model_properties.resist2.resistance = 1000.0;
//    simple_hydraulic_scheme scheme(model_properties);
//    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph);
//
//    // Несколько разных начальных приближений
//    std::vector<Eigen::VectorXd> initial_guesses = {
//        Eigen::Vector2d(1.1e5, 1.05e5),
//        Eigen::Vector2d(1.2e5, 1.1e5),
//        Eigen::Vector2d(1.05e5, 1.02e5)
//    };
//
//    bool any_solution_found = false;
//
//    for (const auto& estimation : initial_guesses) {
//        try {
//            // ACT
//            auto [pressures, flows1, flows2] = solver.solve(estimation);
//            any_solution_found = true;
//
//            // ASSERT
//            EXPECT_GT(pressures[0], pressures[1]) << "Pressure should decrease";
//            EXPECT_GT(pressures[1], pressures[2]) << "Pressure should decrease";
//
//            // Если решение найдено, выходим из цикла
//            break;
//
//        }
//        catch (const std::exception& e) {
//            // Продолжаем пробовать следующие начальные условия
//            continue;
//        }
//    }
//
//    EXPECT_TRUE(any_solution_found) << "Solver should find solution with at least one initial guess";
//}

//TEST(NodalSolver, SolverWorksWithSmallPressureDrops)
//{
//    // Arrange
//    simple_hydraulic_scheme_properties model_properties;
//    model_properties.src.std_volflow = 0.00;
//    model_properties.sink.pressure = 1e5;
//    model_properties.resist1.resistance = 1000.0;
//    model_properties.resist2.resistance = 1000.0;
//    simple_hydraulic_scheme scheme(model_properties);
//    graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver(scheme.graph);
//
//    // Несколько разных начальных приближений
//    std::vector<Eigen::VectorXd> initial_guesses = {
//        Eigen::Vector2d(1.1e5, 1.05e5),
//        Eigen::Vector2d(1.2e5, 1.1e5),
//        Eigen::Vector2d(1.05e5, 1.02e5),
//        Eigen::Vector2d(1.01e5, 1.005e5)  // Добавляем очень близкие значения
//    };
//
//    bool any_solution_found = false;
//    std::string last_error;
//
//    for (const auto& estimation : initial_guesses) {
//        try {
//            // ACT
//            auto [pressures, flows1, flows2] = solver.solve(estimation);
//            any_solution_found = true;
//
//            // ASSERT
//            // При нулевом расходе давления должны быть равны во всех узлах
//            EXPECT_NEAR(pressures[0], pressures[1], 1e-6) << "Pressures should be equal with zero flow";
//            EXPECT_NEAR(pressures[1], pressures[2], 1e-6) << "Pressures should be equal with zero flow";
//
//            //// Проверяем, что расходы близки к нулю
//            //EXPECT_NEAR(flows1, 0.0, 1e-9) << "Flow should be zero";
//            //EXPECT_NEAR(flows2, 0.0, 1e-9) << "Flow should be zero";
//
//            // Если решение найдено, выходим из цикла
//            break;
//
//        }
//        catch (const std::exception& e) {
//            last_error = e.what();
//            // Продолжаем пробовать следующие начальные условия
//            continue;
//        }
//    }
//
//    if (!any_solution_found) {
//        // Добавляем отладочную информацию
//        std::cout << "All initial guesses failed. Last error: " << last_error << std::endl;
//
//        // Пробуем с очень маленьким, но не нулевым расходом
//        try {
//            model_properties.src.std_volflow = 1e-10;  // Очень маленький, но не нулевой расход
//            simple_hydraulic_scheme scheme2(model_properties);
//            graphlib::nodal_solver_t<graphlib::nodal_edge_t> solver2(scheme2.graph);
//
//            auto [pressures, flows1, flows2] = solver2.solve(Eigen::Vector2d(1.01e5, 1.005e5));
//            any_solution_found = true;
//
//            std::cout << "Solution found with tiny flow: pressures = ["
//                << pressures[0] << ", " << pressures[1] << ", " << pressures[2]
//                << "], flows = [" << flows1 << ", " << flows2 << "]" << std::endl;
//        }
//        catch (const std::exception& e) {
//            std::cout << "Even tiny flow failed: " << e.what() << std::endl;
//        }
//    }
//
//    EXPECT_TRUE(any_solution_found) << "Solver should find solution with at least one initial guess";
//}

/// @brief Проверка получения начального распределения давления - случайно распределенного относительно известных P-притоков
TEST(NodalSolver, EstimatesInitialPressuresAroundGivenPressure)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();
    simple_hydraulic_scheme scheme(model_properties);
    auto settings = graphlib::nodal_solver_settings_t::default_values();
    settings.random_pressure_deviation = 1e3;
    nodal_solver_Test<nodal_edge_t> solver(scheme.graph, settings);

    const double spread = settings.random_pressure_deviation;

    Eigen::VectorXd estimation = solver.test_estimate_random();

    const double sink_pressure = scheme.sink_model.get_known_pressure();

    constexpr size_t veriteces_with_pressure = 1;
    const size_t vertices_without_pressure = static_cast<Eigen::Index>(scheme.graph.get_vertex_count() - veriteces_with_pressure);

    ASSERT_EQ(estimation.size(), vertices_without_pressure);

    for (Eigen::Index i = 0; i < estimation.size(); ++i) {
        double diff = std::abs(estimation[i] - sink_pressure);
        EXPECT_LE(diff, spread)
            << "Начальное давление в узле " << i
            << " выходит за пределы допустимого разброса относительно давления в стоке";
    }
}

/// @brief Проверяет корректное формирование вектора известных давлений - 
/// порядок давлений должен соответствовать порядку возрастания номеров вершин с давлениями
TEST(NodalSolver, ExtractsKnownPressuresForNontrivialNodesOrder)
{
    using namespace graphlib;
    using namespace oil_hydro_simple;

    // Arrange
    auto model_properties = simple_hydraulic_scheme_properties::default_scheme();

    model_properties.src.pressure = 100e3;
    model_properties.src.std_volflow = std::numeric_limits<double>::quiet_NaN();

    model_properties.sink.pressure = 90e3;
    model_properties.sink.std_volflow = std::numeric_limits<double>::quiet_NaN();

    model_properties.resist1.resistance = 11550;
    model_properties.resist2.resistance = 12918;

    simple_hydraulic_scheme scheme(model_properties, false);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(0), &scheme.src_model);
    scheme.graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(2), &scheme.sink_model);

    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(0, 1), &scheme.resist1_model);
    scheme.graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 2), &scheme.resist2_model);

    // Используем renumbering для получения графа с корректным порядком узлов
    auto settings = nodal_solver_settings_t::default_values();
    graphlib::nodal_solver_renumbering_t<nodal_edge_t> solver_renumbering(scheme.graph, settings);
    auto& renumbered_graph = const_cast<basic_graph_t<nodal_edge_t>&>(solver_renumbering.get_renumbered_graph());

    // Act - получаем вектор известных давлений
    Eigen::VectorXd known_pressures = nodal_solver_Test<nodal_edge_t>::extract_known_pressures(
        renumbered_graph.get_vertex_count(), 
        renumbered_graph.edges1);

    // Assert - в векторе должен быть сохранен порядок вершин - сначала вершина 1 (приток), потом вершина 2 (отбор)
    Eigen::VectorXd expected_known_pressures(2);
    expected_known_pressures << scheme.src_model.get_known_pressure(),
        scheme.sink_model.get_known_pressure();

    ASSERT_EQ(known_pressures, expected_known_pressures);
}