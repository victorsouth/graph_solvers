#include "oil_transport.h"

#include "boost/pfr.hpp"

namespace oil_transport {
;


bool is_empty(const measurements_container_t& measurement)
{
    bool has_values = false;
    boost::pfr::for_each_field(
        measurement, [&](const auto& field)
        {
            typedef decltype(field) field_type_const_ref;
            typedef typename std::remove_reference<field_type_const_ref>::type field_type_const;
            typedef typename std::remove_const<field_type_const>::type field_type;

            if constexpr (std::is_same<field_type, transport_measurement_data_t>::value) {
                const transport_measurement_data_t& data = field;
                if (data.has_value()) {
                    has_values = true;
                }
            }
        });

    return has_values == false;
}

/// @brief В расчетное значение проставляется значение из измерения
/// Возвращает привязанное к графу расчетное значение, замененное измерением. 
/// @param measurement_data Измерение
/// @param endo_value Ссылка на расчетное значение на буфере
graph_quantity_value_t swap_with_measurement(
    const graph_quantity_value_t& measurement_data,
    pde_solvers::endogenous_confident_value_t& endo_value)
{
    graph_quantity_value_t calc_data = measurement_data;
    std::swap(calc_data.data.value, endo_value.value);
    calc_data.data.status = endo_value.confidence
        ? measurement_status_t::confident_calculation
        : measurement_status_t::inconfident_calculation;
    endo_value.confidence = true;
    return calc_data;
}

void overwrite_endogenous_values_from_measurements(
    const std::vector<graph_quantity_value_t>& measurements, 
    std::vector<graph_quantity_value_t>* overwritten_by_measurements, 
    pde_solvers::endogenous_values_t* _calculated_endogenous)
{
    pde_solvers::endogenous_values_t& calculated_endogenous = *_calculated_endogenous;
    for (const graph_quantity_value_t& measurement : measurements)
    {
        switch (measurement.type) {
        // TODO: Транспортное уравнение только для сортовой плотности. Нельзя переопределять плотностью в рабочих условиях
        case measurement_type::density_std:
            overwritten_by_measurements->emplace_back(
                swap_with_measurement(measurement, calculated_endogenous.density_std)
            );
            break;
        case measurement_type::viscosity_working:
            overwritten_by_measurements->emplace_back(
                swap_with_measurement(measurement, calculated_endogenous.viscosity_working)
            );
            break;
        case measurement_type::temperature:
            overwritten_by_measurements->emplace_back(
                swap_with_measurement(measurement, calculated_endogenous.temperature)
            );
            break;
        default:
            throw std::runtime_error("Unknown endogenous parameter");
        }
    }
}

}