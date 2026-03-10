#pragma once

namespace graphlib {
;

/// @brief Параметры транспортного солвера для подграфа с полной распространимостью
struct transport_graph_solver_settings {
    // Параметры будут добавлены по мере необходимости

    /// @brief Создаёт набор настроек со значениями по умолчанию
    static transport_graph_solver_settings default_values() {
        return transport_graph_solver_settings{};
    }
};

/// @brief Моносолвера задачи адвекции эндогенных свойств на проточном подграфе со свойством полной распространимости
/// Ожидается, что дуги ориентированы по фактическим расходам.
/// @tparam ModelIncidences Базовый тип отраслевых моделей и их инциденций
/// @tparam BoundMeasurement Тип измерения с привязкой к графу basic_graph_t
/// @tparam EndogenousValue Тип стркутуры для хранения эндогенных свойств
/// @tparam Mixer Тип для расчета свойств смеси
/// @tparam EndogenousSelector Тип структуры для селектора эндогенных параметров (каждому параметру соответствует логическое значение -
/// участвует или не участвует в расчете)
template <typename ModelIncidences,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
class full_propagatable_endogenous_solver {
    /// @brief Проточный граф без циклов
    const basic_graph_t<ModelIncidences>& oriented_graph;
    /// @brief Селектор рассчитываемых эндогенных свойств
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const transport_graph_solver_settings& settings;

public:
    /// @brief Тип настроек солвера
    using Settings = transport_graph_solver_settings;

public:
    /// @brief Инициализация 
    /// @param graph Подграф для расчета
    /// @param selector Селектор эндогенных параметров
    /// @param settings Настройки расчета
    full_propagatable_endogenous_solver(const basic_graph_t<ModelIncidences>& oriented_graph,
        const EndogenousSelector& selector,
        const transport_graph_solver_settings& settings)
        : oriented_graph(oriented_graph)
        , selector(selector)
        , settings(settings)
    {

    }

    /// @brief Выполняет эндогенный шаг транспортного расчета.
    /// @warning Требуется, чтобы расходы на всех ребрах были заданы. Предполагается, что переориентация графа уже проведена
    /// @param time_step Временной шаг
    /// @param oriented_flows1 Расходы через ребра 1-го типа
    /// @param oriented_flows2 Расходы через ребра 2-го типа
    /// @param endogenous Измерения эндогенных своств
    /// @return Возвращает информацию об использованных эндогенных измерениях
    transport_problem_info<BoundMeasurement> step(double time_step, 
        const std::vector<double>& oriented_flows1, const std::vector<double>& oriented_flows2,
        const std::vector<BoundMeasurement>& endogenous)
    {
        auto has_nan = [](const std::vector<double>& v) {
            return std::any_of(v.begin(), v.end(), [](double x) { return std::isnan(x); });
            };

        bool no_undefined_flows = !has_nan(oriented_flows1) && !has_nan(oriented_flows2);

        bool has_flow_for_every_edge =
            oriented_graph.edges1.size() == oriented_flows1.size() &&
            oriented_graph.edges2.size() == oriented_flows2.size();

        if (not (no_undefined_flows && has_flow_for_every_edge)) {
            throw std::runtime_error("Not all flows are definied");
        }
        
        transport_problem_completeness_info dummy_completness;
        transport_problem_info<BoundMeasurement> result = step(
            dummy_completness, time_step, oriented_flows1, oriented_flows2, endogenous);
        return result;
    }

