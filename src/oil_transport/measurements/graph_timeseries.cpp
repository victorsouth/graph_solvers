#include "oil_transport.h"

#include "boost/pfr.hpp"

namespace oil_transport {
;


graph_vector_timeseries_t graph_vector_timeseries_t::from_files(
    const std::string& tag_files_path, 
    const std::vector<tag_info_t>& measurement_tags, 
    const std::vector<graphlib::graph_place_binding_t>& measurement_bindings, 
    const std::vector<measurement_type>& measurement_types)
{
    csv_multiple_tag_reader reader(measurement_tags, tag_files_path);
    std::vector< // по каждому тегу...
        std::pair< // .. получаем пару ..
        std::vector<time_t>,  // .. временных меток ..
        std::vector<double>   // .. и значений
        >
    > tag_data = reader.read_csvs();

    graph_vector_timeseries_t result(measurement_bindings, measurement_types, tag_data);
    return result;
}

graph_vector_timeseries_t graph_vector_timeseries_t::from_files(const std::string& start_period, const std::string& end_period, 
    const std::string& tag_files_path, const std::vector<tag_info_t>& measurement_tags, 
    const std::vector<graphlib::graph_place_binding_t>& measurement_bindings, 
    const std::vector<measurement_type>& measurement_types)
{
    csv_multiple_tag_reader reader(measurement_tags, tag_files_path);
    std::vector<std::pair<std::vector<time_t>, std::vector<double>>> tag_data
        = reader.read_csvs(start_period, end_period);

    graph_vector_timeseries_t result(measurement_bindings, measurement_types, tag_data);
    return result;
}

const vector_timeseries_t<>& graph_vector_timeseries_t::get_time_series_object() const
{
    return time_series;
}

std::vector<graph_quantity_value_t> graph_vector_timeseries_t::prepare_measurement_values(
    const std::vector<double>& values, 
    const std::vector<graphlib::graph_place_binding_t>& bindings, 
    const std::vector<measurement_type>& measurement_types)
{
    std::vector<graph_quantity_value_t> result;

    for (std::size_t index = 0; index < values.size(); ++index) {
        graph_quantity_value_t measurements;
        static_cast<graphlib::graph_place_binding_t&>(measurements) = bindings[index];
        measurements.data.value = values[index];
        measurements.type = measurement_types[index];
        result.push_back(measurements);
    }

    return result;
}

std::time_t graph_vector_timeseries_t::get_start_date() const
{
    std::time_t t = time_series.get_start_date();
    return t;
}

std::time_t graph_vector_timeseries_t::get_end_date() const
{
    std::time_t t = time_series.get_end_date();
    return t;
}

std::vector<graph_quantity_value_t> graph_vector_timeseries_t::operator()(time_t t) const
{
    std::vector<double> measurement_values = time_series(t);

    std::vector<graph_quantity_value_t>
        measurement_layer = prepare_measurement_values(
            measurement_values, measurement_bindings, measurement_types);

    return measurement_layer;
}

graph_vector_timeseries_t::graph_vector_timeseries_t(
    const std::vector<graphlib::graph_place_binding_t>& measurement_bindings, 
    const std::vector<measurement_type>& measurement_types, 
    const std::vector<std::pair<std::vector<time_t>, std::vector<double>>>& tag_data) 
    : time_series(tag_data)
    , measurement_bindings(measurement_bindings)
    , measurement_types(measurement_types)
{

}


}