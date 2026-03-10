
namespace oil_transport {
;

/// @brief Управление гидравлическим объектом
struct hydraulic_object_control_t {
    /// @brief Привязка объекта на графе
    graphlib::graph_binding_t binding;
    /// @brief Нужен виртуальный деструктор, чтобы не словить утечку памяти
    virtual ~hydraulic_object_control_t() = default; 
};

/// @brief Гидравлическая модель, выполняющая вычисления для объекта при гидравлическом расчете
/// Базовый класс для объектов всех типов
class hydraulic_model_t : public graphlib::closing_relation_t {
public:
    virtual ~hydraulic_model_t() = default;
    
    /// @brief Получить тип модели с точки зрения декомпозиции графа
    /// @return Тип модели для гидравлического расчета
    virtual graphlib::computational_type model_type() const = 0;
    
    /// @brief Проверить, имеет ли поставщик строго нулевой расход 
    virtual bool is_zero_flow() const = 0;
    
    /// @brief Для получения модели одной из частей рассеченного ребра
    /// @param is_end_side Получить начало или конец ребра после рассечения
    /// @return Модель части рассеченного ребра
    virtual hydraulic_model_t* get_single_sided_model(bool is_end_side) = 0;
    
    /// @brief Управление объектом 
    virtual void send_control(const hydraulic_object_control_t& control) = 0;

    /// @brief Возвращает буфер модели
    /// @return Указатель на буфер модели (для труб - ring_buffer, для sources - endohydro_values_t, для lumpeds - массив указателей)
    virtual void* get_edge_buffer() const = 0;

    /// @brief Возвращает замыкающее соотношение МД для данного объекта
    /// @return Указатель на closing_relation_t
    //virtual graphlib::closing_relation_t* get_closing_relation() = 0;
    
    /// @brief Возвращает константный указатель на замыкающее соотношение МД
    //virtual const graphlib::closing_relation_t* get_closing_relation() const = 0;
};

/// @brief Гидравлическая модель и ее инцидентности
typedef graphlib::model_incidences_t < hydraulic_model_t* > hydraulic_model_incidences_t;
/// @brief Граф с исходной полной топологией, но объектам которого сопоставлены гидравлические модели
typedef graphlib::basic_graph_t<hydraulic_model_incidences_t> hydraulic_graph_t;

/// @brief Заглушка - дефолтная реализация состояния объекта
/// При инициализации состояния в конструкторе объекта, вызывается конструктор состояния
struct hydraulic_model_state_t {
    /// @brief Заглушка, чтобы инициализировать состояние модели
    /// Специально ничего не делаем, чтобы компилировалось
    /// Для кастомной инициализации, надо отнаследовать
    template <typename ModelProperties>
    hydraulic_model_state_t(const ModelProperties&)
    {}    
};

/// @brief Базовый реализация гидравлической модели 
/// (копирует параметры модели и хранит замыкающее соотношение)
/// @tparam ModelParameters Тип данных, определяющий параметры объекта гидравлического расчета 
/// @tparam ModelState Тип состояния модели
template <typename ModelParameters, typename ModelState = hydraulic_model_state_t>
class hydraulic_model_base_t : public hydraulic_model_t
{
protected:
    /// @brief Параметры объекта для гидравлического расчета
    const ModelParameters parameters;
    /// @brief Текущее состояние модели
    mutable ModelState state;
    
protected:
    /// @brief Конструктор инициализирует параметры и состояние
    hydraulic_model_base_t(const ModelParameters& parameters)
        : parameters(parameters)
        , state(parameters)
    { }
    
    /// @brief Дефолтная реализация - кидает исключение
    virtual void send_control(const hydraulic_object_control_t& control) {
        throw std::runtime_error("Please, implement me in successor");
    }

    /// @brief Численная производная расхода по давлениям (дефолт для двусторонних моделей)
    std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) {
        auto flow_by_p_in = [&](double pressure_in) {
            return calculate_flow(pressure_in, pressure_out);
        };

        auto flow_by_p_out = [&](double pressure_out) {
            return calculate_flow(pressure_in, pressure_out);
        };

        // Аналитическое решение не имеет производной в точке (dP=0, Q=0). Считаем численно.
        double dQ_dP_in = two_sided_derivative(flow_by_p_in, pressure_in, 1e-6);
        double dQ_dP_out = two_sided_derivative(flow_by_p_out, pressure_out, 1e-6);

        return { dQ_dP_in, dQ_dP_out };
    }

//public:
//    /// @brief Возвращает замыкающее соотношение МД для данного объекта
//    virtual graphlib::closing_relation_t* get_closing_relation() override {
//        return closing_relation.get();
//    }
//    
//    /// @brief Возвращает константный указатель на замыкающее соотношение МД
//    virtual const graphlib::closing_relation_t* get_closing_relation() const override {
//        return closing_relation.get();
//    }
};

/// @brief Проверяет, является ли одностороннее ребро непроточным
inline bool is_zero_flow_hydro_edge(const hydraulic_model_t* model)
{
    if (model == nullptr) {
        throw std::runtime_error("Cannot determine model zeroflow property, model == nullptr");
    }
    return model->is_zero_flow();
}

/// @brief Проверяет, является ли одностороннее ребро непроточным
inline bool is_zero_flow_hydro_incidences(const hydraulic_model_incidences_t& model)
{
    return is_zero_flow_hydro_edge(model.model);
}

}
