#pragma once

namespace graphlib {
;
/// @brief Тип индекса MIMO вершины
typedef long long id_type;

/// @brief Коннектор MIMO вершины
struct connector_t
{
    /// @brief Возвращает значение, соответствующее коннектору неопределенного типа
    static int no_connector_value()
    {
        return std::numeric_limits<int>::max();
    }
    /// @brief Индекс MIMO вершины
    id_type vertex;
    /// @brief Тип коннектора: 1 - вход, 2 - выход,  иное - неопределенный
    int connector;
    /// @brief Конструктор коннектора MIMO вершины, индекс которой задан
    connector_t(id_type vertex)
        : vertex(vertex)
        , connector(no_connector_value())
    {

    }
    /// @brief Конструктора коннектора заданного типа для MIMO вершины с известным индексом
    /// @param vertex Индекс вершины
    /// @param connector Тип коннектора
    connector_t(id_type vertex, int connector)
        : vertex(vertex)
        , connector(connector)
    {

    }
    /// @brief Возвращает коннектор выхода для вершины с заданным индексом
    static connector_t output(id_type vertex)
    {
        return connector_t(vertex, 2);
    }
    /// @brief Возвращает коннектор входа для вершины с заданным индексом
    static connector_t input(id_type vertex)
    {
        return connector_t(vertex, 1);
    }
    /// @brief Проверяет, что коннектор является входом или выходом
    bool has_connector() const
    {
        return connector != no_connector_value();
    }
    /// @brief Возвращает хеш коннектора. 
    /// Для использования с boost и stdext
    size_t hash() const
    {
        return static_cast<size_t>(vertex + connector);
    }
    /// @brief Оператор "меньше" для упорядочивания объектов connector_t. 
    /// Сравнивает сначала по индексу вершины, далее по типу коннектора
    bool operator<(const connector_t& other) const
    {
        if (vertex != other.vertex)
            return vertex < other.vertex;
        else
            return connector < other.connector;
    }
    /// @brief Проверяет равенство двух коннекторов
    bool operator==(const connector_t& other) const
    {
        return vertex == other.vertex && connector == other.connector;
    }

};

}

namespace std {
    /// @brief Специализация std::hash для использования коннектора как ключа 
    /// в неупорядоченных контейнерах (std::unordered_map, std::unordered_set)
    template <>
    struct hash<graphlib::connector_t> {
        /// @brief Возвращает значение хэш-функции для переданного коннектора
        std::size_t operator()(const graphlib::connector_t& connector) const {
            return connector.hash();
        }
    };
}

