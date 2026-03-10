#pragma once

namespace graphlib {
;

/// @brief Разрез графа по ребрам с расходом
/// @tparam ModelIncidences 
/// @tparam BoundMeasurement 
/// @param graph 
/// @param flows 
/// @return 
template <typename ModelIncidences,
    typename BoundMeasurement
> inline std::pair<
    graphlib::general_structured_graph_t<basic_graph_t<ModelIncidences>>,
    std::vector<BoundMeasurement>
> cutset_with_flows(const basic_graph_t<ModelIncidences>& graph, 
    const std::vector<BoundMeasurement>& flows)
{
    // Рубим лес по ребрам с расходом
    auto get_model_split_type_by_flow_binding = [&flows](const size_t edge2_index) {
        auto is_measurement_binded_to_edge2 = 
            [edge2_index](const BoundMeasurement& flow_measurement) 
            {
                return (flow_measurement.binding_type == graphlib::graph_binding_type::Edge2) &&
                    (flow_measurement.edge == edge2_index);
            };
        // TODO: Очень сложная логика, каждый раз поиск по всему списку замеров
        // один раз пройти по всем замерам, сделать vector<bool> или unordered_set
        // у которого значение будет иметь смысл edge2_has_flow_measurement, 
        // а индекс соответствовать edge2_index
        auto it = std::find_if(flows.begin(), flows.end(), is_measurement_binded_to_edge2);
        bool edge2_has_flow_measurement = (it != flows.end());
        return edge2_has_flow_measurement
            ? graphlib::computational_type::Splittable
            : graphlib::computational_type::Instant;
        };

    auto broken = break_and_renumber_t<basic_graph_t<ModelIncidences>, 
        basic_graph_t<ModelIncidences>, decltype(get_model_split_type_by_flow_binding)>::break_splittables(
        graph, false, get_model_split_type_by_flow_binding, broken_graph_vertex_numbering::renumber_never);
    auto& forest = broken.first.subgraphs.begin()->second;

    // Переносим замеры расхода в лес
    std::vector<BoundMeasurement> forest_flows;
    for (const BoundMeasurement& measurement : flows) {
        auto forest_bindings = broken.first.descend_map.equal_range(measurement);
        for (auto& it = forest_bindings.first; it != forest_bindings.second; ++it) {
            BoundMeasurement forest_flow;
            forest_flow = measurement; // данные от исходного графа
            static_cast<graphlib::graph_place_binding_t&>(forest_flow) = it->second; // привязка в лесу
            forest_flows.emplace_back(std::move(forest_flow));
        }
    }
    return std::make_pair(std::move(broken.first), std::move(forest_flows));
}


/// @brief Солвер траснпортной задачи для проточного подграфа со свойстом полной распространимости на графе блоков
/// @tparam ModelIncidences Инцидентности модели ребра
/// @tparam PipeModel Модель трубы (распрострение эндогенных свойств по трубе-мосту обрабаывается особым образом)
/// @tparam BoundMeasurement Структура - значение измерения и его привязка к дереву блоков
/// @tparam EndogenousValue Структура - значение эндогенного свойства
/// @tparam Mixer Тип для расчета свойств смеси
/// @tparam EndogenousSelector Тип селектора, задающего перечень рассчитываемых параметров
/// @tparam FlowGetter Тип геттера для получения расхода из измерения BoundMeasurement
template <typename ModelIncidences,
    typename PipeModel,
    typename BoundMeasurement,
    typename EndogenousValue,
    typename Mixer,
    typename EndogenousSelector
>
class transport_block_solver_template_t {
    /// @brief Проточный граф без циклов
    const basic_graph_t<ModelIncidences>& graph;
    /// @brief Селектор эндогенных параметров для расчета
    const EndogenousSelector& selector;
    /// @brief Настройки расчета
    const transport_block_solver_settings& settings;
public:
    /// @brief Тип настроек солвера
    using Settings = transport_block_solver_settings;
public:
    /// @brief Инициализация 
    /// @param graph Проточный транспортный подграф для расчета
    transport_block_solver_template_t(const basic_graph_t<ModelIncidences>& graph, 
        const EndogenousSelector& selector,
        const transport_block_solver_settings& settings
    )
        : graph(graph)
        , selector(selector)
        , settings(settings)
    {
    }
public:
    /// @brief Решает транспортную задачу в стационарном режиме
    /// @param flow_measurements Вектор измерений расхода
    /// @param endogenous_measurements Вектор эндогенных измерений
    /// @return Информация о полноте и использовании измерений
    transport_problem_info<BoundMeasurement> solve(
        const std::vector<BoundMeasurement>& flow_measurements,
        const std::vector<BoundMeasurement>& endogenous_measurements)
    {
        constexpr double time_step = std::numeric_limits<double>::infinity();
        return step(time_step, flow_measurements, endogenous_measurements);
    }
protected:
    /// @brief Выполняет шаг расчета расходов
    /// @details Выполняет декомпозицию графа на лес, корректировку измерений расхода по ММП и расчет расходов в блоках
    /// @param use_flow_MLE Флаг использования метода максимального правдоподобия для корректировки расходов
    /// @param flows Вектор измерений расхода
    /// @return Кортеж из дерева блоков, вектора расходов, 
    /// расходов в точках сочленения, вектора расходов через ребра и вектора псевдоизмерений
    std::tuple<
        block_tree_t<ModelIncidences>,
        std::vector<double>,
        std::vector<std::vector<double>>,
        std::vector<double>,
        std::vector<BoundMeasurement>
    > flow_step(bool use_flow_MLE, const std::vector<BoundMeasurement>& flows)
    {
        // Рубим лес
        graphlib::general_structured_graph_t<basic_graph_t<ModelIncidences>> forest_decomposition;
        std::vector<BoundMeasurement> forest_flows;
        std::tie(forest_decomposition, forest_flows) = cutset_with_flows(graph, flows);
        // нужно ли тащить весь decompostion или достаточно сразу отдавать forest?

        // Корректировать замеры расхода по ММП
        if (use_flow_MLE) {
            throw std::runtime_error("Please, uncomment and refactor");
            //redundant_flows_corrector<ModelIncidences, BoundMeasurement, FlowGetter, FlowSetter> 
            //    corrector(forest_decomposition, flows);
            //auto [flows_MLE, flows_correction, initial_balance] = corrector.correct_flows();
        }

        // Выделяем блоки, собираем граф обратно, не меняя структуру блоков
        block_tree_t<ModelIncidences> block_tree =
            block_tree_builder_t<ModelIncidences>::ascend_block_tree(graph, forest_decomposition);

        // TODO: Идея для теста - проверить, что расходы, которые вычисляются при распространении, 
        // но также заданы будут совпадать с откорректированным значением по ММП
        block_flow_propagator<ModelIncidences, BoundMeasurement>
            flow_propagator(graph, block_tree, settings);

        std::vector<double> flows1;
        std::vector<std::vector<double>> cut_flows;
        std::tie(flows1, cut_flows) = flow_propagator.propagate_flow(flows);
        std::vector<double> flows2 = flow_propagator.propagate_flow_to_edges(cut_flows);

        block_tree.redirect_cut_vertices_with_flows(&cut_flows);

        std::vector<BoundMeasurement> flow_pseudomeasurements =
            flow_propagator.block_local_hydraulic_calc(flows.at(0), cut_flows, &flows2);


        return std::make_tuple(
            std::move(block_tree),
            std::move(flows1),
            std::move(cut_flows),
            std::move(flows2),
            std::move(flow_pseudomeasurements)
        );
    }
public:
    /// @brief Расчет на шаг вперед. Из измерений извлекаются расходы и эндогены. Расходы переносятся на трубы. Ребра графа ориентируются по расходам
    /// @param time_step Временной шаг
    /// @param measurements SISO измерения
    transport_problem_info<BoundMeasurement> step(double time_step,
        const std::vector<BoundMeasurement>& flows,
        const std::vector<BoundMeasurement>& endogenous,
        std::vector<double>* flows1_out = nullptr,
        std::vector<double>* flows2_out = nullptr)
    {
        transport_problem_info<BoundMeasurement> result;    
        
        // TODO: Полнота по расходам
        auto [block_tree, flows1, cut_flows, flows2, flow_pseudomeasurements] = 
            flow_step(false, flows);
        if (!flow_pseudomeasurements.empty()) {
            std::vector<BoundMeasurement> flows_extended = std::move(flow_pseudomeasurements);
            flows_extended.insert(flows_extended.end(),
                flows.begin(), flows.end());
            std::tie(block_tree, flows1, cut_flows, flows2, flow_pseudomeasurements) =
                flow_step(false, flows_extended);
        }

        // Переориентируем поставщики, потребители и ребра в мостах
        // как отработает, если расход=nan
        // где меняются расходы flows1, flows2, если надо переориентировать расходы? Просто модуль взять?
        basic_graph_t<ModelIncidences> graph_with_oriented_edges = 
            graphlib::get_flow_relative_graph(graph, flows1, flows2); 

        block_endogenous_propagator<ModelIncidences, PipeModel, 
            BoundMeasurement, EndogenousValue, Mixer, EndogenousSelector> 
            endogenous_propagator(graph_with_oriented_edges, block_tree, 
                selector, endogenous, flows1, flows2, cut_flows, settings);

        result.measurement_status.endogenous =
            endogenous_propagator.propagate_endogenous(time_step);

        // Полнота не реализована в block_endogenous_propagator, пока заглушка. 
        // См. реализацию в full propagatable
        //subgraph_measurements_completeness_info fake_result; 
        //return fake_result; 

        // Записываем расходы в переданные указатели
        if (flows1_out != nullptr) {
            *flows1_out = flows1;
        }
        if (flows2_out != nullptr) {
            *flows2_out = flows2;
        }

        return result;
    }
};

}