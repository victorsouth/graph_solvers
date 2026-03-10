#pragma once

#include "oil_transport.h"


#define GTEST_BREAK_ON_FAILURE 1
#define GTEST_CATCH_EXCEPTIONS 0
#define GTEST_HAS_SEH 0
#include <gtest/gtest.h>


/// @brief Генерирует out-папку тестов с учетом вида теста. Если пути к папке нет - создает путь
std::string prepare_test_folder(const std::string& subfolder = "");

/// @brief Возвращает путь к папке с расчетными схемами
std::string get_schemes_folder();

/// @brief Возвращает путь к папке с json файлами топологии и параметров для тестовой схемы с поддиректорией scheme_subdir
std::string get_scheme_folder(const std::string& scheme_subdir);

inline void AssertBetween (double v, double a, double b) {
    const double lo = std::min(a, b);
    const double hi = std::max(a, b);
    ASSERT_TRUE(v >= lo && v <= hi);
};

/// @brief Проверяет, что поля типов double и std::vector<double> структуры T не содержат NaN
/// @param obj Объект для проверки
template<typename T>
void AssertNoNanFields(const T& obj) {
    constexpr auto field_names = oil_transport::get_field_names<T>();
    boost::pfr::for_each_field(obj, [&](const auto& field, std::size_t index) {
        std::string_view field_name = field_names[index];
        using field_type = std::remove_cvref_t<decltype(field)>;
        
        if constexpr (std::is_same_v<field_type, double>) {
            ASSERT_FALSE(std::isnan(field)) 
                << "Field '" << field_name << "' is NaN";
        } else if constexpr (std::is_same_v<field_type, std::vector<double>>) {
            ASSERT_FALSE(field.empty()) 
                << "Field '" << field_name << "' is empty";
            for (size_t i = 0; i < field.size(); ++i) {
                ASSERT_FALSE(std::isnan(field[i])) 
                    << "Field '" << field_name << "[" << i << "]' is NaN";
            }
        }
        // bool поля не проверяем, так как они всегда имеют значение
    });
}

/// @brief Проверяет, что в JSON нет лишних полей, которых нет в структуре T
/// @param json_ptree JSON объект в виде property_tree
/// @param object_name Имя объекта в JSON для проверки
template<typename T>
void AssertNoExtraJsonFields(
    const boost::property_tree::ptree& json_ptree, 
    const std::string& object_name) 
{
    // Получение полей структуры T через рефлексию
    constexpr auto field_names = oil_transport::get_field_names<T>();
    std::set<std::string> expected_fields;
    for (const auto& field_name : field_names) {
        expected_fields.insert(std::string(field_name));
    }
    
    // Обход всех полей объекта в JSON
    if (auto obj_node = json_ptree.get_child_optional(object_name)) {
        for (const auto& [key, value] : *obj_node) {
            ASSERT_TRUE(expected_fields.count(key) > 0) 
                << "Unexpected field '" << key << "' in JSON for " << object_name;
        }
    }
}

/// @brief Создает временный файл с заданным содержимым в директории теста
/// @return Путь к созданному файлу
inline std::string create_temp_file(const std::string& filename, const std::string& content) {
    std::string file_path = prepare_test_folder() + filename;
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create temporary file: " + file_path);
    }
    file << content;
    file.flush();
    file.close();
    return file_path;
}

#include "utils/scheme_loaders.h"
#include "utils/simple_scheme.h"
#include "utils/task_test_utils.h"
#include "utils/serialization_utils.h"