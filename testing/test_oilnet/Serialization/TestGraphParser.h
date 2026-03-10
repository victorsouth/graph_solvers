#include "test_oilnet.h"

/// @brief Публичные методы-обертки для json_graph_parser_t
class TestableJsonGraphParser : public oil_transport::json_graph_parser_t {
public:
    /// @brief Публичная обертка для protected метода load_source_data
    static std::unordered_map<std::string, oil_transport::source_json_data> load_source_data_public(const std::string& filename) {
        return TestableJsonGraphParser::load_source_data(filename);
    }
    /// @brief Публичная обертка для protected метода load_lumped_data
    static std::unordered_map<std::string, oil_transport::lumped_json_data> load_lumped_data_public(const std::string& filename) {
        return TestableJsonGraphParser::load_lumped_data(filename);
    }
    /// @brief Публичная обертка для protected метода load_valve_data
    static std::unordered_map<std::string, oil_transport::valve_json_data> load_valve_data_public(const std::string& filename) {
        return TestableJsonGraphParser::load_valve_data(filename);
    }
    /// @brief Публичная обертка для protected метода load_pump_data
    static std::unordered_map<std::string, oil_transport::pump_json_data> load_pump_data_public(const std::string& filename) {
        return TestableJsonGraphParser::load_pump_data(filename);
    }
    /// @brief Публичная обертка для protected метода load_check_valve_data
    static std::unordered_map<std::string, oil_transport::check_valve_json_data> load_check_valve_data_public(const std::string& filename) {
        return TestableJsonGraphParser::load_check_valve_data(filename);
    }
};

/// @brief Шаблонная функция для проверки зачитки JSON данных
/// Выполняет общую логику проверки: создание временного файла, зачитка данных, проверка полей
/// @tparam JsonDataType Тип данных для проверки (pump_json_data, source_json_data, valve_json_data, lumped_json_data)
/// @tparam LoadFunction Тип функции загрузки данных
/// @param json_content JSON содержимое файла
/// @param object_name Имя объекта в JSON
/// @param load_function Функция для загрузки данных из файла
/// @param temp_filename Имя временного файла
template<typename JsonDataType, typename LoadFunction>
void AssertParsesAllJsonFields(
    const std::string& json_content,
    const std::string& object_name,
    LoadFunction load_function,
    const std::string& temp_filename)
{
    using namespace oil_transport;

    // Создаем временный файл с JSON данными
    std::string temp_file_path = create_temp_file(temp_filename, json_content);

    // Act
    auto parsed_data = load_function(temp_file_path);

    // Assert
    // Проверяем, что зачитан объект
    ASSERT_EQ(parsed_data.size(), 1);
    ASSERT_TRUE(parsed_data.find(object_name) != parsed_data.end());

    // Проверяем, что все поля объекта зачитаны корректно (не содержат NaN)
    // Используем рефлексию для автоматической проверки всех полей
    const auto& data = parsed_data.at(object_name);
    AssertNoNanFields(data);

    // Проверяем, что в JSON нет лишних полей
    boost::property_tree::ptree json_ptree;
    boost::property_tree::read_json(temp_file_path, json_ptree);
    AssertNoExtraJsonFields<JsonDataType>(json_ptree, object_name);
}

/// @brief Проверяет корректность зачитки привязок тегов состояния (контролов) из файла controls_data.json
TEST(GraphParser, ParsesControlTagsForEdges2)
{
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    std::string tags_filename = scheme_folder + "/tags_data.json";
    const std::string etalon_control_name = "Vlv1_Status";
    constexpr auto etalon_control_type = control_tag_conversion_type::valve_state_scada;

    // Act
    graph_siso_data siso_data =
        json_graph_parser_t::parse_transport<qsm_pipe_transport_parameters_t>(
            scheme_folder + "", tags_filename);

    // Assert
    ASSERT_EQ(siso_data.controls2.front().name, etalon_control_name);
    ASSERT_EQ(siso_data.controls2.front().type, etalon_control_type);
}

