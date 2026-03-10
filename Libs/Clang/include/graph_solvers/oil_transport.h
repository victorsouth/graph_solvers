#pragma once

#include "hydronet.h"

#include <fixed/fixed.h>
#include "pde_solvers/pde_solvers.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <fstream>
#include "boost/pfr.hpp"


#include "oil_transport/transport_buffer.h"

#include "oil_transport/solvers_buffered_decorators/transport_graph_solver_buffered.h"
#include "oil_transport/solvers_buffered_decorators/transport_block_solver_buffered.h"

#include "oil_transport/measurements/transport_measurements.h"
#include "oil_transport/measurements/graph_timeseries.h"

namespace oil_transport {
;
/// @brief Базовая структура параметров для всех моделей - транспортных, гидравлических
struct model_parameters_t
{
    /// @brief Внешний идентификтор объекта (от внешнего пользователя библиотеки)
    long long external_id{ -1 };
    /// @brief Чтобы в наследниках были виртуальные деструктуры, а то память в unique_ptr течет
    virtual ~model_parameters_t() = default;
};


/// @brief Типы сценариев квазистационарного расчета
enum class qsm_problem_type {
    /// @brief Только транспортный расчет
    Transport,
    /// @brief Гидравлический расчет по посчитанным ранее эндогенным свойствам
    Hydraulic,
    /// @brief Совмещенный расчет
    HydroTransport
};

}

namespace oil_transport {
;

/// @brief Расширенный перечень параметров транспортного расчета - эндогенные параметры + расходы
struct transport_values : pde_solvers::endogenous_values_t {
    /// @brief Объемный расход
    double vol_flow{ std::numeric_limits<double>::quiet_NaN() };
};

/// @brief Параметры гидравлического квазистационарного расчета (эндогенные параметры с достоверностью + гидравлические параметры) и их достоверность
struct endohydro_values_t : transport_values {
    /// @brief Давление
    double pressure{ std::numeric_limits<double>::quiet_NaN() };
    /// @brief Массовый расход
    double mass_flow{ std::numeric_limits<double>::quiet_NaN() };
};

}

#include "oil_transport/serialization/json_model_parameters.h"
#include "oil_transport/models_transport/transport_model_interface.h"
#include "oil_transport/models_transport/transport_source.h"
#include "oil_transport/models_transport/transport_lumped.h"
#include "oil_transport/models_transport/transport_pipe.h"


// TODO: Продумать положение гидравлических моделей в уровнях абстракции graph_solvers
#include "oil_transport/models_hydro/hydro_model_interface.h"
#include "oil_transport/models_hydro/hydro_source.h"
#include "oil_transport/models_hydro/hydro_local_resistance.h"
#include "oil_transport/models_hydro/check_valve.h"
#include "oil_transport/models_hydro/hydro_pipe.h"
#include "oil_transport/models_hydro/hydro_pump.h"
#include "oil_transport/models_hydro/simple_bufferless_models.h"

#include "oil_transport/transport_mixer.h"

#include "oil_transport/measurements/transport_measurements_funcs.h"

#include "oil_transport/serialization/graph_parser.h"
#include "oil_transport/transport_topology/transport_topology.h"
#include "oil_transport/graph_builder.h"
#include "oil_transport/transport_task.h"
#include "oil_transport/hydro_transport_task.h"
#include "oil_transport/hydro_task.h"

#include "oil_transport/serialization/results_serialization.h"
#include "oil_transport/transport_batch_profiles.h"
#include "oil_transport/transport_batch_series.h"
#include "oil_transport/transport_ident.h"

#include "oil_transport/serialization/bounds_parser.h"
#include "oil_transport/serialization/settings_parser.h"

namespace oil_transport {
;


}