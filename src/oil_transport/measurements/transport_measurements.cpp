#include "oil_transport.h"

#include "boost/pfr.hpp"


namespace oil_transport {
;

transport_siso_measurements convert_to_measurement_group(
    const graph_quantity_value_t& measurement)
{
    // Возможно, не нужно
    // Не собиралось в CI из-за boost::pfr::for_each_field_with_name.
    // Нужно обновить boost
    throw std::runtime_error("Please, reimplement, if needed");

    //transport_siso_measurements measurement_group;
    //measurement_group.binding = measurement;

    //boost::pfr::for_each_field_with_name(measurement_group.data,
    //    [&](std::string_view name, oil_transport::transport_measurement_data_t& value) {
    //        if (oil_transport::to_string(measurement.type) == std::string(name)) {
    //            throw std::runtime_error("Please, reimplement");
    //            //value = measurement.data;
    //        }
    //    });
    //return measurement_group;
}

}