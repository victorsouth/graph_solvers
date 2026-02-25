#include "hydronet.h"

namespace graphlib {
;

#include <vector>
#include <tuple>
#include <utility>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>


/// @brief Класс для согласования потоков методом максимального правдоподобия
class flow_reconciler {
    const std::vector<double> original_flows_;      ///< Исходные значения потоков
    const std::vector<graphlib::vertex_incidences_t> constraints_; ///< Ограничения баланса

public:
    /// @brief Конструктор класса
    /// @param flows Вектор измеренных значений потоков
    /// @param constraints Вектор ограничений баланса для вершин
    flow_reconciler(const std::vector<double>& flows,
        const std::vector<graphlib::vertex_incidences_t>& constraints)
        : original_flows_(flows)
        , constraints_(constraints) 
    {
        if (original_flows_.empty() || constraints_.empty()) {
            throw std::invalid_argument("Flows and constraints cannot be empty");
        }
    }

    /// @brief Выполняет процедуру согласования потоков
    /// @return Кортеж из трех векторов:
    ///         1. Согласованные значения потоков
    ///         2. Величины корректировок
    ///         3. Исходные невязки баланса
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>> reconciliate() const {
        Eigen::SparseMatrix<double> weight_matrix = build_weight_matrix(original_flows_);
        Eigen::SparseMatrix<double> balance_matrix = build_balance_matrix(constraints_, original_flows_.size());

        auto [system_matrix, rhs] = build_mle_problem(weight_matrix, balance_matrix, original_flows_);

        Eigen::VectorXd solution = solve_system(system_matrix, rhs);
        std::vector<double> corrected_flows = extract_flows_from_solution(solution, original_flows_.size());

        std::vector<double> corrections = calculate_corrections(corrected_flows, original_flows_);
        std::vector<double> balances = calculate_original_balances(balance_matrix, original_flows_);

        return { corrected_flows, corrections, balances };
    }

private:

    /// @brief Строит матрицу задачи МНК для согласования потоков
    /// @param weight_matrix Матрица весов
    /// @param balance_matrix Матрица балансов
    /// @param flows Вектор потоков
    /// @return Пара: матрица системы и вектор правой части
    static std::pair<Eigen::SparseMatrix<double>, Eigen::VectorXd> build_mle_problem(
        const Eigen::SparseMatrix<double>& weight_matrix,
        const Eigen::SparseMatrix<double>& balance_matrix,
        const std::vector<double>& flows) 
    {

        int num_flows = static_cast<int>(weight_matrix.rows());
        int num_constraints = static_cast<int>(balance_matrix.rows());

        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(num_flows * (1 + num_constraints) + num_constraints * num_flows);

        // Блок 2R
        for (int i = 0; i < num_flows; ++i) {
            triplets.emplace_back(i, i, 2 * weight_matrix.coeff(i, i));
        }

        // Блоки A_balance^T и A_balance
        for (int i = 0; i < num_constraints; ++i) {
            for (int j = 0; j < num_flows; ++j) {
                double val = balance_matrix.coeff(i, j);
                triplets.emplace_back(j, num_flows + i, val);
                triplets.emplace_back(num_flows + i, j, val);
            }
        }

        Eigen::SparseMatrix<double> system_matrix(num_flows + num_constraints,
            num_flows + num_constraints);
        system_matrix.setFromTriplets(triplets.begin(), triplets.end());

        Eigen::VectorXd m = Eigen::VectorXd::Map(flows.data(), num_flows);
        Eigen::VectorXd rhs(num_flows + num_constraints);
        rhs.setZero();
        rhs.head(num_flows) = 2 * weight_matrix * m;

        return { system_matrix, rhs };
    }

    /// @brief Строит диагональную матрицу весов
    /// @param flows Вектор потоков
    /// @return Матрица весов
    static Eigen::SparseMatrix<double> build_weight_matrix(const std::vector<double>& flows) {
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(flows.size());

        double default_std = 1.0;
        for (int i = 0; i < flows.size(); ++i) {
            double flow_abs = std::max(std::abs(flows[i]), 1e-8);
            triplets.emplace_back(i, i, default_std / flow_abs);
        }

        Eigen::SparseMatrix<double> R(flows.size(), flows.size());
        R.setFromTriplets(triplets.begin(), triplets.end());
        return R;
    }

