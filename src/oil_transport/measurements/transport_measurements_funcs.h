#pragma once

#include <cmath>

namespace oil_transport {
;

/// @brief Формирует несколько измерений в привязке к заданному элементу графа
/// @param binding Привзяка к графу
/// @param values {}
/// @return 
//inline graph_quantity_value_t make_measurement_with_single_binding(graphlib::graph_binding_t binding,  
//    std::vector<std::tuple<transport_measurement_type, double>> values) {
//    graph_quantity_value_t result;
//    result.binding = binding;
//    for (const auto& [type, value] : values)
//    {
//        boost::pfr::for_each_field_with_name(result.data,
//            [&](std::string_view name, oil_transport::measurement_data_t& measurement_data) {
//                if (oil_transport::to_string(type) == std::string(name)) {
//                    measurement_data.type = type;
//                    measurement_data.value = value;
//                }
//            });
//    }
//    return result;
//}
//



/// @brief Обобщенный метод для получения расхода из транспортных измерений в задачах уровня ТГЦ. 
/// Возвращает объемный (!) расход из контейнра. Объемный расход более естественен для транспорта нефти.
/// TODO: Алгоритм zeroflow требует чтения и задания (!) измерений расхода. Zeroflow явялется алгоритмом уровня ТГЦ.
/// На уровне ТГЦ можно говорить об объемном и массовом расходе.Кажется, в ТГЦ предпочтение отдается массовому.
/// На отраслевом уровне есть иные причины для предпочтения массового или объемного расхода.
/// При этом может быть и более подробная классификация(объемный расход в стандартных и в рабочих условиях). Эта специфика не должна проявляться на уровне ТГЦ.
/// Задача преобразования отраслевого расхода в ТГЦ - расход перекладывается на разработчика структуры под отраслевые измерения.
/// Возможно, стоит сделать измерение массового расхода или измерение объемного расхода на уровне ТГЦ.
/// Для отраслевых измерений потребовать адаптера, который преобразует измерение к уровню ТГЦ.
//inline auto transport_get_vol_flow = [](const transport_siso_measurements& measurement) {
//    return measurement.data.vol_flow_std.value;
//};
//
//
///// @brief Устанавливает значение объемного расхода для заданного транспортного контейнера измерений
//inline auto transport_set_vol_flow = [](transport_siso_measurements* measurement, double vol_std_flow) {
//    measurement->data.vol_flow_std.value = vol_std_flow;
// };


/// @brief Возвращает привязку на графе данного транспортного контейнера измерений
inline auto get_binding = [](const transport_siso_measurements& measurement) {
    return measurement.binding;
};



/// @brief Устанавливает привязка на грае для данного транспортного контейнера измерений
inline auto set_binding = [](transport_siso_measurements* measurement, const graphlib::graph_place_binding_t& binding) {
    measurement->binding = binding;
};


/// @brief Возвращает контейнер, содержащий ТОЛЬКО замеры расхода
inline measurements_container_t get_flows(const measurements_container_t& measurement)
{
    measurements_container_t result;
    result.mass_flow = measurement.mass_flow;
    result.vol_flow_std = measurement.vol_flow_std;
    result.vol_flow_working = measurement.vol_flow_working;
    return result;
}
/// @brief Возвращает контейнер, содержащий ТОЛЬКО замеры эндогенных свойств
inline measurements_container_t get_endogeniuos(const measurements_container_t& measurement)
{
    measurements_container_t result;
    result.density_working = measurement.density_working;
    result.density_std = measurement.density_std;

    result.viscosity_working = measurement.viscosity_working;

    result.temperature = measurement.temperature;
    result.sulfur = measurement.sulfur;

    result.mass_flow_improver = measurement.mass_flow_improver;
    result.vol_flow_improver = measurement.vol_flow_improver;

    return result;
}

/// @brief Возвращает новое измерение с такой же привязкой, как у заданного, но содержащее ТОЛЬКО значения эндогенных свойств
/// @param measurement Измерение в приязке к SISO графу
/// @return SISO измерение только с эндогенными свойствами
inline transport_siso_measurements get_flows(const transport_siso_measurements& measurement) {
    transport_siso_measurements result;
    result.binding = measurement.binding;
    result.data = get_flows(measurement.data);
    return result;
}

/// @brief Возвращает новое измерение с такой же привязкой, как у заданного, но содержащее ТОЛЬКО замеры расхода
/// @param measurement Измерение в приязке к SISO графу
/// @return SISO измерение только с расходами
inline transport_siso_measurements get_endogeniuos(const transport_siso_measurements& measurement)
{
    transport_siso_measurements result;
    result.binding = measurement.binding;
    result.data = get_endogeniuos(measurement.data);
    return result;
}


/// @brief Проверяет наличие хотя бы одного измерения в контейнере
/// @param measurement Измерения в приязке к SISO графу
bool is_empty(const measurements_container_t& measurement);

/// @brief Проверяет наличие хотя бы одного измерения в контейнере SISO измерений
/// @param measurement Измерения в приязке к SISO графу
inline bool is_empty(const transport_siso_measurements& measurement)
{
    return is_empty(measurement.data);
}


/// @brief Выделяет из набора SISO измерений измерения расхода и измерения эндогенных свойств 
/// @param measurements Набор SISO измерений
/// @return Измерения расхода; Измерения эндогенных свойств
inline std::pair<
    std::vector<graph_quantity_value_t>, 
    std::vector<graph_quantity_value_t>
> classify_measurements(const std::vector<graph_quantity_value_t>& measurements)
{
    std::vector<graph_quantity_value_t> flows;
    std::vector<graph_quantity_value_t> endogenous;
    for (const graph_quantity_value_t& measurement : measurements) {

        if (measurement.type == measurement_type::vol_flow_std ||
            measurement.type == measurement_type::vol_flow_working ||
            measurement.type == measurement_type::mass_flow
            )
        {
            flows.emplace_back(std::move(measurement));
        }
        else {
            endogenous.emplace_back(std::move(measurement));

        }
    }
    return std::make_pair(std::move(flows), std::move(endogenous));
}


/// @brief Переопределяет эндогенные свойства для ребра/вершины графа из измерений
/// ВАЖНО! При наличии нескольких замеров одного параметра используется последний в списке замер
/// @param measurements Измерения для ребра/вершины SISO графа
/// @param _calculated_endogenous Эндогенные свойства ребра/вершины
void overwrite_endogenous_values_from_measurements(
    const std::vector<graph_quantity_value_t>& measurements,
    std::vector<graph_quantity_value_t>* overwritten_by_measurements,
    pde_solvers::endogenous_values_t* _calculated_endogenous);

/// @brief Класс для накопления и усреднения измерений эндогенных/гидравлических параметров
class hydro_measurements_averager_t {
    struct Aggregator {
        double sum{ 0.0 };
        std::size_t count{ 0 };
        void add(double v) {
            sum += v;
            ++count;
        }
        bool has_value() const {
            return count > 0;
        }
        double mean() const {
            return sum / static_cast<double>(count);
        }
    };

