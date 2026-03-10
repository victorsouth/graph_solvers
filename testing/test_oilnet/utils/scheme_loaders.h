#pragma once

#include "test_oilnet.h"

namespace oil_transport {
;

/// @brief Загружает данные схемы из JSON и формирует SISO граф для совмещенного гидравлического и транспортного расчета
/// @tparam SolverPipeParameters Тип параметров трубы для солвера
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @return Кортеж из JSON данных и SISO графа
template <
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
> 
inline std::tuple<graph_json_data, graph_siso_data> load_hydro_transport_siso_graph(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings)
{
    graph_json_data json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::HydroTransport);

    auto hparams = json_data.get_hydro_object_parameters<SolverPipeParameters>();
    auto tparams = json_data.get_transport_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data =
        json_graph_parser_t::prepare_hydro_transport_siso_graph(
            tparams, hparams, json_data);

    return { std::move(json_data), std::move(siso_data) };

}


/// @brief Зачитывает схему из JSON, строит транспортные и гидравлические модели и графы.
/// @tparam PipeSolver            Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters  Тип параметров трубы, используемый при построении моделей
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::endohydro_values_t,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    transport_graph_t,
    oil_transport::hydraulic_graph_t
> load_hydro_transport_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }

    std::tie(*json_data, *siso_data) = load_hydro_transport_siso_graph<SolverPipeParameters>(
        scheme_folder, tags_filename, settings
    );
    auto [models_with_buffers, transport_graph, hydro_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_hydro_transport(
            *siso_data, settings.structured_transport_solver.endogenous_options);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(transport_graph),
        std::move(hydro_graph));
}

/// @brief Зачитывает схему из JSON и строит транспортные модели и граф
/// @tparam PipeSolver Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters Тип параметров трубы, используемый при построении моделей
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @param json_data Указатель на структуру для сохранения JSON данных (опционально)
/// @param siso_data Указатель на структуру для сохранения SISO данных (опционально)
/// @return Кортеж из моделей с буферами и транспортного графа
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::transport_values,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    transport_graph_t
> load_transport_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const transport_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    *json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::Transport);

    auto tparams = json_data->get_transport_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }
    *siso_data =
        json_graph_parser_t::prepare_transport_siso_graph(tparams, *json_data);

    auto [models_with_buffers, transport_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_transport(
            *siso_data, settings.structured_transport_solver.endogenous_options);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(transport_graph)
    );
}


/// @brief Зачитывает схему из JSON и строит гидравлические модели и граф
/// @tparam PipeSolver Тип солвера для труб (по умолчанию qsm_pipe_transport_solver_t)
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
/// @tparam SolverPipeParameters Тип параметров трубы, используемый при построении моделей
/// @param scheme_folder Путь к папке со схемой
/// @param tags_filename Имя файла с тегами
/// @param settings Настройки транспортной задачи
/// @param json_data Указатель на структуру для сохранения JSON данных (опционально)
/// @param siso_data Указатель на структуру для сохранения SISO данных (опционально)
/// @return Кортеж из моделей с буферами и гидравлического графа
template <
    typename PipeSolver = qsm_pipe_transport_solver_t,
    typename LumpedCalcParametersType = oil_transport::endohydro_values_t,
    typename SolverPipeParameters = qsm_pipe_transport_parameters_t
>
inline std::tuple<
    graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
    oil_transport::hydraulic_graph_t
> load_hydro_scheme(
    const std::string& scheme_folder,
    const std::string& tags_filename,
    const hydro_task_settings& settings,
    graph_json_data* json_data = nullptr,
    graph_siso_data* siso_data = nullptr)
{
    graph_json_data json_data_carrier;
    if (json_data == nullptr) {
        json_data = &json_data_carrier;
    }
    *json_data = json_graph_parser_t::load_json_data(
        scheme_folder, tags_filename, qsm_problem_type::Hydraulic);

    auto hparams = json_data->get_hydro_object_parameters<SolverPipeParameters>();

    graph_siso_data siso_data_carrier;
    if (siso_data == nullptr) {
        siso_data = &siso_data_carrier;
    }
    *siso_data =
        json_graph_parser_t::prepare_hydro_siso_graph(hparams, *json_data);

    auto [models_with_buffers, hydro_graph] =
        graph_builder<PipeSolver, LumpedCalcParametersType>::build_hydro(*siso_data);

    return std::make_tuple(
        std::move(models_with_buffers),
        std::move(hydro_graph));
}

} 

