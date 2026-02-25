#pragma once

namespace oil_transport {
;

/// @brief Параметры сосредоточенного объекта для транспортного расчета
struct lumped_json_data {
    /// @brief Начальное состояние
    bool is_initially_opened{ true };

    // TODO: == для vavle_json_data
};

/// @brief JSON-данные задвижки
struct valve_json_data {
    /// @brief Начальное состояние
    double initial_opening{std::numeric_limits<double>::quiet_NaN()};
    /// @brief Коэффициент местного сопротивления
    double xi{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Диаметр
    double diameter{ std::numeric_limits<double>::quiet_NaN() };
};


/// @brief Параметры поставщика/потребителя для транспортного расчета
struct source_json_data {
    /// @brief Начальное состояние
    bool is_initially_zeroflow{ false };
    /// @brief Сортовая плотность
    double density{ std::numeric_limits < double >::quiet_NaN() };
    /// @brief Температура на входе
    double temperature{ std::numeric_limits < double >::quiet_NaN() };
    /// @brief Давление
    double pressure{ std::numeric_limits < double >::quiet_NaN() };
    /// @brief Объемный расход при номинальных условиях
    double std_volflow{ std::numeric_limits<double>::quiet_NaN() };

};

}

