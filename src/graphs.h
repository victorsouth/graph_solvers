#pragma once

#pragma warning (disable: 4503)

#include <fixed/fixed.h>


#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set> 
#include <unordered_map> 
#include <set>
#include <queue>
#include <limits>
#include <iostream>
#include <functional>
#include <istream>
#include <type_traits>


#include "graphs/helpers/invertex_index.h"
#include "graphs/helpers/map_helpers.h"
#include "graphs/helpers/vector_helpers.h"
#include "graphs/helpers/set_helpers.h"
#include "graphs/graph_bindings.h"
#include "graphs/graph_topology.h"
#include "graphs/basic_graph.h"
#include "graphs/graph_exception.h"
#include "graphs/general_structured_graph.h"
#include "graphs/mimo_graph.h"
#include "graphs/graph_bicomps.h"
#include "graphs/graph_bicomps_algorithms.h"



namespace graphlib {
;

/// @brief Ребро и его инциденции
/// @tparam ModelClass Тип модели ребра (например: static_model, dynamic_model)
template <typename ModelClass>
struct model_incidences_t : public edge_incidences_t {
    /// @brief Тип модели ребра
    typedef ModelClass model_type;
    /// @brief Модель ребра
    model_type model;
    /// @brief Конструктор на основе инцидений ребра incidences и модели model
    model_incidences_t(const edge_incidences_t& incidences, const ModelClass& model)
        : edge_incidences_t(incidences)
        , model(model)
    {

    }

};




}

