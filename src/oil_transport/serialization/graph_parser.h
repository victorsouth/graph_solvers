#pragma once

namespace oil_transport {
;


/// @brief Представление данных о схеме, близкое к тому, как они хранятся в файлах формата JSON
/// В таком виде данные еще требует преобразования для создания SISO-графа, 
/// для этого используется graph_siso_data
/// Зачитка данных из файлов делается в другом месте, в этом смысле данная структура является контейнером
/// Для конвертации в graph_siso_data в этой структуре есть полезные геттеры и чекеры
/// Самое важное - проверить, что имена объектов нигде не повторяются и все они задействованы
struct graph_json_data {
    /// @brief Список ребер всех типов с указанием имени объекта
    std::vector<graphlib::model_incidences_t<std::string>> edges;
    /// @brief Список свойств сосредоточенных объектов с привязкой к имени
    std::unordered_map<std::string, lumped_json_data> lumpeds;
    /// @brief Параметры задвижек
    std::unordered_map<std::string, valve_json_data> valves;
    /// @brief Список свойств труб с привязкой к имени
    std::unordered_map<std::string, pde_solvers::pipe_json_data> pipes;
    /// @brief Список свойств источников с привязкой к имени
    std::unordered_map<std::string, source_json_data> sources;
    /// @brief Список свойств насосов с привязкой к имени
    std::unordered_map<std::string, lumped_json_data> pumps;
    /// @brief Список тегов по всем объектам
    std::unordered_map<std::string, object_measurement_tags_t> object_tags;
    /// @brief Cписок тегов состояний по объектам
    std::unordered_map<std::string, control_tag_info_t> object_controls;

    /// @brief Количество объектов всех типов
    std::size_t get_total_object_count() const;

    /// @brief Выполняет конвертацию свойств объекта гидравлического расчета в сосредоточенный объект
    /// транспортного расчета
    /// @tparam JsonModelParameters Тип данных для свойств объекта гидравлического расчета
    /// @param json_parameters Свойства объекта гидравлического расчета
    /// @return Свойства сосредоточенного транспортного объекта
    template <typename JsonModelParameters>
    static std::unordered_map<std::string, lumped_json_data> convert_to_lumped_data(
        const std::unordered_map<std::string, JsonModelParameters>& json_parameters) {
        
        
        auto converter_to_lumped = [](const std::pair<const std::string, JsonModelParameters>& kvp) {

            const JsonModelParameters& original_data = kvp.second;
            lumped_json_data converted;

            if constexpr (std::is_same<JsonModelParameters, valve_json_data>::value) {
                converted.is_initially_opened = original_data.initial_opening > 0;
            }

            std::string object_name = kvp.first;
            return std::make_pair(object_name, converted);
            };
        
        std::unordered_map<std::string, lumped_json_data> result;

        std::transform(json_parameters.begin(), json_parameters.end(), 
            std::inserter(result, result.end()),
            converter_to_lumped
            );

        return result;
    }

//private: // вернуть private
    template <typename ModelParameters, typename JsonModelParameters>
    /// @brief Преобразует JSON-параметры в параметры моделей
    /// @param json_parameters Отображение имен объектов на их JSON-параметры
    /// @param object_list Отображение имен объектов на параметры моделей
    static void convert_objects(
        const std::unordered_map<std::string, JsonModelParameters>& json_parameters,
        std::unordered_map <std::string, std::unique_ptr<model_parameters_t>>& object_list) 
    {
        for (const auto& [name, params] : json_parameters) {
            ModelParameters model_parameters(params);
            object_list[name] = std::move(std::make_unique<ModelParameters>(model_parameters));
        }
    }

public:
    /// @brief Возвращает полный список транспортных параметров объектов с привязкой по имени объекта
    template <typename SolverPipeParameters>
    std::unordered_map<
        std::string,
        std::unique_ptr<model_parameters_t>
    > get_transport_object_parameters() const
    {
        std::unordered_map <std::string, std::unique_ptr<model_parameters_t>> result;

        auto copy_and_convert_pipes = [&](const auto& parameters) {
            for (const auto& [name, params] : parameters) {
                using TransportPipeType = transport_pipe_parameters_t<SolverPipeParameters>;

                const pde_solvers::pipe_json_data& json_pipe = params;
                SolverPipeParameters solver_pipe(json_pipe);
                TransportPipeType transport_pipe(solver_pipe);

                result[name] = std::move(std::make_unique<TransportPipeType>(transport_pipe));
            }
        };

        convert_objects<transport_source_parameters_t>(sources, result);
        convert_objects<transport_lumped_parameters_t>(lumpeds, result);
        //convert_objects<transport_lumped_parameters_t>(pumps, result);
        // Задвижки обрабатываем как сосредоточенные транспортные объекты
        convert_objects<transport_lumped_parameters_t>(convert_to_lumped_data(valves), result);
        copy_and_convert_pipes(pipes);

        return result;
    }

