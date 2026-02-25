#include "test_oilnet.h"



/// @brief Простейшая модель двустороннего ребра с буферами давлений для тестирования estimate_from_buffers
namespace nodal_solver_buffer_based_test_helpers {
;

/// @brief Частная функция для записи одинакового значения давления в буферы моделей графа
/// @param graph Гидравлический граф
/// @param offset Смещение буфера
/// @param pressure Значение давления
void update_pressure_in_edges(const oil_transport::hydraulic_graph_t& graph,
    const graphlib::layer_offset_t offset, double pressure) {
    for (const auto& edge : graph.edges2) {
        edge.model->update_pressure(offset, pressure, pressure);
    }

    for (const auto& edge : graph.edges1) {
        edge.model->update_pressure(offset, pressure);
    }
}


/// @brief Очень частная функция для инициализации плотности в буферах моделей сосредоточенного (!) транспортного графа
/// TODO: убрать после реализации общего случая
inline void init_density_in_sources_and_lumpeds(const oil_transport::transport_graph_t& transport_graph,
    const double density = 850) {

    auto edges = transport_graph.get_edge_binding_list();
    for (const auto& edge_binding : edges) {

        oil_transport::oil_transport_model_t* model = transport_graph.get_model_incidences(edge_binding).model;

        void* buffer_pointer = model->get_edge_buffer();

        if (buffer_pointer == nullptr) {
            throw std::runtime_error("Model has null buffer");
        }

        if (auto source_model = dynamic_cast<oil_transport::transport_source_model_t*>(model)) {
            auto buffer = reinterpret_cast<oil_transport::endohydro_values_t*>(buffer_pointer);
            buffer->density_std.value = density;
        }
        else if (auto lumped_model = dynamic_cast<oil_transport::transport_lumped_model_t*>(model)) {
            auto* buffers_for_getter = reinterpret_cast<std::array<oil_transport::endohydro_values_t*, 2>*>(buffer_pointer);
            buffers_for_getter->at(0)->density_std.value = density;
            buffers_for_getter->at(1)->density_std.value = density;
        }
    }
}
}

/// @brief Класс-обёртка для тестирования формирования начального состояния для варианта узлового солвера, 
/// работающего с буферами моделей 
template <typename ModelIncidences>
class nodal_solver_buffer_based_Test : public graphlib::nodal_solver_buffer_based_t<ModelIncidences>
{
public:
    /// @brief Создаёт тестовый буферный солвер на основе указанного графа и настроек узлового решателя
    /// @param graph Граф, используемый в тестах буферного узлового солвера
    /// @param settings Настройки узлового солвера, передаваемые во внутренний решатель
    nodal_solver_buffer_based_Test(graphlib::basic_graph_t<ModelIncidences>& graph, const graphlib::nodal_solver_settings_t& settings)
        : graphlib::nodal_solver_buffer_based_t<ModelIncidences>(graph, settings)
    {
    }

    /// @brief Для доступа к защищённому методу `estimate_from_buffers`, строящему начальное приближение по данным в буферах моделей
    Eigen::VectorXd test_estimate_from_buffers(graphlib::solver_estimation_type_t estimation_type) const {
        return this->estimate_from_buffers(estimation_type);
    }
};