    /// @brief Выполняет эндогенный шаг транспортного расчета.
    /// @warning Требуется, чтобы расходы на всех ребрах были заданы. 
    /// Предполагается, что переориентация графа уже проведена
    /// Предполагается, что completness и значения в oriented_flows консистентны
    /// @param time_step Временной шаг
    /// @param oriented_flows1 Расходы через ребра 1-го типа
    /// @param oriented_flows2 Расходы через ребра 2-го типа
    /// @param endogenous Измерения эндогенных своств
    /// @return Возвращает информацию об использованных эндогенных измерениях
    transport_problem_info<BoundMeasurement> step(const transport_problem_completeness_info& completeness,
        double time_step, const std::vector<double>& oriented_flows1, const std::vector<double>& oriented_flows2,
        const std::vector<BoundMeasurement>& endogenous)
    {
        // Нет труб с неизвестным расходом
        if (completeness.lacking_flows.empty()) {
            // TODO Для пользователя неявно, что эндогенное распространение фактически может не состояться

            // Подготовка данных и расчет смешения в вершинах ориентированного графа в порядке, определяемом топологической сортировкой
            graph_endogenous_propagator_t<ModelIncidences, BoundMeasurement,
                EndogenousValue, Mixer, EndogenousSelector
            > endogenous_propagator(oriented_graph, selector, settings);
            endogenous_propagator.propagate(time_step, oriented_flows1, oriented_flows2, endogenous);
        }

        // TODO Добавить информацию об использовании эндогенных измерений
        transport_problem_info<BoundMeasurement> result;
        result.completeness = completeness;
        return result;
    }
};


/// @brief Солвер. Решает транспортную задачу (расчет распространения эндогенных параметров) для проточного подграфа
/// со свойством полной распространимости расхода от замеров на все (!) ребра (не только трубы)
/// @tparam BoundMeasurement Тип измерения с привязкой к графу basic_graph_t
/// @tparam ModelIncidences Базовый тип отраслевых моделей и их инциденций
/// @tparam Mixer Тип для расчета свойств смеси
/// @tparam EndogenousValue Тип стркутуры для хранения эндогенных свойств
/// @tparam EndogenousSelector Тип структуры для селектора эндогенных параметров (каждому параметру соответствует логическое значение -
/// участвует или не участвует в расчете)
template <typename ModelIncidences,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
class transport_graph_solver_template_t {
    /// @brief Проточный граф без циклов
    const basic_graph_t<ModelIncidences>& graph;
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const transport_graph_solver_settings& settings;
public:
    /// @brief Тип настроек солвера
    using Settings = transport_graph_solver_settings;
public:
    /// @brief Инициализация 
    /// @param graph Подграф для расчета
    transport_graph_solver_template_t(const basic_graph_t<ModelIncidences>& graph,
        const EndogenousSelector& selector,
        const transport_graph_solver_settings& settings)
        : graph(graph)
        , selector(selector)
        , settings(settings)
    {

    }

public:
    /// @brief Решает транспортную задачу в стационарном режиме на графе с полной распространимостью расхода
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @return Информация о полноте и использовании измерений
    transport_problem_info<BoundMeasurement> solve(
        const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous_measurements) 
    {
        constexpr double static_time_step = std::numeric_limits<double>::infinity();
        auto result = step(static_time_step, flow_measurements, endogenous_measurements);
        return result;
    }
    /// @brief Расчет на шаг вперед. Из измерений извлекаются расходы и эндогены. Расходы переносятся на трубы. 
    /// Ребра графа ориентируются по расходам. Для ориентированного графа в порядке, определяемом топологической
    /// сортировкой, расчитываются смешения в вершинах и продвижение партий по ребрам
    /// @param time_step Временной шаг
    /// @param measurements SISO измерения
    transport_problem_info<BoundMeasurement> step(
        double time_step, const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous)
    {
        transport_problem_completeness_info completeness;

        // Распространение измерений расхода
        graph_flow_propagator_t<ModelIncidences, BoundMeasurement> flow_propagator(
            graph, settings);
        std::vector<double> flows1, flows2;

        std::tie(flows1, flows2, completeness.lacking_flows, completeness.useless_flows) =
            flow_propagator.propagate(flow_measurements);

        // Переориентирование ребер по фактическим расходам
        auto [oriented_graph, oriented_flows1, oriented_flows2] =
            prepare_flow_relative_graph(graph, flows1, flows2);

        // Эндогенный расчет на графе с полной распространимостью
        full_propagatable_endogenous_solver<
            ModelIncidences, BoundMeasurement, EndogenousValue, Mixer, EndogenousSelector
        > endogenous_solver(oriented_graph, selector, settings);
        transport_problem_info<BoundMeasurement> result = endogenous_solver.step(
            completeness, time_step, oriented_flows1, oriented_flows2, endogenous);
        
        return result;
    }
};

}