    /// @brief Возвращает полный список транспортных параметров объектов с привязкой по имени объекта
    template <typename SolverPipeParameters = qsm_pipe_transport_parameters_t>
    std::unordered_map<
        std::string,
        std::unique_ptr<model_parameters_t>
    > get_hydro_object_parameters() const
    {
        std::unordered_map <std::string, std::unique_ptr<model_parameters_t>> result;

        if (!lumpeds.empty()) {
            throw std::runtime_error("Cannot create hydraulic model for transport-only lumped object");
        }

        auto copy_and_convert_pipes = [&](const auto& parameters) {
            for (const auto& [name, params] : parameters) {
                using TransportPipeType = oil_transport::hydraulic_pipe_parameters_t<SolverPipeParameters>;

                const pde_solvers::pipe_json_data& json_pipe = params;
                SolverPipeParameters solver_pipe(json_pipe);
                TransportPipeType transport_pipe(solver_pipe);

                result[name] = std::move(std::make_unique<TransportPipeType>(transport_pipe));
            }
        };

        convert_objects<oil_transport::qsm_hydro_source_parameters>(sources, result);
        convert_objects<oil_transport::qsm_hydro_local_resistance_parameters>(valves, result);
        //convert_objects<hydro_objects_properties::pump>(pumps, result);
        copy_and_convert_pipes(pipes);

        return result;
    }
   

    /// @brief Вызвращает список имен объектов всех типов
    std::unordered_set<std::string> get_object_names() const;
    /// @brief Проверяет, что среди объектов разных типов нет повторяющихся имен
    void check_names_uniqueness() const;
    /// @brief Провеяет, что топология соответствует списку объектов
    /// Ровно все объекты, что есть, задействованы в топологии
    void check_topology_and_object_links() const;
};



/// @brief Представление данных о схеме, непосредственно используемое для создания SISO-графа
/// Под данными о схеме подразумевается топология, параметры объектов, привязки тегов измерений к объектам
/// Сами временные ряд измерений реализованы сбоку, здесь храним только 
struct graph_siso_data {
    /// @brief Односторонние и двусторонние ребра
    std::pair<
        std::vector<graphlib::edge_incidences_t>, 
        std::vector<graphlib::edge_incidences_t>
    > edges;
    /// @brief Связь имени с идентификатором ребра (одностороннего или двустороннего)
    std::unordered_map<std::string, graphlib::graph_binding_t> name_to_binding;
    
    /// @brief Список свойств объектов-односторонних и двусторонних ребер
    std::pair<
        std::vector<std::unique_ptr<model_parameters_t>>, 
        std::vector<std::unique_ptr<model_parameters_t>>
    > transport_props;

    /// @brief Список свойств объектов-односторонних и двусторонних ребер (гидравлические)
    std::pair<
        std::vector<std::unique_ptr<model_parameters_t>>, 
        std::vector<std::unique_ptr<model_parameters_t>>
    > hydro_props;


