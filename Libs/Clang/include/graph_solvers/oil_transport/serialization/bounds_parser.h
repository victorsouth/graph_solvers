#pragma once

namespace oil_transport {

/// @brief Парсер для transport_measurements
class transport_measurements_parser_json {
public:
    /// @brief Парсит transport_measurements из JSON
    /// @param layers_node Узел layers из JSON
    static std::unordered_map<std::size_t, std::vector<oil_transport::graph_quantity_value_t>> parse_layers(
        const boost::property_tree::ptree& layers_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        std::unordered_map<std::size_t, std::vector<oil_transport::graph_quantity_value_t>> result;

        for (const auto& layer_pair : layers_node) {
            const auto& layer_node = layer_pair.second;
            
            std::string step_str = layer_node.get<std::string>("step");
            std::size_t step_index = std::stoull(step_str);

            if (auto measurements_node_opt = layer_node.get_child_optional("transport_measurements")) {
                const auto& measurements_node = measurements_node_opt.get();
                std::vector<oil_transport::graph_quantity_value_t> measurements;

                for (const auto& measurement_pair : measurements_node) {
                    const auto& measurement_node = measurement_pair.second;
                    auto measurement = parse_single_measurement(
                        measurement_node, name_to_binding);
                    measurements.push_back(measurement);
                }

                result[step_index] = std::move(measurements);
            }
        }

        return result;
    }


    /// @brief Парсит одно измерение
    /// @param measurement_node Узел измерения
    static oil_transport::graph_quantity_value_t parse_single_measurement(
        const boost::property_tree::ptree& measurement_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        std::string type_str = measurement_node.get<std::string>("type");
        double value = measurement_node.get<double>("value");

        if (auto edge_name_opt = measurement_node.get_optional<std::string>("edge")) {
            const std::string& edge_name = edge_name_opt.get();
            return parse_edge_measurement(edge_name, type_str, value, name_to_binding);
        } else if (auto vertex_opt = measurement_node.get_optional<std::size_t>("vertex")) {
            std::size_t vertex_index = vertex_opt.get();
            return parse_vertex_measurement(vertex_index, type_str, value);
        } else {
            throw std::runtime_error("Measurement must have either 'edge' or 'vertex' field");
        }
    }

    /// @brief Парсит измерение на ребре
    static oil_transport::graph_quantity_value_t parse_edge_measurement(
        const std::string& edge_name,
        const std::string& type_str,
        double value,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        const auto& binding = name_to_binding.at(edge_name);

        auto [measurement_type, side] = parse_measurement_type(type_str);

        oil_transport::graph_quantity_value_t result;
        result.type = measurement_type;
        result.data.value = value;
        static_cast<graphlib::graph_binding_t&>(result) = binding;
        
        bool has_side_suffix = type_str.size() >= 3 &&
            (type_str.substr(type_str.size() - 3) == "_in" ||
                type_str.substr(type_str.size() - 4) == "_out");

        if (has_side_suffix) {
            result.is_sided = true;
            result.side = side;
        }

        return result;
    }

    /// @brief Парсит измерение на вершине
    static oil_transport::graph_quantity_value_t parse_vertex_measurement(
        std::size_t vertex_index,
        const std::string& type_str,
        double value) {
        auto [measurement_type, side] = parse_measurement_type(type_str);

        oil_transport::graph_quantity_value_t result;
        result.type = measurement_type;
        result.data.value = value;
        result.binding_type = graphlib::graph_binding_type::Vertex;
        result.vertex = vertex_index;
        result.is_sided = false;

        return result;
    }

