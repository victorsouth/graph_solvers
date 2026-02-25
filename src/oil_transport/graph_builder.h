#pragma once

namespace oil_transport {
;

/// @brief Контейнер экземпляров моделей и их буферов
template <typename PipeLayerType, typename LumpedCalcParametersType>
struct graph_models_t
{
    /// @brief Тип контейнера гидравлических моделей
    using hydro_models_t = std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>;
    /// @brief Тип контейнера транспортных моделей
    using transport_models_t = std::vector<std::unique_ptr<oil_transport_model_t>>;
    /// @brief Тип контейнера буферов моделей
    using model_buffers_t = std::vector<oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType>>;

    /// @brief Буферы моделей (первый элемент - односторонние, второй - двусторонние)
    std::pair<model_buffers_t, model_buffers_t> buffers;
    /// @brief Модели (первый элемент - односторонние, второй - двусторонние)
    std::pair<transport_models_t, transport_models_t> transport_models;
    /// @brief Гидравлические модели (первый элемент - односторонние, второй - двусторонние)
    std::pair<hydro_models_t, hydro_models_t> hydro_models;
    /// @brief Сбрасывает код достоверности всех объектов 
    /// (используется после анализа статического кода достоверности)
    void reset_confidence() {
        for (oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType>& buffer : buffers.first) {
            transport_buffer_helpers::reset_confidence<PipeLayerType, LumpedCalcParametersType>(&buffer);
        }
        for (oil_transport_buffer_t<PipeLayerType, LumpedCalcParametersType>& buffer : buffers.second) {
            transport_buffer_helpers::reset_confidence<PipeLayerType, LumpedCalcParametersType>(&buffer);
        }
    }

    /// @brief Создает транспортный граф из инциденций ребер
    /// @param edges Пара векторов инциденций (первый - односторонние, второй - двусторонние)
    /// @return Транспортный граф (ребра + модели с уже сгенерированными буферами)
    transport_graph_t get_transport_graph(const std::pair<
        std::vector<graphlib::edge_incidences_t>,
        std::vector<graphlib::edge_incidences_t>
    >& edges
    ) const {

        transport_graph_t transport_graph;

        auto create_edge_for_model = [&transport_graph](const graphlib::edge_incidences_t& edge,
            const std::unique_ptr<oil_transport_model_t>& transport_model) {
                return transport_model_incidences_t(edge, transport_model.get());
        };

        std::transform(edges.first.begin(), edges.first.end(),
            transport_models.first.begin(), std::back_inserter(transport_graph.edges1), create_edge_for_model);

        std::transform(edges.second.begin(), edges.second.end(),
            transport_models.second.begin(), std::back_inserter(transport_graph.edges2), create_edge_for_model);

        return transport_graph;
    }

    /// @brief Создает гидравлический граф из инциденций ребер
    /// @param edges Пара векторов инциденций (первый - односторонние, второй - двусторонние)
    /// @return Гидравлический граф (ребра + модели с уже сгенерированными буферами)
    oil_transport::hydraulic_graph_t get_hydro_graph(const std::pair<
        std::vector<graphlib::edge_incidences_t>,
        std::vector<graphlib::edge_incidences_t>
    >& edges
    ) const {

        oil_transport::hydraulic_graph_t hydro_graph;

        auto create_edge_for_model = [&hydro_graph](const graphlib::edge_incidences_t& edge,
            const std::unique_ptr<oil_transport::hydraulic_model_t>& hydro_model) {
                return oil_transport::hydraulic_model_incidences_t(edge, hydro_model.get());
        };

        std::transform(edges.first.begin(), edges.first.end(),
            hydro_models.first.begin(), std::back_inserter(hydro_graph.edges1), create_edge_for_model);

        std::transform(edges.second.begin(), edges.second.end(),
            hydro_models.second.begin(), std::back_inserter(hydro_graph.edges2), create_edge_for_model);

        return hydro_graph;
    }

};


template <typename PipeSolver, typename LumpedCalcParametersType>
/// @brief Контекст построения графа
struct graph_build_context {
    /// @brief Тип буфера для моделей
    using buffer_t = oil_transport_buffer_t<typename PipeSolver::layer_type, LumpedCalcParametersType>;

    /// @brief Свойства трубы для гидравлического расчета
    using templated_hydraulic_pipe_parameters =
        oil_transport::hydraulic_pipe_parameters_t<typename PipeSolver::pipe_parameters_type>;

