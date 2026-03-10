#pragma once

namespace oil_transport {
;

/// @brief Класс для чтения результатов расчета по слоям из JSON
class step_results_json_reader {
public:
private:
    /// @brief Обрабатывает различные варианты записи достоверности - в виде double или bool
    static bool parse_confidence_scalar(const boost::property_tree::ptree& node, const std::string& key)
    {
        // 1) scalar: "true"/"false"/"0"/"1"/"0.0"/"1.0"
        if (auto v = node.get_optional<std::string>(key)) {
            std::string s = *v;
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (s == "true") return true;
            if (s == "false") return false;
            return std::stod(s) > 0.0;
        }

        // 2) array: ["0","0",...] — берём первый элемент
        if (auto child = node.get_child_optional(key)) {
            for (const auto& [k, v] : *child) {
                (void)k;
                std::string s = v.get_value<std::string>();
                std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (s == "true") return true;
                if (s == "false") return false;
                return std::stod(s) > 0.0;
            }
        }
        return false;
    }

public:
    /// @brief Определяет количество слоев в JSON структуре
    /// @param input_stream Входной поток с JSON данными
    static size_t get_layers_count(std::istream& input_stream) {
        boost::property_tree::ptree root;
        try {
            boost::property_tree::read_json(input_stream, root);
        }
        catch (const boost::property_tree::json_parser_error& e) {
            throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
        }

        // Проверяем наличие ключа "layers"
        auto layers_optional = root.get_child_optional("layers");
        if (!layers_optional) {
            throw std::runtime_error("JSON structure does not contain 'layers' key");
        }

        const auto& layers_node = *layers_optional;

        // Подсчитываем количество элементов в массиве layers
        size_t count = 0;
        for (const auto& [key, value] : layers_node) {
            (void)key; // подавляем предупреждение о неиспользуемой переменной
            (void)value;
            ++count;
        }

        return count;
    }

    /// @brief Получает узел слоя по индексу из JSON потока
    /// @param input_stream Входной поток с JSON данными
    /// @param layer_index Индекс слоя (0-based)
    /// @return Узел слоя
    static boost::property_tree::ptree get_layer_node(std::istream& input_stream, size_t layer_index) {
        boost::property_tree::ptree root;
        try {
            boost::property_tree::read_json(input_stream, root);
        }
        catch (const boost::property_tree::json_parser_error& e) {
            throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
        }

        auto layers_optional = root.get_child_optional("layers");
        if (!layers_optional) {
            throw std::runtime_error("JSON structure does not contain 'layers' key");
        }

        const auto& layers_node = *layers_optional;

        size_t index = 0;
        for (const auto& [key, value] : layers_node) {
            if (index == layer_index) {
                return value;
            }
            ++index;
        }

        throw std::runtime_error("Layer index " + std::to_string(layer_index) + " is out of range");
    }

    /// @brief Получает узел слоя по индексу из уже прочитанного JSON дерева
    static boost::property_tree::ptree get_layer_node_from_tree(
        const boost::property_tree::ptree& root, size_t layer_index) {
        auto layers_optional = root.get_child_optional("layers");
        if (!layers_optional) {
            throw std::runtime_error("JSON structure does not contain 'layers' key");
        }

        const auto& layers_node = *layers_optional;

        size_t index = 0;
        for (const auto& [key, value] : layers_node) {
            if (index == layer_index) {
                return value;
            }
            ++index;
        }

        throw std::runtime_error("Layer index " + std::to_string(layer_index) + " is out of range");
    }

    /// @brief Парсит профиль с достоверностью из JSON узла
    /// @param profiles_node Узел с профилями
    /// @param name Имя профиля
    /// @return Пара значений и достоверности
    static std::pair<std::vector<double>, std::vector<double>> parse_profile_with_confidence(
        const boost::property_tree::ptree& profiles_node, const std::string& name) {
        std::vector<double> values;
        std::vector<double> confidence;

        auto value_node = profiles_node.get_child_optional(name);
        if (value_node) {
            for (const auto& [key, val] : *value_node) {
                values.push_back(std::stod(val.get_value<std::string>()));
            }
        }

        auto confidence_node = profiles_node.get_child_optional(name + "_confidence");
        if (confidence_node) {
            for (const auto& [key, val] : *confidence_node) {
                confidence.push_back(std::stod(val.get_value<std::string>()));
            }
        }

        return { values, confidence };
    }

    /// @brief Парсит параметры источника/сосредоточенного объекта из JSON узла
    /// @return Структура endohydro_values_t
    static oil_transport::endohydro_values_t parse_parameters(
        const boost::property_tree::ptree& params_node) {
        oil_transport::endohydro_values_t result;

        auto density_std_opt = params_node.get_optional<std::string>("density_std");
        if (density_std_opt) {
            result.density_std.value = std::stod(*density_std_opt);
        }

        result.density_std.confidence = parse_confidence_scalar(params_node, "density_std_confidence");

        auto viscosity_working_opt = params_node.get_optional<std::string>("viscosity_working");
        if (viscosity_working_opt) {
            result.viscosity_working.value = std::stod(*viscosity_working_opt);
        }

        result.viscosity_working.confidence = parse_confidence_scalar(params_node, "viscosity_working_confidence");

        auto pressure_opt = params_node.get_optional<std::string>("pressure");
        if (pressure_opt) {
            result.pressure = std::stod(*pressure_opt);
        }

        auto mass_flow_opt = params_node.get_optional<std::string>("mass_flow");
        if (mass_flow_opt) {
            result.mass_flow = std::stod(*mass_flow_opt);
        }

        return result;
    }

    /// @brief Парсит координаты трубы из JSON узла
    /// @param pipe_node Узел с данными трубы
    /// @return Вектор координат
    static std::vector<double> parse_coordinates(const boost::property_tree::ptree& pipe_node) {
        std::vector<double> coordinates;
        auto coords_node = pipe_node.get_child_optional("coordinates");
        if (coords_node) {
            for (const auto& [key, val] : *coords_node) {
                coordinates.push_back(std::stod(val.get_value<std::string>()));
            }
        }
        return coordinates;
    }

