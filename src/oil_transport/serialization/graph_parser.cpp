#include "oil_transport.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "boost/pfr.hpp"
namespace oil_transport {
;


/// @brief Зачитка списка тегов для объекта. Вызывается для при обходе каждого объекта в load_*_data
/// @param tags Узел в json-файле
object_measurement_tags_t parse_tags(boost::property_tree::ptree& tags) {
    using namespace std; // for ""s

    auto read_tag = [&](measurement_type type, const std::string& variable,
        std::vector<measurement_tag_info_t>* tag_list)
        {
            auto data_for_var = tags.get_child_optional(variable);
            auto child = tags.get_optional<std::string>(variable);

            if (!data_for_var)
                return;
            if (!child)
                return;

            measurement_tag_info_t tag_info;
            tag_info.type = type;
            if (auto tag_name = data_for_var.get().get_optional<std::string>("Tag")) {
                tag_info.name = tag_name.get();
                if (auto dimension = data_for_var.get().get_optional<std::string>("Dim")) {
                    tag_info.dimension = dimension.get();
                }
            }
            else  {
                tag_info.name = child.get();
                tag_info.dimension = ""s;
            }
            tag_list->push_back(tag_info);
        };

    object_measurement_tags_t lp_tags;
    for (const auto& [mtype, mstring] : measurement_type_converter) {
        read_tag(mtype, std::string(mstring) + "_in"s, &lp_tags.in);
    }
    for (const auto& [mtype, mstring] : measurement_type_converter) {
        read_tag(mtype, std::string(mstring) + "_out"s, &lp_tags.out);
    }
    return lp_tags;
}


// @brief Зачитка списка контролов для объекта. Вызывается для при обходе каждого объекта в load_*_data
/// @param tags Узел в json-файле
control_tag_info_t parse_controls(boost::property_tree::ptree& tags) {
    using namespace std; // for ""s
    
    auto read_tag = [&](control_tag_conversion_type type, const std::string& variable,
        control_tag_info_t* tag_info)
        {
            auto data_for_var = tags.get_child_optional(variable);
            auto child = tags.get_optional<std::string>(variable);

            if (!data_for_var)
                return;
            if (!child)
                return;

            tag_info->type = type;
            tag_info->name = child.get();

        };

    control_tag_info_t result_tag;
    for (const auto& [mtype, mstring] : control_type_converter) {
        read_tag(mtype, std::string(mstring), &result_tag);
    }


    return result_tag;
}

/// @brief Внутренняя функция для парсинга топологии из потока
static std::vector<graphlib::model_incidences_t<std::string>> 
load_topology_from_stream(std::istream& stream)
{
    std::vector<graphlib::model_incidences_t<std::string>> edges;
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(stream, pt);

    for (auto [name, value] : pt) {
        graphlib::edge_incidences_t edge;
        if (auto child = value.get_optional<int>("from")) {
            edge.from = child.get();
        }
        if (auto child = value.get_optional<int>("to")) {
            edge.to = child.get();
        }
        edges.emplace_back(edge, name);
    }
    return edges;
}

std::vector<graphlib::model_incidences_t<std::string>> 
json_graph_parser_t::load_topology(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    return load_topology_from_stream(file);
}

std::vector<graphlib::model_incidences_t<std::string>> 
json_graph_parser_t::load_topology_from_string(const std::string& json_string)
{
    std::istringstream stream(json_string);
    return load_topology_from_stream(stream);
}

void json_graph_parser_t::load_edges_properties(const std::string& path, const qsm_problem_type& problem_type, graph_json_data& data)
{
    data.pipes = load_pipe_data(path + "/pipe_data.json");
    data.sources = load_source_data(path + "/source_data.json");
    data.pumps = load_lumped_data(path + "/pump_data.json");
    data.valves = load_valve_data(path + "/valve_data.json");
    
    if (problem_type == qsm_problem_type::Transport)
        data.lumpeds = load_lumped_data(path + "/lumped_data.json");

}

void json_graph_parser_t::load_tags_controls_data(const std::string& path, const std::string& tags_filename, graph_json_data& data)
{
    data.object_tags = load_tags_data(tags_filename);
    data.object_controls = load_controls_data(tags_filename);
    extract_object_tags(path + "/pipe_data.json", &data.object_tags);
    extract_object_tags(path + "/source_data.json", &data.object_tags);
    extract_object_tags(path + "/lumped_data.json", &data.object_tags);
    extract_object_tags(path + "/pump_data.json", &data.object_tags);
    extract_object_tags(path + "/valve_data.json", &data.object_tags);
}

std::size_t graph_json_data::get_total_object_count() const
{
    std::size_t count = pipes.size() + sources.size() +
        lumpeds.size() + pumps.size() + valves.size();
    return count;
}

std::unordered_set<std::string> graph_json_data::get_object_names() const
{
    std::unordered_set<std::string> names;
    auto add_names = [&](const std::unordered_set<std::string>& new_names) {
        names.insert(new_names.begin(), new_names.end());
        };
    add_names(graphlib::get_map_keys(pipes));
    add_names(graphlib::get_map_keys(sources));
    add_names(graphlib::get_map_keys(lumpeds));
    add_names(graphlib::get_map_keys(pumps));
    add_names(graphlib::get_map_keys(valves));

    if (names.size() != get_total_object_count()) {
        throw std::runtime_error("Same names of different objects found");
    }

    return names;
}

void graph_json_data::check_names_uniqueness() const
{
    // в этой функции проверка на уникальность, будет исключение если уникальности нет
    std::unordered_set<std::string> names_from_properties = 
        get_object_names(); 

    // Далее проверяем, что среди тегов не встречаются имена для несуществующих объектов
    std::unordered_set<std::string> names_from_tags = 
        graphlib::get_map_keys(object_tags);

    for (const std::string& name : names_from_properties) {
        names_from_tags.erase(name);
    }

    if (!names_from_tags.empty()) {
        std::stringstream error_text;
        error_text << "Tag bindings for unexistent objects: " << std::endl;
        for (const std::string& name : names_from_tags) {
            error_text << name << std::endl;
        }
        throw std::runtime_error(error_text.str());
    }
}

void graph_json_data::check_topology_and_object_links() const
{
    std::unordered_set<std::string> objects = get_object_names();
    if (edges.size() != objects.size())
        throw std::runtime_error("Edges count and object count are different");

    for (const graphlib::model_incidences_t<std::string>& edge : edges) {
        auto it = objects.find(edge.model);
        if (it == objects.end()) {
            std::stringstream error_text;
            error_text << "Cannot find object with name " << edge.model;
            throw std::runtime_error(error_text.str());
        }

    }
}

std::unordered_map<std::string, object_measurement_tags_t> 
    json_graph_parser_t::load_tags_data(const std::string& tags_filename)
{
    std::unordered_map<std::string, object_measurement_tags_t> result;
    if (!std::filesystem::exists(tags_filename)) {
        return result;
    }

    boost::property_tree::ptree pt;
    boost::property_tree::read_json(tags_filename, pt);

    for (auto [name, tags_node] : pt) {
        auto obj_tags = parse_tags(tags_node);
        result[name] = obj_tags;

    }
    return result;
}

std::unordered_map<std::string, control_tag_info_t>
json_graph_parser_t::load_controls_data(const std::string& controls_filename)
{
    std::unordered_map<std::string, control_tag_info_t> result;
    if (!std::filesystem::exists(controls_filename)) {
        return result;
    }

    boost::property_tree::ptree pt;
    boost::property_tree::read_json(controls_filename, pt);

    for (auto [name, tags_node] : pt) {
        auto obj_tags = parse_controls(tags_node);
        if (!obj_tags.name.empty())
            // Если по объекту найдены контролы, то добавляем их
            result[name] = obj_tags;

    }
    return result;
}

void json_graph_parser_t::extract_object_tags(
    const std::string& filename, 
    std::unordered_map<std::string, object_measurement_tags_t>* tags)
{
    if (!std::filesystem::exists(filename)) {
        return;
    }

    boost::property_tree::ptree pt;
    boost::property_tree::read_json(filename, pt);
    for (auto [name, value] : pt) {
        if (auto tags_node = value.get_child_optional("Tags")) {
            auto obj_tags = parse_tags(tags_node.get());
            (*tags)[name] = (*tags)[name] + obj_tags;
        }
    }
}

/// @brief Внутренняя функция для парсинга одного объекта pipe из ptree node
static void parse_pipe_from_ptree(
    const boost::property_tree::ptree& value,
    pde_solvers::pipe_json_data& pipe)
{
    pipe = pde_solvers::pipe_json_data::default_values();
    
    double X_start = value.get<double>("X_start");
    double X_end = value.get<double>("X_end");
    pipe.x_start = X_start;
    pipe.x_end = X_end;
    
    pipe.diameter = value.get<double>("Diameter");
    // Дополнительные поля из object_params_95.json (Thickness, Temperature, E, k, kT, isActive, H_start, H_end)
    // не являются частью структуры pipe_json_data и игнорируются
    if (auto child = value.get_optional<double>("Z_start")) {
        //lp.Z_start = child.get();
    }
    if (auto child = value.get_optional<double>("Z_end")) {
        //lp.Z_end = child.get();
    }
}

/// @brief Внутренняя функция для загрузки pipes из ptree
/// Поддерживает формат object_params_95.json (с ключом "pipes") и старый формат (без ключа)
static std::unordered_map<std::string, pde_solvers::pipe_json_data> 
load_pipes_from_ptree(const boost::property_tree::ptree& pt)
{
    std::unordered_map<std::string, pde_solvers::pipe_json_data> pipes;
    
    // Проверяем, есть ли ключ "pipes" в корне JSON (формат object_params_95.json)
    if (auto pipes_node = pt.get_child_optional("pipes")) {
        // Формат object_params_95.json: {"pipes": {...}}
        for (auto [name, value] : *pipes_node) {
            parse_pipe_from_ptree(value, pipes[name]);
        }
    } else {
        // Формат старого файла: прямой объект с именами труб
        for (auto [name, value] : pt) {
            parse_pipe_from_ptree(value, pipes[name]);
        }
    }
    
    return pipes;
}

///// @brief Внутренняя функция для парсинга одного объекта valve из ptree node
//static void parse_valve_from_ptree(
//    const boost::property_tree::ptree& value,
//    valve_json_data& valve)
//{
//    load_json_data_reflection_from_ptree<valve_json_data>(value, valve);
//
//
//    if (auto child = value.get_optional<bool>("is_initially_opened")) {
//        valve.initial_opening = child.get() ? 1.0 : 0.0;
//    }
//}

std::unordered_map<std::string, pde_solvers::pipe_json_data> 
    json_graph_parser_t::load_pipe_data(const std::string& filename)
{
    std::unordered_map<std::string, pde_solvers::pipe_json_data> lps;
    if (!std::filesystem::exists(filename)) {
        return lps;
    }

    boost::property_tree::ptree pt;
    boost::property_tree::read_json(filename, pt);
    return load_pipes_from_ptree(pt);
}

/// @brief Возвращает список полей заданной структуры T
/// Полезно, когда нет boost::pfr::for_each_field_with_name и нужно использовать for_each_field
template <typename T>
constexpr auto get_field_names() {
    constexpr size_t field_count = boost::pfr::tuple_size_v<T>;
    std::array<std::string_view, field_count> names{};

    [&names] <size_t... I>(std::index_sequence<I...>) {
        ((names[I] = boost::pfr::get_name<I, T>()), ...);
    }(std::make_index_sequence<field_count>{});

    return names;
}

/// @brief Внутренняя функция для парсинга данных через рефлексию из ptree
template <typename T> 
std::unordered_map<std::string, T> load_json_data_reflection_from_ptree(const boost::property_tree::ptree& pt)
{
    std::unordered_map<std::string, T> objs;

    constexpr auto T_field_names = get_field_names<T>();

    for (auto [name, value] : pt) {
        T& src = objs[name];

        boost::pfr::for_each_field(
            src, [&](auto& field, std::size_t index)
            {
                std::string_view field_name = T_field_names[index];

                typedef decltype(field) field_type_const_ref;
                typedef typename std::remove_reference<field_type_const_ref>::type field_type_const;
                typedef typename std::remove_const<field_type_const>::type field_type;
                auto child = value.template get_optional<field_type>(std::string(field_name));
                if (child) {
                    field = child.get();
                }
            });
    }
    return objs;
}

/// @brief Внутренняя функция для парсинга данных через рефлексию из потока
template <typename T> 
std::unordered_map<std::string, T> load_json_data_reflection_from_stream(std::istream& stream)
{
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(stream, pt);
    return load_json_data_reflection_from_ptree<T>(pt);
}

template <typename T> 
std::unordered_map<std::string, T> load_json_data_reflection(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return std::unordered_map<std::string, T>();
    }
    return load_json_data_reflection_from_stream<T>(file);
}