    /// @brief Свойства трубы для транспортного расчета
    using templated_transport_pipe_parameters =
        transport_pipe_parameters_t<typename PipeSolver::pipe_parameters_type>;
};

/// @brief Построитель буферов для моделей транспортной задачи
/// @tparam PipeSolver Тип солвера для труб
/// @tparam LumpedCalcParametersType Тип параметров расчетных результатов для сосредоточенных объектов
template <typename PipeSolver, typename LumpedCalcParametersType>
class buffer_builder_t
    : graph_build_context<PipeSolver, LumpedCalcParametersType> {
private:

    /// @brief Использование типов из контекста создания графа
    using context = graph_build_context<PipeSolver, LumpedCalcParametersType>;
    using typename context::buffer_t;
    using typename context::templated_hydraulic_pipe_parameters;
    using typename context::templated_transport_pipe_parameters;

private:
    template <typename ModelParameters, typename Model>
    static buffer_t& _create_buffer(
        const model_parameters_t* parameters_pointer,
        std::pair<std::vector<buffer_t>, std::vector<buffer_t>>& buffers)
    {
        auto parameters_casted = dynamic_cast<const ModelParameters*>(parameters_pointer);
        if (parameters_casted == nullptr)
            throw std::runtime_error("Cannot cast parameters pointer");

        if constexpr (std::is_same<Model, transport_pipe_model_t<PipeSolver>>::value ||
            (std::is_same<Model, oil_transport::hydraulic_pipe_model_t<PipeSolver>>::value))
        {
            // для трубы (равношаговый профиль или другой нужный формируется ПЕРЕД этим)
            size_t point_count = parameters_casted->profile.get_coordinates().size();
            auto& model_buffer = buffers.second.emplace_back(
                transport_buffer_helpers::create_pipe<typename PipeSolver::layer_type, LumpedCalcParametersType>(point_count));
            return model_buffer;
        }

        else if constexpr (Model::is_single_side()) {
            // для одностороннего ребра
            auto& model_buffer = buffers.first.emplace_back(
                transport_buffer_helpers::create_single_sided<typename PipeSolver::layer_type, LumpedCalcParametersType>());
            return model_buffer;
        }
        else {
            // для двустороннего ребра НЕ трубы
            auto& model_buffer = buffers.second.emplace_back(
                transport_buffer_helpers::create_two_sided<typename PipeSolver::layer_type, LumpedCalcParametersType>());

            return model_buffer;
        }
    }

public:

    static std::pair<std::vector<buffer_t>, std::vector<buffer_t>> create_buffers(
        const std::pair<
        std::vector<std::unique_ptr<model_parameters_t>>,
        std::vector<std::unique_ptr<model_parameters_t>>
        >& props)
    {
        /// @brief Указатель на метод создания модели 
        using buffer_creator_pointer_t = buffer_t & (*)(const model_parameters_t*,
            std::pair<std::vector<buffer_t>, std::vector<buffer_t>>&);

        static const std::unordered_map<const std::type_info*, buffer_creator_pointer_t> buffer_creator_pointers
        {
            { &typeid (templated_transport_pipe_parameters), &buffer_builder_t::_create_buffer<
                templated_transport_pipe_parameters, transport_pipe_model_t<PipeSolver>> },
            { &typeid (transport_source_parameters_t), &buffer_builder_t::_create_buffer<
                transport_source_parameters_t, transport_source_model_t> },
            { &typeid (transport_lumped_parameters_t), &buffer_builder_t::_create_buffer<
                transport_lumped_parameters_t, transport_lumped_model_t> },
            { &typeid (templated_hydraulic_pipe_parameters), &buffer_builder_t::_create_buffer<
            templated_hydraulic_pipe_parameters, oil_transport::hydraulic_pipe_model_t<PipeSolver>> },
            { &typeid (oil_transport::qsm_hydro_source_parameters), &buffer_builder_t::_create_buffer<
                oil_transport::qsm_hydro_source_parameters, oil_transport::qsm_hydro_source_model_t> },
            { &typeid (oil_transport::qsm_hydro_local_resistance_parameters), &buffer_builder_t::_create_buffer<
                oil_transport::qsm_hydro_local_resistance_parameters, oil_transport::qsm_hydro_local_resistance_model_t> },

        };


        std::pair<std::vector<buffer_t>, std::vector<buffer_t>> result;
        result.first.reserve(props.first.size());
        result.second.reserve(props.second.size());

        auto create_edge_buffers =
            [&](const std::vector<std::unique_ptr<model_parameters_t>>& props)
        {
            for (const std::unique_ptr<model_parameters_t>& prop : props)
            {
                // TODO: check_parameters ?
                auto props_type_info = &typeid(*prop.get());
                buffer_creator_pointer_t creator_pointer = buffer_creator_pointers.at(props_type_info);
                creator_pointer(prop.get(), result);
            }
        };

        create_edge_buffers(props.first);
        create_edge_buffers(props.second);

        return result;

    }
};

/// @brief Формирует модели для квазистационарного расчета
/// @tparam PipeSolver Тип солвера для труб
/// @tparam LumpedCalcParametersType Тип расчетных параметров для сосредоточенных объектов
template <typename PipeSolver, typename LumpedCalcParametersType>
class model_builder_t
    : graph_build_context<PipeSolver, LumpedCalcParametersType> {

private:

    /// @brief Использование типов из контекста создания графа
    using context = graph_build_context<PipeSolver, LumpedCalcParametersType>;
    using typename context::buffer_t;
    using typename context::templated_hydraulic_pipe_parameters;
    using typename context::templated_transport_pipe_parameters;

private:
    template <typename ModelParameters, typename TransportModel>
    static void _add_transport_model(
        const model_parameters_t* parameters_pointer,
        buffer_t& model_buffer,
        const pde_solvers::endogenous_selector_t& endogenous_selector,
        std::pair<
        std::vector<std::unique_ptr<oil_transport_model_t>>,
        std::vector<std::unique_ptr<oil_transport_model_t>>
        >& transport_models
    )
    {
        auto parameters_casted = dynamic_cast<const ModelParameters*>(parameters_pointer);
        if (parameters_casted == nullptr)
            throw std::runtime_error("Cannot cast parameters pointer");

        if constexpr (std::is_same<TransportModel, transport_pipe_model_t<PipeSolver>>::value) {
            // для трубы (равношаговый профиль или другой нужный формируется ПЕРЕД этим)
            transport_models.second.emplace_back(std::make_unique<transport_pipe_model_t<PipeSolver>>(
                std::get<transport_buffer_helpers::edge_buffer_pipe_index>(model_buffer),
                *parameters_casted, endogenous_selector));
        }
        else if constexpr (TransportModel::is_single_side()) {
            // для одностороннего ребра
            transport_models.first.emplace_back(std::make_unique<TransportModel>(
                std::get<transport_buffer_helpers::edge_buffer_single_sided_index>(model_buffer),
                *parameters_casted,
                endogenous_selector));
        }
        else {
            // для двустороннего ребра НЕ трубы
            std::array<LumpedCalcParametersType, 2>& lumped_buffer =
                std::get<transport_buffer_helpers::edge_buffer_two_sided_index>(model_buffer);
            transport_models.second.emplace_back(std::make_unique<TransportModel>(
                lumped_buffer.front(), lumped_buffer.back(),
                *parameters_casted, endogenous_selector));
        }
    }


    template <typename ModelParameters, typename HydroModel>
    static void _add_hydro_model(
        const model_parameters_t* parameters_pointer,
        buffer_t& model_buffer,
        std::pair<
        std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>,
        std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>
        >& hydro_models
    )
    {
        auto parameters_casted = dynamic_cast<const ModelParameters*>(parameters_pointer);
        if (parameters_casted == nullptr)
            throw std::runtime_error("Cannot cast parameters pointer");

        if constexpr (std::is_same<HydroModel, oil_transport::hydraulic_pipe_model_t<PipeSolver>>::value) {
            hydro_models.second.emplace_back(std::make_unique<oil_transport::hydraulic_pipe_model_t<PipeSolver>>(
                std::get<transport_buffer_helpers::edge_buffer_pipe_index>(model_buffer),
                *parameters_casted));
        }
        else if constexpr (HydroModel::is_single_side()) {
            // для одностороннего ребра
            hydro_models.first.emplace_back(std::make_unique<HydroModel>(
                std::get<transport_buffer_helpers::edge_buffer_single_sided_index>(model_buffer),
                *parameters_casted));
        }
        else {
            // для двустороннего ребра НЕ трубы
            hydro_models.second.emplace_back(std::make_unique<HydroModel>(
                std::get<transport_buffer_helpers::edge_buffer_two_sided_index>(model_buffer),
                *parameters_casted));
        }
    }

public:

    /// @brief Создает транспортные модели для односторонних и двусторонних ребер
    /// @param props Параметры моделей (первый элемент - односторонние, второй - двусторонние)
    /// @param buffers Буферы моделей (первый элемент - односторонние, второй - двусторонние)
    /// @param endogenous_selector Селектор эндогенных параметров
    /// @return Пара векторов unique_ptr на транспортные модели
    static std::pair<
        std::vector<std::unique_ptr<oil_transport_model_t>>,
        std::vector<std::unique_ptr<oil_transport_model_t>>
    > create_transport_models(
        const std::pair<
        std::vector<std::unique_ptr<model_parameters_t>>,
        std::vector<std::unique_ptr<model_parameters_t>>
        >& props,
        std::pair<std::vector<buffer_t>, std::vector<buffer_t>>& buffers,
        const pde_solvers::endogenous_selector_t& endogenous_selector)
    {
        using creator_pointer_t = void (*)(
            const model_parameters_t*,
            buffer_t&,
            const pde_solvers::endogenous_selector_t&,
            std::pair<
            std::vector<std::unique_ptr<oil_transport_model_t>>,
            std::vector<std::unique_ptr<oil_transport_model_t>>
            >&
            );

        static const std::unordered_map<const std::type_info*, creator_pointer_t> transport_creator_pointers
        {
            { &typeid (templated_transport_pipe_parameters), &model_builder_t::_add_transport_model<
                templated_transport_pipe_parameters, transport_pipe_model_t<PipeSolver>> },
            { &typeid (transport_source_parameters_t), &model_builder_t::_add_transport_model<
                transport_source_parameters_t, transport_source_model_t> },
            { &typeid (transport_lumped_parameters_t), &model_builder_t::_add_transport_model<
                transport_lumped_parameters_t, transport_lumped_model_t> },
        };


        std::pair<
            std::vector<std::unique_ptr<oil_transport_model_t>>,
            std::vector<std::unique_ptr<oil_transport_model_t>>
        > result;
        result.first.reserve(props.first.size());
        result.second.reserve(props.second.size());

        auto create_models =
            [&](const std::vector<std::unique_ptr<model_parameters_t>>& props,
                std::vector<buffer_t>& buffers
                )
        {
            for (std::size_t index = 0; index < props.size(); ++index)
            {
                const std::unique_ptr<model_parameters_t>& prop = props[index];
                buffer_t& buffer = buffers[index];

                // TODO: check_parameters ?
                auto props_type_info = &typeid(*prop.get());
                creator_pointer_t creator_pointer = transport_creator_pointers.at(props_type_info);
                creator_pointer(prop.get(), buffer, endogenous_selector, result);
            }
        };

        create_models(props.first, buffers.first);
        create_models(props.second, buffers.second);

        return result;
    }


    /// @brief Создает гидравлические модели для односторонних и двусторонних ребер
    /// @param props Параметры моделей (первый элемент - односторонние, второй - двусторонние)
    /// @param buffers Буферы моделей (первый элемент - односторонние, второй - двусторонние)
    /// @return Пара векторов уникальных указателей на гидравлические модели
    static std::pair<
        std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>,
        std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>
    > create_hydro_models(
        const std::pair<
        std::vector<std::unique_ptr<model_parameters_t>>,
        std::vector<std::unique_ptr<model_parameters_t>>
        >& props,
        std::pair<std::vector<buffer_t>, std::vector<buffer_t>>& buffers)
    {
        using creator_pointer_t = void (*)(
            const model_parameters_t*,
            buffer_t&,
            std::pair<
            std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>,
            std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>
            >&
            );

        auto make_hydraulic_creator_pointers = []()
        {
            std::unordered_map<const std::type_info*, creator_pointer_t> result;
            
            // Этот изврат нужен, чтобы hydraulic_pipe_model_t не инстанцировалась с НЕ гидравлическим солвером
            if constexpr (pde_solvers::has_hydro_interface_v<PipeSolver>) {
                result[&typeid(templated_hydraulic_pipe_parameters)] = &model_builder_t::_add_hydro_model<
                    templated_hydraulic_pipe_parameters, oil_transport::hydraulic_pipe_model_t<PipeSolver>>;
            }

            result[&typeid(oil_transport::qsm_hydro_source_parameters)] = &model_builder_t::_add_hydro_model<
                oil_transport::qsm_hydro_source_parameters, oil_transport::qsm_hydro_source_model_t>;
            result[&typeid(oil_transport::qsm_hydro_local_resistance_parameters)] = &model_builder_t::_add_hydro_model<
                oil_transport::qsm_hydro_local_resistance_parameters, oil_transport::qsm_hydro_local_resistance_model_t>;
            return result;
        };
        
        static const std::unordered_map<const std::type_info*, creator_pointer_t> hydraulic_creator_pointers
            = make_hydraulic_creator_pointers();

        std::pair<
            std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>,
            std::vector<std::unique_ptr<oil_transport::hydraulic_model_t>>
        > result;
        result.first.reserve(props.first.size());
        result.second.reserve(props.second.size());

        auto create_models =
            [&](const std::vector<std::unique_ptr<model_parameters_t>>& props,
                std::vector<buffer_t>& buffers
                )
        {
            for (std::size_t index = 0; index < props.size(); ++index)
            {
                const std::unique_ptr<model_parameters_t>& prop = props[index];
                buffer_t& buffer = buffers[index];

                // TODO: check_parameters ?
                auto props_type_info = &typeid(*prop.get());
                creator_pointer_t creator_pointer = hydraulic_creator_pointers.at(props_type_info);
                creator_pointer(prop.get(), buffer, result);
            }
        };

        create_models(props.first, buffers.first);
        create_models(props.second, buffers.second);

        return result;
    }

};

template <typename PipeSolver, typename LumpedCalcParametersType = oil_transport::endohydro_values_t>
/// @brief Построитель графа транспортной задачи
class graph_builder
    : private graph_build_context<PipeSolver, LumpedCalcParametersType> {

private:
    using buffer_builder = buffer_builder_t<PipeSolver, LumpedCalcParametersType>;
    using model_builder = model_builder_t<PipeSolver, LumpedCalcParametersType>;

public:

    /// @brief Строит только транспортные модели и граф
    static std::tuple<
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
        transport_graph_t
    > build_transport(const graph_siso_data& graph_data, const pde_solvers::endogenous_selector_t& endogenous_option)
    {
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
        models.buffers = buffer_builder::create_buffers(graph_data.transport_props);
        models.transport_models = model_builder::create_transport_models(
            graph_data.transport_props, models.buffers, endogenous_option);

        transport_graph_t transport_graph = models.get_transport_graph(graph_data.edges);

        return std::make_tuple(
            std::move(models),
            std::move(transport_graph)
        );
    }

    /// @brief Строит только гидравлические модели и граф
    static std::tuple<
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType>,
        oil_transport::hydraulic_graph_t
    > build_hydro(const graph_siso_data& graph_data)
    {
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
        models.buffers = buffer_builder::create_buffers(graph_data.hydro_props);
        models.hydro_models = model_builder::create_hydro_models(graph_data.hydro_props, models.buffers);

        oil_transport::hydraulic_graph_t hydraulic_graph = models.get_hydro_graph(graph_data.edges);

        return std::make_tuple(
            std::move(models),
            std::move(hydraulic_graph)
        );
    }

    /// @brief Строит все модели и графы (транспортные и гидравлические)
    static std::tuple<
        graph_models_t< typename PipeSolver::layer_type, LumpedCalcParametersType>,
        transport_graph_t,
        oil_transport::hydraulic_graph_t
    > build_hydro_transport(const graph_siso_data& graph_data, const pde_solvers::endogenous_selector_t& endogenous_option)
    {
        graph_models_t<typename PipeSolver::layer_type, LumpedCalcParametersType> models;
        models.buffers = buffer_builder::create_buffers(graph_data.transport_props);
        models.transport_models = model_builder::create_transport_models(
            graph_data.transport_props, models.buffers, endogenous_option);
        models.hydro_models = model_builder::create_hydro_models(graph_data.hydro_props, models.buffers);

        transport_graph_t transport_graph = models.get_transport_graph(graph_data.edges);
        oil_transport::hydraulic_graph_t hydraulic_graph = models.get_hydro_graph(graph_data.edges);

        return std::make_tuple(
            std::move(models),
            std::move(transport_graph),
            std::move(hydraulic_graph)
        );
    }
};







}