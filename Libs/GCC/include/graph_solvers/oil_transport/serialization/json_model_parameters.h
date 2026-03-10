#pragma once

namespace oil_transport {
;

/// @brief Параметры сосредоточенного объекта для транспортного расчета
struct lumped_json_data {
    /// @brief Начальное состояние
    bool is_initially_opened{ true };

    /// @brief Возвращает параметры по умолчанию для сосредоточенного объекта
    /// Сгенерировано в cursor
    static lumped_json_data default_values() {
        lumped_json_data data;
        data.is_initially_opened = true;
        return data;
    }

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

    /// @brief Возвращает параметры по умолчанию для задвижки
    static valve_json_data default_values() {
        valve_json_data data;
        data.initial_opening = 1.0;
        data.xi = 0.03;
        data.diameter = 0.5;
        return data;
    }
};

/// @brief JSON-данные обратного клапана
struct check_valve_json_data {
    /// @brief Коэффициент местного сопротивления для полностью открытого обратного клапана
    double hydraulic_resistance{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Диаметр сечения
    double diameter{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Начальное положение
    double opening{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Начальное состояние в статике
    fixed_solvers::nullable_bool_t is_initially_closed_static;

    /// @brief Возвращает параметры по умолчанию для обратного клапана
    static check_valve_json_data default_values() {
        check_valve_json_data data;
        data.hydraulic_resistance = 0.03;
        data.diameter = 0.5;
        data.opening = 1.0;
        data.is_initially_closed_static = fixed_solvers::nullable_bool_t::Undefined;
        return data;
    }
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

    /// @brief Возвращает параметры по умолчанию для поставщика/потребителя
    static source_json_data default_values() {
        source_json_data data;
        data.is_initially_zeroflow = false;
        data.density = 850.0;
        data.temperature = 273;
        data.pressure = std::numeric_limits<double>::quiet_NaN();
        data.std_volflow = std::numeric_limits<double>::quiet_NaN();
        return data;
    }

};

/// @brief JSON-данные насоса
struct pump_json_data {
    /// @brief Коэффициенты аппроксимации напорной характеристики насоса (полином)
    std::vector<double> head_characteristic;
    /// @brief QH-характеристика для экстраполяции
    std::vector<double> head_characteristic_alternative;
    /// @brief Расход, выше которого надо экстраполировать QH-характеристику (м3/с)
    double switching_volumetric_flow_for_head_characteristic{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Коэффициенты аппроксимации КПД насоса (полином)
    std::vector<double> efficiency_characteristic;
    /// @brief Начальная частота вращения
    double initial_frequency{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Номинальные обороты (об/мин)
    double nominal_frequency{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Обточка рабочего колеса
    double rotor_wheel_reducing{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Насос включен
    bool is_initially_started{ true };

    /// @brief Возвращает параметры по умолчанию для насоса
    static pump_json_data default_values() {
        pump_json_data data;
        data.head_characteristic = {250.739, -0.013956, -2.1523e-6, -2.6927e-10};
        data.head_characteristic_alternative = {303.703928, -0.03989472, 0.0, 0.0};
        data.switching_volumetric_flow_for_head_characteristic = 10.0;
        data.efficiency_characteristic = {0.0076669, 0.00070543, -1.8266e-7, 1.3355e-11};
        data.initial_frequency = 1.0;
        data.nominal_frequency = 3000.0;
        data.rotor_wheel_reducing = 0.0;
        data.is_initially_started = true;
        return data;
    }
};

}