namespace hydro_task_test_helpers {
;
/// @brief Подготавливает схему с трубой и измерения для тестов
/// @param solution_method Метод решения задачи
/// @param pipe_coordinate_step Шаг по координате для трубы (NaN означает использование значения по умолчанию из settings)
/// @param source_pressure Давление источника
/// @param density_measure Значение плотности для измерения
/// @param dummy_density Значение dummy плотности для измерения
/// @return Кортеж из настроек (settings), данных схемы (siso_data) и измерений (measurements)
inline std::tuple<oil_transport::hydro_transport_task_settings, oil_transport::graph_siso_data, std::vector<oil_transport::graph_quantity_value_t>> 
prepare_pipe_scheme_with_measurements(
    oil_transport::hydro_transport_solution_method solution_method,
    double pipe_coordinate_step,
    double source_pressure,
    double density_measure,
    double dummy_density) {
    
    using pipe_solver_type = pde_solvers::iso_nonbaro_pipe_solver_t;
    
    auto settings = oil_transport::hydro_transport_task_settings::get_default_settings();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    settings.solution_method = solution_method;
    if (std::isfinite(pipe_coordinate_step)) {
        settings.pipe_coordinate_step = pipe_coordinate_step;
    }

    auto siso_data =
        oil_transport::json_graph_parser_t::parse_hydro_transport<pipe_solver_type::pipe_parameters_type>(
            get_schemes_folder() + "hydro_pipe_test");

    // Формируем профили для гидравлических и транспортных свойств
    oil_transport::make_pipes_uniform_profile_handled<pipe_solver_type::pipe_parameters_type>(
        settings.pipe_coordinate_step, &siso_data.hydro_props.second);

    oil_transport::make_pipes_uniform_profile_handled<pipe_solver_type::pipe_parameters_type>(
        settings.pipe_coordinate_step, &siso_data.transport_props.second);

    auto& src_properties = siso_data.get_properties<oil_transport::qsm_hydro_source_parameters>("Src", oil_transport::qsm_problem_type::HydroTransport);
    src_properties.pressure = source_pressure;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    // Создаем измерения - dummy чтобы начальная недостоверная партия c усредненными свойствами не совпала с density_measure
    std::vector<oil_transport::graph_quantity_value_t> measurements{
        {siso_data.name_to_binding.at("Vlv1"), oil_transport::measurement_type::vol_flow_std, 1.0},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 1), oil_transport::measurement_type::density_std, density_measure},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 0), oil_transport::measurement_type::density_std, dummy_density},
    };

    return std::make_tuple(std::move(settings), std::move(siso_data), std::move(measurements));
}

