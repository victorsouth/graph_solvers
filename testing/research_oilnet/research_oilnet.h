#pragma once

#define GTEST_BREAK_ON_FAILURE 1
#define GTEST_CATCH_EXCEPTIONS 0
#define GTEST_HAS_SEH 0
#include <gtest/gtest.h>


std::string prepare_test_folder(const std::string& subfolder = "");

std::string get_schemes_folder();

/// @brief Возвращает путь к папке с json файлами топологии и параметров для тестовой схемы с поддиректорией scheme_subdir
std::string get_scheme_folder(const std::string& scheme_subdir);