#pragma once


// Здесь совсем простые объекты - примерный код под обучение студентов
namespace oil_hydro_simple {
;

/// @brief Структура, представляющая свойства простой схемы
struct simple_hydraulic_scheme_properties {
    /// @brief Свойства источника
    oil_hydro_simple::oil_hydro_source_properties src;
    /// @brief Свойства стока
    oil_hydro_simple::oil_hydro_source_properties sink;
    /// @brief Свойства первого местного сопротивления
    oil_hydro_simple::oil_hydro_local_resistance_properties resist1;
    /// @brief Свойства второго местного сопротивления
    oil_hydro_simple::oil_hydro_local_resistance_properties resist2;
    /// @brief Параметры объектов по умолчанию
    static simple_hydraulic_scheme_properties default_scheme() {
        simple_hydraulic_scheme_properties model_properties;
        model_properties.src.std_volflow = 1.0;
        model_properties.sink.pressure = 1e5;
        model_properties.resist1.resistance = 500;
        model_properties.resist2.resistance = 300;
        return model_properties;
    }

};
/// @brief Класс, представляющий простую схему с графовой топологией
struct simple_hydraulic_scheme {
    /// @brief Модель источника системы
    oil_hydro_simple::oil_hydro_source src_model;
    /// @brief Модель стока системы
    oil_hydro_simple::oil_hydro_source sink_model;
    /// @brief Модель первого сопротивления
    oil_hydro_simple::oil_hydro_local_resistance resist1_model;
    /// @brief Модель второго сопротивления
    oil_hydro_simple::oil_hydro_local_resistance resist2_model;
    /// @brief Графовая структура системы
    graphlib::basic_graph_t<graphlib::nodal_edge_t> graph;
    /// @brief Конструктор простой схемы
    /// @param properties Свойства для инициализации всех элементов схемы
    simple_hydraulic_scheme(const simple_hydraulic_scheme_properties& properties, bool build_graph = true)
        : src_model(properties.src)
        , sink_model(properties.sink)
        , resist1_model(properties.resist1)
        , resist2_model(properties.resist2)
    {
        //// Создание графовой топологии:
        // Узел 0: источник
        // Узел 1: промежуточный узел между сопротивлениями  
        // Узел 2: сток
        if (build_graph) {
            graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_to(0), &src_model);
            graph.edges1.emplace_back(graphlib::edge_incidences_t::single_sided_from(2), &sink_model);
            graph.edges2.emplace_back(graphlib::edge_incidences_t(0, 1), &resist1_model);
            graph.edges2.emplace_back(graphlib::edge_incidences_t(1, 2), &resist2_model);
        }
    }
};

}