    /// @brief Список привязок тегов для односторонних ребер
    std::vector<object_measurement_tags_t> tags1;
    /// @brief Список привязок тегов для двусторонних ребер
    std::vector<object_measurement_tags_t> tags2;
    /// @brief Список привязок тегов состояний для односторонних ребер
    std::vector<control_tag_info_t> controls1;
    /// @brief Список привязок тегов состояний для двусторонних ребер
    std::vector<control_tag_info_t> controls2;
    /// @brief Возвращает свойства заданного типа
    template <typename Properties>
    const Properties& get_properties(const std::string& obj_name) const {
        const graphlib::graph_binding_t& binding = name_to_binding.at(obj_name);
        const model_parameters_t* properties =
            binding.binding_type == graphlib::graph_binding_type::Edge1
            ? transport_props.first.at(binding.edge).get()
            : transport_props.second.at(binding.edge).get();

        if (auto casted_properties = dynamic_cast<const Properties*>(properties)) {
            return *casted_properties;
        }
        else {
            throw std::runtime_error("Cannot cast properties");
        }
    }
    /// @brief Возвращает свойства объекта по его имени (неконстантная версия)
    /// @tparam Properties Тип свойств объекта
    /// @param obj_name Имя объекта
    /// @return Ссылка на свойства объекта заданного типа
    /// @throws std::runtime_error Если не удалось привести свойства к заданному типу
    template <typename Properties>
    Properties& get_properties(const std::string& obj_name, qsm_problem_type problem_type = qsm_problem_type::Transport) {
        const graphlib::graph_binding_t& binding = name_to_binding.at(obj_name);
        
        std::pair<
            std::vector<std::unique_ptr<model_parameters_t>>,
            std::vector<std::unique_ptr<model_parameters_t>>
        >& parameters_data = (problem_type == qsm_problem_type::Transport) ? (transport_props) : (hydro_props);
        
        model_parameters_t* properties =
            binding.binding_type == graphlib::graph_binding_type::Edge1
            ? parameters_data.first.at(binding.edge).get()
            : parameters_data.second.at(binding.edge).get();

        if (auto casted_properties = dynamic_cast<Properties*>(properties)) {
            return *casted_properties;
        }
        else {
            throw std::runtime_error("Cannot cast properties");
        }
    }
    //template <typename Properties>
    //const Properties& get_properties(const std::string& obj_name) const {
    //    auto self = const_cast<transport_graph_siso_data*>(this);
    //    return self->get_properties(obj_name);
    //}

    /// @brief Создает маппинг от привязки к ребру (graph_binding_t) к имени объекта (sources/lumpeds/pipes) из topology.json
    /// Учитывает тип привязки (Edge1/Edge2), чтобы различать ребра с одинаковыми индексами
    /// @return Маппинг от graph_binding_t к имени объекта
    std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash> create_edge_binding_to_name() const
    {
        std::unordered_map<
            graphlib::graph_binding_t, 
            std::string, 
            graphlib::graph_binding_t::hash
        > result;
        for (const auto& [name, binding] : name_to_binding) {
            // Учитываем только ребра (Edge1 и Edge2), пропускаем вершины
            if (binding.binding_type == graphlib::graph_binding_type::Edge1 ||
                binding.binding_type == graphlib::graph_binding_type::Edge2) {
                result[binding] = name;
            }
        }
        return result;
    }

    /// @brief Возвращает имя объекта по идентификатору ребра и типу привязки (Edge1/Edge2)
    /// @param edge_id_to_name Маппинг привязки ребра к имени (результат вызова create_edge_binding_to_name())
    static const std::string& get_object_name(
        graphlib::graph_binding_type binding_type,
        size_t edge_id,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name)
    {
        graphlib::graph_binding_t binding(binding_type, edge_id);
        auto it = edge_id_to_name.find(binding);
        if (it == edge_id_to_name.end()) {
            throw std::runtime_error("Object name not found in mapping");
        }
        return it->second;
    }