std::unordered_map<std::string, source_json_data>
    json_graph_parser_t::load_source_data(const std::string& filename)
{
    return load_json_data_reflection<source_json_data>(filename);
}

std::unordered_map<std::string, lumped_json_data>
    json_graph_parser_t::load_lumped_data(const std::string& filename)
{
    std::unordered_map<std::string, lumped_json_data> objs;
    if (!std::filesystem::exists(filename)) {
        return objs;
    }
    return load_json_data_reflection<lumped_json_data>(filename);
}

std::unordered_map<std::string, valve_json_data> json_graph_parser_t::load_valve_data(const std::string& filename)
{
    std::unordered_map<std::string, valve_json_data> objs;
    if (!std::filesystem::exists(filename)) {
        return objs;
    }
    
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(filename, pt);

    return load_json_data_reflection_from_ptree<valve_json_data>(pt);

    //return load_valves_from_ptree(pt);
}

graph_json_data json_graph_parser_t::load_json_data(
    const std::string& path, const std::string& tags_filename, const qsm_problem_type& problem_type)
{
    graph_json_data result;
    result.edges = load_topology(path + "/topology.json");

    // Обработка свойств, используемых во всех типах расчетных сценариев
    load_edges_properties(path, problem_type, result);
    
    // Обработка тегов
    load_tags_controls_data(path, tags_filename, result);

    // проверка, что у объектов разных типов имена не совпадают между собой
    result.check_names_uniqueness();
    // проверка, что топология и список объектов соответствуют друг другу
    result.check_topology_and_object_links();
    return result;
}

