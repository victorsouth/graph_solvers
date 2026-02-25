#pragma once


namespace graphlib {
;

/// @brief Тип привязки с точки зрения элементов графа
enum class graph_binding_type {
    Vertex, Edge1, Edge2
};

/// @brief Базовый тип перечисления GraphBindingType
typedef std::underlying_type<graph_binding_type>::type graph_binding_integral_type;

/// @brief Тип привязки к стороне ребра - вход, выход
enum class graph_edge_side_t {
    Inlet, Outlet
};

/// @brief Привязка к элементу графа
struct graph_binding_t {
    /// @brief Тип привязки
    graph_binding_type binding_type{ graph_binding_type::Vertex };
    /// @brief Индекс элемента
    union {
        size_t vertex{ std::numeric_limits<size_t>::max() };
        size_t edge;
    };
    
    /// @brief Формирует привязку с заданным типом привязки и индексом элемента
    graph_binding_t(graph_binding_type _binding_type, size_t place) 
        : binding_type(_binding_type)
    {
        if (binding_type == graph_binding_type::Vertex) {
            vertex = place;
        }
        else if (binding_type == graph_binding_type::Edge1 || binding_type == graph_binding_type::Edge2) {
            edge = place;
        }
        else
            throw std::runtime_error("Wrong binding_type");
    }

    /// @brief Конструктор по умолчанию для graph_binding_t.
    graph_binding_t() = default;


    /// @brief Проверяет наличие достоверного индекса элемента в привязке  
    bool has_binding() const
    {
        return vertex != std::numeric_limits<size_t>::max();
    }

    /// @brief Возвращает индекс элемента
    size_t get_place_id() const
    {
        if (binding_type == graph_binding_type::Vertex)
            return vertex;
        else
            return edge;
    }

    /// @brief Проверяет равенство двух привязок
    bool operator==(const graph_binding_t& other) const
    {
        return binding_type == other.binding_type
            && get_place_id() == other.get_place_id();
    }

    /// @brief Хэш для объектов типа graph_binding_t
    struct hash {
        /// @brief Возвращает хэш для заданной привязке
        size_t operator()(const graph_binding_t& binding) const
        {
            std::hash<size_t> hash_fn;
            auto binding_type_as_integral = static_cast<graph_binding_integral_type>(binding.binding_type);
            size_t result = (hash_fn(binding.edge)) ^ (hash_fn(binding_type_as_integral) >> 30);
            return result;
        }
    };

    /// @brief Виртуальный деструктор. Использует реализацию по умолчанию
    virtual ~graph_binding_t() = default;

};

/// @brief Привязка к элементу подграфа
struct subgraph_binding_t : public graph_binding_t {
    /// @brief Индекс подграфа
    size_t subgraph_index{ std::numeric_limits<size_t>::max() };

    /// @brief Конструктор по умолчанию
    subgraph_binding_t() = default;

    /// @brief Формирует привязку заданного типа к элементу с заданным индексом в заданном подграфе
    subgraph_binding_t(size_t subgraph_index, graph_binding_type binding_type, size_t place)
        : graph_binding_t(binding_type, place)
        , subgraph_index(subgraph_index)
    {
    }

    /// @brief Формирует привязку в подграфе на основе привязке в графе и индекса подграфа
    subgraph_binding_t(size_t subgraph_index, const graph_binding_t& graph_binding)
        : graph_binding_t(graph_binding)
        , subgraph_index(subgraph_index)
    {
    }

    /// @brief Проверяет равенство двух привязок
    bool operator==(const subgraph_binding_t& other) const
    {
        if (subgraph_index == other.subgraph_index)
            return static_cast<const graph_binding_t&> (*this) == static_cast<const graph_binding_t&> (other);
        else
            return false;
    }

    /// @brief Хэш для объектов типа subgraph_binding_t
    struct hash {
        /// @brief Вычисляет хэш для заданной привязки к подграфу
        size_t operator()(const subgraph_binding_t& binding) const
        {
            std::hash<size_t> hash_fn;
            size_t result =
                (hash_fn(binding.subgraph_index))
                ^ (hash_fn(binding.edge) >> 12)
                ^ (hash_fn(static_cast<graph_binding_integral_type>(binding.binding_type)) >> 30);
            return result;
        }
    };
};

/// @brief Среди заданных привязок на графе выделяет привязки в подграфе с заданным индексом
/// @param bindings Привязке на графе
/// @param subgraph_index Индекс подграфа
inline std::unordered_set<subgraph_binding_t, subgraph_binding_t::hash> get_subgraph_bindings(
    const std::unordered_set<graph_binding_t, graph_binding_t::hash>& bindings, size_t subgraph_index)
{
    std::unordered_set<subgraph_binding_t, subgraph_binding_t::hash> result;
    //result.reserve(bindings.size());
    std::transform(bindings.begin(), bindings.end(),
        std::inserter(result, result.end()),
        [&](const graph_binding_t& local_binding) { return subgraph_binding_t(subgraph_index, local_binding); }
    );
    return result;
}

/// @brief Привязка к ребру графа. Возможен учет стороны ребра (ориентированная привязка)
struct graph_place_binding_t : public graph_binding_t {
    /// @brief Флаг учета стороны
    bool is_sided{ false };
    /// @brief Сторона ребра
    graph_edge_side_t side{ graph_edge_side_t::Inlet };