    /// @brief Парсит тип измерения с учетом постфиксов _in и _out
    /// @return Пара (тип измерения, сторона ребра)
    static std::pair<oil_transport::measurement_type, graphlib::graph_edge_side_t> parse_measurement_type(
        const std::string& type_str) {
        using namespace oil_transport;
        
        std::string base_type = type_str;
        graphlib::graph_edge_side_t side = graphlib::graph_edge_side_t::Inlet;

        if (type_str.size() >= 3) {
            std::string suffix = type_str.substr(type_str.size() - 3);
            if (suffix == "_in") {
                base_type = type_str.substr(0, type_str.size() - 3);
                side = graphlib::graph_edge_side_t::Inlet;
            } else if (type_str.size() >= 4 && type_str.substr(type_str.size() - 4) == "_out") {
                base_type = type_str.substr(0, type_str.size() - 4);
                side = graphlib::graph_edge_side_t::Outlet;
            }
        }

        for (const auto& [mtype, mstring] : measurement_type_converter) {
            if (std::string(mstring) == base_type) {
                return { mtype, side };
            }
        }

        throw std::runtime_error("Unknown measurement type: " + type_str);
    }
};

/// @brief Краевые условия для одного шага
struct hydrotransport_bounds_t {
    /// @brief Транспортные измерения
    std::vector<oil_transport::graph_quantity_value_t> measurements;
    /// @brief Контролы для гидравлических объектов - изменяют состояния объектов гидравлического графа
    std::vector<std::unique_ptr<oil_transport::hydraulic_object_control_t>> hydraulic_controls;
    /// @brief Контроля для транспортных объектов. Получены конвертацией из гидравлических контролов
    std::vector<std::unique_ptr<oil_transport::transport_object_control_t>> transport_controls;

    /// @brief Возвращает вектор сырых указателей на гидравлические контролы
    std::vector<const oil_transport::hydraulic_object_control_t*> get_hydraulic_controls() const {
        std::vector<const oil_transport::hydraulic_object_control_t*> result;
        for (const auto& ptr : hydraulic_controls) {
            result.push_back(ptr.get());
        }
        return result;
    }

    /// @brief Возвращает вектор сырых указателей на транспортные контролы
    std::vector<const oil_transport::transport_object_control_t*> get_transport_controls() const {
        std::vector<const oil_transport::transport_object_control_t*> result;
        for (const auto& ptr : transport_controls) {
            result.push_back(ptr.get());
        }
        return result;
    }
};

/// @brief Краевые условия (измерения + переключения) на несколько моментов времени
using batch_hydrotransport_bounds_t = std::unordered_map<size_t, hydrotransport_bounds_t>;

/// @brief Парсер для controls
class controls_parser_json {
public:
    /// @brief Парсит controls из JSON
    /// @param layers_node Узел layers из JSON
    /// @param name_to_binding Маппинг имен объектов к привязкам
    static batch_hydrotransport_bounds_t parse_layers(
        const boost::property_tree::ptree& layers_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        batch_hydrotransport_bounds_t result;

        for (const auto& layer_pair : layers_node) {
            const auto& layer_node = layer_pair.second;
            
            std::string step_str = layer_node.get<std::string>("step");
            std::size_t step_index = std::stoull(step_str);

            if (auto controls_node_opt = layer_node.get_child_optional("controls")) {
                auto& bounds = result[step_index];
                parse_step_controls(controls_node_opt.get(), name_to_binding, bounds);
            }
        }

        return result;
    }

