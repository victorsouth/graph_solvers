#pragma once

namespace oil_transport {
;

/// @brief Гидравлический расчет нескольких квазистационарных режимов, 
/// выделенных при транспортном расчете
/// @tparam TaskType Тип расчетной задачи
template <typename TaskType>
class quasi_stationary_regimes_hydro_processor {
private:
    /// @brief Расчетная задача
    TaskType& task;
    /// @brief Модельная сетка в абсолютном времени
    const std::vector<std::time_t> astro_times;
    /// @brief Маппинг идентификаторов ребер к именам объектов для записи в JSON
    std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash> edge_id_to_name;
    
    /// @brief Флаг, указывающий, был ли записан первый слой (для правильной расстановки запятых)
    bool first_layer_written;
    /// @brief Выходной поток для записи JSON
    std::ostream& json_output_stream;

    /// @brief Поток для чтения результатов транспортного расчета в формате JSON
    std::istream& json_input_stream;

    /// @brief Кэшированный прочитанный JSON (чтобы не читать поток несколько раз)
    boost::property_tree::ptree cached_json_root;
    /// @brief Кэширован ли JSON с результатами транспортного расчета
    bool json_root_cached;

    /// @brief Хранилище для восстановления эндогенных расчетных параметров
    historical_layers_storage_t<typename TaskType::pipe_solver_type> storage;

public:
    /// @brief Конструктор
    /// @param task Расчетная задача
    /// @param astro_times Модельная сетка в абсолютном времени
    /// @param transport_edge_id_to_name Маппинг идентификаторов ребер транспортного графа к именам объектов
    /// @param input_stream Поток для чтения результатов транспортного расчета в формате JSON
    /// @param output_stream Выходной поток для записи JSON
    quasi_stationary_regimes_hydro_processor(
        TaskType& task,
        const std::vector<std::time_t>& astro_times,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& transport_edge_id_to_name,
        std::istream& input_stream, std::ostream& output_stream)
        : task(task)
        , astro_times(astro_times)
        , edge_id_to_name(transport_edge_id_to_name)
        , first_layer_written(false)
        , json_output_stream(output_stream)
        , json_input_stream(input_stream)
        , json_root_cached(false)
    {
        if (edge_id_to_name.empty()) {
            throw std::invalid_argument("Edge ID to name mapping cannot be empty");
        }

        // Читаем JSON один раз и кэшируем его
        try {
            boost::property_tree::read_json(json_input_stream, cached_json_root);
            json_root_cached = true;
        }
        catch (const boost::property_tree::json_parser_error& e) {
            throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
        }

        step_results_json_writer<typename TaskType::pipe_solver_type>::start_json_structure(json_output_stream);
    }

