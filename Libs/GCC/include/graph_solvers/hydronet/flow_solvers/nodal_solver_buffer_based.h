#pragma once

namespace graphlib {

/// @brief Тип оценки начального приближения для узлового солвера
/// @details Определяет источник начальных значений давлений для расчета потокораспределения
enum class solver_estimation_type_t {
    /// @brief Случайное начальное приближение
    RandomInital,
    /// @brief Начальное приближение из предыдущего слоя буферов
    FromPreviousLayer,
    /// @brief Начальное приближение из текущего слоя буферов
    FromCurrentLayer
};

/// @brief Тип смещения слоя буфера для записи результатов расчета
/// @details Определяет, в какой слой буфера (текущий или предыдущий) записываются результаты расчета
enum class layer_offset_t {
    /// @brief Запись в предыдущий слой буфера
    ToPreviousLayer,
    /// @brief Запись в текущий слой буфера
    ToCurrentLayer
};

/// @brief Обёртка над узловым солвером, умеющая брать начальное приближение из буферов моделей и обновлять буферы по результатам расчёта
template<typename ModelIncidences>
class nodal_solver_buffer_based_t {
    /// @brief Солвер задачи потокораспределения (например, nodal_solver_t)
    nodal_solver_renumbering_t<ModelIncidences> solver;
    /// @brief объект класса сети
    const graphlib::basic_graph_t<ModelIncidences>& graph;

public:
    /// @brief Тип настроек солвера
    using Settings = nodal_solver_settings_t;

public:

    /// @brief Конструктор моносолвера потокораспределения
    /// @param graph Граф исходной схемы
    nodal_solver_buffer_based_t(const graphlib::basic_graph_t<ModelIncidences>& graph, const nodal_solver_settings_t& settings)
        : solver(graph, settings)
        , graph(graph)
    {
    }

    /// @brief Начинает расчет с известного начального приближения 
    /// (возможно, не имеет смысла - здесь заготовка, чтобы идею не забыть)
    flow_distribution_result_t solve(const flow_distribution_result_t& estimation, layer_offset_t update_type)
    {
        throw std::runtime_error("not impl");
    }

    /// @brief Запускает расчёт, используя случайное начальное приближение или значения из буфером. В случае успеха расчета шага обновляет буферы моделей.   
    /// @param estimation_type Способ формирования начального приближения.
    /// @param update_type Слой буферов (текущий или предыдущий), в который будут записаны рассчитанные давления и расходы
    /// @return Рассчитанные давления и расходы, результат численного метода и диагностика
    flow_distribution_result_t solve(solver_estimation_type_t estimation_type, layer_offset_t update_type)
    {
        Eigen::VectorXd initial_pressures;
        if (estimation_type == solver_estimation_type_t::RandomInital) {
            initial_pressures = Eigen::VectorXd(); // пустой вектор по соглашению означает случайную инициализацию
        }
        else {
            initial_pressures = estimate_from_buffers(estimation_type);
        }

        flow_distribution_result_t flow_result = solver.solve(initial_pressures);

        if (flow_result.numerical_result.result_code == numerical_result_code_t::Converged) {
            update_buffers(update_type, flow_result);
        }

        return flow_result;
    }

protected:
    /// @brief Обновляет значения расхода и давления в буферах моделей графа
    /// @param layer_offset Слой буфера (текущий или предыдущий), в который нужно записать данные
    /// @param result Результат расчёта потокораспределения на перенумерованном графе
    void update_buffers(layer_offset_t layer_offset, const flow_distribution_result_t& result) {

        //const auto& [pressures, flows1, flows2] = result;
        // Записываем давления и расходы в модели притоков/отборов
        for (size_t i = 0; i < graph.edges1.size(); i++) {
            auto& edge1 = graph.edges1[i];
            double flow = result.flows1[i];
            size_t vertex_index = edge1.get_vertex_for_single_sided_edge();
            double pressure = result.pressures[vertex_index];
            edge1.model->update_vol_flow(layer_offset, flow);
            edge1.model->update_pressure(layer_offset, pressure);
        }

        // Записываем давления и расходы в модели двусторонних ребер
        for (size_t i = 0; i < graph.edges2.size(); i++) {
            auto& edge2 = graph.edges2[i];
            edge2.model->update_vol_flow(layer_offset, result.flows2[i]);
            edge2.model->update_pressure(layer_offset,
                result.pressures[edge2.from], result.pressures[edge2.to]);
        }
    }


    /// @brief Формирует начальное распределение давлений на основе ранее расчитанных значений из буферов моделей
    /// @return Вектор начальных давлений размерности "количество узлов без P-условия"
    Eigen::VectorXd estimate_from_buffers(solver_estimation_type_t estimation_type) const
    {
        Eigen::VectorXd estimation(graph.get_vertex_count());
        for (const auto& edge1 : graph.edges1) {
            size_t vertex = edge1.get_vertex_for_single_sided_edge();
            estimation(vertex) = edge1.model->get_pressure_from_buffer(estimation_type);
        }
        for (const auto& edge2 : graph.edges2) {
            estimation(edge2.from) = edge2.model->get_pressure_from_buffer(estimation_type, false);
            estimation(edge2.to) = edge2.model->get_pressure_from_buffer(estimation_type, true);
        }
        return estimation;
    }
};

}