    Aggregator density_;
    Aggregator viscosity_;
    Aggregator sulfur_;
    Aggregator improver_;
    Aggregator temperature_;


public:
    /// @brief Добавляет одно измерение в агрегатор
    void add_measurement(const graph_quantity_value_t& m) {
        const double v = m.data.value;
        if (!std::isfinite(v)) {
            return;
        }

        switch (m.type) {
        // Плотность (рабочая и нормированная) усредняются в один параметр density_working
        case measurement_type::density_working:
        case measurement_type::density_std:
            density_.add(v);
            break;

        case measurement_type::viscosity_working:
            viscosity_.add(v);
            break;

        case measurement_type::sulfur:
            sulfur_.add(v);
            break;

        // Массовый/объёмный расход ПТП усредняются в один параметр improver
        case measurement_type::mass_flow_improver:
        case measurement_type::vol_flow_improver:
            improver_.add(v);
            break;

        case measurement_type::temperature:
            temperature_.add(v);
            break;

        default:
            // Другие типы измерений здесь не используются
            break;
        }
    }

    /// @brief Возвращает усреднённые гидравлические/эндогенные значения с учётом значений по умолчанию.
    /// Для параметров, по которым нет измерений, берутся значения из default_values.
    /// Для усреднённых эндогенных параметров confidence всегда остаётся false.
    oil_transport::endohydro_values_t get_average_hydro_values(
        const oil_transport::endohydro_values_t& default_values = oil_transport::endohydro_values_t{}) const
    {
        oil_transport::endohydro_values_t avg_values = default_values;

        if (density_.has_value()) {
            avg_values.density_std.value = density_.mean();
            avg_values.density_std.confidence = false;
        }
        if (viscosity_.has_value()) {
            avg_values.viscosity_working.value = viscosity_.mean();
            avg_values.viscosity_working.confidence = false;
        }
        if (sulfur_.has_value()) {
            avg_values.sulfur.value = sulfur_.mean();
            avg_values.sulfur.confidence = false;
        }
        if (improver_.has_value()) {
            avg_values.improver.value = improver_.mean();
            avg_values.improver.confidence = false;
        }
        if (temperature_.has_value()) {
            avg_values.temperature.value = temperature_.mean();
            avg_values.temperature.confidence = false;
        }

        return avg_values;
    }

