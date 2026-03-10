#pragma once

namespace oil_transport {
;


/// @brief Настройки идентификации расходов
struct flow_ident_settings {
    /// @brief Вектор привязок расходов, которые нужно скорректировать
    std::vector<oil_transport::graph_quantity_binding_t> flow_to_correct;
};

template <typename PipeSolver>
/// @brief Критерий для идентификации расходов методом наименьших квадратов
class flow_ident_criteria : public fixed_least_squares_function_t {
private:
    oil_transport::transport_task_block_tree<PipeSolver>& task;
    const oil_transport::transport_batch_data& batch_data;
    oil_transport::quantity_timeseries_indices_t confident_indices;
    flow_ident_settings ident_settings;
public:
    /// @brief Конструктор критерия идентификации расходов
    /// @param task Транспортная задача на дереве блоков
    /// @param batch_data Данные пакетного расчета
    /// @param ident_settings Настройки идентификации расходов
    flow_ident_criteria(
        oil_transport::transport_task_block_tree<PipeSolver>& task,
        const oil_transport::transport_batch_data& batch_data,
        const flow_ident_settings& ident_settings
    )
        : task(task)
        , batch_data(batch_data)
        , ident_settings(ident_settings)
    {
    }
public:
    /// @brief Основной пакетный расчет здесь. Вызывается из residuals и например в конце идентификации уже внешим юзером
    std::tuple<
        std::vector<std::vector<oil_transport::graph_quantity_value_t>>,
        oil_transport::batch_endogenous_scenario<PipeSolver>
    > perform_batch(const Eigen::VectorXd& parameters)
    {
        // Делаем "unpack" для коррекций расхода и для параметров трубы
        Eigen::VectorXd flow_corrections = parameters; // пока заглушка - считаем, что все параметры идентификации - это расход

        // Делаем "unpack" для коррекций расхода и для параметров трубы
        std::vector<double> flow_corrections_vec(flow_corrections.data(), flow_corrections.data() + flow_corrections.size());

        // Делаем коррекции расходов
        std::vector<std::vector<oil_transport::graph_quantity_value_t>>
            corrected_flow_measurements = batch_data.calc_corrected_flows(
                ident_settings.flow_to_correct, flow_corrections_vec); // здесь нужна коррекция

        oil_transport::batch_endogenous_scenario<PipeSolver> processor(task, batch_data.astro_times);
        oil_transport::transport_batch::perform(processor, batch_data.times,
            corrected_flow_measurements, batch_data.endogenous_measurements, batch_data.controls);

        return std::make_tuple(
            std::move(corrected_flow_measurements),
            std::move(processor)
        );
    }

    /// @brief Вычисляет невязки для поправок расхода
    /// @param parameters Вектор поправок расхода
    /// @return Вектор невязок
    virtual Eigen::VectorXd residuals(const Eigen::VectorXd& parameters) override {
        auto [corrected_flows, processor] = perform_batch(parameters);

        // Выполняем расчет на откорректированных расходах
        if (confident_indices.empty()) {
            confident_indices = processor.get_confident_indices();
        }


        // Формируем вектор итоговых достоверных расчетных результатов и соответствующих измерений
        Eigen::VectorXd calculations;
        Eigen::VectorXd measurements;
        std::tie(calculations, measurements) = processor.get_confident_vectors(confident_indices);

        Eigen::VectorXd error = calculations - measurements;

        return error;
    }

    /// @brief Возвращает начальную оценку поправок расхода
    /// @return Вектор начальных оценок (все равны 1.0)
    Eigen::VectorXd estimation() const {
        std::vector<double> result = batch_data.prepare_default_flow_corrections(
            ident_settings.flow_to_correct);
        Eigen::VectorXd estim = Eigen::VectorXd::Map(result.data(), result.size());
        return estim;
    }

};




}