namespace graphlib {
;

/// @brief Перемычка в MIMO-графа
struct mimo_edge_t {
    /// @brief идентификатор перемычки
    id_type bridge_id{ 0 };
    /// @brief Коннектор, откуда перемычка выходит
    connector_t from;
    /// @brief Коннектор, куда перемычка входит
    connector_t to;
    /// @brief Формирует ребро с заданным индентфикатором, коннекторами входа и выхода
    mimo_edge_t(id_type bridge_id, const connector_t& from, const connector_t& to)
        : bridge_id(bridge_id)
        , from(from)
        , to(to)
    {

    }
    /// @brief Формирует ребро с заданнымм, коннекторами входа и выхода
    mimo_edge_t(const connector_t& from, const connector_t& to) : from(from)
        , to(to)
    {

    }
    /// @brief Возвращает значение индекса, которое интерпретируется как отсутствие коннектора
    static int no_connector_value()
    {
        return std::numeric_limits<int>::max();
    }
    /// @brief Возвращает коннектор на противоположной относительно заданного коннектора стороне ребра
    /// @throws std::logic_error Если переданный коннектор не принадлежит данному ребру.
    const connector_t& get_other_side_connector(const connector_t& this_side) const
    {
        if (from == this_side)
            return to;
        else if (to == this_side)
            return from;
        else
            throw std::logic_error("node_id is not incedent to mimo_edge or absent connector");
    }
    /// @brief Возвращает индекс MIMO вершины на противоположной стороне относительно заданной вершины, инцидентной ребру
    /// @throws std::logic_error Если заданная вершина не инцидентна ребру.
    id_type get_other_side_node(id_type node_id) const
    {
        if (from.vertex == node_id)
            return to.vertex;
        else if (to.vertex == node_id) {
            return from.vertex;
        }
        else {
            throw std::logic_error("node_id is not incedent to mimo_edge");
        }
    }
    /// @brief Возвращает коннектор MIMO вершины на противоположной стороне относительно заданной вершины, инцидентной ребру
    /// @throws std::logic_error Если заданная вершина не инцидентна ребру.
    int get_connector_for_node(id_type node_id) const
    {
        if (from.vertex == node_id)
            return from.connector;
        else if (to.vertex == node_id) {
            return to.connector;
        }
        else {
            throw std::logic_error("node_id is not incedent to mimo_edge");
        }
    }
};

/// @brief Считает сколько MIMO-перемычек инцидентно каждому коннектору
/// @param bridges Список MIMO-перемычик
/// @return Словарь с количеством инциденций для каждого коннектора
inline std::unordered_map<connector_t, size_t> countup_connector_incidences(
    const std::vector<mimo_edge_t>& bridges)
{
    std::unordered_map<connector_t, size_t> result;
    for (const mimo_edge_t& bridge : bridges) {
        result[bridge.from]++;
        result[bridge.to]++;
    }
    return result;
}

/// @brief Структурированное сообщение об ошибке
struct error_message_t
{
    /// @brief Список идентификаторов объектов, с которыми ассоциирована ошибка
    /// (может быть пустым!)
    std::vector<id_type> object_id_list;
    /// @brief Степень доверия к списку объектов
    /// Если список неполный, надо проверить, как сформировался этот список
    bool object_list_is_complete;
    /// @brief Текстовое сообщение об ошибке
    std::wstring message_text;
};

/// @brief Перечень сообщений об ошибках, возникших при выполнении таска
class mimo_task_error {
protected:
    /// @brief Список сообщений
    std::vector<error_message_t> messages;

protected:
    /// @brief Возвращает текстовое представление ошибки в формате: 
    ///     "Код ошибки.
    ///     Перечень индексов объектов схемы, связанных с ошибкой"
    static std::wstring build_error_text(const error_message_t& message)
    {
        std::wstringstream error_text;
        error_text << message.message_text << std::endl;

        if (message.object_id_list.empty()) {
            error_text << L" (Объект схемы не указан)";
        }
        else {
            if (!message.object_list_is_complete)
                error_text << L"список может быть неполным " << std::endl;
            error_text << L"ObjectIds:";
            error_text << message.object_id_list.front(); // нулевой элемент всегда есть, т.к.
            for (size_t index = 1; index < message.object_id_list.size(); ++index) {
                error_text << L", " << message.object_id_list[index];
            }
            error_text << L".";
        }

        return error_text.str();
    }
public:
    /// @brief Много ошибок, уже упакованных в mimo_task_error
    /// Дает возможность для каждой ошибки задать свое подмножество идентификаторов
    /// @param errors
    mimo_task_error(const std::vector<mimo_task_error>& errors)
    {
        for (const auto& error : errors) {
            messages.insert(messages.end(), error.messages.begin(), error.messages.end());
        }
    }

    /// @brief много объектов (std::vector), одна ошибка
    /// @param object_id_list
    /// @param object_list_is_complete
    /// @param message
    mimo_task_error(const std::vector<id_type>& object_id_list,
        bool object_list_is_complete, const std::wstring& message)
    {
        error_message_t msg{
            object_id_list,
            object_list_is_complete,
            message
        };
        messages.emplace_back(msg);
    }
   
    /// @brief один объект, одна ошибка 
    mimo_task_error(id_type object_id, const std::wstring& message)
    {
        error_message_t msg{
            std::vector<id_type>{ object_id },
            true,
            message
        };
        messages.emplace_back(msg);
    }

    /// @brief Много ошибок для одного объекта
    /// @param object_id Идентификатор объекта
    /// @param errors Сообщения об ошибках
    mimo_task_error(id_type object_id, const std::vector<std::wstring>& errors)
    {
        for (const std::wstring& error : errors) {
            error_message_t message;
            message.object_id_list = { object_id };
            message.object_list_is_complete = true;
            message.message_text = error;
            messages.push_back(message);
        }
    }
   
    /// @brief один объект, одна ошибка (std::string)
    mimo_task_error(id_type _object_id, const std::string& _message)
        : mimo_task_error(_object_id, fixed_solvers::string2wide(_message))
    {

    }

