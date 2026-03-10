#pragma once

namespace oil_transport {
;

/// @brief Тип измерения физической величины
enum class measurement_type {
    mass_flow,
    vol_flow_std,
    vol_flow_working,
    temperature,
    density_working,
    density_std,
    viscosity_working,
    sulfur,
    mass_flow_improver,
    vol_flow_improver,
    pressure
};

/// @brief Проверяет, является ли измерение эндогенным измерением
inline bool is_endogenous(measurement_type type) {
    bool is_flow = type == measurement_type::mass_flow ||
        type == measurement_type::vol_flow_std ||
        type == measurement_type::vol_flow_working;
    return !is_flow;
}

/// @brief Сопоставление типов измерений в коде с их строковым наименованием
inline static const std::map<measurement_type, const char*> measurement_type_converter{
    {measurement_type::mass_flow, "mass_flow"},
    {measurement_type::vol_flow_std, "vol_flow_std"},
    {measurement_type::vol_flow_working, "vol_flow_working"},
    {measurement_type::temperature, "temperature"},
    {measurement_type::density_working, "density_working"},
    {measurement_type::density_std, "density_std"},
    {measurement_type::viscosity_working, "viscosity_working"},
    {measurement_type::sulfur, "sulfur"},
    {measurement_type::mass_flow_improver, "mass_flow_improver"},
    {measurement_type::vol_flow_improver, "vol_flow_improver"}
};

/// @brief Возвращает строковое наименование для данного типа измерения
inline std::string to_string(measurement_type type) {
    std::string result(measurement_type_converter.at(type));
    return result;
}

/// @brief Тип преобразования значений 
/// состояния оборудования из SCADA системы
enum class control_tag_conversion_type {
    valve_state_scada,
    pump_state_scada,
};

/// @brief Сопоставление типов преобразования состояний в коде с их строковым наименованием
inline static const std::map<control_tag_conversion_type, const std::string> control_type_converter{
    { control_tag_conversion_type::valve_state_scada, "valve_state_scada" },
    { control_tag_conversion_type::pump_state_scada, "pump_state_scada" }
};


/// @brief Правило преобразования измерений состояния для объектов транспортного расчета
struct transport_control_conversion_rule_t {
    /// @brief Отображение значений управления для преобразования
    const std::unordered_map<int, int> conversion_map;
    /// @brief Значение, при обнаружении которого во временном ряде измерений выбрасывается исключение
    /// (например, задвижка в аварии)
    const int exception_value = std::numeric_limits<int>::max();
};

inline static const std::unordered_map<control_tag_conversion_type, transport_control_conversion_rule_t> conversion_rules = {
    {
        control_tag_conversion_type::valve_state_scada,
        {
            .conversion_map = {
                {-1, -1},  // авария → -1
                {1, 1},    // открыта
                {2, 1},    // закрывается
                {3, 0},    // закрыта
                {4, 1},    // открывается
                {5, 1},    // промежуточное
                {6, 1},    // в движении
            },
            .exception_value = -1  // если результат == -1 → ошибка
        }
    }
};

/// @brief Метаинформация о теге состояния объекта
struct control_tag_info_t {
    /// @brief Имя тега состояния объекта
    std::string name;
    /// @brief Тип состояния (для преобразования int->bool значений, хранящихся по тегу)
    control_tag_conversion_type type{ control_tag_conversion_type::valve_state_scada };
};


/// @brief Информация о теге измерения
struct measurement_tag_info_t : public tag_info_t {
    /// @brief Тип измерения             
    measurement_type type{ measurement_type::sulfur };
};

/// @brief Параметры измерения                   
using transport_measurement_data_t = measurement_data_t<measurement_type>;



/// @brief Привязка физической величины на графе (место привязки + тип физической величины)                                        
struct graph_quantity_binding_t : public graphlib::graph_place_binding_t {
    /// @brief Тип физической величины
    measurement_type type{ measurement_type::sulfur };
    /// @brief Проверяет равенство двух привязок                 
    bool operator==(const graph_quantity_binding_t& other) const
    {
        bool binding_equal =
            static_cast<const graphlib::graph_place_binding_t&> (*this) ==
            static_cast<const graphlib::graph_place_binding_t&> (other);

        bool type_equal = type == other.type;
        return binding_equal && type_equal;
    }
    /// @brief Функтор хэширования
    struct hash {
        /// @brief Возвращает хэш для заданной привязки                                     
        size_t operator()(const graph_quantity_binding_t& binding) const
        {
            std::hash<size_t> hash_fn;
            auto binding_type_as_integral = static_cast<graphlib::graph_binding_integral_type>(binding.binding_type);
            size_t result = (hash_fn(binding.edge)) ^ (hash_fn(binding_type_as_integral) >> 30);
            return result;
        }
    };
};

/// @brief Измерение или расчетное значение физической величины, привязанной к некоторому месту на графе
struct graph_quantity_value_t : public graph_quantity_binding_t {
    /// @brief Численное значение привязанной величины
    measurement_untyped_data_t data;
    /// @brief Оператор присваивания из ориентированной привязки к графу
    /// @param binding Привязка к месту в графе
    /// @return Ссылка на текущий объект
    graph_quantity_value_t& operator=(const graphlib::graph_place_binding_t& binding)
    {
        static_cast<graphlib::graph_place_binding_t&>(*this) = binding;
        return *this;
    }
    /// @brief Оператор присваивания из привязки к графу
    /// @param binding Привязка к графу
    /// @return Ссылка на текущий объект
    graph_quantity_value_t& operator=(const graphlib::graph_binding_t& binding)
    {
        static_cast<graphlib::graph_place_binding_t&>(*this) = binding;
        return *this;
    }
    /// @brief Оператор присваивания копированием
    graph_quantity_value_t& operator=(const graph_quantity_value_t& other) = default;
    /// @brief Конструктор копирования
    graph_quantity_value_t(const graph_quantity_value_t& other) = default;
    graph_quantity_value_t() = default;