    /// @brief Строит матрицу балансов
    /// @param constraints Ограничения баланса
    /// @param num_flows Количество потоков
    /// @return Матрица балансов
    static Eigen::SparseMatrix<double> build_balance_matrix(
        const std::vector<graphlib::vertex_incidences_t>& constraints,
        size_t num_flows) {

        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(constraints.size() * 2);

        for (int i = 0; i < constraints.size(); ++i) {
            for (size_t inlet : constraints[i].inlet_edges) {
                triplets.emplace_back(i, static_cast<int>(inlet), 1.0);
            }
            for (size_t outlet : constraints[i].outlet_edges) {
                triplets.emplace_back(i, static_cast<int>(outlet), -1.0);
            }
        }

        Eigen::SparseMatrix<double> A(constraints.size(), num_flows);
        A.setFromTriplets(triplets.begin(), triplets.end());
        return A;
    }

    /// @brief Решает систему линейных уравнений
    /// @param system_matrix Матрица системы
    /// @param rhs Вектор правой части
    /// @return Вектор решения
    static Eigen::VectorXd solve_system(
        const Eigen::SparseMatrix<double>& system_matrix,
        const Eigen::VectorXd& rhs) {

        Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
        solver.analyzePattern(system_matrix);
        solver.factorize(system_matrix);

        if (solver.info() != Eigen::Success) {
            throw std::runtime_error("Matrix factorization failed");
        }

        Eigen::VectorXd solution = solver.solve(rhs);
        if (solver.info() != Eigen::Success) {
            throw std::runtime_error("System solution failed");
        }
        return solution;
    }

    /// @brief Извлекает значения потоков из решения системы
    /// @param solution Вектор решения
    /// @param num_flows Количество потоков
    /// @return Вектор согласованных потоков
    static std::vector<double> extract_flows_from_solution(
        const Eigen::VectorXd& solution,
        size_t num_flows) {

        std::vector<double> corrected_flows(num_flows);
        Eigen::VectorXd::Map(&corrected_flows[0], num_flows) = solution.head(num_flows);
        return corrected_flows;
    }

    /// @brief Вычисляет величины корректировок
    /// @param corrected_flows Согласованные потоки
    /// @param original_flows Исходные потоки
    /// @return Вектор корректировок
    static std::vector<double> calculate_corrections(
        const std::vector<double>& corrected_flows,
        const std::vector<double>& original_flows) {

        std::vector<double> corrections(original_flows.size());
        for (int i = 0; i < original_flows.size(); ++i) {
            corrections[i] = corrected_flows[i] - original_flows[i];
        }
        return corrections;
    }

    /// @brief Вычисляет исходные невязки баланса
    /// @param balance_matrix Матрица балансов
    /// @param flows Вектор потоков
    /// @return Вектор невязок
    static std::vector<double> calculate_original_balances(
        const Eigen::SparseMatrix<double>& balance_matrix,
        const std::vector<double>& flows) {

        Eigen::VectorXd m = Eigen::VectorXd::Map(flows.data(), flows.size());
        Eigen::VectorXd balances = balance_matrix * m;

        std::vector<double> result(balance_matrix.rows());
        Eigen::VectorXd::Map(&result[0], result.size()) = balances;
        return result;
    }
};

/// @brief Класс-обертка для исключения малых расходов при согласовании
class flow_reconciler_small_flows_exclusion {
private:
    const double flow_epsilon;
    const std::vector<double> original_flows_;
    const std::vector<graphlib::vertex_incidences_t> constraints_;
public:
    /// @brief Конструктор
    /// @param flows Вектор измеренных значений потоков
    /// @param constraints Вектор ограничений баланса для вершин
    /// @param eps Порог для определения малых расходов (0 < eps < 1)
    flow_reconciler_small_flows_exclusion(const std::vector<double>& flows,
        const std::vector<graphlib::vertex_incidences_t>& constraints,
        double eps = 1e-8)
        : flow_epsilon(eps)
        , original_flows_(flows)
        , constraints_(constraints)
    {
        if (flow_epsilon <= 0 || flow_epsilon >= 1) {
            throw std::invalid_argument("eps must be in (0, 1) range");
        }
    }