    /// @brief Формирует список соответствия между заданными тегами и их привязками в графе 
    /// для ребер заданного типа. Вызывается из более общей версии с таким же именем
    std::tuple<
        std::vector<graphlib::graph_place_binding_t>,
        std::vector<measurement_type>,
        std::vector<tag_info_t>
    > prepare_measurements_tags_and_bindings(graphlib::graph_binding_type edge_type) const
    {
        const std::vector<object_measurement_tags_t>& tags_list =
            edge_type == graphlib::graph_binding_type::Edge1
            ? tags1 : tags2;
        const std::vector<graphlib::edge_incidences_t>& edges_list =
            edge_type == graphlib::graph_binding_type::Edge1
            ? edges.first : edges.second;



        std::vector<graphlib::graph_place_binding_t> bindings;
        std::vector<measurement_type> types;
        std::vector<tag_info_t> tag_list;
        for (std::size_t edge = 0; edge < edges_list.size(); ++edge)
        {
            const object_measurement_tags_t& tags = tags_list[edge];

            for (const measurement_tag_info_t& tag_info : tags.in) {
                bindings.emplace_back(edge_type, edge, graphlib::graph_edge_side_t::Inlet);
                tag_list.emplace_back(tag_info.name, tag_info.dimension);
                types.push_back(tag_info.type);
            }

            for (const measurement_tag_info_t& tag_info : tags.out) {
                bindings.emplace_back(edge_type, edge, graphlib::graph_edge_side_t::Outlet);
                tag_list.emplace_back(tag_info.name, tag_info.dimension);
                types.push_back(tag_info.type);
            }
        }

        return std::make_tuple(
            std::move(bindings),
            std::move(types),
            std::move(tag_list)
        );
    }
    /// @brief Формирует список соответствия между заданными тегами и их привязками в графе
    /// @return Список привязок на графе; Список тегов
    std::tuple<
        std::vector<graphlib::graph_place_binding_t>,
        std::vector<measurement_type>,
        std::vector<tag_info_t>
    > prepare_measurements_tags_and_bindings() const
    {
        auto [measurements1, measurement_types1, measurement_tags1] =
            prepare_measurements_tags_and_bindings(graphlib::graph_binding_type::Edge1);
        auto [measurements2, measurement_types2, measurement_tags2] =
            prepare_measurements_tags_and_bindings(graphlib::graph_binding_type::Edge2);

        measurements1.insert(measurements1.end(), measurements2.begin(), measurements2.end());
        measurement_types1.insert(measurement_types1.end(), measurement_types2.begin(), measurement_types2.end());
        measurement_tags1.insert(measurement_tags1.end(), measurement_tags2.begin(), measurement_tags2.end());

        return std::make_tuple(
            std::move(measurements1),
            std::move(measurement_types1),
            std::move(measurement_tags1)
        );
    }
};



template <typename PipeParameters>
inline std::size_t make_pipes_uniform_profile(
    double desired_dx,
    std::vector<std::unique_ptr<model_parameters_t>>* properties)
{
    std::size_t result = 0;
    for (auto& parameters : *properties) {
        if (auto pipe = dynamic_cast<PipeParameters*>(parameters.get())) {
            pipe->make_uniform_profile(desired_dx);
            result++;
        }
    }

    return result;
}


template <typename PipeParameters>
inline void make_pipes_uniform_profile_handled(
    double desired_dx,
    std::vector<std::unique_ptr<model_parameters_t>>* properties)
{
    std::size_t pipes_processed = make_pipes_uniform_profile<PipeParameters>(desired_dx, properties);
    // TODO: Точно нужно? Схемы без труб полезны для тестирования (например, для контролов)
    /*if (pipes_processed == 0) {
        throw std::runtime_error("No pipes profile processed");
    }*/

}

/// @brief Парсит папку транспортной схемой 
/// Рассчитан на 4 типа транспортных объектов: поставщик, труба, сосредоточенный объект, насос
/// Насос парсится как сосредоточенный объект. На момент разработки кода полноценной тепловой модели насоса нет
class json_graph_parser_t {
    /// @brief Зачитка списка ребер (вершины и имя объекта) из файла
    static std::vector<graphlib::model_incidences_t<std::string>>
        load_topology(const std::string& filename);
    
    /// @brief Зачитка списка ребер (вершины и имя объекта) из JSON-строки
    static std::vector<graphlib::model_incidences_t<std::string>>
        load_topology_from_string(const std::string& json_string);

    /// @brief Зачитка свойств ребер с учетом специфики заданного пользователем типа сценария
    /// @param path Путь к директории с файлами свойств ребер
    /// @param problem_type Тип сценария
    /// @param data Переменная, куда будут записаны прочитанные свойства ребер
    static void load_edges_properties(
        const std::string& path, const qsm_problem_type& problem_type, graph_json_data& data);

    /// @brief Зачитка тегов свойств и переключений, сопоставленных ребрам графа
    /// @param path Путь к директории со свойствами ребер
    /// @param tags_filename Путь к файлу с тегами
    /// @param data Переменная, куда будут записаны прочитанные свойства ребер
    static void load_tags_controls_data(
        const std::string& path, const std::string& tags_filename, graph_json_data& data);