    /// @brief Проверяет равенство двух привязок
    bool operator==(const graph_place_binding_t& other) const
    {
        bool binding_equal =
            (static_cast<const graph_binding_t&> (*this) == static_cast<const graph_binding_t&> (other));
        bool sided_equal = is_sided == other.is_sided;

        bool sides_equal = !is_sided
            ? true
            : side == other.side;

        return binding_equal && sided_equal && sides_equal;
    }

    /// @brief Создает пустую привязку без указания стороны ребра
    graph_place_binding_t() = default;
    /// @brief Конструктор копирования. Использует реализацию по умолчанию
    graph_place_binding_t(const graph_place_binding_t& binding) = default;
    /// @brief Формирует привязку из неориентированной привязки
    /// @warning Создаваемая привязка не учитывает сторону ребра
    graph_place_binding_t(const graph_binding_t& binding)
        : graph_binding_t(binding)
    {
    }
    /// @brief Формирует ориентированную привязку на основе неориентированной привязки и заданной стороны ребра
    graph_place_binding_t(const graph_binding_t& binding, graph_edge_side_t side)
        : graph_binding_t(binding)
        , is_sided(true)
        , side(side)
    {
    }
    /// @brief Формирует ориентированную привязку полному набору параметров - типу элемента графа, его индексу, стороне привязке
    /// @param _binding_type 
    /// @param place 
    /// @param side 
    graph_place_binding_t(graph_binding_type _binding_type, size_t place, graph_edge_side_t side)
        : graph_place_binding_t(graph_binding_t(_binding_type, place), side)
    {
    }
    /// @brief Хэш для объектов типа graph_binding_t
    struct hash {
        /// @brief Возвращает хэш для заданной привязке
        size_t operator()(const graph_place_binding_t& binding) const
        {
            std::hash<size_t> hash_fn;
            auto binding_type_as_integral = static_cast<graph_binding_integral_type>(binding.binding_type);
            size_t result = (hash_fn(binding.edge)) ^ (hash_fn(binding_type_as_integral) >> 30);
            return result;
        }
    };
};

// Type trait to check for the existence of 'my_field'
template <typename T, typename = std::void_t<>>
struct binding_type_is_sided : std::false_type {};

template <typename T>
struct binding_type_is_sided<T, std::void_t<decltype(std::declval<T>().side)>> : std::true_type {};


/// @brief Привязка к ребру подграфа. Возможен учет стороны ребра (ориентированная привязка)
struct subgraph_place_binding_t : public graph_place_binding_t {
    /// @brief Индекс подграфа
    size_t subgraph_index{ std::numeric_limits<size_t>::max() };
    /// @brief Формирует ориентированную привязку к подграфу
    /// @param subgraph_index Индекс подграфа
    /// @param binding_type Тип элемента подграфа
    /// @param place Индекс элемента
    /// @param side Сторона привязки
    subgraph_place_binding_t(size_t subgraph_index, graph_binding_type binding_type, size_t place, graph_edge_side_t side)
        : subgraph_place_binding_t(subgraph_index, graph_place_binding_t(binding_type, place, side))
    {
    }
    /// @brief Формирует ориентированную привязку в ПОДГРАФЕ по ориентированной привязке в ГРАФЕ
    /// @param subgraph_index Индекс подграфа
    /// @param graph_binding Ориентированная привязка в графе 
    subgraph_place_binding_t(size_t subgraph_index, const graph_place_binding_t& graph_place_binding)
        : graph_place_binding_t(graph_place_binding)
        , subgraph_index(subgraph_index)
    {
    }
    /// @brief Конструктор из индекса подграфа и привязки к графу
    /// @param subgraph_index Индекс подграфа
    /// @param graph_binding Привязка к графу
    subgraph_place_binding_t(size_t subgraph_index, const graph_binding_t& graph_binding)
        : graph_place_binding_t(graph_binding)
        , subgraph_index(subgraph_index)
    {
    }
    /// @brief Формирует ориентированную привязку в ПОДГРАФЕ на основе НЕориентированной привязки в ПОДГРАФЕ
    /// @param subgraph_binding Неориентированная привязка в подграфе
    /// @param side Сторона привязки
    subgraph_place_binding_t(const subgraph_binding_t& subgraph_binding, graph_edge_side_t side)
        : subgraph_place_binding_t(subgraph_binding.subgraph_index, graph_place_binding_t(subgraph_binding, side))
    {
    }


};

/// @brief Множество неориентированных привязок к элементам графа
typedef std::unordered_set<graph_binding_t, graph_binding_t::hash> graph_places_set_t;
/// @brief Вектор неориентированных привязок к элементам графа
typedef std::vector<graph_binding_t> graph_places_vector_t;


}