    /// @brief Выполняет согласование с исключением малых расходов
    /// @return Кортеж из трех векторов:
    ///         1. Согласованные значения потоков
    ///         2. Величины корректировок
    ///         3. Исходные невязки баланса
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>> reconciliate() const {
        // Определяем малые расходы
        auto [filtered_flows, filtered_constraints, small_flow_indices] =
            filter_small_flows(original_flows_, constraints_);

        // Создаем временный согласователь с фильтрованными данными
        flow_reconciler temp_reconciler(filtered_flows, filtered_constraints);

        // Выполняем согласование
        auto [corrected_flows, corrections, balances] = temp_reconciler.reconciliate();

        // Восстанавливаем исходный размер векторов
        auto full_corrected_flows = restore_flows(original_flows_, corrected_flows, small_flow_indices);
        auto full_corrections = calculate_full_corrections(original_flows_, full_corrected_flows);

        return { full_corrected_flows, full_corrections, balances };
    }

private:
    /// @brief Фильтрует малые расходы
    std::tuple<std::vector<double>,
        std::vector<graphlib::vertex_incidences_t>,
        std::vector<size_t>>
        filter_small_flows(const std::vector<double>& flows,
            const std::vector<graphlib::vertex_incidences_t>& constraints) const 
    {
        if (flows.empty()) 
            return { flows, constraints, {} };

        // Находим максимальный по модулю расход
        const double max_flow = std::abs(*std::max_element(
            flows.begin(), flows.end(),
            [](double a, double b) { return std::abs(a) < std::abs(b); }
        ));
        const double threshold = flow_epsilon * max_flow;

        // Собираем индексы малых расходов
        std::vector<size_t> small_flow_indices;
        for (size_t i = 0; i < flows.size(); ++i) {
            if (std::abs(flows[i]) < threshold) {
                small_flow_indices.push_back(i);
            }
        }

        // Фильтруем потоки (исключаем малые)
        std::vector<double> filtered_flows;
        for (size_t i = 0; i < flows.size(); ++i) {
            if (std::abs(flows[i]) >= threshold) {
                filtered_flows.push_back(flows[i]);
            }
        }

        // Фильтруем ограничения (обновляем индексы)
        std::vector<graphlib::vertex_incidences_t> filtered_constraints;
        for (const auto& constraint : constraints) {
            graphlib::vertex_incidences_t filtered_constraint;

            // Обрабатываем входящие ребра
            for (const auto edge : constraint.inlet_edges) {
                if (!contains_index_value(small_flow_indices, edge)) {
                    filtered_constraint.inlet_edges.push_back(
                        get_reduced_index(edge, small_flow_indices)
                    );
                }
            }

            // Обрабатываем исходящие ребра
            for (const auto edge : constraint.outlet_edges) {
                if (!contains_index_value(small_flow_indices, edge)) {
                    filtered_constraint.outlet_edges.push_back(
                        get_reduced_index(edge, small_flow_indices)
                    );
                }
            }

            filtered_constraints.push_back(filtered_constraint);
        }

        return { filtered_flows, filtered_constraints, small_flow_indices };
    }

    /// @brief Проверяет содержится ли индекс в векторе
    static bool contains_index_value(const std::vector<size_t>& indices, size_t value) {
        return std::find(indices.begin(), indices.end(), value) != indices.end();
    }

    /// @brief Вычисляет новый индекс после исключения малых расходов
    static size_t get_reduced_index(size_t original_index, const std::vector<size_t>& excluded_indices) 
    {
        size_t count = 0;
        for (size_t idx : excluded_indices) {
            if (idx < original_index) 
                ++count;
        }
        return original_index - count;
    }

    /// @brief Восстанавливает полный вектор потоков
    static std::vector<double> restore_flows(const std::vector<double>& original,
        const std::vector<double>& filtered,
        const std::vector<size_t>& excluded_indices) 
    {
        std::vector<double> result = original;
        size_t filtered_idx = 0;

        for (size_t i = 0; i < result.size(); ++i) {
            if (!contains_index_value(excluded_indices, i)) {
                result[i] = filtered[filtered_idx++];
            }
            // Малые расходы остаются без изменений
        }

        return result;
    }

