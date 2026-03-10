#pragma once

#include "test_oilnet.h"
#include <boost/pfr.hpp>
#include <cmath>
#include <tuple>
#include <utility>

/// @brief Вспомогательная функция для сравнения одного поля по индексу
template<std::size_t Index, typename T>
void compare_field(const T& lhs, const T& rhs, bool& result) {
    const auto& lhs_field = boost::pfr::get<Index>(lhs);
    const auto& rhs_field = boost::pfr::get<Index>(rhs);
    using field_type = std::remove_cvref_t<decltype(lhs_field)>;
    
    if constexpr (std::is_same_v<field_type, double>) {
        // Для double учитываем NaN: два NaN считаются равными
        bool lhs_nan = std::isnan(lhs_field);
        bool rhs_nan = std::isnan(rhs_field);
        if (lhs_nan && rhs_nan) {
            return; // Оба NaN - равны
        }
        if (lhs_nan || rhs_nan) {
            result = false; // Один NaN, другой нет - не равны
            return;
        }
        // Оба не NaN - сравниваем значения
        if (std::abs(lhs_field - rhs_field) > 1e-10) {
            result = false;
        }
    }
    else if constexpr (std::is_same_v<field_type, std::vector<double>>) {
        // Для векторов сравниваем размер и элементы
        if (lhs_field.size() != rhs_field.size()) {
            result = false;
            return;
        }
        for (size_t i = 0; i < lhs_field.size(); ++i) {
            bool lhs_nan = std::isnan(lhs_field[i]);
            bool rhs_nan = std::isnan(rhs_field[i]);
            if (lhs_nan && rhs_nan) {
                continue;
            }
            if (lhs_nan || rhs_nan || std::abs(lhs_field[i] - rhs_field[i]) > 1e-10) {
                result = false;
                return;
            }
        }
    }
    else {
        // Для остальных типов используем стандартное сравнение
        if (lhs_field != rhs_field) {
            result = false;
        }
    }
}

/// @brief Сравнивает две структуры json_data с учетом NaN значений
/// @tparam T Тип структуры (source_json_data, valve_json_data и т.д.)
/// @param lhs Первая структура
/// @param rhs Вторая структура
/// @return true если структуры равны (с учетом NaN)
template<typename T>
bool compare_json_data(const T& lhs, const T& rhs) {
    bool result = true;
    constexpr std::size_t field_count = boost::pfr::tuple_size_v<T>;
    
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((compare_field<I>(lhs, rhs, result)), ...);
    }(std::make_index_sequence<field_count>{});
    
    return result;
}