/// @brief Проверяет способность json_graph_parser_t распарсить схемы через JSON-строку
/// Создается простейшая схема поставщик-задвижка-потребитель, повторяющая схему valve_control_test/scheme/
/// Проверяются зачитанные параметры в graph_siso_data
TEST(GraphParser, ParsesFromStringSimpleScheme)
{
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange
    // Топология: поставщик (Src) -> задвижка (Vlv1) -> потребитель (Snk)
    const std::string topology_json = R"({
        "Src": {
            "to": 0
        },
        "Vlv1": {
            "from": 0,
            "to": 1
        },
        "Snk": {
            "from": 1
        }
    })";

    // Параметры объектов: поставщик, задвижка, потребитель
    const std::string objects_json = R"({
        "sources": {
            "Src": {
                "std_volflow": 1.0
            },
            "Snk": {
                "pressure": 1e5
            }
        },
        "valves": {
            "Vlv1": {
                "initial_opening": 100,
                "xi": 0.03,
                "diameter": 0.2
            }
        }
    })";

    // Act
    graph_siso_data siso_data = json_graph_parser_t::parse_hydro_transport_from_strings<qsm_pipe_transport_parameters_t>(
        topology_json, objects_json);

    // Assert
    // Проверка топологии: должны быть 3 объекта
    ASSERT_EQ(siso_data.name_to_binding.size(), 3);
    ASSERT_TRUE(siso_data.name_to_binding.find("Src") != siso_data.name_to_binding.end());
    ASSERT_TRUE(siso_data.name_to_binding.find("Vlv1") != siso_data.name_to_binding.end());
    ASSERT_TRUE(siso_data.name_to_binding.find("Snk") != siso_data.name_to_binding.end());

    // Проверка параметров поставщика (Src) - транспортные
    const auto& src_transport = siso_data.get_properties<transport_source_parameters_t>("Src");
    ASSERT_FALSE(src_transport.is_initially_zeroflow);

    // Проверка параметров поставщика (Src) - гидравлические
    auto& src_hydro = siso_data.get_properties<qsm_hydro_source_parameters>("Src", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(src_hydro.std_volflow, 1.0);

    // Проверка параметров задвижки (Vlv1) - транспортные
    const auto& vlv1_transport = siso_data.get_properties<transport_lumped_parameters_t>("Vlv1");
    ASSERT_TRUE(vlv1_transport.is_initially_opened); // initial_opening = 100 > 0

    // Проверка параметров задвижки (Vlv1) - гидравлические
    auto& vlv1_hydro = siso_data.get_properties<qsm_hydro_local_resistance_parameters>("Vlv1", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(vlv1_hydro.initial_opening, 100.0);
    ASSERT_DOUBLE_EQ(vlv1_hydro.xi, 0.03);
    ASSERT_DOUBLE_EQ(vlv1_hydro.diameter, 0.2);

    // Проверка параметров потребителя (Snk) - гидравлические
    auto& snk_hydro = siso_data.get_properties<qsm_hydro_source_parameters>("Snk", qsm_problem_type::HydroTransport);
    ASSERT_DOUBLE_EQ(snk_hydro.pressure, 1e5);
}

/// @brief Проверяет зачитку всех свойств насоса из JSON формата
/// Тест использует рефлексию для автоматической проверки всех полей без явного перечисления
/// Это гарантирует, что при добавлении нового поля в структуру тест автоматически проверит его зачитку
TEST(GraphParser, ParsesAllPumpFieldsFromJson)
{
    using namespace oil_transport;

    // Arrange - JSON данные для одного насоса
    const std::string pump_json_content = R"({
    "TEST_PUMP": {
        "head_characteristic": [250.739, -0.013956, -2.1523e-6, -2.6927e-10],
        "head_characteristic_alternative": [303.703928, -0.03989472, 0, 0],
        "switching_volumetric_flow_for_head_characteristic": 1.0,
        "efficiency_characteristic": [0.0076669, 0.00070543, -1.8266e-7, 1.3355e-11],
        "initial_frequency": 1.0,
        "nominal_frequency": 3000.0,
        "rotor_wheel_reducing": 0.0,
        "is_initially_started": true
    }
    })";

    // Act - парсинг JSON для насоса. Assert - все возможные поля структуры для насоса заданы и не заданы никакие лишние поля
    AssertParsesAllJsonFields<pump_json_data>(
        pump_json_content,
        "TEST_PUMP",
        TestableJsonGraphParser::load_pump_data_public,
        "test_pump_data.json"
    );
}

/// @brief Проверяет зачитку всех свойств источника из JSON формата
/// Рефлексия гарантирует, что при добавлении нового поля в структуру тест автоматически проверит его зачитку
TEST(GraphParser, ParsesAllSourceFieldsFromJson)
{
    using namespace oil_transport;

    // Arrange - JSON данные для одного источника
    const std::string source_json_content = R"({
    "TEST_SOURCE": {
        "is_initially_zeroflow": false,
        "density": 850.0,
        "temperature": 20.0,
        "pressure": 1e5,
        "std_volflow": 1.5
    }
    })";

    // Act - парсинг JSON для источника. 
    // Assert - все возможные поля структуры для источника заданы и не заданы никакие лишние поля
    AssertParsesAllJsonFields<source_json_data>(
        source_json_content,
        "TEST_SOURCE",
        TestableJsonGraphParser::load_source_data_public,
        "test_source_data.json"
    );
}

/// @brief Проверяет зачитку всех свойств задвижки из JSON формата
TEST(GraphParser, ParsesAllValveFieldsFromJson)
{
    using namespace oil_transport;

    // Arrange - JSON данные для одной задвижки
    const std::string valve_json_content = R"({
    "TEST_VALVE": {
        "initial_opening": 100.0,
        "xi": 0.03,
        "diameter": 0.2
    }
    })";

    // Act - парсинг JSON для задвижки. 
    // Assert - все возможные поля структуры для задвижки заданы и не заданы никакие лишние поля
    AssertParsesAllJsonFields<valve_json_data>(
        valve_json_content,
        "TEST_VALVE",
        TestableJsonGraphParser::load_valve_data_public,
        "test_valve_data.json"
    );
}