    /// @brief Парсит слой трубы из JSON узла
    /// @tparam PipeSolver Тип солвера трубы
    /// @param pipe_node Узел с данными трубы
    /// @param coordinates Координаты трубы
    /// @return Слой трубы
    template <typename PipeSolver>
    static typename PipeSolver::layer_type parse_pipe_layer(
        const boost::property_tree::ptree& pipe_node,
        const std::vector<double>& coordinates) {
        typename PipeSolver::layer_type layer(coordinates.size());

        auto std_volflow_opt = pipe_node.template get_optional<std::string>("std_volumetric_flow");
        if (std_volflow_opt) {
            layer.std_volumetric_flow = std::stod(*std_volflow_opt);
        }

        auto profiles_node = pipe_node.get_child_optional("profiles");
        if (profiles_node) {
            if constexpr (std::is_same_v<typename PipeSolver::layer_type, pde_solvers::iso_nonbaro_pipe_layer_t>) {
                auto [density_values, density_confidence] = parse_profile_with_confidence(*profiles_node, "density_std");
                if (!density_values.empty()) {
                    layer.density_std.value = density_values;
                    if (!density_confidence.empty()) {
                        layer.density_std.confidence = density_confidence;
                    }
                }

                auto pressure_node = profiles_node->get_child_optional("pressure");
                if (pressure_node) {
                    layer.pressure.clear();
                    for (const auto& [key, val] : *pressure_node) {
                        layer.pressure.push_back(std::stod(val.template get_value<std::string>()));
                    }
                }
            }
        }

        return layer;
    }
};


/// @brief Тип для хранения сохраненных расчетных слоев труб
template <typename PipeSolver>
using stored_pipe_layers_t = std::map<
    size_t,  // момент времени
    std::map<
    std::string,  // идентификатор трубы (имя из topology.json)
    typename PipeSolver::layer_type  // данные расчетного слоя трубы
    >
>;

/// @brief Класс для хранения истории результатов расчета
template <typename PipeSolver>
class historical_layers_storage_t {
private:
    /// @brief Хранилище данных расчетных слоев труб
    stored_pipe_layers_t<PipeSolver> pipes_results;
    /// @brief Хранилище профилей координат труб (хранится один раз для каждой трубы)
    std::map<std::string, std::vector<double>> pipes_coordinates;
    /// @brief Хранилище расчетных значений для sources (односторонние объекты)
    std::map<size_t, std::map<std::string, oil_transport::endohydro_values_t>> sources_results;
    /// @brief Хранилище расчетных значений для двусторонних сосредоточенных объектов (lumpeds)
    std::map<size_t, std::map<std::string, std::array<oil_transport::endohydro_values_t, 2>>> lumpeds_results;

    /// @brief Получает имя трубы из маппинга по edge_id
    /// @param edge_id Идентификатор ребра
    /// @param edge_id_to_object_name Маппинг от привязки к ребру к имени объекта
    /// @return Имя трубы
    std::string get_pipe_name(
        size_t edge_id,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name) const
    {
        graphlib::graph_binding_t pipe_binding(graphlib::graph_binding_type::Edge2, edge_id);
        auto name_it = edge_id_to_object_name.find(pipe_binding);
        if (name_it == edge_id_to_object_name.end()) {
            throw std::runtime_error("Pipe name not found for edge_id " + std::to_string(edge_id));
        }
        return name_it->second;
    }

    /// @brief Сохраняет профиль координат трубы, если он еще не сохранен
    /// @param pipe_name Имя трубы
    /// @param coordinates Указатель на координаты
    void save_pipe_coordinates_if_needed(
        const std::string& pipe_name,
        const std::vector<double>* coordinates)
    {
        if (pipes_coordinates.find(pipe_name) == pipes_coordinates.end()) {
            pipes_coordinates[pipe_name] = *coordinates;
        }
    }

    /// @brief Конвертирует слой от qsm_pipe_transport_solver_t к iso_nonbaro_pipe_solver_t
    /// @param qsm_layer Исходный слой от qsm_pipe_transport_solver_t
    /// @param point_count Количество точек в профиле
    /// @return Конвертированный слой для iso_nonbaro_pipe_solver_t
    static typename PipeSolver::layer_type convert_qsm_layer_to_condensate_layer(
        const pde_solvers::pipe_endogenous_calc_layer_t& qsm_layer,
        size_t point_count)
    {
        typename PipeSolver::layer_type condensate_layer(point_count);

        // Копируем расход
        condensate_layer.std_volumetric_flow = qsm_layer.std_volumetric_flow;

        // Копируем плотность (оба используют confident_layer_t)
        condensate_layer.density_std.value = qsm_layer.density_std.value;
        condensate_layer.density_std.confidence = qsm_layer.density_std.confidence;

        // Копируем вспомогательные данные для метода конечных объемов
        condensate_layer.specific = qsm_layer.specific;

        // Давление не копируем, так как оно будет рассчитано позже в гидравлическом расчете
        condensate_layer.pressure.assign(point_count, std::numeric_limits<double>::quiet_NaN());

        return condensate_layer;
    }

    /// @brief Сохраняет расчетные слои в трубах для заданного момента времени
    /// @param time_step Момент времени, для которого сохраняются данные
    /// @param result Результаты расчета, полученные из get_result()
    /// @param edge_id_to_object_name Маппинг от привязки к ребру к имени объекта из topology.json (включая трубы)
    void save_pipes(
        size_t time_step,
        const oil_transport::task_common_result_t<PipeSolver, oil_transport::endohydro_values_t>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        auto& time_data = pipes_results[time_step];

        for (const auto& [pipe_edge_id, pipe_result_ptr] : result.pipes) {
            if (pipe_result_ptr == nullptr) {
                throw std::runtime_error("Null pipe result pointer for edge_id " + std::to_string(pipe_edge_id));
            }

            std::string pipe_name = get_pipe_name(pipe_edge_id, edge_id_to_object_name);

            const oil_transport::pipe_common_result_t<PipeSolver>& pipe_result = *pipe_result_ptr;
            if (pipe_result.coordinates == nullptr || pipe_result.layer == nullptr) {
                throw std::runtime_error("Null coordinates or layer for pipe '" + pipe_name + "'");
            }

            save_pipe_coordinates_if_needed(pipe_name, pipe_result.coordinates);
            time_data.insert({ pipe_name, *(pipe_result.layer) });
        }
    }