    /// @brief ошибка без указания объекта
    /// @param _message 
    mimo_task_error(const std::wstring& _message)
        : mimo_task_error(-1, _message)
    {

    }


public:
    /// @brief Собирает ошибки по всем сообщениям в одну строку
    /// (не рекомендуется к использованию)
    std::wstring error_message() const
    {
        std::wstringstream error_text;
        for (const auto& message : messages) {
            error_text << build_error_text(message) << std::endl;
        }
        return error_text.str();
    }

    /// @brief Структурированные сообщения об ошибках
    const std::vector<error_message_t>& get_messages() const;
};

/// @brief Строит отображение уникальных коннекторов (connector_t) на последовательные индексы вершин.
inline std::unordered_map<connector_t, size_t> build_connection_mapping(const std::vector<mimo_edge_t>& bridges)
{
    size_t vertex = 0;
    std::unordered_map<connector_t, size_t> mapping;
    auto ensure_mapping = [&](const connector_t& point) {
        if (mapping.find(point) == mapping.end())
            mapping[point] = vertex++;
        };

    for (const mimo_edge_t& bridge : bridges) {
        if (bridge.from.has_connector()) {
            ensure_mapping(connector_t(bridge.from.vertex, bridge.from.connector));
        }
        else {
            ensure_mapping(connector_t(bridge.from.vertex));
        }

        if (bridge.to.has_connector()) {
            ensure_mapping(connector_t(bridge.to.vertex, bridge.to.connector));
        }
        else {
            ensure_mapping(connector_t(bridge.to.vertex));
        }
    }

    return mapping;
}

/// @brief Определяет принадлежность MIMO вершин к компонентам связности в графе, заданном перемычками
/// @param vertex_count Количество вершин
/// @param bridges Перемычки MIMO-графа
/// @return Вектор, где индекс - индекс вершины, значение - индекс компонента связности
inline std::vector<size_t> find_equialent_vertices(size_t vertex_count, const std::vector<edge_incidences_t>& bridges)
{
    // формирование списка смежности графа перемычек
    std::vector<std::unordered_set<size_t>> incidences_list(vertex_count);
    std::for_each(bridges.begin(), bridges.end(), [&](const edge_incidences_t& incidences)
        {
        incidences_list[incidences.from].insert(incidences.to);
        incidences_list[incidences.to].insert(incidences.from);
        });

    // поиск эквивалентных вершин - компонент связности графа перемычек
    std::vector<bool> visited_vertices(vertex_count, false);
    std::unordered_map<size_t, std::unordered_set<size_t>> equivalence_classes;
    for (size_t start_vertex = 0; start_vertex < vertex_count; ++start_vertex) {
        if (incidences_list[start_vertex].empty() || visited_vertices[start_vertex])
            continue;

        std::unordered_set<size_t> equivalence_class;
        std::queue<size_t> bfs_queue;
        bfs_queue.push(*incidences_list[start_vertex].begin());
        while (!bfs_queue.empty()) {
            size_t vertex = bfs_queue.front();
            equivalence_class.insert(vertex);
            visited_vertices[vertex] = true;
            bfs_queue.pop();

            std::for_each(incidences_list[vertex].begin(), incidences_list[vertex].end(), [&](size_t vertex) {
                if (!visited_vertices[vertex])
                    bfs_queue.push(vertex);
                });
        }
        // здесь имеем класс эквивалентности
        // со всех эквивалентных вершин ссылаемся на этот класс эквивалентности
        std::for_each(equivalence_class.begin(), equivalence_class.end(), [&](size_t vertex) {
            equivalence_classes[vertex] = equivalence_class;
            });
    }

    std::vector<size_t> equivalent_vertices(vertex_count, std::numeric_limits<size_t>::max());
    size_t reduced_vertex = 0;
    for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
        std::unordered_map<size_t, std::unordered_set<size_t>>::iterator
            equivalence_class = equivalence_classes.find(vertex);

        // если вершина не имеет класса эквивалентности, то это внутреняя вершина
        if (equivalence_class == equivalence_classes.end()) {
            equivalent_vertices[vertex] = reduced_vertex++;
        }
        else {
            // если для вершины уже задана эквивалентная ей, значит эта вершина не является
            // минимальной в своем классе эквивалентности и она уже обработана ранее, делать ничего не надо
            if (equivalent_vertices[vertex] == std::numeric_limits<size_t>::max()) {
                // если для вершины, имеющей класс эквивалентности не задана эквивалентная ей
                // нужно установить эквивалентную вершину всем вершинам класса эквивалентности
                std::for_each(equivalence_class->second.begin(), equivalence_class->second.end(), [&](size_t vertex) {
                    equivalent_vertices[vertex] = reduced_vertex;
                    });
                reduced_vertex++;
            }
        }
    }
    return equivalent_vertices;
}