    /// @brief Возвращает усреднённые эндогенные значения (без давления и расходов)
    pde_solvers::endogenous_values_t get_average_endogenous_values(
        const oil_transport::endohydro_values_t& default_values) const
    {
        oil_transport::endohydro_values_t avg_hydro =
            get_average_hydro_values(default_values);

        pde_solvers::endogenous_values_t result;
        result.density_std     = avg_hydro.density_std;
        result.viscosity_working   = avg_hydro.viscosity_working;
        result.sulfur      = avg_hydro.sulfur;
        result.improver    = avg_hydro.improver;
        result.temperature = avg_hydro.temperature;
        return result;
    }

    /// @brief Проверяет, есть ли измерения плотности
    /// @return true, если плотность была измерена
    bool has_density() const { return density_.has_value(); }

    /// @brief Среднее значение плотности по накопленным измерениям
    /// @return Средняя плотность
    double density_mean() const { return density_.mean(); }

    /// @brief Проверяет, есть ли измерения вязкости
    /// @return true, если вязкость была измерена
    bool has_viscosity() const { return viscosity_.has_value(); }

    /// @brief Среднее значение вязкости по накопленным измерениям
    /// @return Средняя вязкость
    double viscosity_mean() const { return viscosity_.mean(); }

    /// @brief Проверяет, есть ли измерения содержания серы
    /// @return true, если сера была измерена
    bool has_sulfur() const { return sulfur_.has_value(); }

    /// @brief Среднее значение содержания серы по накопленным измерениям
    /// @return Среднее содержание серы
    double sulfur_mean() const { return sulfur_.mean(); }

    /// @brief Проверяет, есть ли измерения депрессора
    /// @return true, если депрессор был измерен
    bool has_improver() const { return improver_.has_value(); }

    /// @brief Среднее значение депрессора по накопленным измерениям
    /// @return Среднее значение депрессора
    double improver_mean() const { return improver_.mean(); }

    /// @brief Проверяет, есть ли измерения температуры
    /// @return true, если температура была измерена
    bool has_temperature() const { return temperature_.has_value(); }