/// @brief Подготавливает схему с задвижкой и измерения для тестов
/// @param solution_method Метод решения задачи (может быть не задан, если не используется)
/// @param source_pressure Давление источника
/// @param vol_flow_measure_value Значение расхода для измерения
/// @param vol_flow_measure_binding Привязка для измерения расхода (имя объекта, например "Vlv1" или "Src")
/// @param density_measure_value Значение плотности для измерения
/// @return Кортеж из настроек (settings), данных схемы (siso_data) и измерений (measurements)
inline std::tuple<oil_transport::hydro_transport_task_settings, oil_transport::graph_siso_data, std::vector<oil_transport::graph_quantity_value_t>> 
prepare_valve_scheme(
    std::optional<oil_transport::hydro_transport_solution_method> solution_method,
    double source_pressure,
    double vol_flow_measure_value,
    const std::string& vol_flow_measure_binding,
    double density_measure_value) {
    
    auto settings = oil_transport::hydro_transport_task_settings::get_default_settings();
    settings.structured_transport_solver.endogenous_options.density_std = true;
    if (solution_method.has_value()) {
        settings.solution_method = solution_method.value();
    }

    auto siso_data =
        oil_transport::json_graph_parser_t::parse_hydro_transport<oil_transport::qsm_pipe_transport_parameters_t>(
            get_schemes_folder() + "valve_control_test/scheme/");

    auto& src_properties = siso_data.get_properties<oil_transport::qsm_hydro_source_parameters>("Src", oil_transport::qsm_problem_type::HydroTransport);
    src_properties.pressure = source_pressure;
    src_properties.std_volflow = std::numeric_limits<double>::quiet_NaN();

    // Создаем измерения
    std::vector<oil_transport::graph_quantity_value_t> measurements{
        {siso_data.name_to_binding.at(vol_flow_measure_binding), oil_transport::measurement_type::vol_flow_std, vol_flow_measure_value},
        {graphlib::graph_binding_t(graphlib::graph_binding_type::Vertex, 1), oil_transport::measurement_type::density_std, density_measure_value},
    };

    return std::make_tuple(std::move(settings), std::move(siso_data), std::move(measurements));
}

}

/// @brief Генератор схем из цепочки объектов заданных типов для тестов
/// @tparam Types... Последовательность структур данных для объектов схемы
/// Пример: chain_scheme_generator_t<source_json_data, valve_json_data, pump_json_data, source_json_data>
template <typename... Types>
class chain_scheme_generator_t {
    
    // Проверка, что первый и последний типы - source_json_data
    using first_type = std::tuple_element_t<0, std::tuple<Types...>>;
    using last_type = std::tuple_element_t<sizeof...(Types) - 1, std::tuple<Types...>>;
    static_assert(std::is_same_v<first_type, oil_transport::source_json_data>, "chain_scheme_generator_t must start with source_json_data");
    static_assert(std::is_same_v<last_type, oil_transport::source_json_data>, "chain_scheme_generator_t must end with source_json_data");

public:
    /// @brief Тип кортежа для данных объектов
    using data_tuple_t = std::tuple<Types...>;
    
    /// @brief Генерирует JSON файлы для тестовой схемы. Предыдущие файлы топологии и параметров удаляются
    /// @param data Кортеж с данными объектов, соответствующий последовательности типов
    /// @param scheme_dir Имя директории для сохранения JSON файлов
    /// @param object_names Опциональный список имен объектов. Если не передан, используются имена по умолчанию (Src, Obj1, Obj2, ..., Snk)
    /// @details Создает директорию (если не существует) и генерирует:
    /// - topology.json - топология графа (цепочка объектов)
    /// - source_data.json, valve_data.json, pump_data.json и т.д. - данные объектов
    static void generate(const data_tuple_t& data, const std::string& scheme_dir,
                         const std::optional<std::vector<std::string>>& object_names = std::nullopt) 
    {
        // Проверяем размерность, если имена переданы
        check_predefined_names_size(object_names);

        // Создаем директорию, если не существует
        std::filesystem::create_directories(scheme_dir);

        // Удаляем json-файлы предыдущей схемы, которые будут заменены
        cleanup_scheme_files(scheme_dir);

        // Генерируем топологию
        generate_topology(scheme_dir, object_names);
        
        // Генерируем файлы данных для каждого типа
        generate_data_files(data, scheme_dir, object_names, std::make_index_sequence<sizeof...(Types)>{});
    }

private:
    /// @brief Удаляет topology.json и файлы данных объектов для новой схемы
    static void cleanup_scheme_files(const std::string& scheme_dir) {
        const std::string topology_path = scheme_dir + "/topology.json";
        if (std::filesystem::exists(topology_path)) {
            std::filesystem::remove(topology_path);
        }

        (cleanup_data_file_for_type<Types>(scheme_dir), ...);
    }