/// @brief Проверка получения начального распределения давления - из давлений, хранящихся в буферах моделей
/// Рассмотрены оба возможных случая - текущий и предыдущий расчетные слои
TEST(NodalSolverBufferBased, EstimatesFromBuffer)
{
    using namespace graphlib;
    using namespace oil_transport;

    // Arrange: схема из JSON
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    auto settings = transport_task_settings::default_values();

    auto [models_with_buffers, transport_graph, hydro_graph] =
        load_hydro_transport_scheme<>(scheme_folder, "", settings);

    nodal_solver_buffer_based_test_helpers::init_density_in_sources_and_lumpeds(transport_graph);

    auto nodal_settings = nodal_solver_settings_t::default_values();
    nodal_solver_buffer_based_Test<oil_transport::hydraulic_model_incidences_t> solver(hydro_graph, nodal_settings);

    // Act - Записываем одинаковые давления в текущий и предыдущий слои буферов
    const double pressure_current = 2 * 10e5;
    nodal_solver_buffer_based_test_helpers::update_pressure_in_edges(hydro_graph, 
        layer_offset_t::ToCurrentLayer, pressure_current);
    Eigen::VectorXd estimation_current = solver.test_estimate_from_buffers(
        graphlib::solver_estimation_type_t::FromCurrentLayer);
    
    const double pressure_previous = 3 * 10e5;
    nodal_solver_buffer_based_test_helpers::update_pressure_in_edges(hydro_graph, 
        layer_offset_t::ToPreviousLayer, pressure_previous);
    Eigen::VectorXd estimation_previous = solver.test_estimate_from_buffers(
        graphlib::solver_estimation_type_t::FromPreviousLayer);  
    
    // Assert: для всех вершин оценка давления равна значению из буферов
    ASSERT_EQ(estimation_current.size(), estimation_previous.size());

    for (Eigen::Index i = 0; i < estimation_current.size(); ++i) {
        EXPECT_DOUBLE_EQ(estimation_current[i], pressure_current);
        EXPECT_DOUBLE_EQ(estimation_previous[i], pressure_previous);
    }
}

/// @brief Проверка, что flow_distribution_monosolver_t::update_estimation корректно записывает давления и расходы в модели
// TODO Дополнить тест проверкой случая предыдущего слоя после реализации гидравлической модели трубы
TEST(NodalSolverBufferBased, UpdatesModelBuffers)
{
    using namespace graphlib;
    using namespace oil_transport;

    // Arrange: схема из JSON
    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    auto settings = transport_task_settings::default_values();

    auto [json_data, siso_data] = load_hydro_transport_siso_graph(
        scheme_folder, "", settings);
    auto prop = dynamic_cast<qsm_hydro_source_parameters*>(siso_data.hydro_props.first[0].get());
    prop->pressure = 90e3;
    prop->std_volflow = std::numeric_limits<double>::quiet_NaN();

    auto [models_with_buffers, transport_graph, hydro_graph] =
        graph_builder<pde_solvers::iso_nonbaro_pipe_solver_t>::build_hydro_transport(
            siso_data, settings.structured_transport_solver.endogenous_options);

    nodal_solver_buffer_based_test_helpers::init_density_in_sources_and_lumpeds(transport_graph);

    auto nodal_settings = nodal_solver_settings_t::default_values();
    nodal_solver_buffer_based_Test<oil_transport::hydraulic_model_incidences_t> solver(hydro_graph, nodal_settings);

    // Act: решаем задачу потокораспределения с обновлением буферов моделей
    graphlib::flow_distribution_result_t result =
        solver.solve(solver_estimation_type_t::RandomInital,
            graphlib::layer_offset_t::ToCurrentLayer, nullptr);

    // Assert: давления в буферах моделей совпадают с рассчитанными давлениями в узлах

    // Ребро 0-1
    auto estimation_type = graphlib::solver_estimation_type_t::FromCurrentLayer;
    const auto& model1 = hydro_graph.edges2[0].model;
    double e2_num1_Pin = model1->get_pressure_from_buffer(estimation_type, false);
    double e2_num1_Pout = model1->get_pressure_from_buffer(estimation_type, true);
    ASSERT_DOUBLE_EQ(e2_num1_Pin, result.pressures[0]);
    ASSERT_DOUBLE_EQ(e2_num1_Pout, result.pressures[1]);

    // Ребро 1-2
    const auto& model2 = hydro_graph.edges2[1].model;
    double e2_num2_Pin = model2->get_pressure_from_buffer(estimation_type, false);
    double e2_num2_Pour = model2->get_pressure_from_buffer(estimation_type, true);
    ASSERT_DOUBLE_EQ(e2_num2_Pin, result.pressures[1]);
    ASSERT_DOUBLE_EQ(e2_num2_Pour, result.pressures[2]);

}