    /// @brief Среднее значение температуры по накопленным измерениям
    /// @return Средняя температура
    double temperature_mean() const { return temperature_.mean(); }

};

/// @brief Переносит измерения из непроточных подграфов на ближайшие проточные вершины
/// @param structured_graph  Подграфы исходного графа после проточной декомпозиции в автономных контурах
/// @param flow_enabled_subgraphs Идентификаторы проточных подграфов
/// @param zeroflow_subgraphs Идентификаторы непроточных подграфов
/// @param _subgraph_measurements Измерения по всем подграфам. После вызова функции непроточные измерения будут перенесены в проточную часть
inline void rebind_measurements_to_flow_subgraphs(
    const graphlib::general_structured_graph_t<transport_graph_t>& structured_graph,
    const std::vector<size_t>& flow_enabled_subgraphs, const std::vector<size_t>& zeroflow_subgraphs,
    std::unordered_map<size_t, std::vector<graph_quantity_value_t>>* _subgraph_measurements
)
{
    auto& subgraph_measurements = *_subgraph_measurements;

    // Вершины проточного подграфа
    std::unordered_map<size_t, std::unordered_set<size_t>> flow_enabled_vertices =
        structured_graph.get_subgraph_global_vertices(flow_enabled_subgraphs);

    // Вершины непроточного подграфа с разбиением на примыкающие и независимые
    std::unordered_map<size_t, std::unordered_set<size_t>> linked_vertices, separated_vertices, flow_vertices;
    graphlib::get_zeroflow_vertices_by_type(
        structured_graph, flow_enabled_subgraphs, zeroflow_subgraphs,
        &flow_vertices, &linked_vertices, &separated_vertices);


    // Перетащить замеры из примыкающих непроточных частей в проточные
    // имеет смысл только для эндогенных параметров. Замеры расхода мы тут дропаем 
    // (это в результатах отразить - почему дропнули).
    // Пока неясно, как написать



    std::unordered_map<size_t, size_t> vertex_to_flow_subgraph = 
        graphlib::get_inverted_index(flow_enabled_vertices);
    std::unordered_map<size_t, size_t> vertex_to_linked_subgraph = 
        graphlib::get_inverted_index(linked_vertices);
    std::unordered_set<size_t> flow_vertices_set;
    graphlib::get_map_keys(vertex_to_flow_subgraph, &flow_vertices_set);
    // Обход всех вершин, принадлежащих одновременно проточному и непроточному подграфам
    for (const auto& linked_kvp : linked_vertices) {
        size_t linked_zeroflow_subgraph = linked_kvp.first;
        const std::unordered_set<size_t>& linked_vertices_of_subgraph = linked_kvp.second;

        auto zeroflow_measurements_iter = subgraph_measurements.find(linked_zeroflow_subgraph);
        if (zeroflow_measurements_iter == subgraph_measurements.end())
            continue;
        const std::vector<graph_quantity_value_t>& zeroflow_measurements = 
            zeroflow_measurements_iter->second;
        if (zeroflow_measurements.empty())
            continue;

        // Общие ПРОТОЧНЫЕ вершины проточного и непроточного подграфов
        std::unordered_set<size_t> common_vertices =
            graphlib::set_intersection(linked_vertices_of_subgraph, flow_vertices_set);

        if (common_vertices.empty())
            throw graphlib::graph_exception(structured_graph.subgraphs.at(linked_zeroflow_subgraph).get_binding_list(),
                L"Отсутствуют общие вершины у проточного и примыкающе-непроточного графа");
        if (common_vertices.size() >= 2)
            throw graphlib::graph_exception(structured_graph.subgraphs.at(linked_zeroflow_subgraph).get_binding_list(),
                L"Имеется более двух общих вершины у проточного и примыкающе-непроточного графа");

        size_t common_vertex = *common_vertices.begin();

        // Идентификатор проточного графа, инцидентного непроточному
        size_t flow_enabled_subgraph = vertex_to_flow_subgraph.at(common_vertex);

        auto local_vertices = structured_graph.descend_map.equal_range(
            graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, common_vertex));

        auto flow_subgraph_vertex_iter = std::find_if(local_vertices.first, local_vertices.second,
            [&](const auto& value) {
                const graphlib::subgraph_binding_t& binding = value.second;
                return binding.subgraph_index == flow_enabled_subgraph;
            }
        );

        if (flow_subgraph_vertex_iter == structured_graph.descend_map.end()) {
            // почему-то не нашли вершину, общую с непроточной частью в проточном подграфе
            throw std::runtime_error("Cannot find linked local vertex for flow subgraph");
        }
        const graphlib::subgraph_binding_t& common_vertex_as_flow_subgraph_binding =
            flow_subgraph_vertex_iter->second;

        // Перевесим все замеры в проточку, из непроточки удалим (сюда попадаем, если замеры в непроточке есть)
        for (graph_quantity_value_t measurement : subgraph_measurements.at(linked_zeroflow_subgraph)) {
            static_cast<graphlib::graph_place_binding_t&>(measurement) = common_vertex_as_flow_subgraph_binding;
            subgraph_measurements[flow_enabled_subgraph].push_back(measurement);
        }
        subgraph_measurements.erase(linked_zeroflow_subgraph);
    }
}

/// @brief Сопоставляет измерения SISO графа его подграфам
/// @param siso_measurements Измерения на SISO графе
/// @param structured_graph Подграфы исходного графа
/// @return Маппинг (идентификатор подграфа, замеры этого подграфа)
template <typename Graph>
inline std::unordered_map<size_t, std::vector<graph_quantity_value_t>> 
rebind_measurements_to_subgraphs(const std::vector<graph_quantity_value_t>& siso_measurements,
    const graphlib::general_structured_graph_t<Graph>& structured_graph)
{
    std::unordered_map<size_t, std::vector<graph_quantity_value_t>> subgraph_measurements;

    for (graph_quantity_value_t measurement : siso_measurements)
    {
        const std::vector<graphlib::subgraph_place_binding_t> subgraph_bindings = 
            structured_graph.descend_graph_binding(static_cast<const graphlib::graph_place_binding_t&>(measurement), 
                graphlib::get_map_keys(structured_graph.subgraphs) /* Разрешаем привязки во всех подграфах */);
                
        for (const auto& binding : subgraph_bindings) {
            static_cast<graphlib::graph_place_binding_t&>(measurement) = binding; // здесь у нас копия siso-измерений, правим ее
            subgraph_measurements[binding.subgraph_index].emplace_back(std::move(measurement));
        }
    }

    return subgraph_measurements;
}



}