    /// @brief Получает имя источника из маппинга по edge_id
    std::string get_source_name(
        size_t edge_id,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name) const
    {
        graphlib::graph_binding_t source_binding(graphlib::graph_binding_type::Edge1, edge_id);
        auto name_it = edge_id_to_object_name.find(source_binding);
        if (name_it == edge_id_to_object_name.end()) {
            throw std::runtime_error("Source name not found for edge_id " + std::to_string(edge_id));
        }
        return name_it->second;
    }

    /// @brief Сохраняет расчетные значения для edge1
    void save_sources(
        size_t time_step,
        const oil_transport::task_common_result_t<PipeSolver, oil_transport::endohydro_values_t>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        auto& sources_at_time = sources_results[time_step];
        for (const auto& [edge_id, values_ptr] : result.sources) {
            if (values_ptr == nullptr) {
                throw std::runtime_error("Null source values pointer for edge_id " + std::to_string(edge_id));
            }

            std::string source_name = get_source_name(edge_id, edge_id_to_object_name);
            sources_at_time[source_name] = *values_ptr;
        }
    }

    /// @brief Получает имя сосредоточенного объекта из маппинга по edge_id
    std::string get_lumped_name(
        size_t edge_id,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name) const
    {
        graphlib::graph_binding_t lumped_binding(graphlib::graph_binding_type::Edge2, edge_id);
        auto name_it = edge_id_to_object_name.find(lumped_binding);
        if (name_it == edge_id_to_object_name.end()) {
            throw std::runtime_error("Lumped name not found for edge_id " + std::to_string(edge_id));
        }
        return name_it->second;
    }

    /// @brief Сохраняет расчетные значения для сосредоточенных объектов
    void save_lumpeds(
        size_t time_step,
        const oil_transport::task_common_result_t<PipeSolver, oil_transport::endohydro_values_t>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        auto& lumpeds_at_time = lumpeds_results[time_step];
        for (const auto& [edge_id, values_array] : result.lumpeds) {
            std::string lumped_name = get_lumped_name(edge_id, edge_id_to_object_name);
            lumpeds_at_time[lumped_name] = { *values_array[0], *values_array[1] };
        }
    }

    /// @brief Возвращает расчетные значения для ребер1 на заданный момент времени
    const std::map<std::string, oil_transport::endohydro_values_t>& get_sources_at_time(size_t time_step) const {
        auto it = sources_results.find(time_step);
        if (it == sources_results.end()) {
            static const std::map<std::string, oil_transport::endohydro_values_t> empty_map;
            return empty_map;
        }
        return it->second;
    }

    /// @brief Возвращает расчетные значения для сосредоточенных ребер2 для заданного момента времени
    const std::map<std::string, std::array<oil_transport::endohydro_values_t, 2>>& get_lumpeds_at_time(size_t time_step) const {
        auto it = lumpeds_results.find(time_step);
        if (it == lumpeds_results.end()) {
            static const std::map<std::string, std::array<oil_transport::endohydro_values_t, 2>> empty_map;
            return empty_map;
        }
        return it->second;
    }

    /// @brief Проверяет соответствие текущей сетки координат трубы сетке, на которой сохранены расчетные результаты 
    void validate_coordinate_grid(
        const std::string& pipe_name,
        const std::vector<double>& pipe_coordinates,
        size_t time_step) const
    {
        auto coordinates_it = pipes_coordinates.find(pipe_name);
        if (coordinates_it == pipes_coordinates.end()) {
            throw std::runtime_error(
                "Coordinate grid not found for pipe '" + pipe_name +
                "' at time step " + std::to_string(time_step));
        }

        const auto& history_coordinates = coordinates_it->second;
        if (pipe_coordinates.size() != history_coordinates.size()) {
            throw std::runtime_error(
                "Coordinate grid mismatch for pipe '" + pipe_name +
                "' at time step " + std::to_string(time_step) +
                ": pipe has " + std::to_string(pipe_coordinates.size()) +
                " points, history has " + std::to_string(history_coordinates.size()) + " points");
        }
    }

    /// @brief Записывает слой в буфер трубы
    void write_layer_to_pipe_buffer(
        oil_transport::hydraulic_model_t* model,
        const typename PipeSolver::layer_type& layer)
    {
        void* buffer_pointer = model->get_edge_buffer();
        if (buffer_pointer == nullptr) {
            throw std::runtime_error("Null buffer pointer for pipe model");
        }

        using buffer_type = typename PipeSolver::buffer_type;
        auto* pipe_buffer = reinterpret_cast<buffer_type*>(buffer_pointer);
        pipe_buffer->current() = layer;
    }

    /// @brief Восстанавливает  из истории текущие слои в трубах гидравлического графа
    void restore_pipes(
        oil_transport::hydraulic_graph_t& hydro_graph,
        size_t time_step,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& hydro_edge_id_to_object_name)
    {
        auto time_data_it = pipes_results.find(time_step);
        if (time_data_it == pipes_results.end()) {
            throw std::runtime_error("No pipe data found for time step " + std::to_string(time_step));
        }
        const auto& time_data = time_data_it->second;

        auto edge_bindings = hydro_graph.get_edge_binding_list();

        for (const auto& edge_binding : edge_bindings) {
            if (edge_binding.binding_type != graphlib::graph_binding_type::Edge2) {
                continue;
            }

            auto& edge = hydro_graph.get_model_incidences(edge_binding);
            oil_transport::hydraulic_model_t* model = edge.model;

            auto* pipe_model = dynamic_cast<oil_transport::hydraulic_pipe_model_t<PipeSolver>*>(model);
            if (pipe_model == nullptr) {
                continue;
            }

            graphlib::graph_binding_t pipe_binding(edge_binding.binding_type, edge_binding.edge);
            auto name_it = hydro_edge_id_to_object_name.find(pipe_binding);
            if (name_it == hydro_edge_id_to_object_name.end()) {
                throw std::runtime_error("Pipe name not found in mapping for edge_id " + std::to_string(edge_binding.edge));
            }
            const std::string& pipe_name = name_it->second;

            auto pipe_data_it = time_data.find(pipe_name);
            if (pipe_data_it == time_data.end()) {
                throw std::runtime_error("No pipe data found for '" + pipe_name + "' at time step " + std::to_string(time_step));
            }
            const typename PipeSolver::layer_type& layer = pipe_data_it->second;

            const auto& pipe_coordinates = pipe_model->parameters.profile.get_coordinates();
            validate_coordinate_grid(pipe_name, pipe_coordinates, time_step);

            write_layer_to_pipe_buffer(model, layer);
        }
    }

