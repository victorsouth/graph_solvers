#pragma once

namespace graphlib {
;

/// @brief Настройки корректора избыточных расходов
struct redundant_flows_corrector_settings_t {
    /// @brief Порог для определения малых расходов (0 < epsilon < 1)
    double flow_epsilon{ std::numeric_limits<double>::quiet_NaN() };
    
    /// @brief Создаёт набор настроек со значениями по умолчанию
    static redundant_flows_corrector_settings_t default_values() {
        redundant_flows_corrector_settings_t result;
        result.flow_epsilon = 1e-8;
        return result;
    }
};

/// @brief Задача ММП-оценивания. Принимает оценки расходов и информацию об их связи по объемному балансу
/// Пока что не учитываются конкретные величины дисперсии. 
/// @param epsilon Порог для определения малых расходов (0 < epsilon < 1)
/// @return Кортеж: 
/// Скорректированные значения измерения,
/// Поправки на исходные значения,
/// Балансы расходов для неисправленных расходов
std::tuple<
    std::vector<double>,
    std::vector<double>,
    std::vector<double>
> estimate_flows_MLE(
    const std::vector<double>& flows, 
    const std::vector<graphlib::vertex_incidences_t>& constraints,
    double epsilon = 1e-8);

/// @brief Корректор изыбточных расходов в графе, декомпозированном по ребрам с измерением расхода
/// По сути, обертка над estimate_flows_MLE, распаковывающая транспортные измерения 
template <
    typename ModelIncidences,
    typename BoundMeasurement
> class redundant_flows_corrector
{
    /// @brief Структура с мапами для сопоставления исходного и разрезанного графов
    const graphlib::general_structured_graph_t<basic_graph_t<ModelIncidences>>& forest_decomposition;
    /// @brief Граф, полученный разрезанием по ребрам с расходом
    const basic_graph_t<ModelIncidences>& forest;
    // Измерения расхода на разрезанном графе
    const std::vector<BoundMeasurement>& flows;
    /// @brief Настройки корректора
    const redundant_flows_corrector_settings_t& settings;
    /// @brief По индексу в исходном списке расходов получить индекс в списке избыточных расходов
    std::unordered_map<size_t, size_t> global_to_redundant;
    /// @brief По индексу избыточного измерения сказать индекс в исходном списке
    std::vector<size_t> redundant_to_global;
    /// @brief Список входных и выходных измерений в компонентах связности (индексация по избыточным измерениями)
    std::vector<graphlib::vertex_incidences_t> constraints;
    typedef std::unordered_map<graphlib::subgraph_binding_t, size_t, graphlib::subgraph_binding_t::hash> ascend_measurements_map_t;

protected:

    /// @brief Формирует сопоставление "Привязка в компоненте связности -> 
    /// глобальный индекс измерения расхода (из списка измерений расхода на недекомпозированном графе)"
    ascend_measurements_map_t get_ascend_measurements_map() const
    {
        ascend_measurements_map_t ascend_measurements_map; // в итоге по ребрам1 адресуемся к замерам
        for (size_t measurement_index = 0; measurement_index < flows.size(); ++measurement_index) {
            auto descend_bindings = forest_decomposition.descend_map.equal_range(flows[measurement_index]);
            for (auto& it = descend_bindings.first; it != descend_bindings.second; ++it) {
                ascend_measurements_map[it->second] = measurement_index;
            }

        }
        return ascend_measurements_map;
    }

    /// @brief Возвращает глобальные индексы измерений расхода на притоках/отборах для каждого компонента связности
    /// @param subgraphs Компоненты связности леса
    /// @param ascend_measurements_map Сопоставление "Привязка в КС - Индекс измерения"
    static std::vector<std::vector<size_t>> edge1_measurements_by_subgraph(const std::vector<graphlib::subgraph_t>& subgraphs,
        const ascend_measurements_map_t& ascend_measurements_map)
    {
        std::vector<std::vector<size_t>> subgraph_measurements;
        for (const graphlib::subgraph_t& subgraph : subgraphs) {
            std::vector<size_t> flows_indices;
            for (size_t edge1 : subgraph.edges1) {
                graphlib::subgraph_binding_t binding(0, graphlib::graph_binding_type::Edge1, edge1);

                auto it = ascend_measurements_map.find(binding);
                if (it != ascend_measurements_map.end())
                    flows_indices.push_back(it->second);

            }
            subgraph_measurements.emplace_back(flows_indices);
        }
        return subgraph_measurements;
    }

    /// @brief Выявляет индексы измеерний на притоках компонентов связности с избыточностью. 
    /// Компоненты без избыточности игнорируются
    /// @param subgraphs Компоненты связности, полученные после разреза расходомеров с измерениями расхода
    std::tuple<
        std::vector<graphlib::vertex_incidences_t>,
        std::unordered_map<size_t, size_t>,
        std::vector<size_t>
    > find_redundant_flows(const std::vector<graphlib::subgraph_t>& subgraphs,
        const std::vector<std::vector<size_t>>& subgraph_measurements) const
    {
        std::unordered_map<size_t, size_t> global_to_redundant;
        std::vector<size_t> redundant_to_global;
        std::vector<graphlib::vertex_incidences_t> constraints;

        for (size_t subgraph_index = 0; subgraph_index < subgraphs.size(); ++subgraph_index) {
            const graphlib::subgraph_t& subgraph = subgraphs[subgraph_index];
            const std::vector<size_t>& flows_indices = subgraph_measurements[subgraph_index];

            if (subgraph.edges1.empty()) {
                throw std::runtime_error("Subgraph source count is unexpected");
            }

            if (flows_indices.size() < subgraph.edges1.size() - 1) {
                throw std::runtime_error("Insufficient flow count"); // TODO: облагородить, юзеру надо больше деталей
            }

            if (flows_indices.size() == subgraph.edges1.size() - 1) {
                // Нет избыточности. Один неизвестный расход, можно найти по балансу
                continue;
            }

            // Сюда попадаем, если измерения избыточны, т.е. расход задан на всех притоках

            // Обновляем списки избыточных расходов индексам расхода для текущего компонента связности
            update_redundant_flows_map(flows_indices, &global_to_redundant, &redundant_to_global);

            // Разделение на индексы притоков и индексы отборов (для записи объемного баланса позднее)
            graphlib::vertex_incidences_t subgraph_constraints = get_subgraph_constraints(subgraph, flows_indices,
                global_to_redundant);

            constraints.emplace_back(subgraph_constraints);
        }

        return std::make_tuple(
            std::move(constraints),
            std::move(global_to_redundant),
            std::move(redundant_to_global));
    }

    /// @brief Обновляет сопоставление индексов избыточных расходов
    /// @param redundant_flows_indices Индексы расходов нового подграфа с избыточностью
    static void update_redundant_flows_map(const std::vector<size_t>& redundant_flows_indices,
        std::unordered_map<size_t, size_t>* global_to_redundant,
        std::vector<size_t>* redundant_to_global)
    {
        for (size_t flow_index : redundant_flows_indices) {
            size_t redundant_index;
            if (global_to_redundant->count(flow_index) == 0) {
                // Вводим ранее не встречавшееся избыточное измерение
                redundant_index = redundant_to_global->size();
                global_to_redundant->emplace(flow_index, redundant_index);
                redundant_to_global->push_back(flow_index);
            }
        }
    }

    /// @brief Возвращает локальные индексы расходов 
    /// для входящих (притоков) и выходящих (отборов) ребер1 компонента связности
    graphlib::vertex_incidences_t get_subgraph_constraints(const graphlib::subgraph_t& subgraph,
        const std::vector<size_t>& flows_indices, const std::unordered_map<size_t, size_t>& global_to_redundant) const
    {
        graphlib::vertex_incidences_t subgraph_constraints;
        for (size_t edge1_index_index = 0; edge1_index_index < subgraph.edges1.size(); ++edge1_index_index) {
            // поскольку здесь рассматривается только подграф с измерениями, заданными на всех притоках, 
            // сколько элементов в subgraph.edges1, столько же элементов в flows_indices
            size_t edge1_index = subgraph.edges1[edge1_index_index];
            size_t redundant_flow_global_index = flows_indices[edge1_index_index];
            size_t redundant_flow_local_index = global_to_redundant.at(redundant_flow_global_index);

            if (forest.edges1[edge1_index].has_vertex_to()) {
                subgraph_constraints.inlet_edges.push_back(redundant_flow_local_index);
            }
            else {
                subgraph_constraints.outlet_edges.push_back(redundant_flow_local_index);
            }

        }
        return subgraph_constraints;
    }


public:

    /// @brief Подготовка исходных данных для расчета скорректированных расходов:
    /// выявление измерений расхода на притоках и отборах компонентов связности с избыточностью расходов
    /// @warning Ожидаются измерения расхода на недекомпозированном графе. 
    /// Использование таких расходов позволяет не создавать ограничения на равенство расходов, полученных при разрезе ребра
    /// @param forest_decomposition Декомпозиция графа на лес
    /// @param flows_on_uncut_graph Измерения расхода на недекомпозированном графе
    /// @param settings Настройки корректора
    redundant_flows_corrector(const graphlib::general_structured_graph_t<basic_graph_t<ModelIncidences>>& forest_decomposition,
        const std::vector<BoundMeasurement>& flows_on_uncut_graph,
        const redundant_flows_corrector_settings_t& settings)
        : forest_decomposition(forest_decomposition),
        forest(forest_decomposition.subgraphs.at(0)),
        flows(flows_on_uncut_graph),
        settings(settings)
    {
        auto select_any = [](size_t, const ModelIncidences& model) {
            return true;
            };
        std::vector<graphlib::subgraph_t> subgraphs = forest.bfs_search_subgraph(select_any, select_any);

        ascend_measurements_map_t ascend_measurements_map =
            get_ascend_measurements_map();

        std::vector<std::vector<size_t>> subgraph_measurements = edge1_measurements_by_subgraph(subgraphs, ascend_measurements_map);

        std::tie(constraints, global_to_redundant, redundant_to_global)
            = find_redundant_flows(subgraphs, subgraph_measurements);
    }

    /// @brief Возвращает измерения расхода с уже внесенными поправками, скорректированные по методу максимального правдоподобия
    std::tuple<
        std::vector<BoundMeasurement>,
        std::vector<double>,
        std::vector<double>
    > correct_flows()
    {
        // Если нет избыточности, то корректировка расхода не требуется
        if (constraints.size() == 0) {
            return std::make_tuple(flows, 
                std::vector<double>(flows.size()), 
                std::vector<double>());
        }

        // Собираем избыточные измерения
        std::vector<double> redundant_flows_list;
        for (size_t redundant_flow_global_index : redundant_to_global) {
            redundant_flows_list.push_back(
                flows[redundant_flow_global_index].data.value
            );
        }


        auto [corrected_flow_values, corrections, balance] =
            estimate_flows_MLE(redundant_flows_list, constraints, settings.flow_epsilon);

        auto corrections_global = corrections;

        // Среди всех измерений меняем значения у избыточных
        std::vector<BoundMeasurement> flows_updated = flows;
        for (size_t redundant_index = 0; redundant_index < redundant_to_global.size(); ++redundant_index) {
            size_t global_index = redundant_to_global[redundant_index];
            flows_updated[global_index].data.value = corrected_flow_values[redundant_index];
            corrections_global[global_index] = corrections[redundant_index];
        }

        return std::make_tuple(
            std::move(flows_updated),
            std::move(corrections_global),
            std::move(balance)
            );
    }


};


/// @brief Обертка над ММП-коррекцией расходов. 
/// Разрезает граф по расходам и вызывает redundant_flows_corrector
/// Если разрез уже есть, то надо использовать redundant_flows_corrector
/// @return См. redundant_flows_corrector::correct_flows
template <typename ModelIncidences, typename BoundMeasurement> 
std::tuple<
    std::vector<BoundMeasurement>,
    std::vector<double>,
    std::vector<double>
>  analyze_flow_balance(
    const basic_graph_t<ModelIncidences>& graph,
    const std::vector<BoundMeasurement>& flow_measurements,
    const redundant_flows_corrector_settings_t& settings)
{
    // Рубим лес
    auto [cutset_decomposition, flows_bound_to_cutset] =
        cutset_with_flows(graph, flow_measurements);

    // Корректировать замеры расхода по ММП
    redundant_flows_corrector<ModelIncidences, BoundMeasurement>
        corrector(cutset_decomposition, flow_measurements, settings);
    auto [flows_MLE, corrections, balance] = corrector.correct_flows();

    return std::make_tuple(
        std::move(flows_MLE),
        std::move(corrections),
        std::move(balance)
    );
}


}