    /// @brief Зачитка списка свойств труб
    static std::unordered_map<std::string, pde_solvers::pipe_json_data> load_pipe_data(const std::string& filename);
    /// @brief Зачитка списка свойств притоков/отборов
    static std::unordered_map<std::string, source_json_data> load_source_data(const std::string& filename);
    /// @brief Зачитка списка свойств сосредоточенных объектов
    static std::unordered_map<std::string, lumped_json_data> load_lumped_data(const std::string& filename);
    /// @brief Зачитка списка свойств сосредоточенных объектов
    static std::unordered_map<std::string, valve_json_data> load_valve_data(const std::string& filename);

    static std::unordered_map<std::string, object_measurement_tags_t> load_tags_data(const std::string& tags_filename);
    static void extract_object_tags(
        const std::string& filename,
        std::unordered_map<std::string, object_measurement_tags_t>* tags);


    /// @brief Зачитка тегов переключений объектов. Для одного объекта - один тег.
    static std::unordered_map<std::string, control_tag_info_t> load_controls_data(const std::string& controls_filename);

    static void add_edge1_to_siso_data(
        const graphlib::model_incidences_t<std::string>& edge,
        const std::unordered_map<std::string, object_measurement_tags_t>& object_tags,
        const std::unordered_map<std::string, control_tag_info_t>& object_controls,
        graph_siso_data& result
    ) {
        result.name_to_binding[edge.model] =
            graphlib::graph_binding_t{ graphlib::graph_binding_type::Edge1, result.edges.first.size() };
        result.edges.first.push_back(edge);
        
        if (auto it = object_tags.find(edge.model); it != object_tags.end()) {
            result.tags1.push_back(it->second);
        }
        else {
            result.tags1.push_back(object_measurement_tags_t());
        }

        if (auto it = object_controls.find(edge.model); it != object_controls.end()) {
            result.controls1.push_back(it->second);
        }
    }

    static void add_edge2_to_siso_data(
        const graphlib::model_incidences_t<std::string>& edge,
        const std::unordered_map<std::string, object_measurement_tags_t>& object_tags,
        const std::unordered_map<std::string, control_tag_info_t>& object_controls,
        graph_siso_data& result
    ) {
        result.name_to_binding[edge.model] =
            graphlib::graph_binding_t{ graphlib::graph_binding_type::Edge2, result.edges.second.size() };
        result.edges.second.push_back(edge);
        
        if (auto it = object_tags.find(edge.model); it != object_tags.end()) {
            result.tags2.push_back(it->second);
        }
        else {
            result.tags2.push_back(object_measurement_tags_t());
        }

        if (auto it = object_controls.find(edge.model); it != object_controls.end()) {
            result.controls2.push_back(it->second);
        }
    }

    static void add_specific_props(
        const graphlib::model_incidences_t<std::string>& edge,
        bool is_single_sided,
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>* transport_objects,
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>* hydro_objects,
        graph_siso_data& result
    ) {
        if (transport_objects) {
            if (is_single_sided)
                result.transport_props.first.emplace_back(std::move((*transport_objects)[edge.model]));
            else
                result.transport_props.second.emplace_back(std::move((*transport_objects)[edge.model]));
        }
        if (hydro_objects) {
            if (is_single_sided)
                result.hydro_props.first.emplace_back(std::move((*hydro_objects)[edge.model]));
            else
                result.hydro_props.second.emplace_back(std::move((*hydro_objects)[edge.model]));
        }
    }