    /// @brief Записывает значения в буфер источника
    void write_values_to_source_buffer(
        oil_transport::hydraulic_model_t* model,
        const oil_transport::endohydro_values_t& values)
    {
        void* buffer_pointer = model->get_edge_buffer();
        if (buffer_pointer == nullptr) {
            throw std::runtime_error("Null buffer pointer for source model");
        }

        auto* source_model = dynamic_cast<oil_transport::qsm_hydro_source_model_t*>(model);
        if (source_model == nullptr) {
            throw std::runtime_error("Model is not a source model");
        }

        auto* buffer = reinterpret_cast<oil_transport::endohydro_values_t*>(buffer_pointer);
        static_cast<pde_solvers::endogenous_values_t&>(*buffer) = values;
    }

    /// @brief Восстанавливает эндогенные свойста и расход в буферах потребителей из истории
    void restore_sources(
        oil_transport::hydraulic_graph_t& hydro_graph,
        size_t time_step,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& hydro_edge_id_to_object_name)
    {
        const auto& sources_time = get_sources_at_time(time_step);
        if (sources_time.empty()) {
            throw std::runtime_error("No source data found for time step " + std::to_string(time_step));
        }

        auto edge_bindings = hydro_graph.get_edge_binding_list();

        for (const auto& edge_binding : edge_bindings) {
            if (edge_binding.binding_type != graphlib::graph_binding_type::Edge1) {
                continue;
            }

            graphlib::graph_binding_t source_binding(edge_binding.binding_type, edge_binding.edge);
            auto name_it = hydro_edge_id_to_object_name.find(source_binding);
            if (name_it == hydro_edge_id_to_object_name.end()) {
                throw std::runtime_error("Source name not found in mapping for edge_id " + std::to_string(edge_binding.edge));
            }

            const std::string& obj_name = name_it->second;
            auto src_it = sources_time.find(obj_name);
            if (src_it == sources_time.end()) {
                throw std::runtime_error("No source data found for '" + obj_name + "' at time step " + std::to_string(time_step));
            }

            auto& edge = hydro_graph.get_model_incidences(edge_binding);
            oil_transport::hydraulic_model_t* model = edge.model;
            write_values_to_source_buffer(model, src_it->second);
        }
    }

    /// @brief Записывает значения в буферы сосредоточенного объекта
    void write_values_to_lumped_buffer(
        oil_transport::hydraulic_model_t* model,
        const std::array<oil_transport::endohydro_values_t, 2>& values)
    {
        void* buffer_pointer = model->get_edge_buffer();
        if (buffer_pointer == nullptr) {
            throw std::runtime_error("Null buffer pointer for lumped model");
        }

        auto* lumped_model = dynamic_cast<oil_transport::qsm_hydro_local_resistance_model_t*>(model);
        if (lumped_model == nullptr) {
            throw std::runtime_error("Model is not a lumped model");
        }

        auto* buffers_for_getter = reinterpret_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(buffer_pointer);
        static_cast<pde_solvers::endogenous_values_t&>(*(buffers_for_getter->at(0))) = values[0];
        static_cast<pde_solvers::endogenous_values_t&>(*(buffers_for_getter->at(1))) = values[1];
    }

    /// @brief Восстанавливает эндогенные свойста и расход в буферы сосредточенных ребер из истории
    void restore_lumpeds(
        oil_transport::hydraulic_graph_t& hydro_graph,
        size_t time_step,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& hydro_edge_id_to_object_name)
    {
        const auto& lumpeds_time = get_lumpeds_at_time(time_step);
        if (lumpeds_time.empty()) {
            return;
        }

        auto edge_bindings = hydro_graph.get_edge_binding_list();

        for (const auto& edge_binding : edge_bindings) {
            if (edge_binding.binding_type != graphlib::graph_binding_type::Edge2) {
                continue;
            }

            auto& edge = hydro_graph.get_model_incidences(edge_binding);
            oil_transport::hydraulic_model_t* model = edge.model;
            if (auto pipe_model = dynamic_cast<oil_transport::hydraulic_pipe_model_t<PipeSolver>*>(model)) {
                // Труба - не сосредоточенный объект
                continue;
            }

            graphlib::graph_binding_t lumped_binding(edge_binding.binding_type, edge_binding.edge);
            auto name_it = hydro_edge_id_to_object_name.find(lumped_binding);
            if (name_it == hydro_edge_id_to_object_name.end()) {
                throw std::runtime_error("Lumped name not found in mapping for edge_id " + std::to_string(edge_binding.edge));
            }

            const std::string& obj_name = name_it->second;
            auto lump_it = lumpeds_time.find(obj_name);
            if (lump_it == lumpeds_time.end()) {
                throw std::runtime_error("No lumped data found for '" + obj_name + "' at time step " + std::to_string(time_step));
            }

            write_values_to_lumped_buffer(model, lump_it->second);
        }
    }

public:
    /// @brief Сохраняет все расчетные результаты (pipes, sources, lumpeds) для заданного момента времени
    void save_results(
        size_t time_step,
        const oil_transport::task_common_result_t<PipeSolver, oil_transport::endohydro_values_t>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        save_pipes(time_step, result, edge_id_to_object_name);
        save_sources(time_step, result, edge_id_to_object_name);
        save_lumpeds(time_step, result, edge_id_to_object_name);
    }

