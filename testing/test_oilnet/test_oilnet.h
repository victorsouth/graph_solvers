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

inline void ASSERT_BETWEEN (double v, double a, double b) {
    const double lo = std::min(a, b);
    const double hi = std::max(a, b);
    ASSERT_TRUE(v >= lo && v <= hi);
};

#include "utils/scheme_loaders.h"
#include "utils/simple_scheme.h"
#include "utils/task_test_utils.h"