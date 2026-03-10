#pragma once

#include "oil_transport.h"

namespace oil_transport {

/// @brief Парсер настроек hydro_transport_task_settings из JSON
class settings_parser_json {
private:
    /// @brief Проверяет, является ли тип агрегатом (структурой)
    template <typename T>
    static constexpr bool is_aggregate_v = std::is_aggregate_v<T> && !std::is_array_v<T>;

    /// @brief Преобразует строку в enum hydro_transport_solution_method
    static oil_transport::hydro_transport_solution_method parse_solution_method(const std::string& method_str) {
        const std::unordered_map<std::string_view, oil_transport::hydro_transport_solution_method> map = {
            {"FindFlowFromMeasurements", oil_transport::hydro_transport_solution_method::FindFlowFromMeasurements},
            {"CalcFlowWithHydraulicSolver", oil_transport::hydro_transport_solution_method::CalcFlowWithHydraulicSolver},
        };
        auto it = map.find(method_str);
        if (it == map.end())
            throw std::runtime_error("Unknown solution_method: " + method_str);
        return it->second;
    }

    /// @brief Преобразует строку в enum analysis_execution_mode
    static graphlib::analysis_execution_mode parse_analysis_mode(const std::string& mode_str) {
        static const std::unordered_map<std::string_view, graphlib::analysis_execution_mode> map = {
            {"never", graphlib::analysis_execution_mode::never},
            {"if_failed", graphlib::analysis_execution_mode::if_failed},
            {"always", graphlib::analysis_execution_mode::always},
        };
        auto it = map.find(mode_str);
        if (it == map.end())
            throw std::runtime_error("Unknown analysis_mode: " + mode_str);
        return it->second;
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
            else if constexpr (std::is_same_v<field_type, graphlib::analysis_execution_mode>) {
                if (auto opt = pt.get_optional<std::string>(std::string(field_name))) {
                    field = parse_analysis_mode(opt.get());
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