    /// @brief Заполняет общие поля graph_siso_data (edges, name_to_binding, tags, controls)
    /// и опционально заполняет transport_props и/или hydro_props
    /// @param edges Ребра графа помеченные текстовыми идентификаторами (именами)
    /// @param object_tags Теги измерений
    /// @param object_controls Теги переключений
    /// @param transport_objects Транспортные объекты (используются если переданы)
    /// @param hydro_objects Гидравлические объекты (используются если переданы)
    /// @return Заполненная структура graph_siso_data
    static graph_siso_data prepare_siso_graph_structure(
        const std::vector<graphlib::model_incidences_t<std::string>>& edges,
        const std::unordered_map<std::string, object_measurement_tags_t>& object_tags,
        const std::unordered_map<std::string, control_tag_info_t>& object_controls,
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>* transport_objects = nullptr,
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>* hydro_objects = nullptr
    )
    {
        graph_siso_data result;
        // раскидываем объекты по типам (односторонний/двусторонний) по текстовому идентификатору
        for (const auto& edge : edges) {
            bool is_single_sided = edge.is_single_sided_edge();
            
            if (is_single_sided)
                add_edge1_to_siso_data(edge, object_tags, object_controls, result);
            else
                add_edge2_to_siso_data(edge, object_tags, object_controls, result);

            // Заполняем специфичные свойства
            add_specific_props(edge, is_single_sided, transport_objects, hydro_objects, result);  
        }

        return result;
    }

public:
    /// @brief Преобразует данные графа из json-представления в представление данных, 
    /// удобное для создания SISO-графа транспортной задачи
    static graph_siso_data prepare_transport_siso_graph(
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>& transport_objects,
        const graph_json_data& json_data
        )
    {
        
        return prepare_siso_graph_structure(json_data.edges, json_data.object_tags, json_data.object_controls, 
            &transport_objects, nullptr);
    }

    /// @brief Преобразует данные графа из json-представления в представление данных, 
    /// удобное для создания SISO-графа гидравлической квазистационарной задачи
    static graph_siso_data prepare_hydro_siso_graph(
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>& hydro_objects,
        const graph_json_data& json_data
    )
    {
        return prepare_siso_graph_structure(json_data.edges, json_data.object_tags, json_data.object_controls, 
            nullptr, &hydro_objects);
    }

    /// @brief Преобразование зачитанных из json данных о топологии и свойствах объектов 
    /// с одновременным заполнением транспортных и гидравлических свойств
    static graph_siso_data prepare_hydro_transport_siso_graph(
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>& transport_objects,
        std::unordered_map<std::string, std::unique_ptr<model_parameters_t>>& hydro_objects,
        const graph_json_data& json_data
    )
    {
        return prepare_siso_graph_structure(json_data.edges, json_data.object_tags, json_data.object_controls, 
            &transport_objects, &hydro_objects);
    }

    /// @brief Зачитка всей информации по схеме из json-файлов
    /// Выполняются проверки целостности данных (у объектов разных типов имена не совпадают и все это совпадает с именами в ребрах)
    /// @param path Путь к схеме
    /// @param tags_filename Опциональный путь к файлу с тегами в той же папке
    static graph_json_data load_json_data(const std::string& path, const std::string& tags_filename, 
        const qsm_problem_type& problem_type);
    
    /// @brief Зачитка всей информации по схеме из JSON-строк
    /// Топология передается в формате graph_stat_66.json: {"имя_объекта": {"from": int, "to": int}}
    /// Свойства объектов передаются в формате object_params_95.json: {"valves": {...}, "pipes": {...}}
    /// @param topology_json JSON-строка с топологией графа
    /// @param objects_json JSON-строка со свойствами объектов (valves и pipes)
    /// @param problem_type Тип расчетного сценария
    static graph_json_data load_json_data_from_strings(
        const std::string& topology_json,
        const std::string& objects_json,
        const qsm_problem_type& problem_type = qsm_problem_type::Transport);
    
    /// @brief Зачитка свойств объектов из JSON-строки в формате object_params_95.json
    /// @param objects_json JSON-строка со свойствами объектов: {"valves": {...}, "pipes": {...}}
    /// @param problem_type Тип расчетного сценария
    /// @param data Переменная, куда будут записаны прочитанные свойства ребер
    static void load_edges_properties_from_string(
        const std::string& objects_json,
        const qsm_problem_type& problem_type,
        graph_json_data& data);
    
    /// @brief Зачитка списка свойств труб из JSON-строки
    static std::unordered_map<std::string, pde_solvers::pipe_json_data> 
        load_pipe_data_from_string(const std::string& json_string);
    
    /// @brief Зачитка списка свойств задвижек из JSON-строки
    static std::unordered_map<std::string, valve_json_data> 
        load_valve_data_from_string(const std::string& json_string);
    