    /// @brief Выполняет гидравлический расчет и записывает профили в JSON
    /// @param time_step Временной шаг
    /// @param index Индекс слоя
    /// @param controls Управление объектами для данного момента времени
    void operator()(double time_step, std::size_t index,
        const std::vector<const typename TaskType::base_control_type*>& controls)
    {
        task.send_controls(controls);

        // Используем кэшированный JSON вместо повторного чтения потока
        if (!json_root_cached) {
            throw std::runtime_error("JSON root not cached");
        }
        boost::property_tree::ptree layer_node = 
            oil_transport::step_results_json_reader::get_layer_node_from_tree(cached_json_root, index);
        
        // Зачитка эндогенных свойств из результатов и восстановление эндогенов в объектах гидравлического графа
        storage.restore_from_json_layer_node(layer_node, edge_id_to_name);
        storage.restore_results(task.get_graph(), index, edge_id_to_name);

        task.solve();

        const auto& results = task.get_result();

        transport_graph_t empty_transport_graph;
        step_results_json_writer<typename TaskType::pipe_solver_type>::write_single_layer(
            results, index, edge_id_to_name, empty_transport_graph, json_output_stream, first_layer_written);

        first_layer_written = true;

        if (index == astro_times.size() - 1) {
            // После последнего слоя закрывем json
            step_results_json_writer<typename TaskType::pipe_solver_type>::end_json_structure(json_output_stream);
            json_output_stream.flush();
        }

    }
};

/// @brief Правило выделения квазистационарного режима при пакетном транспортном расчете
enum class quasi_stationary_regime_selection_strategy {
    /// @brief Режим фиксируется каждый час в течение интервала пакетного расчета
    EveryHour,
    /// @brief Каждый шаг транспортного расчета считается режимом
    EveryStep
};

/// @brief Выделяет квазистационарные режимы на временном интервале моделирования
/// в соответствии с заданной стратегией
template <typename TaskType>
class quasi_stationary_regime_selector {

private:
    /// @brief Тип результатов эндогенного расчета эндогенного расчета
    using task_result_t = task_common_result_t<typename TaskType::pipe_solver_type,
        typename TaskType::lumped_calc_parameters_type>;

private:
    /// @brief Правило выделения квазистационарного режима
    quasi_stationary_regime_selection_strategy regime_strategy;

private:
    /// @brief Простая стратегия выявления квазистац режима - "Режим каждый час"
    bool check_every_hour_strategy(double time_step, std::size_t index) {
        return std::abs(std::fmod(index * time_step, 3600.0)) < 1e-4;
    }

public:
    /// @brief Конструктор
    quasi_stationary_regime_selector(
        quasi_stationary_regime_selection_strategy regime_strategy)
        : regime_strategy(regime_strategy)
    {
    }

    /// @brief Проверяет, является ли текущее состояние (сети, трубы?) квазистационарным режимом
    bool check_state_for_regime(double time_step, std::size_t current_step_index,
        const task_result_t& current_step_result) 
    {
        if (regime_strategy == quasi_stationary_regime_selection_strategy::EveryHour)
            return check_every_hour_strategy(time_step, current_step_index);
        else if (regime_strategy == quasi_stationary_regime_selection_strategy::EveryStep)
            return true;

        // TODO: видимо, для более сложных стратегий понадобится 
        // хранить несколько последних результатов расчета

        throw std::runtime_error("Unsupported quasi stationary regime selection strategy");
    }

};

/// @brief Пакетный расчет с записью результатов в JSON 
/// после каждого шага (потоковая запись без накопления в памяти)
/// @tparam TaskType Тип расчетной задачи
template <typename TaskType>
class batch_serializing_result {
private:
    /// @brief Расчетная задача
    TaskType& task;
    /// @brief Модельная сетка в абсолютном времени
    const std::vector<std::time_t>& astro_times;

    quasi_stationary_regime_selector<TaskType> regime_selector;

private:
    /// @brief Сериализует результаты расчета
    void serialize_results(
        const task_common_result_t<typename TaskType::pipe_solver_type,
        typename TaskType::lumped_calc_parameters_type>& results,
        std::size_t index)
    {
        if constexpr (std::is_same_v<TaskType,
            hydro_transport_task_template<typename TaskType::pipe_solver_type,
            typename TaskType::lumped_calc_parameters_type>>)
        {
            // Запись результатов для совмещенного таска (specific по транспортным и гидравлическим моделям)
            const transport_graph_t& transport_graph = task.get_transport_graph();
            const hydraulic_graph_t& hydro_graph = task.get_hydro_graph();
            (void)hydro_graph;

            // TODO: Похоже, после реализации hydro_specific понадобится перегрузка step_results_json_writer под два графа
            step_results_json_writer<typename TaskType::pipe_solver_type>::write_single_layer(
                results, index, edge_id_to_name, transport_graph, json_output_stream, first_layer_written);
        }
        else {
            // Здесь либо только транспортный граф, либо только гидравлический граф.
            // Оба случая обрабатываются единообразно
            const auto& graph = task.get_graph();
            step_results_json_writer<typename TaskType::pipe_solver_type>::write_single_layer(
                results, index, edge_id_to_name, graph, json_output_stream, first_layer_written);
        }

        first_layer_written = true;
    }

private:
    /// @brief Маппинг идентификаторов ребер к именам объектов для записи в JSON
    std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash> edge_id_to_name;
    /// @brief Флаг, указывающий, был ли записан первый слой (для правильной расстановки запятых)
    bool first_layer_written;
    /// @brief Выходной поток для записи JSON
    std::ostream& json_output_stream;

public:
    /// @brief Конструктор
    /// @param task Расчетная задача
    /// @param astro_times Модельная сетка в абсолютном времени
    /// @param edge_id_to_name Маппинг идентификаторов ребер к именам объектов
    /// @param output_stream Выходной поток для записи JSON
    batch_serializing_result(
        TaskType& task,
        const std::vector<std::time_t>& astro_times,
        const std::unordered_map<graphlib::graph_binding_t, std::string, graphlib::graph_binding_t::hash>& edge_id_to_name,
        std::ostream& output_stream,
        quasi_stationary_regime_selection_strategy regime_strategy)
        : task(task)
        , astro_times(astro_times)
        , edge_id_to_name(edge_id_to_name)
        , first_layer_written(false)
        , json_output_stream(output_stream)
        , regime_selector(regime_strategy)
    {
        if (edge_id_to_name.empty()) {
            throw std::invalid_argument("Edge ID to name mapping cannot be empty");
        }
        
        step_results_json_writer<typename TaskType::pipe_solver_type>::start_json_structure(json_output_stream);
    }

