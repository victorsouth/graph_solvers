#pragma once



namespace oil_transport {
;

/// @brief Управление объектом
struct transport_object_control_t {
    /// @brief Привязка объекта на графе
    graphlib::graph_binding_t binding;
    /// @brief Нужен виртуальный деструктор, чтобы не словить утечку памяти
    virtual ~transport_object_control_t() = default; 
};


/// @brief Forward declaration структуры измерений в привязке к SISO графу
/// @details Полное определение находится в measurements/transport_measurements.h
struct transport_siso_measurements;

/// @brief Транспортная модель, выполняющая вычисления для объекта при транспортном расчете
/// Базовый класс для объектов всех типов
class oil_transport_model_t {
public:
    /// @brief Получить тип модели с точки зрения декомпозиции графа
    /// @return Все транспортные модели полагаются Instant, кроме закрытой арматуры
    virtual graphlib::computational_type model_type() const = 0;
    /// @brief Проверить, имеет ли поставщик строго нулевой расход 
    /// Имеет смысл только для поставщиков - притоков, БИК, закрытых задвижек
    virtual bool is_zero_flow() const = 0;
    /// @brief Для получения модели одной из частей рассеченного ребра
    /// @param is_end_side Получить начало или конец ребра после рассечения
    /// @return Модель части рассеченного ребра
    virtual oil_transport_model_t* get_single_sided_model(bool is_end_side) = 0;
    /// @brief Управление объектом 
    virtual void send_control(const transport_object_control_t& control) = 0;
    /// @brief Расчет продвижения эндогенных свойств по ребру, которому соответствует модель
    /// @param time_step Временной шаг
    /// @param volumetric_flow Расход для определения начала и конца ребра
    /// @param endogenous_input Эндогенные свойства с предшествующего ребра
    /// @param endogenous_measurements Измерения, из которых могут быть переопределены расчетные значения эндогенных свойств
    virtual void propagate_endogenous(
        double time_step,
        double volumetric_flow,
        const pde_solvers::endogenous_values_t* endogenous_input,
        const std::vector<graph_quantity_value_t>& endogenous_measurements,
        std::vector<graph_quantity_value_t>* overriden_with_measurements) = 0;
    /// @brief Эндогенные свойства на выходе ребра
    /// @param volumetric_flow Расход через ребро для выявления фактической ориентации ребра
    /// @return Эндогенные свойства в формате распространения между ребрами
    virtual pde_solvers::endogenous_values_t get_endogenous_output(double volumetric_flow) const = 0;
    /// @brief Возврашает буфер модели
    virtual void* get_edge_buffer() const = 0;
    /// @brief Запись значения расхода в буфер модели
    virtual void update_flow(graphlib::layer_offset_t layer_offset, double flow) = 0;
    /// @brief Возвращает рассчитанный ранее объемный расход через дугу
    /// @details Берется объемный расход из буфера гидравлической модели источника/потребителя
    /// @param estimation_type Тип оценки для выбора слоя буфера
    virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) = 0;
};


/// @brief Транспортная модель и ее инцидентности
typedef graphlib::model_incidences_t < oil_transport_model_t* > transport_model_incidences_t;
/// @brief Граф с исходной полной топологией, но объектам которого сопоставлены транспортные модели (не гидравлические)
typedef graphlib::basic_graph_t<transport_model_incidences_t> transport_graph_t;
/// @brief Дерево блоков на графе с транспортными моделями (не гидравлическими!)
typedef graphlib::block_tree_t<transport_model_incidences_t> transport_block_tree_t;
/// @brief Вспомогательный класс для формирования дерева блоков на транспортном графе
typedef graphlib::block_tree_builder_t<transport_model_incidences_t> block_tree_builder_t; // версия graphlib

/// @brief Заглушка - дефолтная реализация состояния объекта
/// При инициализации состояния в конструкторе объекта, вызывается конструктор состояния
struct transport_model_state_t {
    /// @brief Заглушка, чтобы инициализировать состояние модели трубы
    /// Специально ничего не делаем, чтобы компилировалось
    /// Для кастомной инициализации, надо отнаследовать
    template <typename ModelProperties>
    transport_model_state_t(const ModelProperties&)
    {}    
};

/// @brief Базовый реализация транспортной модели 
/// (копирует параметры модели и берет ссылку на буфер)
/// @tparam ModelParameters Тип данных, определяющий параметры объекта транспортного расчета 
template <typename ModelParameters, typename ModelState = transport_model_state_t>
class oil_transport_model_base_t : public oil_transport_model_t
{
protected:
    /// @brief Параметры объекта для транспортного расчета
    const ModelParameters parameters;
    /// @brief Селектор рассчитываемых эндогенных параметров
    const pde_solvers::endogenous_selector_t endogenous_selector;
    /// @brief Текущее состояние модели (закрыта/открыта задвижки и т.п.)
    mutable ModelState state;
protected:
    /// @brief Конструктор не инициализирует ссылку на буфер. 
    /// Это может быть zeroflow-поставщик, инициализированные на буфере закрытой задвижки
    /// Остальное - как у основго конструктора
    oil_transport_model_base_t(const ModelParameters& parameters,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
        : parameters(parameters)
        , endogenous_selector(endogenous_selector)
        , state(parameters)
    { }
    /// @brief Дефолтная реализация - кидает исключение
    virtual void send_control(const transport_object_control_t& control) override {
        throw std::runtime_error("Please, implement me in successor");
    }
};

/// @brief Проверяет, является ли одностороннее ребро непроточным
inline bool is_zero_flow_transport_edge(const oil_transport::oil_transport_model_t* model)
{
    if (model == nullptr) {
        throw std::runtime_error("Cannot determine model zeroflow property, model == nullptr");
    }
    return model->is_zero_flow();
}

/// @brief Проверяет, является ли одностороннее ребро непроточным
inline bool is_zero_flow_transport_incidences(const oil_transport::transport_model_incidences_t& model)
{
    return is_zero_flow_transport_edge(model.model);
}


}