    /// @brief Парсит данные из json-файлов для транспортной задачи
    /// @tparam TransportPipeParameters Тип параметров трубы для транспортного расчета
    template <typename TransportPipeParameters>
    static graph_siso_data parse_transport(const std::string& path, const std::string& tags_filename = "")
    {
        graph_json_data json_graph = load_json_data(path, tags_filename, qsm_problem_type::Transport);
        auto transport_objects = json_graph.get_transport_object_parameters<TransportPipeParameters>();
        graph_siso_data siso_graph = prepare_transport_siso_graph(transport_objects, json_graph);
        return siso_graph;
    }

    /// @brief Парсит данные из json-файлов для гидравлической задачи
    /// @tparam HydroPipeParameters Тип параметров трубы для гидравлического расчета
    template <typename HydroPipeParameters>
    static graph_siso_data parse_hydro(const std::string& path, const std::string& tags_filename = "")
    {
        graph_json_data json_graph = load_json_data(path, tags_filename, qsm_problem_type::Hydraulic);
        auto hydro_objects = json_graph.get_hydro_object_parameters<HydroPipeParameters>();
        graph_siso_data siso_graph = prepare_hydro_siso_graph(hydro_objects, json_graph);
        return siso_graph;
    }

    /// @brief Парсит данные из json-файлов для задач транспортного и гидравлического расчета
    /// @tparam TransportPipeParameters Тип параметров трубы для транспортного солвера
    /// @tparam HydroPipeParameters Тип параметров трубы для гидравлического солвера или совмещенного солвера
    /// @return (структура с транспортными параметрами объектов графа для НЕЗАВИСИМОГО транспортного расчета;
    /// структура с транспортными и гидравлическими параметрами объектов графа для совмещенного расчета)
    template <typename TransportPipeParameters, typename HydroPipeParameters>
    static std::pair<graph_siso_data, graph_siso_data> parse_hydro_transport(
        const std::string& path, const std::string& tags_filename = "")
    {
        graph_json_data json_graph = load_json_data(path, tags_filename, qsm_problem_type::HydroTransport);
        auto transport_objects = json_graph.get_transport_object_parameters<TransportPipeParameters>();
        auto hydro_objects = json_graph.get_hydro_object_parameters<HydroPipeParameters>();

        // Создаем транспортный граф для независимого транспортного расчета
        graph_siso_data transport_siso_graph = prepare_transport_siso_graph(transport_objects, json_graph);

        // Создаем граф для совмещенного расчета
        graph_siso_data hydro_transport_siso_graph = prepare_hydro_transport_siso_graph(transport_objects, hydro_objects, json_graph);

        return std::make_pair(std::move(transport_siso_graph), std::move(hydro_transport_siso_graph));
    }

    /// @brief Частный случай парсера для совмещенного солвера, выполняющего и транспортный, и гидравлический расчеты
    /// @tparam HydroTransportSolverPipeParameters Тип параметров трубы из совмещенного солвера
    template <typename HydroTransportSolverPipeParameters>
    static graph_siso_data parse_hydro_transport(
        const std::string& path, const std::string& tags_filename = "")
    {
        std::pair<graph_siso_data, graph_siso_data> result = parse_hydro_transport <
            HydroTransportSolverPipeParameters,
            HydroTransportSolverPipeParameters
        >(path, tags_filename);
        result.second.transport_props = std::move(result.first.transport_props);
        return std::move(result.second);
    }
    
    /// @brief Парсит данные из JSON-строк для независимого транспортного расчета
    /// @param topology_json JSON-строка с топологией (формат graph_stat_66.json)
    /// @param objects_json JSON-строка со свойствами объектов (формат object_params_95.json)
    /// @tparam TransportPipeParameters Тип параметров трубы для транспортного расчета
    template <typename TransportPipeParameters>
    static graph_siso_data parse_transport_from_strings(
        const std::string& topology_json,
        const std::string& objects_json)
    {
        graph_json_data json_graph = load_json_data_from_strings(topology_json, objects_json, qsm_problem_type::Transport);
        auto transport_objects = json_graph.get_transport_object_parameters<TransportPipeParameters>();
        graph_siso_data siso_graph = prepare_transport_siso_graph(transport_objects, json_graph);
        return siso_graph;
    }