    /// @brief Удаляет файл данных для конкретного типа объекта, если он существует
    template <typename T>
    static void cleanup_data_file_for_type(const std::string& scheme_dir) {
        std::string filename = get_data_filename<T>();
        std::string full_path = scheme_dir + "/" + filename;
        if (std::filesystem::exists(full_path)) {
            std::filesystem::remove(full_path);
        }
    }

    /// @brief Проверяет размерность переданных имен объектов
    static void check_predefined_names_size(const std::optional<std::vector<std::string>>& object_names) {
        if (object_names.has_value()) {
            constexpr size_t expected_size = sizeof...(Types);
            if (object_names->size() != expected_size) {
                throw std::invalid_argument(
                    "chain_scheme_generator_t::generate: object_names size (" + std::to_string(object_names->size()) +
                    ") does not match tuple size (" + std::to_string(expected_size) + ")");
            }
        }
    }
    
    /// @brief Генерирует topology.json
    static void generate_topology(const std::string& scheme_dir,
                                  const std::optional<std::vector<std::string>>& object_names) {
        boost::property_tree::ptree topology;
        
        constexpr size_t num_objects = sizeof...(Types);
        size_t vertex_id = 0;
        
        // Первый объект (source) - только "to"
        std::string first_name = get_object_name(0, object_names);
        add_source_to_topology(topology, first_name, vertex_id);
        
        // Промежуточные объекты - "from" и "to"
        for (size_t i = 1; i < num_objects - 1; ++i) {
            std::string obj_name = get_object_name(i, object_names);
            vertex_id = add_edge_to_topology(topology, obj_name, vertex_id);
        }
        
        // Последний объект (source/sink) - только "from"
        std::string last_name = get_object_name(num_objects - 1, object_names);
        add_sink_to_topology(topology, last_name, vertex_id);
        
        // Записываем topology.json
        std::ofstream topology_file(scheme_dir + "/topology.json");
        boost::property_tree::write_json(topology_file, topology);
    }
    
    /// @brief Добавляет приток в топологию
    static void add_source_to_topology(boost::property_tree::ptree& topology, 
                                       const std::string& name, 
                                       size_t to_vertex_id) {
        boost::property_tree::ptree source_obj;
        source_obj.put("to", to_vertex_id);
        topology.add_child(name, source_obj);
    }
    
    /// @brief Добавляет промежуточное ребро в топологию
    static size_t add_edge_to_topology(boost::property_tree::ptree& topology,
                                       const std::string& name,
                                       size_t from_vertex_id) 
    {
        boost::property_tree::ptree edge_obj;
        edge_obj.put("from", from_vertex_id);
        size_t to_vertex_id = from_vertex_id + 1;
        edge_obj.put("to", to_vertex_id);
        topology.add_child(name, edge_obj);
        return to_vertex_id;
    }
    
    /// @brief Добавляет отбор в топологию
    static void add_sink_to_topology(boost::property_tree::ptree& topology,
                                     const std::string& name,
                                     size_t from_vertex_id) 
    {
        boost::property_tree::ptree sink_obj;
        sink_obj.put("from", from_vertex_id);
        topology.add_child(name, sink_obj);
    }
    
    /// @brief Генерирует имя объекта по индексу
    /// @param index Индекс объекта в цепочке
    /// @param object_names Опциональный список имен объектов
    /// @return Имя объекта
    static std::string get_object_name(size_t index, const std::optional<std::vector<std::string>>& object_names) {
        if (object_names.has_value()) {
            return (*object_names)[index];
        }
        
        // Используем автоматическую генерацию имен
        constexpr size_t num_objects = sizeof...(Types);
        if (index == 0) {
            return "Src";
        }
        else if (index == num_objects - 1) {
            return "Snk";
        }
        else {
            // Промежуточные объекты: Obj1, Obj2, ...
            return "Obj" + std::to_string(index);
        }
    }
    
