#pragma once

namespace oil_transport {
;


/// @brief Формирует срезы измерений с привязкой к графу.
/// Обертка над vector_timeseries_t
class graph_vector_timeseries_t {
    /// @brief Экземпляр абстракции векторного временного ряда 
    vector_timeseries_t<> time_series;
    /// @brief Вектор привязок измерений 
    std::vector<graphlib::graph_place_binding_t> measurement_bindings;
    /// @brief Типы измерений
    std::vector<measurement_type > measurement_types;
public:
    /// @brief Конструируем на основе вектора временных рядов и вектора привязок на графе (а также)
    graph_vector_timeseries_t(
        const std::vector<graphlib::graph_place_binding_t>& measurement_bindings,
        const std::vector<measurement_type>& measurement_types,
        const std::vector<std::pair<std::vector<time_t>, std::vector<double>>>& tag_data);
    /// @brief Возвращает вектор привязанных измерений с указанием типа для заданного момента времени
    std::vector<graph_quantity_value_t> operator()(time_t t) const;
public:
    /// @brief Начальная дата зачитанных временных рядов
    std::time_t get_start_date() const;
    /// @brief Возвращает конечную дату временного ряда
    /// @return Конечная дата временного ряда
    std::time_t get_end_date() const;


    /// @brief Возвращает нижележащий объект, надо которым сделана обертка
    const vector_timeseries_t<>& get_time_series_object() const;
protected:
    /// @brief Собирает вектор привязанных измерений
    static std::vector<graph_quantity_value_t>
        prepare_measurement_values(
            const std::vector<double>& values,
            const std::vector<graphlib::graph_place_binding_t>& bindings,
            const std::vector<measurement_type>& measurement_types
        );
public:
    /// @brief Фабрика для зачитки временных рядов из файлов с указанием диапазона времени
    static graph_vector_timeseries_t from_files(
        const std::string& start_period,
        const std::string& end_period,
        const std::string& tag_files_path,
        const std::vector<tag_info_t>& measurement_tags,
        const std::vector<graphlib::graph_place_binding_t>& measurement_bindings,
        const std::vector<measurement_type>& measurement_types
    );
    /// @brief Фабрика для зачитки временных рядов из файлов на весь временной диапазон
    static graph_vector_timeseries_t from_files(
        const std::string& tag_files_path,
        const std::vector<tag_info_t>& measurement_tags,
        const std::vector<graphlib::graph_place_binding_t>& measurement_bindings,
        const std::vector<measurement_type>& measurement_types
    );
};

}