    /// @brief Перегрузка для сохранения результатов от qsm_pipe_transport_solver_t
    /// Конвертирует слои от qsm_pipe_transport_solver_t к iso_nonbaro_pipe_solver_t
    /// @param time_step Момент времени, для которого сохраняются данные
    /// @param result Результаты расчета с qsm_pipe_transport_solver_t
    /// @param edge_id_to_object_name Маппинг от привязки к ребру к имени объекта из topology.json
    template <typename LumpedCalcParametersType>
    void save_results(
        size_t time_step,
        const oil_transport::task_common_result_t<oil_transport::qsm_pipe_transport_solver_t, LumpedCalcParametersType>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        // Сохраняем источники и сосредоточенные объекты (они не зависят от типа солвера)
        save_sources_from_qsm_result(time_step, result, edge_id_to_object_name);
        save_lumpeds_from_qsm_result(time_step, result, edge_id_to_object_name);

        // Сохраняем трубы с конвертацией слоев
        auto& time_data = pipes_results[time_step];

        for (const auto& [pipe_edge_id, pipe_result_ptr] : result.pipes) {
            if (pipe_result_ptr == nullptr) {
                throw std::runtime_error("Null pipe result pointer for edge_id " + std::to_string(pipe_edge_id));
            }

            std::string pipe_name = get_pipe_name(pipe_edge_id, edge_id_to_object_name);

            const oil_transport::pipe_common_result_t<oil_transport::qsm_pipe_transport_solver_t>& pipe_result = *pipe_result_ptr;
            if (pipe_result.coordinates == nullptr || pipe_result.layer == nullptr) {
                throw std::runtime_error("Null coordinates or layer for pipe '" + pipe_name + "'");
            }

            save_pipe_coordinates_if_needed(pipe_name, pipe_result.coordinates);

            // Конвертируем слой от qsm_pipe_transport_solver_t к iso_nonbaro_pipe_solver_t
            size_t point_count = pipe_result.coordinates->size();
            typename PipeSolver::layer_type condensate_layer = convert_qsm_layer_to_condensate_layer(
                *(pipe_result.layer), point_count);

            time_data.insert({ pipe_name, std::move(condensate_layer) });
        }
    }

private:
    /// @brief Сохраняет источники из результатов qsm_pipe_transport_solver_t
    template <typename LumpedCalcParametersType>
    void save_sources_from_qsm_result(
        size_t time_step,
        const oil_transport::task_common_result_t<oil_transport::qsm_pipe_transport_solver_t, LumpedCalcParametersType>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        auto& sources_at_time = sources_results[time_step];
        for (const auto& [edge_id, values_ptr] : result.sources) {
            if (values_ptr == nullptr) {
                throw std::runtime_error("Null source values pointer for edge_id " + std::to_string(edge_id));
            }

            std::string source_name = get_source_name(edge_id, edge_id_to_object_name);
            // Конвертируем LumpedCalcParametersType в endohydro_values_t
            oil_transport::endohydro_values_t endohydro_values = static_cast<oil_transport::endohydro_values_t>(*values_ptr);
            sources_at_time[source_name] = endohydro_values;
        }
    }

    /// @brief Сохраняет сосредоточенные объекты из результатов qsm_pipe_transport_solver_t
    template <typename LumpedCalcParametersType>
    void save_lumpeds_from_qsm_result(
        size_t time_step,
        const oil_transport::task_common_result_t<oil_transport::qsm_pipe_transport_solver_t, LumpedCalcParametersType>& result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_object_name)
    {
        auto& lumpeds_at_time = lumpeds_results[time_step];
        for (const auto& [edge_id, values_array] : result.lumpeds) {
            std::string lumped_name = get_lumped_name(edge_id, edge_id_to_object_name);
            // Конвертируем LumpedCalcParametersType в endohydro_values_t
            std::array<oil_transport::endohydro_values_t, 2> endohydro_array;
            endohydro_array[0] = static_cast<oil_transport::endohydro_values_t>(*values_array[0]);
            endohydro_array[1] = static_cast<oil_transport::endohydro_values_t>(*values_array[1]);
            lumpeds_at_time[lumped_name] = endohydro_array;
        }
    }

public:

    /// @brief Восстанавливает все данные (pipes, sources, lumpeds) из истории в буферы гидравлического графа
    void restore_results(
        oil_transport::hydraulic_graph_t& hydro_graph,
        size_t time_step,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& hydro_edge_id_to_object_name)
    {
        restore_pipes(hydro_graph, time_step, hydro_edge_id_to_object_name);
        restore_sources(hydro_graph, time_step, hydro_edge_id_to_object_name);
        restore_lumpeds(hydro_graph, time_step, hydro_edge_id_to_object_name);
    }

    /// @brief Восстанавливает данные из JSON потока только из одного заданного слоя
    /// @param input_stream Входной поток с JSON данными
    /// @param layer_index Индекс слоя для загрузки (0-based)
    /// @param edge_id_to_name Маппинг идентификаторов ребер к именам объектов
    void restore_from_json_common_results(
        std::istream& input_stream,
        size_t layer_index,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name)
    {
        boost::property_tree::ptree layer_node = oil_transport::step_results_json_reader::get_layer_node(input_stream, layer_index);
        restore_from_json_layer_node(layer_node, edge_id_to_name);
    }

