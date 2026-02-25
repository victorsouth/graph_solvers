#pragma once

#include "graphs.h"
#include "pde_solvers/pde_solvers.h"

#include "hydronet/helpers.h"
#include "hydronet/full_propagatable_graph/endogenous_propagator.h"
#include "hydronet/full_propagatable_graph/flow_propagator.h"
#include "hydronet/full_propagatable_graph/transport_graph_solver.h"

namespace graphlib {
;
/// @brief Типы моделей с точки зрения декомпозиции графа
enum class computational_type { Instant, Splittable, SplittableOrdered };

/// @brief Помощник для получения типа настроек солвера
/// @details Для любого типа Solver получает его Settings через вложенный тип
template <typename Solver>
using solver_settings_t = typename Solver::Settings;


} // namespace graphlib


#include "hydronet/block_tree/endogenous_propagator.h"
#include "hydronet/redundant_flows_corrector.h"
#include "hydronet/block_tree/flow_propagator.h"
#include "hydronet/block_tree/transport_block_solver.h"

#include "hydronet/topology/zeroflow.h"
#include "hydronet/topology/topology.h"

#include "hydronet/flow_solvers/nodal_solver.h"
#include "hydronet/flow_solvers/renumber_pressure_vertex.h"
#include "hydronet/flow_solvers/nodal_solver_buffer_based.h"
#include "hydronet/structured_solvers/structured_hydro_solver.h"

#include "hydronet/structured_solvers/structured_transport_solver.h"

namespace graphlib {
;


}