std::unordered_map<std::string, pde_solvers::pipe_json_data> 
json_graph_parser_t::load_pipe_data_from_string(const std::string& json_string)
{
    std::istringstream stream(json_string);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(stream, pt);

    return load_pipes_from_ptree(pt);


}

std::unordered_map<std::string, valve_json_data> 
json_graph_parser_t::load_valve_data_from_string(const std::string& json_string)
{
    std::istringstream stream(json_string);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(stream, pt);
    return load_json_data_reflection_from_ptree<valve_json_data>(pt);
    //return load_valves_from_ptree(pt);
}


void json_graph_parser_t::load_edges_properties_from_string(
    const std::string& objects_json,
    const qsm_problem_type& problem_type,
    graph_json_data& data)
{
    std::istringstream stream(objects_json);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(stream, pt);

    // Загружаем pipes из секции "pipes" (если есть)
    // Трубы могут отсутствовать в данных
    if (auto pipes_node = pt.get_child_optional("pipes")) {
        for (auto [name, value] : *pipes_node) {
            parse_pipe_from_ptree(value, data.pipes[name]);
        }
    }

    // Загружаем valves из секции "valves" (если есть)
    // Задвижки могут отсутствовать в данных
    if (auto valves_node = pt.get_child_optional("valves")) {
        data.valves = load_json_data_reflection_from_ptree<valve_json_data>(*valves_node);
    }

    // Для других типов объектов (sources, lumpeds, pumps) используем рефлексию
    // Они могут быть в отдельных секциях или отсутствовать
    if (auto sources_node = pt.get_child_optional("sources")) {
        data.sources = load_json_data_reflection_from_ptree<source_json_data>(*sources_node);
    }

    if (problem_type == qsm_problem_type::Transport) {
        if (auto lumpeds_node = pt.get_child_optional("lumpeds")) {
            data.lumpeds = load_json_data_reflection_from_ptree<lumped_json_data>(*lumpeds_node);
        }
    }

    if (auto pumps_node = pt.get_child_optional("pumps")) {
        data.pumps = load_json_data_reflection_from_ptree<lumped_json_data>(*pumps_node);
    }
}



graph_json_data json_graph_parser_t::load_json_data_from_strings(
    const std::string& topology_json,
    const std::string& objects_json,
    const qsm_problem_type& problem_type)
{
    graph_json_data result;
    
    // Загружаем топологию из JSON-строки
    result.edges = load_topology_from_string(topology_json);

    // Загружаем свойства объектов из JSON-строки
    load_edges_properties_from_string(objects_json, problem_type, result);

    // проверка, что у объектов разных типов имена не совпадают между собой
    result.check_names_uniqueness();
    // проверка, что топология и список объектов соответствуют друг другу
    result.check_topology_and_object_links();
    
    return result;
}



}