    /// @brief Восстанавливает данные из уже прочитанного JSON узла слоя
    void restore_from_json_layer_node(
        const boost::property_tree::ptree& layer_node,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name)
    {
        size_t step = layer_node.template get<size_t>("step", 0);

        auto buffers_node = layer_node.get_child_optional("buffers");
        if (!buffers_node) {
            return;
        }

        auto pipes_node = buffers_node->get_child_optional("pipes");
        if (pipes_node) {
            for (const auto& [pipe_name, pipe_data] : *pipes_node) {
                auto coordinates = oil_transport::step_results_json_reader::parse_coordinates(pipe_data);
                if (!coordinates.empty()) {
                    save_pipe_coordinates_if_needed(pipe_name, &coordinates);
                }

                auto layer = oil_transport::step_results_json_reader::template parse_pipe_layer<PipeSolver>(pipe_data, coordinates);
                pipes_results[step].insert({ pipe_name, std::move(layer) });
            }
        }

        auto sources_node = buffers_node->get_child_optional("sources");
        if (sources_node) {
            auto& sources_at_time = sources_results[step];
            for (const auto& [source_name, source_data] : *sources_node) {
                auto params_node = source_data.get_child_optional("parameters");
                if (params_node) {
                    sources_at_time[source_name] = oil_transport::step_results_json_reader::parse_parameters(*params_node);

                    auto std_volflow_opt = source_data.template get_optional<std::string>("std_volumetric_flow");
                    if (std_volflow_opt) {
                        sources_at_time[source_name].vol_flow = std::stod(*std_volflow_opt);
                    }
                }
            }
        }

        auto lumpeds_node = buffers_node->get_child_optional("lumpeds");
        if (lumpeds_node) {
            auto& lumpeds_at_time = lumpeds_results[step];
            for (const auto& [lumped_name, lumped_data] : *lumpeds_node) {
                std::array<oil_transport::endohydro_values_t, 2> values;

                auto params_in_node = lumped_data.get_child_optional("parameters_in");
                if (params_in_node) {
                    values[0] = oil_transport::step_results_json_reader::parse_parameters(*params_in_node);
                }

                auto params_out_node = lumped_data.get_child_optional("parameters_out");
                if (params_out_node) {
                    values[1] = oil_transport::step_results_json_reader::parse_parameters(*params_out_node);
                }

                auto std_volflow_opt = lumped_data.template get_optional<std::string>("std_volumetric_flow");
                if (std_volflow_opt) {
                    values[0].vol_flow = std::stod(*std_volflow_opt);
                    values[1].vol_flow = std::stod(*std_volflow_opt);
                }

                lumpeds_at_time[lumped_name] = values;
            }
        }
    }
};

/// @brief Класс для записи результатов расчета по слоям в JSON-файл
template <typename PipeSolver>
class step_results_json_writer {
private:
    /// @brief Получает имена полей структуры через рефлексию
    template <typename T>
    static constexpr auto get_field_names() {
        constexpr size_t field_count = boost::pfr::tuple_size_v<T>;
        std::array<std::string_view, field_count> names{};

        [&names] <size_t... I>(std::index_sequence<I...>) {
            ((names[I] = boost::pfr::get_name<I, T>()), ...);
        }(std::make_index_sequence<field_count>{});

        return names;
    }

    /// @brief Добавляет профиль с достоверностью в JSON-узел
    static void add_profile(boost::property_tree::ptree& profiles_node,
        const std::string& name,
        const std::vector<double>& values,
        const std::vector<double>& confidence) {
        if (values.empty()) {
            return;
        }

        boost::property_tree::ptree value_array;
        for (double val : values) {
            boost::property_tree::ptree val_node;
            val_node.put("", val);
            value_array.push_back(std::make_pair("", val_node));
        }
        profiles_node.add_child(name, value_array);

        // Добавляем достоверность, если она есть
        if (!confidence.empty() && confidence.size() == values.size()) {
            boost::property_tree::ptree conf_array;
            for (double conf : confidence) {
                boost::property_tree::ptree conf_node;
                conf_node.put("", conf);
                conf_array.push_back(std::make_pair("", conf_node));
            }
            profiles_node.add_child(name + "_confidence", conf_array);
        }
    }

    /// @brief Обрабатывает одно поле структуры слоя и добавляет его в JSON-узел профилей
    /// @tparam FieldType Тип поля структуры
    /// @param profiles_node JSON-узел для профилей
    /// @param field Поле структуры для обработки
    /// @param field_name Имя поля
    template <typename FieldType>
    static void process_layer_field(boost::property_tree::ptree& profiles_node, const FieldType& field, const std::string& field_name) {
        typedef decltype(field) field_type_const_ref;
        typedef typename std::remove_reference<field_type_const_ref>::type field_type_const;
        typedef typename std::remove_const<field_type_const>::type field_type;

        // Профиль значений с достоверностью
        if constexpr (std::is_same_v<field_type, pde_solvers::confident_layer_t>) {
            const auto& confident_layer = field;
            if (!confident_layer.value.empty()) {
                add_profile(profiles_node, field_name,
                    confident_layer.value, confident_layer.confidence);
            }
        }
        // Постоянное значение вдоль трубы
        else if constexpr (std::is_same_v<field_type, double>) {
            if (std::isfinite(field)) {
                profiles_node.put(field_name, field);
            }
        }
        // Профиль значения без достоверности
        else if constexpr (std::is_same_v<field_type, std::vector<double>>) {
            if (!field.empty()) {
                boost::property_tree::ptree array_node;
                for (double value : field) {
                    boost::property_tree::ptree item_node;
                    item_node.put("", value);
                    array_node.push_back(std::make_pair("", item_node));
                }
                profiles_node.add_child(field_name, array_node);
            }
        }
    }

    /// @brief Обрабатывает поля в составе расчтеного слоя трубы.
    /// @warning Требуется указать все поля для каждого типа используемого слоя
    template <typename LayerType>
    static boost::property_tree::ptree process_layer_profiles(const LayerType& layer) {
        boost::property_tree::ptree profiles_node;
        if constexpr (std::is_same_v<LayerType, pde_solvers::pipe_endogenous_calc_layer_t>) {
            // Обрабатываем pipe_endogenous_calc_layer_t напрямую без рефлексии
            process_layer_field(profiles_node, layer.density_std, "density_std");
            process_layer_field(profiles_node, layer.viscosity_working, "viscosity_working");
            process_layer_field(profiles_node, layer.sulfur, "sulfur");
            process_layer_field(profiles_node, layer.improver, "improver");
            process_layer_field(profiles_node, layer.temperature, "temperature");
            process_layer_field(profiles_node, layer.viscosity0, "viscosity0");
            process_layer_field(profiles_node, layer.viscosity20, "viscosity20");
            process_layer_field(profiles_node, layer.viscosity50, "viscosity50");
            process_layer_field(profiles_node, layer.std_volumetric_flow, "std_volumetric_flow");
        }
        else if constexpr (std::is_same_v<LayerType, pde_solvers::iso_nonbaro_pipe_layer_t>) {
            process_layer_field(profiles_node, layer.density_std, "density_std");
            process_layer_field(profiles_node, layer.pressure, "pressure");
            process_layer_field(profiles_node, layer.std_volumetric_flow, "std_volumetric_flow");
        }
        else {
            throw std::runtime_error("process_layer_profiles: unsupported layer type. Only pipe_endogenous_calc_layer_t and condensate_pipe_layer are supported.");
        }
        return profiles_node;
    }