    /// @brief Выполняет расчет слоя и записывает профили в JSON
    /// @param time_step Временной шаг
    /// @param index Индекс слоя
    /// @param measurements_layer Срез измерения для данного момента времени
    /// @param controls Управление объектами для данного момента времени
    void operator()(double time_step, std::size_t index,
        const std::vector<oil_transport::graph_quantity_value_t>& measurements_layer,
        const std::vector<const typename TaskType::base_control_type*>& controls)
    {
        
        // 1. Применяем управление для данного шага
        task.send_controls(controls);
        
        auto [flows, endo] = oil_transport::classify_measurements(measurements_layer);

        if (index == 0) {
            task.solve(flows, endo, true);
        }
        else {
            task.step(time_step, flows, endo);
        }

        const auto& results = task.get_result();

        
        if (regime_selector.check_state_for_regime(time_step, index, results)) {
            // Сераилазция только моментов, которые выбраны в качестве квазистац. режимов
            serialize_results(results, index);
        }

        if (index == astro_times.size() - 1) {
            // После последнего слоя закрывем json
            step_results_json_writer<typename TaskType::pipe_solver_type>::end_json_structure(json_output_stream);
            json_output_stream.flush();
        }

    }

    /// @brief Специлизированная перегрузка только(!) для таска совмещенного расчета.
    /// Выполняет расчет слоя и записывает профили в JSON
    void operator()(double time_step, std::size_t index,
        const std::vector<oil_transport::graph_quantity_value_t>& measurements_layer,
        const std::vector<const transport_object_control_t*>& transport_controls,
        const std::vector<const hydraulic_object_control_t*>& hydro_controls)
    {
        // Перегрузка только для таска совмещенонго расчета
        if constexpr (!std::is_same_v<TaskType,
            hydro_transport_task_template<typename TaskType::pipe_solver_type,
            typename TaskType::lumped_calc_parameters_type>>)
        {
            throw std::runtime_error("Not supported type of task for overload of operator()");
        }

        // 1. Применяем управление для данного шага
        task.send_controls(transport_controls, hydro_controls);

        auto [flows, endo] = oil_transport::classify_measurements(measurements_layer);

        if (index == 0) {
            task.solve(flows, endo, true);
        }
        else {
            task.step(time_step, flows, endo);
        }

        const auto& results = task.get_result();

        if (regime_selector.check_state_for_regime(time_step, index, results)) {
            // Сераилазция только моментов, которые выбраны в качестве квазистац. режимов
            serialize_results(results, index);
        }

        if (index == astro_times.size() - 1) {
            // После последнего слоя закрывем json
            step_results_json_writer<typename TaskType::pipe_solver_type>::end_json_structure(json_output_stream);
            json_output_stream.flush();
        }
    }
};

}

