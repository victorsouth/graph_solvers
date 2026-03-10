#pragma once

namespace graphlib {
;



/// @brief Исключение с привязками (несколькими) на графе
struct graph_exception {
    /// @brief Множество неориентированных привязок к элементам графа
    typedef std::unordered_set<graph_binding_t, graph_binding_t::hash> places_set;
    /// @brief Вектор неориентированных привязок к элементам графа
    typedef std::vector<graph_binding_t> places_vector;

    /// @brief Привязки на графе
    places_set places;
    /// @brief Код ошибки в текстовом виде
    std::wstring message;

    /// @brief Нужно для создания векторов исключений graph_exception
    graph_exception() = default;



    /// @brief Конструктор только с кодом ошибки
    template <typename Places>
    graph_exception(const Places& places,
        const std::wstring& message)
        : places(places.begin(), places.end())
        , message(message)
    {
    }

    /// @brief Исключение, порождаемое исключением модели какого-то одного ребра
    /// в момент локализации его на графе
    graph_exception(const graph_binding_t& place)
        : graph_exception(places_vector(1, place), L"")
    {
    }
    /// @brief Формирует исключение с заданной привязкой и кодом ошибки.
    graph_exception(const graph_binding_t& place, const std::wstring& message)
        : graph_exception(places_vector(1, place), message)
    {
    }

    /// @brief Формирует исключение с заданным кодом
    graph_exception(const std::wstring& message)
        : message(message)
    {
    }

    /// @brief Виртуальный деструктор с реализацией по умолчанию
    virtual ~graph_exception() = default;
};




}