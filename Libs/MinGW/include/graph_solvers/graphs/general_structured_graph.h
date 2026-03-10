#pragma once

namespace graphlib {
;

/// @brief Структура для сопоставления "элемент графа - элемент подграфа"
typedef std::unordered_multimap<graph_binding_t, subgraph_binding_t, graph_binding_t::hash> descend_map_t;
/// @brief Структура для сопоставления "элемент подграфа - элемент графа"
typedef std::unordered_map<subgraph_binding_t, graph_binding_t, subgraph_binding_t::hash> ascend_map_t;


/// @brief Граф с выделенными в нем подграфами
/// @tparam SubgraphType 
template <typename SubgraphType>
struct general_structured_graph_t {
    /// @brief Подграфы на графе
    std::unordered_map<size_t, SubgraphType> subgraphs;
    /// @brief Сопоставление "элемент графа - элемент подграфа"
    descend_map_t descend_map;
    /// @brief Сопоставление "элемент подграфа - элемент графа"
    ascend_map_t ascend_map;

    /// @brief Проверяет наличие ровно одного нисходящего индекса (что будет автоматически означает и биективность)
    /// @return Элемент, на который указывает единственный индекс
    /// @throws std::runtime_error Если нисходящий индекс не единственный
    const subgraph_binding_t& get_bijective_descend_binding(const graph_binding_t& binding) const {
        if (descend_map.count(binding) != 1)
            throw std::runtime_error("find_ident_parameters_from_active_subnets(): ambigous parameter binding");

        auto range = descend_map.equal_range(binding);
        const auto& binding_pair = range.first;
        const subgraph_binding_t& local_binding = binding_pair->second;

        return local_binding;
    }

    /// @brief Проверяет наличие ровно одного нисходящего индекса на множестве разрешенных подграфов allowed_subgraphs
    /// @return Элемент, на который указывает индекс
    /// @throws std::runtime_error Если нисходящий индекс не единственный
    const subgraph_binding_t& get_bijective_descend_binding(const graph_binding_t& binding,
        const std::unordered_set<size_t>& allowed_subgraphs) const
    {
        // Достаем все нисходящие индексы, указывающие на 

        auto range = descend_map.equal_range(binding);
        const auto& binding_pair = range.first;

        std::vector<const subgraph_binding_t*> result;
        for (auto iter = range.first; iter != range.second; ++iter) {
            size_t subgraph = iter->second.subgraph_index;
            if (allowed_subgraphs.find(subgraph) != allowed_subgraphs.end())
                result.push_back(&iter->second);
        }

        if (result.size() != 1)
            throw std::runtime_error("find_ident_parameters_from_active_subnets(): ambigous parameter binding");
        return *result.front();
    }

    /// @brief То же, что get_bijective_descend_binding, но учитывает привязку к стороне ребра
    subgraph_place_binding_t get_bijective_descend_binding(const graph_place_binding_t& binding) const {
        const subgraph_binding_t& subgraph_binding = get_bijective_descend_binding(static_cast<const graph_binding_t&>(binding));

        if (binding.is_sided) {
            subgraph_place_binding_t result(subgraph_binding, binding.side); // конструктор, задающий привязку в стороне ребра
            return result;
        }
        else {
            subgraph_place_binding_t result(subgraph_binding.subgraph_index, subgraph_binding); // конструктор, не задающий привязку к стороне ребра
            return result;
        }
    }
   
    /// @brief То же, что get_bijective_descend_binding, но учитывает привязку к стороне ребра
    /// @param allowed_subgraphs мнжество разрешенных подграфов
    subgraph_place_binding_t get_bijective_descend_binding(
        const graph_place_binding_t& binding, const std::unordered_set<size_t>& allowed_subgraphs) const
    {
        const subgraph_binding_t& subgraph_binding =
            get_bijective_descend_binding(
                static_cast<const graph_binding_t&>(binding), allowed_subgraphs);

        if (binding.is_sided) {
            subgraph_place_binding_t result(subgraph_binding, binding.side); // конструктор, задающие привязку в стороне ребра
            return result;
        }
        else {
            subgraph_place_binding_t result(subgraph_binding.subgraph_index, subgraph_binding); // конструктор, не задающий привязку к стороне ребра
            return result;
        }

    }

    /// @brief Возвращает все нисходящие индексы для заданной привязки на множестве разрешенных подграфов
    /// @return элементы, на которые указывают нисходящие индексы
    std::vector<subgraph_binding_t> descend_graph_binding(const graph_binding_t& binding,
        const std::unordered_set<size_t>& allowed_subgraphs) const
    {
        std::vector<subgraph_binding_t> result;
        auto range = descend_map.equal_range(binding);
        const auto& binding_pair = range.first;

        for (auto iter = range.first; iter != range.second; ++iter) {
            const subgraph_binding_t& subgraph_binding = iter->second;

            if (allowed_subgraphs.find(subgraph_binding.subgraph_index) != allowed_subgraphs.end())
                result.push_back(iter->second);
        }
        return result;
    }