    /// @brief Добавляет параметры в JSON-узел для одной стороны дуги
    /// (вход или выход lumped; source)
    template <typename LumpedCalcParametersType>
    static void add_parameters(boost::property_tree::ptree& params_node,
        const LumpedCalcParametersType& lumped_result)
    {
        if constexpr (std::is_same_v<LumpedCalcParametersType, endohydro_values_t>) {
            params_node.put("density_std", lumped_result.density_std.value);
            params_node.put("density_std_confidence", lumped_result.density_std.confidence);
            params_node.put("viscosity_working", lumped_result.viscosity_working.value);
            params_node.put("viscosity_working_confidence", lumped_result.viscosity_working.confidence);
            params_node.put("pressure", lumped_result.pressure);
            params_node.put("mass_flow", lumped_result.mass_flow);
        }
        else {
            throw std::runtime_error(
                "step_results_json_writer::add_parameters is not implemented "
                "for this parameter type. Provide specialization for this structure.");
        }
    }

    /// @brief Обрабатывает результаты источника
    template <typename LumpedCalcParametersType>
    static boost::property_tree::ptree process_source(
        const LumpedCalcParametersType* source_result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        size_t edge_id) {
        boost::property_tree::ptree source_node;

        if (source_result == nullptr) {
            throw std::runtime_error("source_result is nullptr");
        }
        
        if constexpr (std::is_same_v<LumpedCalcParametersType, endohydro_values_t>) {
            if (std::isfinite(source_result->vol_flow)) {
                source_node.put("std_volumetric_flow", source_result->vol_flow);
            }
        }
        
        // Буферы обернуты в объект parameters
        boost::property_tree::ptree params_node;
        add_parameters(params_node, *source_result);
        source_node.add_child("parameters", params_node);

        return source_node;
    }

    /// @brief Обрабатывает результаты сосредоточенного объекта
    template <typename LumpedCalcParametersType>
    static boost::property_tree::ptree process_lumped(
        const std::array<LumpedCalcParametersType*, 2>& lumped_result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        size_t edge_id) {
        boost::property_tree::ptree lumped_node;

        if constexpr (std::is_same_v<LumpedCalcParametersType, endohydro_values_t>) {
            if (lumped_result[0] != nullptr && std::isfinite(lumped_result[0]->vol_flow)) {
                lumped_node.put("std_volumetric_flow", lumped_result[0]->vol_flow);
            }
        }

        const std::array<const char*, 2> param_names = {"parameters_in", "parameters_out"};
        for (size_t i = 0; i < 2; ++i) {
            if (lumped_result[i] == nullptr) {
                throw std::runtime_error("lumped_result[" + std::to_string(i) + "] is nullptr");
            }
            boost::property_tree::ptree params;
            add_parameters(params, *(lumped_result[i]));
            lumped_node.add_child(param_names[i], params);
        }

        return lumped_node;
    }

    /// @brief Обрабатывает данные одной трубы
    static boost::property_tree::ptree process_pipe(
        const pipe_common_result_t<PipeSolver>* pipe_result,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        size_t edge_id) {
        boost::property_tree::ptree pipe_node;

        if (pipe_result == nullptr) {
            throw std::runtime_error("pipe_result is nullptr");
        }

        // Добавляем координаты
        if (pipe_result->coordinates != nullptr && !pipe_result->coordinates->empty()) {
            boost::property_tree::ptree coords_array;
            for (double coord : *(pipe_result->coordinates)) {
                boost::property_tree::ptree coord_node;
                coord_node.put("", coord);
                coords_array.push_back(std::make_pair("", coord_node));
            }
            pipe_node.add_child("coordinates", coords_array);
        }

        // Добавляем профили параметров
        if (pipe_result->layer != nullptr) {
            const auto& layer = *(pipe_result->layer);
            boost::property_tree::ptree profiles = process_layer_profiles(layer);
            if (!profiles.empty()) {
                pipe_node.add_child("profiles", profiles);
            }
        }

        // Добавляем std_volumetric_flow если есть
        if (pipe_result->layer != nullptr) {
            const auto& layer = *(pipe_result->layer);
            if (std::isfinite(layer.std_volumetric_flow)) {
                pipe_node.put("std_volumetric_flow", layer.std_volumetric_flow);
            }
        }

        return pipe_node;
    }



 private:

    /// @brief Добавляет в узел слоя секцию buffers.pipes
    template <typename LumpedCalcParametersType>
    static void add_buffers_pipes(
        const task_common_result_t<PipeSolver, LumpedCalcParametersType>& results,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        boost::property_tree::ptree& buffers_node)
    {
        boost::property_tree::ptree pipes_object;
        for (const auto& [edge_id, pipe_result] : results.pipes) {
            if (pipe_result == nullptr) {
                throw std::runtime_error("pipe_result is nullptr for edge_id " + std::to_string(edge_id));
            }
            bool has_coords = pipe_result->coordinates != nullptr && !pipe_result->coordinates->empty();
            bool has_profiles = pipe_result->layer != nullptr;
            bool has_flow = false;
            if (pipe_result->layer != nullptr) {
                has_flow = std::isfinite(pipe_result->layer->std_volumetric_flow);
            }

            // Записываем только если есть данные
            if (has_coords || has_profiles || has_flow) {
                const std::string& pipe_name =
                    graph_siso_data::get_object_name(graphlib::graph_binding_type::Edge2, edge_id, edge_id_to_name);
                boost::property_tree::ptree child = process_pipe(pipe_result, edge_id_to_name, edge_id);
                // Формируем value_type явно, чтобы избежать проблем с "." в названиях объектов
                pipes_object.push_back(boost::property_tree::ptree::value_type(pipe_name, child));
            }
        }
        if (!pipes_object.empty()) {
            buffers_node.add_child("pipes", pipes_object);
        }
    }

    /// @brief Добавляет в узел слоя секцию buffers.sources
    template <typename LumpedCalcParametersType>
    static void add_buffers_sources(
        const task_common_result_t<PipeSolver, LumpedCalcParametersType>& results,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        boost::property_tree::ptree& buffers_node)
    {
        boost::property_tree::ptree sources_object;
        for (const auto& [edge_id, source_result] : results.sources) {
            if (source_result == nullptr) {
                throw std::runtime_error("source_result is nullptr for edge_id " + std::to_string(edge_id));
            }
            const std::string& source_name =
                graph_siso_data::get_object_name(graphlib::graph_binding_type::Edge1, edge_id, edge_id_to_name);
            boost::property_tree::ptree child = process_source(source_result, edge_id_to_name, edge_id);
            sources_object.push_back(boost::property_tree::ptree::value_type(source_name, child));
        }
        if (!sources_object.empty()) {
            buffers_node.add_child("sources", sources_object);
        }
    }