    /// @brief Вычисляет полный вектор корректировок
    static std::vector<double> calculate_full_corrections(
        const std::vector<double>& original,
        const std::vector<double>& corrected) {

        std::vector<double> corrections(original.size());
        for (size_t i = 0; i < original.size(); ++i) {
            corrections[i] = corrected[i] - original[i];
        }
        return corrections;
    }
};


std::tuple<
    std::vector<double>,
    std::vector<double>,
    std::vector<double>
> estimate_flows_MLE(
    const std::vector<double>& flows,
    const std::vector<vertex_incidences_t>& constraints,
    double epsilon)
{

    flow_reconciler_small_flows_exclusion recon(flows, constraints, epsilon);
    return recon.reconciliate();
}

/// @brief Оригинальная реализация Глеба. На всякий случай пока оставим
std::tuple<
    std::vector<double>,
    std::vector<double>,
    std::vector<double>
> estimate_flows_MLE_original(
    const std::vector<double>& flows, 
    const std::vector<vertex_incidences_t>& constraints)
{
    int num_flows = (int)flows.size();
    int num_constraints = (int)constraints.size();

    // Для заполнения SparseMatrix
    std::vector<Eigen::Triplet<double>> triplets;

    // Формируем матрицу дисперсий. Используем относительные погрешности!
    double default_std = 1;
    Eigen::SparseMatrix<double> R(num_flows, num_flows);
    for (int i = 0; i < num_flows; ++i) {
        double flow_abs = std::max(std::abs(flows[i]), 1e-8);
        double flow_weight = default_std / flow_abs;
        triplets.emplace_back(i, i, flow_weight);
    }
    R.setFromTriplets(triplets.begin(), triplets.end());
    triplets.clear();

    // Формируем матрицу объемных балансов
    Eigen::SparseMatrix<double> A_balance(num_constraints, num_flows);
    for (int i = 0; i < num_constraints; ++i) {
        // Если расход не входит в i-ое ограничение, то его коэффициент = 0
        for (size_t inlet : constraints[i].inlet_edges)
            triplets.emplace_back(i, (int)inlet, 1.0);

        for (size_t outlet : constraints[i].outlet_edges)
            triplets.emplace_back(i, (int)outlet, -1.0);
    }
    A_balance.setFromTriplets(triplets.begin(), triplets.end());
    triplets.clear();

    // Матрица коэффициентов системы уравнений
    Eigen::SparseMatrix<double> A(num_constraints + num_flows, num_constraints + num_flows);

    // блок 2R
    for (int i = 0; i < num_flows; ++i) {
        triplets.emplace_back(i, i, 2 * R.coeff(i, i));
    }

    // блок A^T
    for (int i = 0; i < num_constraints; ++i) {
        for (int j = 0; j < num_flows; ++j) {
            triplets.emplace_back(j, num_flows + i, A_balance.coeff(i, j)); // A_balance^T
            triplets.emplace_back(num_flows + i, j, A_balance.coeff(i, j)); // A_balance
        }
    }
    A.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::VectorXd m = Eigen::VectorXd::Map(flows.data(), flows.size());

    // Вектор правых частей уравнений системы
    Eigen::VectorXd B(num_constraints + num_flows);
    B.setZero();
    B.head(num_flows) = 2 * R * m;

    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;

    solver.analyzePattern(A);
    solver.factorize(A);

    if (solver.info() != Eigen::Success)
        throw std::logic_error("Factorization of balance matrix failed");

    Eigen::VectorXd X = solver.solve(B);
    if (solver.info() != Eigen::Success) {
        throw std::runtime_error("No solution found for MLE task");
    }
    X = Eigen::VectorXd(X.head(flows.size())); // выпиливаем множители Лагранжа из решения

    std::vector<double> corrected_flows(flows.size());
    Eigen::VectorXd::Map(&corrected_flows[0], corrected_flows.size()) 
        = X;

    std::vector<double> corrections(flows.size());
    Eigen::VectorXd::Map(&corrections[0], corrections.size()) 
        = X - m; // насколько надо исходные измерения поправить, чтобы получить исправленные X

    std::vector<double> balance(constraints.size());
    Eigen::VectorXd::Map(&balance[0], balance.size()) 
        = A_balance * m; // исходные невязки для неоткорректированного расхода
#ifdef _DEBUG
    Eigen::VectorXd balance_new = A_balance* X;
#endif
    return std::make_tuple(
        std::move(corrected_flows),
        std::move(corrections),
        std::move(balance)
    );
}

}