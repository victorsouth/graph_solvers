#include "test_oilnet.h"

#include <fstream>
#include <filesystem>
#include <sstream>

TEST(TimeGridGenerator, InitialStateHeuristicUsage)
{
    using namespace oil_transport;
    using namespace graphlib;

    std::string tags_folder = get_schemes_folder() + "valve_control_test/tags_db";

    // Тег переключения
	control_tag_info_t control_info{
        .name = "Vlv2_Status",
		.type = oil_transport::control_tag_conversion_type::valve_state_scada
    };
    
    // Тег измерения
    tag_info_t measurement_info{ .name = "Vlv2_vol_flow_in" };

    // Временной ряд переключений
    transport_control_timeseries_t control_timeseries = get_control_timeseries({ control_info }, tags_folder);

    // Временной ряд измерений
    auto measurement_timeseries = graph_vector_timeseries_t::from_files(tags_folder, { measurement_info }, 
        { }, { } /* Тип эндогенного измерения и его привязка на графе не важны для теста */).get_time_series_object();

    // Активация эвристики начальных состояний
    auto batch_settings = batch_control_settings_t::get_default_settings();
    batch_settings.use_initial_state_as_control = true;
    batch_settings.extrapolate_last_control = false;

    // Интерполяция данных на единую временную сетку
    std::vector<double> times;
    std::vector<std::time_t> astro_times;
    double dt = 60;
    std::tie(times, astro_times) = time_grid_generator::prepare_time_grid(batch_settings, dt,
        measurement_timeseries, control_timeseries);

    // Границы временных рядов переключений, измерений и временной сетки  
    time_t modelling_start = astro_times.front();
    time_t modelling_end = astro_times.back();
    time_t measurement_start = measurement_timeseries.get_start_date();
    auto [_, control_end] = vector_timeseries_t<bool>::get_timeseries_period(control_timeseries);
    
    // Временная сетка начинается до первого переключения из временного ряда
    // и заканчивается с последним переключением
    ASSERT_TRUE(modelling_start == measurement_start);
    ASSERT_TRUE(modelling_end = control_end);
}

/// @brief Проверка корректного вызова конвертера csv из pde_solvers при подготовке таска и временных рядов измерений
/// и переключений на стороне graph_solvers
TEST(TaskWithMeasurementsAndControls, ConvertsControlsAccordingToType)
{
    using namespace oil_transport;
    using namespace graphlib;

    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    std::string tags_folder = get_schemes_folder() + "valve_control_test/tags_db";
    std::string tags_filename = scheme_folder + "/tags_data.json";

    auto task_settings = transport_task_settings::default_values();

    // Получаем преобразованные временные ряды переключений через интерфейс в graph_solvers
    auto [models, graph, measurements_data,
        controls_data] = prepare_task_and_measurements_and_controls(
            task_settings, scheme_folder, tags_filename, tags_folder);
    auto [times_from_task, values_from_task] = controls_data.timeseries.front();

    // Конвертируем файл прямым вызовом конвертера из pde_solvers
    const std::string ethalon_control_name = "Vlv1_Status";
    constexpr auto etalon_control_type = control_tag_conversion_type::valve_state_scada;
    std::string original_filename = get_schemes_folder() + "valve_control_test/tags_db/" + ethalon_control_name;
    std::string new_filename = convert_csv_values(original_filename, conversion_rules.at(etalon_control_type).conversion_map,
        conversion_rules.at(etalon_control_type).exception_value);
    
    // Зачитываем преобразованный файл
    auto reader = csv_tag_reader(new_filename, "" /* Безразмерный параметр */);
    auto [new_times, new_values] = reader.read_csv<bool>();

    // Сравниваем результаты преобразования при подготовке таска с ручным преобразованием
    ASSERT_EQ(new_times, times_from_task);
    ASSERT_EQ(new_values, values_from_task);
}



TEST(BatchTransportWithControls, AppliesControlsFromCsvToObjects)
{
    using namespace oil_transport;

    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    std::string tags_filename = scheme_folder + "/tags_data.json";
    std::string tags_folder = get_schemes_folder() + "valve_control_test/tags_db";

    auto task_settings = transport_task_settings::default_values();
    task_settings.structured_transport_solver.endogenous_options.density_std = true;

    auto [models, graph, measurements_data, controls_data] = prepare_task_and_measurements_and_controls(
        task_settings, scheme_folder, tags_filename, tags_folder);   

    double dt = 60;

    // Пакетный расчет
    auto batch_settings = batch_control_settings_t::get_default_settings();
    transport_batch_data batch_data(graph, batch_settings, dt, measurements_data, controls_data);
    transport_task_block_tree<qsm_pipe_transport_solver_t> task(std::move(models), std::move(graph), task_settings);
    batch_endogenous_scenario processor(task, batch_data.astro_times);
    
    oil_transport::transport_batch::perform(processor,
        batch_data.times, batch_data.flow_measurements, batch_data.endogenous_measurements, batch_data.controls);

    auto overwritten_calculation = processor.get_calculations();

    // Проверка - если единственный на схеме эндоген не использован, схема непроточная, 
    // т.е. переключение задвижки в закрытое состояние обработано корректно
    for (const auto& [measurement_type, measurement_data] : overwritten_calculation) {
        for (const auto& measurement : measurement_data) {
            if (measurement.status == measurement_status_t::unused &&
                measurement_type.type == measurement_type::density_std) {
                return; // Досрочное успешное завершение теста
            }
        }
    }
    FAIL();

}

/// @brief Проверяет возможность применения переключений из временных рядов 
/// к транспортному и гидравлическому графам в совмещенном пакетном расчете 
TEST(BatchHydroTransportWithControls, DISABLED_AppliesControlsFromCsvToObjects)
{
    using namespace oil_transport;

    std::string scheme_folder = get_scheme_folder("valve_control_test/scheme/");
    std::string tags_filename = scheme_folder + "/tags_data.json";
    std::string tags_folder = get_schemes_folder() + "valve_control_test/tags_db";


    // TODO: реализовать зачитку данных из JSON и CSV для совмещенного расчета. Подход такой же, как в адаптере - 
    // гидравлические контролы считаем наиболее общими и на их основе формируем транспортные.
    
    
    // 1. Зачитать данные

    // 2. Создать таск совмещенного расчета. Создать процессор пакетного расчета. Выполнить расчет.
    
    FAIL();

}