    /// @brief Добавляет в узел слоя секцию buffers.lumpeds
    template <typename LumpedCalcParametersType>
    static void add_buffers_lumpeds(
        const task_common_result_t<PipeSolver, LumpedCalcParametersType>& results,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        boost::property_tree::ptree& buffers_node)
    {
        boost::property_tree::ptree lumpeds_object;
        for (const auto& [edge_id, lumped_result] : results.lumpeds) {
            if (lumped_result[0] == nullptr) {
                throw std::runtime_error("lumped_result[0] is nullptr for edge_id " + std::to_string(edge_id));
            }
            if (lumped_result[1] == nullptr) {
                throw std::runtime_error("lumped_result[1] is nullptr for edge_id " + std::to_string(edge_id));
            }
            const std::string& lumped_name =
                graph_siso_data::get_object_name(graphlib::graph_binding_type::Edge2, edge_id, edge_id_to_name);
            boost::property_tree::ptree child = process_lumped(lumped_result, edge_id_to_name, edge_id);
            lumpeds_object.push_back(boost::property_tree::ptree::value_type(lumped_name, child));
        }
        if (!lumpeds_object.empty()) {
            buffers_node.add_child("lumpeds", lumpeds_object);
        }
    }

    /// @brief Добавляет в узел слоя секцию transport_specific.*
    /// TODO: Не передавать граф. Как вариант - get_specific() для всех моделей, 
    /// заранее созданное сопоставление "ребро - тип модели"
    template <typename LumpedCalcParametersType>
    static void add_transport_specific(
        const task_common_result_t<PipeSolver, LumpedCalcParametersType>& results,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        const transport_graph_t& graph,
        boost::property_tree::ptree& layer_node)
    {
        boost::property_tree::ptree transport_specific_node;

        // Собираем все объекты из графа для transport_specific
        auto edges = graph.get_edge_binding_list();
        std::map<std::string, boost::property_tree::ptree> transport_objects;

        for (const auto& edge_binding : edges) {
            // Определяем тип объекта
            std::string type = get_transport_object_type_string<PipeSolver>(graph, edge_binding.binding_type, edge_binding.edge);

            bool has_results = false;

            // TODO: добавить обработку transport_specific

            if (!has_results) {
                continue;
            }

            // Добавляем объект с ключом-именем
            const std::string& object_name =
                graph_siso_data::get_object_name(edge_binding.binding_type, edge_binding.edge, edge_id_to_name);
            boost::property_tree::ptree obj_node;
            transport_objects[type].add_child(object_name, std::move(obj_node));
        }

        // Добавляем объекты в transport_specific
        for (auto& [type, object] : transport_objects) {
            if (!object.empty()) {
                transport_specific_node.add_child(type, object);
            }
        }

        if (!transport_specific_node.empty()) {
            layer_node.add_child("transport_specific", transport_specific_node);
        }
    }

public:

    /// @brief Записывает один слой в JSON-поток как JSON-объект (без накопления)
    /// @tparam LumpedCalcParametersType Тип параметров для сосредоточенных объектов
    /// @param results Результаты расчета
    /// @param step_index Номер расчетного слоя
    /// @param edge_id_to_name Маппинг идентификаторов ребер к именам объектов
    /// @param graph Транспортный граф для определения типов транспортных объектов
    /// @param output_stream Выходной поток для записи JSON
    /// @param need_comma Нужна ли запятая перед слоем
    template <typename LumpedCalcParametersType>
    static void write_single_layer(
        const task_common_result_t<PipeSolver, LumpedCalcParametersType>& results,
        size_t step_index,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        const transport_graph_t& graph,
        std::ostream& output_stream,
        bool need_comma) 
    {

        // Создаем узел для текущего слоя
        boost::property_tree::ptree layer_node;
        layer_node.put("step", step_index);

        // Формируем JSON узоы из результатов
        boost::property_tree::ptree buffers_node;
        add_buffers_pipes(results, edge_id_to_name, buffers_node);
        add_buffers_sources(results, edge_id_to_name, buffers_node);
        add_buffers_lumpeds(results, edge_id_to_name, buffers_node);

        if (!buffers_node.empty()) {
            layer_node.add_child("buffers", buffers_node);
        }

        // Обрабатываем transport_specific
        //add_transport_specific(results, edge_id_to_name, graph, layer_node);

        // Записываем JSON для одного слоя в строку
        std::ostringstream layer_stream;
        boost::property_tree::write_json(layer_stream, layer_node);
        std::string layer_json = layer_stream.str();

        // Убираем завершающий перевод строки
        if (!layer_json.empty() && layer_json.back() == '\n') {
            layer_json.pop_back();
        }

        // Записываем в поток с правильным форматированием
        try {
            if (need_comma) {
                output_stream << ",\n";
            }
            // Записываем JSON слоя с отступом (добавляем 4 пробела к каждой строке кроме первой)
            std::istringstream layer_input(layer_json);
            std::string line;
            bool first_line = true;
            while (std::getline(layer_input, line)) {
                if (!first_line) {
                    output_stream << "\n";
                }
                output_stream << "    " << line;  // 4 пробела отступа
                first_line = false;
            }
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to write layer JSON to stream: " + std::string(e.what()));
        }
    }


    /// @brief Инициализирует корневой узел JSON для записи результатов
    /// @return Инициализированный корневой узел с пустым массивом "layers"
    static boost::property_tree::ptree initialize_json() {
        boost::property_tree::ptree root;
        root.add_child("layers", boost::property_tree::ptree());
        return root;
    }

    /// @brief Записывает начало JSON структуры с массивом layers
    /// @param output_stream Выходной поток для записи JSON
    static void start_json_structure(std::ostream& output_stream) {
        output_stream << "{\n  \"layers\": [\n";
    }

    /// @brief Завершает JSON структуру с массивом layers
    /// @param output_stream Выходной поток для записи JSON
    static void end_json_structure(std::ostream& output_stream) {
        output_stream << "\n  ]\n}";
        output_stream.flush();
    }
};

}

