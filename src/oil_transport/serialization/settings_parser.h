#pragma once

#include "oil_transport.h"

namespace oil_transport {

/// @brief Парсер настроек hydro_transport_task_settings из JSON
class settings_parser_json {
private:
    /// @brief Получает имена полей структуры через рефлексию
    template <typename T>
    static constexpr auto get_field_names() {
        constexpr size_t field_count = boost::pfr::tuple_size_v<T>;
        std::array<std::string_view, field_count> names{};

        [&names] <size_t... I>(std::index_sequence<I...>) {
            ((names[I] = boost::pfr::get_name<I, T>()), ...);
        }(std::make_index_sequence<field_count>{});

        return names;
    }

    /// @brief Проверяет, является ли тип агрегатом (структурой)
    template <typename T>
    static constexpr bool is_aggregate_v = std::is_aggregate_v<T> && !std::is_array_v<T>;

    /// @brief Преобразует строку в enum hydro_transport_solution_method
    static oil_transport::hydro_transport_solution_method parse_solution_method(const std::string& method_str) {
        if (method_str == "FindFlowFromMeasurements") {
            return oil_transport::hydro_transport_solution_method::FindFlowFromMeasurements;
        }
        else if (method_str == "CalcFlowWithHydraulicSolver") {
            return oil_transport::hydro_transport_solution_method::CalcFlowWithHydraulicSolver;
        }
        else {
            throw std::runtime_error("Unknown solution_method: " + method_str);
        }
    }

    /// @brief Рекурсивно парсит структуру из ptree с использованием boost::pfr
    template <typename T>
    static void parse_from_ptree(T& obj, const boost::property_tree::ptree& pt) {
        constexpr auto field_names = get_field_names<T>();

        boost::pfr::for_each_field(obj, [&](auto& field, std::size_t index) {
            std::string_view field_name = field_names[index];
            using field_type = std::remove_cvref_t<decltype(field)>;

            if constexpr (std::is_same_v<field_type, oil_transport::hydro_transport_solution_method>) {
                if (auto opt = pt.get_optional<std::string>(std::string(field_name))) {
                    field = parse_solution_method(opt.get());
                }
            }
            else if constexpr (is_aggregate_v<field_type>) {
                if (auto child_opt = pt.get_child_optional(std::string(field_name))) {
                    parse_from_ptree(field, child_opt.get());
                }
            }
            else if constexpr (std::is_same_v<field_type, bool>) {
                if (auto opt = pt.get_optional<bool>(std::string(field_name))) {
                    field = opt.get();
                }
            }
            else if constexpr (std::is_same_v<field_type, double>) {
                if (auto opt = pt.get_optional<double>(std::string(field_name))) {
                    field = opt.get();
                }
            }
            else if constexpr (std::is_same_v<field_type, size_t>) {
                if (auto opt = pt.get_optional<size_t>(std::string(field_name))) {
                    field = opt.get();
                }
            }
            else {
                if (auto opt = pt.get_optional<field_type>(std::string(field_name))) {
                    field = opt.get();
                }
            }
        });
    }

public:
    /// @brief Парсит hydro_transport_task_settings из JSON потока
    /// @param stream Входной поток с JSON данными
    /// @return Настройки задачи
    static oil_transport::hydro_transport_task_settings parse(std::istream& stream) {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(stream, pt);

        oil_transport::hydro_transport_task_settings settings = 
            oil_transport::hydro_transport_task_settings::get_default_settings();
        
        parse_from_ptree(settings, pt);
        
        return settings;
    }
};

}