    /// @brief Парсит данные из JSON-строк для независимого гидравлического расчета 
    /// @warning Нет обработки транспортных свойств
    /// @param topology_json JSON-строка с топологией (формат graph_stat_66.json)
    /// @param objects_json JSON-строка со свойствами объектов (формат object_params_95.json)
    /// @tparam HydroPipeParameters Тип параметров трубы для гидравлического расчета
    template <typename HydroPipeParameters>
    static graph_siso_data parse_hydro_from_strings(
        const std::string& topology_json,
        const std::string& objects_json)
    {
        graph_json_data json_graph = load_json_data_from_strings(topology_json, objects_json, qsm_problem_type::Hydraulic);
        auto hydro_objects = json_graph.get_hydro_object_parameters<HydroPipeParameters>();
        graph_siso_data siso_graph = prepare_hydro_siso_graph(hydro_objects, json_graph);
        return siso_graph;
    }

    /// @brief Парсит данные из JSON-строк для совмещенной гидравлической и транспортной задачи
    /// @param topology_json JSON-строка с топологией (формат graph_stat_66.json)
    /// @param objects_json JSON-строка со свойствами объектов (формат object_params_95.json)
    /// @tparam TransportPipeParameters Тип параметров трубы для транспортного расчета
    /// @tparam HydroPipeParameters Тип параметров трубы для гидравлического расчета
    /// @return Пара: граф для НЕЗАВИСИМОГО транспортного расчета;
    // граф для совмещенного квазистационарного расчета или независимого гидравлического расчета
    template <typename TransportPipeParameters, typename HydroPipeParameters>
    static std::pair<graph_siso_data, graph_siso_data> parse_hydro_transport_from_strings(
        const std::string& topology_json,
        const std::string& objects_json)
    {
        graph_json_data json_graph = load_json_data_from_strings(topology_json, objects_json, qsm_problem_type::HydroTransport);
        auto transport_objects = json_graph.get_transport_object_parameters<TransportPipeParameters>();
        auto hydro_objects = json_graph.get_hydro_object_parameters<HydroPipeParameters>();
        
        // Создаем транспортный граф
        graph_siso_data transport_siso_graph = prepare_transport_siso_graph(transport_objects, json_graph);
        
        // Создаем граф для совмещенного расчета
        graph_siso_data hydro_transport_siso_graph = prepare_hydro_transport_siso_graph(transport_objects, hydro_objects, json_graph);
        
        return std::make_pair(std::move(transport_siso_graph), std::move(hydro_transport_siso_graph));
    }

    /// @brief Частный случай парсера для совмещенного солвера, выполняющего и транспортный, и гидравлический расчеты
    /// @tparam HydroTransportSolverPipeParameters Тип параметров трубы из совмещенного солвера
    template <typename HydroTransportSolverPipeParameters>
    static graph_siso_data parse_hydro_transport_from_strings(
        const std::string& topology_json,
        const std::string& objects_json)
    {
        std::pair<graph_siso_data, graph_siso_data> result = parse_hydro_transport_from_strings<
            HydroTransportSolverPipeParameters,
            HydroTransportSolverPipeParameters
        >(topology_json, objects_json);
        result.second.transport_props = std::move(result.first.transport_props);
        return std::move(result.second);
    }
};

/// @brief Возвращает строковое представление типа транспортного объекта для сериализации
/// @tparam PipeSolver Тип солвера трубы
/// @return Строковое представление типа: "sources", "lumpeds" или "pipes"
template <typename PipeSolver>
inline std::string get_transport_object_type_string(
    const transport_graph_t& graph,
    graphlib::graph_binding_type binding_type,
    size_t edge_id)
{
    graphlib::graph_binding_t binding(binding_type, edge_id);
    auto* model = graph.get_model_incidences(binding).model;

    if (dynamic_cast<transport_source_model_t*>(model)) {
        return "sources";
    }
    else if (dynamic_cast<transport_lumped_model_t*>(model)) {
        return "lumpeds";
    }
    else if (dynamic_cast<transport_pipe_model_t<PipeSolver>*>(model)) {
        return "pipes";
    }
    // pumps, heaters, improvers пока не поддерживаются
    throw std::runtime_error("Unsupported transport model type");
}

}