    /// @brief Возвращает все нисходящие индексы для заданной привязки на множестве разрешенных подграфов
    /// @warning Учитывает привязку к стороне ребра
    /// @return элементы, на которые указывают нисходящие индексы
    std::vector<subgraph_place_binding_t> descend_graph_binding(const graph_place_binding_t& binding,
        const std::unordered_set<size_t>& allowed_subgraphs) const
    {
        std::vector<subgraph_place_binding_t> result_vector;
        auto range = descend_map.equal_range(binding);
        const auto& binding_pair = range.first;

        for (auto iter = range.first; iter != range.second; ++iter) {
            const subgraph_binding_t& subgraph_binding = iter->second;
            if (allowed_subgraphs.find(subgraph_binding.subgraph_index) != allowed_subgraphs.end()) {
                if (binding.is_sided) {
                    subgraph_place_binding_t result(subgraph_binding, binding.side); // конструктор, задающие привязку в стороне ребра
                    result_vector.emplace_back(result);
                }
                else {
                    subgraph_place_binding_t result(subgraph_binding.subgraph_index, subgraph_binding); // конструктор, не задающий привязку к стороне ребра
                    result_vector.emplace_back(result);
                }
            }

        }
        return result_vector;
    }

    /// @brief Возвращает все нисходящие индексы для заданной привязки
    /// @return элементы, на которые указывают нисходящие индексы
    std::vector<subgraph_binding_t> descend_graph_binding(const graph_binding_t& binding) const {
        std::vector<subgraph_binding_t> result;
        auto range = descend_map.equal_range(binding);
        const auto& binding_pair = range.first;

        for (auto iter = range.first; iter != range.second; ++iter) {
            result.push_back(iter->second);
        }
        return result;
    }

    /// @brief Возвращает все восходящие индексы для заданной привязки к подграфу
    /// @tparam SubgraphBindings Тип привязки к подграфу
    template <typename SubgraphBindings>
    std::vector<graph_binding_t> ascend_graph_bindings(const SubgraphBindings& local_bindings) const {
        std::vector<graph_binding_t> global_bindings;
        std::transform(local_bindings.begin(), local_bindings.end(),
            std::back_inserter(global_bindings),
            [&](const subgraph_binding_t& local_binding) { return ascend_map.at(local_binding); }
        );
        return global_bindings;
    }

    // 
    
    /// @brief Возвращает все восходящие индексы для заданной привязки к подграфу
    /// @warning Для всех привязок принудительно задается индекс подграфа 
    /// @tparam SubgraphBindings Тип привязки к подграфу 
    /// @param subgraph_index Принудительно проставляемый индекс подграфа
    template <typename SubgraphBindings>
    std::vector<graph_binding_t> ascend_graph_bindings(size_t subgraph_index, const SubgraphBindings& bindings) const {
        std::vector<subgraph_binding_t> subgraph_bindings;

        for (const graph_binding_t& binding : bindings) {
            subgraph_bindings.emplace_back(subgraph_binding_t(subgraph_index, binding));
        }

        return ascend_graph_bindings(subgraph_bindings);
    }

    /// @brief Возвращает восходящий индекс для заданной привязки к подграфу
    /// @param subgraph_index Подграф
    /// @param binding Привязка в подграфе
    graph_binding_t ascend_graph_binding(size_t subgraph_index, 
        const graph_binding_t& binding) const 
    {
        subgraph_binding_t subgraph_binding(subgraph_index, binding);
        return ascend_map.at(subgraph_binding);
    }

    /// @brief Возвращает глобальные индексы вершин подграфа с заданным индексом
    std::unordered_set<size_t> get_subgraph_global_vertices(size_t subgraph_index) const
    {
        // Реализация учитывает, что индексы вершин в подграфе могут идти не подряд
        const auto& subgraph = subgraphs.at(subgraph_index);
        auto local_vertices = get_map_keys(subgraph.get_vertex_incidences());
        std::unordered_set<size_t> result;

        for (size_t local_vertex : local_vertices) {
            subgraph_binding_t local_binding(subgraph_index, graph_binding_t(graph_binding_type::Vertex, local_vertex));
            const graph_binding_t& global_binding = ascend_map.at(local_binding);
            result.insert(global_binding.vertex);
        }
        return result;
    }

    /// @brief Возвращает глобальные индексы вершин по каждому подграфу из заданного множества
    /// @tparam Container Тип структуры, содержащей подграфы
    /// @param subgraphs Множество подграфов
    template <typename Container>
    std::unordered_map<size_t, std::unordered_set<size_t>>get_subgraph_global_vertices(const Container& subgraphs) const
    {
        std::unordered_map<size_t, std::unordered_set<size_t>> result;
        for (const auto& subgraph : subgraphs) {
            result.emplace(std::piecewise_construct,
                std::forward_as_tuple(subgraph),
                std::forward_as_tuple(get_subgraph_global_vertices(subgraph)));
        }
        return result;
    }