/// @brief Проверяет зачитку всех свойств сосредоточенного объекта из JSON формата
TEST(GraphParser, ParsesAllLumpedFieldsFromJson)
{
    using namespace oil_transport;

    // Arrange - JSON данные для одного сосредоточенного объекта
    const std::string lumped_json_content = R"({
    "TEST_LUMPED": {
        "is_initially_opened": true
    }
    })";

    // Act - парсинг JSON для сосредоточенного объекта. 
    // Assert - все возможные поля структуры для сосредоточенного объекта заданы и не заданы никакие лишние поля
    AssertParsesAllJsonFields<lumped_json_data>(
        lumped_json_content,
        "TEST_LUMPED",
        TestableJsonGraphParser::load_lumped_data_public,
        "test_lumped_data.json"
    );
}

/// @brief Проверяет зачитку всех свойств обратного клапана из JSON формата
TEST(GraphParser, ParsesAllCheckValveFieldsFromJson)
{
    using namespace oil_transport;

    // Arrange - JSON данные для одного обратного клапана
    const std::string check_valve_json_content = R"({
    "TEST_CHECK_VALVE": {
        "hydraulic_resistance": 0.5,
        "diameter": 0.3,
        "opening": 1.0,
        "is_initially_closed_static": "False"
    }
    })";

    // Act - парсинг JSON для обратного клапана. 
    // Assert - все возможные поля структуры для обратного клапана заданы и не заданы никакие лишние поля
    AssertParsesAllJsonFields<check_valve_json_data>(
        check_valve_json_content,
        "TEST_CHECK_VALVE",
        TestableJsonGraphParser::load_check_valve_data_public,
        "test_check_valve_data.json"
    );
}

/// @brief Тест проверяет корректность работы генератора цепных схем
/// Создается простая схема source -> valve -> source с уникальными параметрами,
/// затем схема читается обратно и проверяется, что параметры совпадают
TEST(ChainSchemeGenerator, CreatesSchemeFromPassedData) {
    using namespace oil_transport;
    using namespace graphlib;

    // Arrange - создаем уникальные параметры для каждого объекта
    source_json_data src1 = source_json_data::default_values();
    src1.std_volflow = 10.5;
    src1.pressure = 1e5;
    src1.temperature = 293.15;

    valve_json_data valve = valve_json_data::default_values();
    valve.initial_opening = 75.0;
    valve.xi = 0.05;
    valve.diameter = 0.3;

    source_json_data snk = source_json_data::default_values();
    snk.pressure = 0.8e5;
    snk.density = 860.0;
    snk.temperature = 295.0;
    snk.std_volflow = std::numeric_limits<double>::quiet_NaN();
    snk.is_initially_zeroflow = true;

    // Создаем временную директорию для схемы
    std::string scheme_dir = prepare_test_folder("chain_scheme_test");

    // Act - генерируем схему через chain_scheme_generator_t
    using Generator = chain_scheme_generator_t<source_json_data, valve_json_data, source_json_data>;
    Generator::generate(std::make_tuple(src1, valve, snk), scheme_dir);

    // Читаем схему обратно
    graph_json_data loaded_data = json_graph_parser_t::load_json_data(
        scheme_dir, "", qsm_problem_type::HydroTransport);

    // Assert - проверяем, что параметры совпадают
    // Проверка первого источника (Src)
    ASSERT_TRUE(loaded_data.sources.find("Src") != loaded_data.sources.end());
    const auto& loaded_src1 = loaded_data.sources.at("Src");
    ASSERT_TRUE(compare_json_data(loaded_src1, src1)) << "Source Src data mismatch";

    // Проверка задвижки (Obj1)
    ASSERT_TRUE(loaded_data.valves.find("Obj1") != loaded_data.valves.end());
    const auto& loaded_valve = loaded_data.valves.at("Obj1");
    ASSERT_TRUE(compare_json_data(loaded_valve, valve)) << "Valve Obj1 data mismatch";

    // Проверка второго источника (Snk)
    ASSERT_TRUE(loaded_data.sources.find("Snk") != loaded_data.sources.end());
    const auto& loaded_snk = loaded_data.sources.at("Snk");
    ASSERT_TRUE(compare_json_data(loaded_snk, snk)) << "Source Snk data mismatch";

    // Проверка топологии - должны быть 3 объекта
    ASSERT_EQ(loaded_data.edges.size(), 3);
    
    // Проверяем, что все объекты присутствуют в топологии
    std::set<std::string> edge_names;
    for (const auto& edge : loaded_data.edges) {
        edge_names.insert(edge.model);
    }
    ASSERT_TRUE(edge_names.find("Src") != edge_names.end());
    ASSERT_TRUE(edge_names.find("Obj1") != edge_names.end());
    ASSERT_TRUE(edge_names.find("Snk") != edge_names.end());
}