/// @brief Агрегирует связные подграфы из перемычек в виде вершин. В полученном графе MIMO-вершины становятся SISO-ребрами
/// @return сопоставление "коннектор → индекс свёрнутой вершины".
inline std::unordered_map<connector_t, size_t> collapse_bridges(
    const std::vector<mimo_edge_t>& computational_bridges /* Мосты и коротыши */
/*const std::vector<edge_with_connectors_t>& passways */)
{
    // 1. Отображение точек соединения в вершины
    std::unordered_map<connector_t, size_t> connection_mapping = build_connection_mapping(computational_bridges);

    // 2. Мосты и коротыши
    std::vector<edge_incidences_t> model_bridges;
    for (const mimo_edge_t& bridge : computational_bridges) {
        size_t vertex_from = connection_mapping.at(bridge.from);
        size_t vertex_to = connection_mapping.at(bridge.to);

        model_bridges.push_back(edge_incidences_t(vertex_from, vertex_to));
    }

    // 3. Схлопнем перемычки
    size_t vertex_count = 1 + get_max_value(connection_mapping);
    std::vector<size_t> redindexate_vertices = find_equialent_vertices(vertex_count, model_bridges);
    for (auto& connection : connection_mapping) {
        connection.second = redindexate_vertices[connection.second];
    }

    return connection_mapping;
}

/// @brief Преобразует отображение "коннектор → SISO-вершина" в сопоставление "SISO-вершина → коннекторы"
/// @throws mimo_task_error Если найдены "висячие перемычки", т.е. несогласованные или потерянные связи.
inline std::unordered_map<id_type, std::unordered_map<int, size_t>>
connector_as_vertex_for_all_objects(
    const std::unordered_map<connector_t, size_t>& connector_to_vertex)
{
    std::unordered_map<id_type, std::unordered_map<int, size_t>> result;
    for (const auto& kvp : connector_to_vertex) {
        const connector_t& connector = kvp.first;
        size_t vertex = kvp.second;

        // Добавляем только те MIMO-вершины, у которых есть коннекторы, т.е. не добавляем перемычки
        bool is_connector_of_object_and_not_bridge = connector.has_connector();
        if (is_connector_of_object_and_not_bridge) {
            result[connector.vertex][connector.connector] = vertex;
        }
    }

    // Если некоторая SISO-вершина соответствует только MIMO-вершинам без коннекторов
    // т.е. имеются висячие перемычки, то SISO-вершины не будут идти по порядку

    // Найти все задействованные вершины
    // Проверить, что они идут по порядку
    // Найти для вершины
    std::unordered_set<size_t> used_vertices;
    for (const auto& object_kvp : result) {
        const std::unordered_map<int, size_t>& connector_number_to_vertex = object_kvp.second;
        for (const auto& connector_kvp : connector_number_to_vertex) {
            // вершина, имеющая соответствие с коннектором какого-либо объекта
            size_t vertex = connector_kvp.second;
            used_vertices.insert(vertex);
        }
    }
    // Проверить, что есть все номера вершин
    for (size_t vertex = 0; vertex < used_vertices.size(); ++vertex) {
        if (used_vertices.find(vertex) == used_vertices.end()) {
            // несколько перемычек могут схлопнуться в одну вершину, поэтому multimap
            std::unordered_multimap<size_t, connector_t> vertex_to_connector;
            get_inverted_index(connector_to_vertex, &vertex_to_connector);

            std::vector<id_type> lost_bridges;
            auto range = vertex_to_connector.equal_range(vertex);
            for (auto range_iter = range.first; range_iter != range.second; ++range_iter) {
                lost_bridges.push_back(range_iter->second.vertex);
            }

            if (!lost_bridges.empty()) {
                throw mimo_task_error(lost_bridges, true, std::wstring(L"Одна или несколько висячих перемычек"));
            }
            else {
                throw mimo_task_error(std::wstring(L"Висячая перемычка (не удалось обнаружить идентификатор, обратитесь к разработчику)"));
            }
        }
    }

    return result;
}

}