    /// @brief На основе исходного строит новый структурированный граф, 
    /// который получается разрезанием подграфов исходного графа
    /// @param subgraphs_index Задает подграфы, на которые надо разрезать 
    /// каждый из подграфов исходного структурированного графа
    general_structured_graph_t<SubgraphType> split(const std::unordered_map<size_t, std::vector<subgraph_t>>& subgraphs_index) const
    {
        general_structured_graph_t<SubgraphType> result;

        // indices - плоский список, перебирающий все пары
        // из индексов исходных подграфов и индексов новых подграфов
        // Номер элементов в этом списке дадут новые индексы подграфов
        std::vector<std::pair<size_t, size_t>> indices;
        for (const auto& sub_subgraphs : subgraphs_index) {
            size_t subgraph = sub_subgraphs.first;
            for (size_t sub_subgraph = 0; sub_subgraph < sub_subgraphs.second.size(); ++sub_subgraph) {
                indices.emplace_back(std::make_pair(subgraph, sub_subgraph));
            }
        }

        result.ascend_map.reserve(indices.size());
        result.descend_map.reserve(indices.size());

        // new_graph_id - сквозная нумерация всех отрезаемых кусочков от всех подграфов
        //#pragma omp parallel for schedule(dynamic) // делает только хуже
        for (int new_graph_id = 0; new_graph_id < static_cast<int>(indices.size()); ++new_graph_id) {
            const auto& index = indices[new_graph_id];
            size_t old_graph_id = index.first; // индекс подграфа в исходного графе
            size_t subsubgraph_number = index.second; // номер отрезаемого кусочка из подграфа

            // хранит информацию подмножестве объектов, которые надо взять из текущего подграфа
            const subgraph_t& sub_subgraph_indices = subgraphs_index.at(old_graph_id)[subsubgraph_number];

            // Создать подграф расчетных моделей (который затем попадает расчетчикам)
            const SubgraphType& subgraph = subgraphs.at(old_graph_id);
            //#pragma omp critical
            {
                result.subgraphs.emplace(new_graph_id, subgraph.template create_subgraph<SubgraphType>(new_graph_id, sub_subgraph_indices));
            }

            // Создать индексы
            // Вырезаемые объекты из подграфа, индекация соответствует подграфу
            auto old_subgraph_objects = sub_subgraph_indices.get_global_binding_list(subgraph.id);
            // Те же вырезаемые объекты, но с новой локальной индексацией внутри отрезаемого кусочка
            auto local_list = sub_subgraph_indices.get_local_binding_list(new_graph_id);
            for (size_t old_index = 0; old_index < old_subgraph_objects.size(); ++old_index) {
                // Найдем глобальный идентификатор объекта в исходном ГРАФЕ (не подграфе!)
                // и привяжем глобальный идентификатор к локальному индексу объекта в отрезаемом кусочке
                const auto& old_subgraph_object = old_subgraph_objects[old_index]; // идентификатор в исходном (не резаном) подграфе
                auto global_id_iter = ascend_map.find(old_subgraph_object);
                if (global_id_iter == ascend_map.end())
                    throw graph_exception(std::wstring(L"Невозможно найти глобальный индекс"));
                const auto& global_id = global_id_iter->second;  // глобальный идентфикатор
                const auto& local_id = local_list[old_index]; // локальный идентификатор в отрезанном кусочке

                //#pragma omp critical
                {
                    result.ascend_map.emplace(local_id, global_id);
                    result.descend_map.emplace(global_id, local_id);
                }
            }
        }

        return result;
    }

};


template <typename StructuredGraph, typename GraphBinding>
inline GraphBinding ascend_graph_binding(const StructuredGraph& structured_graph, size_t subgraph_index,
    const GraphBinding& binding)
{
    GraphBinding result = binding;

    subgraph_binding_t local_binding(subgraph_index, binding);
    graph_binding_t graph_binding = structured_graph.ascend_map.at(local_binding);

    if constexpr (binding_type_is_sided<GraphBinding>::value) {
        if (binding.is_sided) {
            graphlib::graph_place_binding_t place_binding(graph_binding, binding.side);
            result = place_binding;
        }
        else {
            result = graph_binding;
        }
    }
    else {
        result = graph_binding;
    }

    return result;
}

template <typename StructuredGraph, typename GraphBinding>
inline std::vector<GraphBinding> ascend_graph_bindings(const StructuredGraph& structured_graph, size_t subgraph_index,
    const std::vector<GraphBinding>& bindings)
{
    std::vector<GraphBinding> result;
    result.reserve(bindings.size());

    for (const GraphBinding& binding : bindings) {
        result.emplace_back(
            ascend_graph_binding(structured_graph, subgraph_index, binding)
            );
    }

    return result;
}


}