    /// @brief Парсит контролы для одного шага
    /// @param controls_node Узел controls для шага
    /// @param bounds Структура для записи результатов парсинга
    static void parse_step_controls(
        const boost::property_tree::ptree& controls_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding,
        hydrotransport_bounds_t& bounds) 
        {
            
        if (auto pumps_node_opt = controls_node.get_child_optional("pumps")) {
            const auto& pumps_node = pumps_node_opt.get();
            if (!pumps_node.empty()) {
                throw std::runtime_error("Pumps controls are not implemented yet");
            }
        }

        if (auto improvers_node_opt = controls_node.get_child_optional("improvers")) {
            const auto& improvers_node = improvers_node_opt.get();
            if (!improvers_node.empty()) {
                throw std::runtime_error("Improvers controls are not implemented yet");
            }
        }

        if (auto valves_node_opt = controls_node.get_child_optional("valves")) {
            parse_valves_controls(valves_node_opt.get(), name_to_binding, bounds);
        }

        if (auto sources_node_opt = controls_node.get_child_optional("sources")) {
            parse_sources_controls(sources_node_opt.get(), name_to_binding, bounds);
        }
    }

private:
    /// @brief Заполняет поля структуры из JSON узла по маппингу
    /// @tparam ControlType Тип контрола
    /// @param control Контрол для заполнения
    /// @param json_node JSON узел с данными
    /// @param field_mapping Маппинг имен JSON ключей на функции-сеттеры полей
    template<typename ControlType>
    static void fill_control_fields_from_json(
        ControlType& control,
        const boost::property_tree::ptree& json_node,
        const std::unordered_map<std::string, std::function<void(double)>>& field_mapping) {
        for (const auto& [json_key, setter] : field_mapping) {
            if (auto value_opt = json_node.get_optional<double>(json_key)) {
                setter(value_opt.get());
            }
        }
    }

    /// @brief Возвращает гидравлический контрол для задвижки
    static std::unique_ptr<oil_transport::qsm_hydro_local_resistance_control_t> create_hydro_valve_control(
        const graphlib::graph_binding_t& binding,
        const boost::property_tree::ptree& valve_node) {
        auto hydro_control = std::make_unique<oil_transport::qsm_hydro_local_resistance_control_t>();
        hydro_control->binding = binding;

        if (auto opening_opt = valve_node.get_optional<double>("opening")) {
            hydro_control->opening = opening_opt.get();
        } else if (auto is_opened_opt = valve_node.get_optional<bool>("is_opened")) {
            hydro_control->opening = is_opened_opt.get() ? 1.0 : 0.0;
        } else {
            throw std::runtime_error("Valve control must have either 'opening' or 'is_opened' field");
        }

        return hydro_control;
    }

    /// @brief Возвращает транспортный контрол для задвижки
    static std::unique_ptr<oil_transport::transport_lumped_control_t> create_transport_valve_control(
        const graphlib::graph_binding_t& binding,
        const boost::property_tree::ptree& valve_node) {
        auto transport_control = std::make_unique<oil_transport::transport_lumped_control_t>();
        transport_control->binding = binding;

        if (auto opening_opt = valve_node.get_optional<double>("opening")) {
            transport_control->is_opened = opening_opt.get() > 0.0;
        } else if (auto is_opened_opt = valve_node.get_optional<bool>("is_opened")) {
            transport_control->is_opened = is_opened_opt.get();
        } else {
            throw std::runtime_error("Valve control must have either 'opening' or 'is_opened' field");
        }

        return transport_control;
    }

    /// @brief Парсит контролы для задвижек (valves)
    /// @param bounds Структура для записи результата парсинга
    static void parse_valves_controls(
        const boost::property_tree::ptree& valves_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding,
        hydrotransport_bounds_t& bounds) {
        for (const auto& valve_pair : valves_node) {
            std::string valve_name = valve_pair.first;
            const auto& valve_node = valve_pair.second;

            auto binding_it = name_to_binding.find(valve_name);
            if (binding_it == name_to_binding.end()) {
                throw std::runtime_error("Edge name not found in graph: " + valve_name);
            }
            const auto& binding = binding_it->second;
            
            auto hydro_control = create_hydro_valve_control(binding, valve_node);
            auto transport_control = create_transport_valve_control(binding, valve_node);

            bounds.hydraulic_controls.push_back(std::move(hydro_control));
            bounds.transport_controls.push_back(std::move(transport_control));
        }
    }

    /// @brief Зачитка гидравлического контрола для источника
    static std::unique_ptr<oil_transport::hydraulic_source_control_t> create_hydraulic_source_control(
        const graphlib::graph_binding_t& binding,
        const boost::property_tree::ptree& source_node) {
        auto hydro_control = std::make_unique<oil_transport::hydraulic_source_control_t>();
        hydro_control->binding = binding;

        const std::unordered_map<std::string, std::function<void(double)>> field_mapping_hydro = {
            {"pressure", [&](double val) { hydro_control->pressure = val; }},
            {"std_vol_flow", [&](double val) { hydro_control->std_volflow = val; }},
        };

        fill_control_fields_from_json(*hydro_control, source_node, field_mapping_hydro);

        return hydro_control;
    }