    /// @brief Конструктор из ориентированной привязки на графе, типа измерения и значения
    /// @param place_binding Привязка к месту в графе
    /// @param type Тип измерения
    /// @param value Значение измерения
    graph_quantity_value_t(const graphlib::graph_place_binding_t& place_binding,
        measurement_type type, double value)
    {
        *this = place_binding;
        this->type = type;
        this->data.value = value;
    }
};



template <typename T>
/// @brief Шаблон измерения для объекта
/// @tparam T Тип значения измерения
struct measurement_template {
    /// @brief Массовый расход               
    T mass_flow;
    /// @brief Объемный расход, приведенный к стандартным условиям (20 °С ?)
    T vol_flow_std;
    /// @brief Объемный расход в рабочих условиях                                  
    T vol_flow_working;
    /// @brief Температура            
    T temperature;
    /// @brief Плотность в рабочих условиях                             
    T density_working;
    /// @brief Плотность при 20 °С   
    T density_std;
    /// @brief Вязкость в рабочих условиях                             
    T viscosity_working;
    /// @brief Серосодержание               
    T sulfur;
    /// @brief Массовый расход ПТП (эндогенный, пересчитаем в концентрацию)
    T mass_flow_improver;
    /// @brief Объемный расход ПТП (эндогенный, пересчитаем в концентрацию)
    T vol_flow_improver;
};

/// @brief Контейнер для хранения значений измерений                                        
typedef measurement_template<transport_measurement_data_t> measurements_container_t;

/// @brief Теги для транспортного объекта
struct object_measurement_tags_t {
    /// @brief Теги измерений на входе объекта
    std::vector<measurement_tag_info_t> in;
    /// @brief Теги измерений на выходе объекта
    std::vector<measurement_tag_info_t> out;
    /// @brief Объединяет теги измерений двух объектов
    /// @param other Теги измерений другого объекта
    /// @return Объединенные теги измерений
    object_measurement_tags_t operator + (const object_measurement_tags_t& other) const
    {
        object_measurement_tags_t result = *this;
        result.in.insert(result.in.end(), other.in.begin(), other.in.end());
        result.out.insert(result.out.end(), other.out.begin(), other.out.end());

        return result;
    }
};


/// @brief Измерения в приязке к SISO графу     
struct transport_siso_measurements
{
    /// @brief Идентификатор точки привязки замеров в SISO графе     
    graphlib::graph_place_binding_t binding;
    /// @brief Контейнер для хранения значений измерений                                          
    measurements_container_t data;
};

// forward declaration
void overwrite_endogenous_values_from_measurements(
    const std::vector<graph_quantity_value_t>& measurements,
    std::vector<graph_quantity_value_t>* overwritten_by_measurements,
    pde_solvers::endogenous_values_t* _calculated_endogenous);



transport_siso_measurements convert_to_measurement_group(
    const graph_quantity_value_t& measurement
);


}