    /// @brief Генерирует файлы данных для объектов
    template <size_t... Indices>
    static void generate_data_files(const data_tuple_t& data, const std::string& scheme_dir,
                                    const std::optional<std::vector<std::string>>& object_names,
                                    std::index_sequence<Indices...>)
    {
        (generate_single_data_file<Indices>(data, scheme_dir, object_names), ...);
    }
    
    /// @brief Генерирует файл данных для одного объекта
    template <size_t Index>
    static void generate_single_data_file(const data_tuple_t& data, const std::string& scheme_dir,
                                          const std::optional<std::vector<std::string>>& object_names) {
        using object_type = std::tuple_element_t<Index, data_tuple_t>;
        const auto& obj_data = std::get<Index>(data);
        std::string obj_name = get_object_name(Index, object_names);
        std::string filename = get_data_filename<object_type>();
        
        // Читаем существующий файл или создаем новый
        boost::property_tree::ptree root;
        std::string full_path = scheme_dir + "/" + filename;
        if (std::filesystem::exists(full_path)) {
            boost::property_tree::read_json(full_path, root);
        }
        
        // Добавляем данные объекта
        boost::property_tree::ptree obj_node = serialize_object_to_ptree(obj_data);
        root.add_child(obj_name, obj_node);
        
        // Записываем файл
        std::ofstream file(full_path);
        boost::property_tree::write_json(file, root);
    }
    
    /// @brief Определяет имя файла данных по типу объекта
    template <typename T>
    static std::string get_data_filename() {
        if constexpr (std::is_same_v<T, oil_transport::source_json_data>) {
            return "source_data.json";
        }
        else if constexpr (std::is_same_v<T, oil_transport::valve_json_data>) {
            return "valve_data.json";
        }
        else if constexpr (std::is_same_v<T, oil_transport::pump_json_data>) {
            return "pump_data.json";
        }
        else if constexpr (std::is_same_v<T, oil_transport::check_valve_json_data>) {
            return "check_valve_data.json";
        }
        else if constexpr (std::is_same_v<T, oil_transport::lumped_json_data>) {
            return "lumped_data.json";
        }
        else {
            throw std::runtime_error("Unknown JSON data type: " + std::string(typeid(T).name()));
        }
    }

    /// @brief Сериализует объект в ptree используя рефлексию
    template <typename T>
    static boost::property_tree::ptree serialize_object_to_ptree(const T& obj) {
        boost::property_tree::ptree pt;
        constexpr auto field_names = oil_transport::get_field_names<T>();
        
        boost::pfr::for_each_field(obj, [&](const auto& field, std::size_t index) {
            std::string_view field_name = field_names[index];
            serialize_field_to_ptree(field, field_name, pt);
        });
        
        return pt;
    }
    
    /// @brief Сериализует поле в ptree
    template <typename FieldType>
    static void serialize_field_to_ptree(const FieldType& field, std::string_view field_name, boost::property_tree::ptree& pt) {
        if constexpr (std::is_same_v<FieldType, std::vector<double>>) {
            write_array(field, field_name, pt);
        }
        else if constexpr (std::is_same_v<FieldType, fixed_solvers::nullable_bool_t>) {
            pt.put(std::string(field_name), fixed_solvers::nullable_bool_to_str(field));
        }
        else if constexpr (std::is_same_v<FieldType, bool>) {
            pt.put(std::string(field_name), field);
        }
        else if constexpr (std::is_same_v<FieldType, double>) {
            if (std::isfinite(field)) {
                pt.put(std::string(field_name), field);
            }
            // NaN поля не записываются
        }
        else {
            pt.put(std::string(field_name), field);
        }
    }
    
    /// @brief Записывает массив чисел в ptree
    static void write_array(const std::vector<double>& field, std::string_view field_name, boost::property_tree::ptree& pt) {
        boost::property_tree::ptree array_node;
        for (double val : field) {
            boost::property_tree::ptree item;
            item.put("", val);
            array_node.push_back(std::make_pair("", item));
        }
        pt.add_child(std::string(field_name), array_node);
    }
    
};