    /// @brief Зачитка транспортного контрола для источника
    static std::unique_ptr<oil_transport::transport_source_control_t> create_transport_source_control(
        const graphlib::graph_binding_t& binding,
        const boost::property_tree::ptree& source_node) 
    {
        auto transport_control = std::make_unique<oil_transport::transport_source_control_t>();
        transport_control->binding = binding;

        const std::unordered_map<std::string, std::function<void(double)>> field_mapping_transport = {
            {"density_std_out", [&](double val) { transport_control->density_std = val; }},
            {"viscosity_working", [&](double val) { transport_control->viscosity_working = val; }},
            {"temperature", [&](double val) { transport_control->temperature = val; }}
        };

        fill_control_fields_from_json(*transport_control, source_node, field_mapping_transport);

        return transport_control;
    }

    /// @brief Парсит контролы для притоков (sources)
    /// @param bounds Структура для записи результата парсинга
    static void parse_sources_controls(
        const boost::property_tree::ptree& sources_node,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding,
        hydrotransport_bounds_t& bounds) {
        for (const auto& source_pair : sources_node) {
            std::string source_name = source_pair.first;
            const auto& source_node = source_pair.second;

            auto binding_it = name_to_binding.find(source_name);
            if (binding_it == name_to_binding.end()) {
                throw std::runtime_error("Edge name not found in graph: " + source_name);
            }
            const auto& binding = binding_it->second;
            
            auto hydro_control = create_hydraulic_source_control(binding, source_node);
            bounds.hydraulic_controls.push_back(std::move(hydro_control));

            auto transport_control = create_transport_source_control(binding, source_node);
            bounds.transport_controls.push_back(std::move(transport_control));
            
        }
    }
};

/// @brief Парсер для файла multilayer_bounds_format.json
class bounds_parser_json {
public:
    /// @brief Парсит несколько слоев краевых условий
    /// @return Краевые условия для нескольких шагов
    static batch_hydrotransport_bounds_t parse_multiple_layers(
        std::istream& stream,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(stream, pt);

        auto layers_node = pt.get_child("layers");

        auto measurements_map = transport_measurements_parser_json::parse_layers(
            layers_node, name_to_binding);

        auto controls_map = controls_parser_json::parse_layers(
            layers_node, name_to_binding);

        batch_hydrotransport_bounds_t result;
        for (const auto& [step_index, measurements] : measurements_map) {
            result[step_index].measurements = measurements;
        }

        for (auto& [step_index, bounds] : controls_map) {
            result[step_index].hydraulic_controls = std::move(bounds.hydraulic_controls);
            result[step_index].transport_controls = std::move(bounds.transport_controls);
        }

        return result;
    }

    /// @brief Парсит краевые условия без разделения на слои
    /// @return Краевые условия для одного шага
    static hydrotransport_bounds_t parse_single_layer(
        std::istream& stream,
        const std::unordered_map<std::string, graphlib::graph_binding_t>& name_to_binding) {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(stream, pt);

        hydrotransport_bounds_t result;

        if (auto measurements_node_opt = pt.get_child_optional("transport_measurements")) {
            const auto& measurements_node = measurements_node_opt.get();
            for (const auto& measurement_pair : measurements_node) {
                const auto& measurement_node = measurement_pair.second;
                auto measurement = transport_measurements_parser_json::parse_single_measurement(
                    measurement_node, name_to_binding);
                result.measurements.push_back(measurement);
            }
        }

        if (auto controls_node_opt = pt.get_child_optional("controls")) {
            controls_parser_json::parse_step_controls(
                controls_node_opt.get(), name_to_binding, result);
        }

        return result;
    }
};

}

