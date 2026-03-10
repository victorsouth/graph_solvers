#pragma once

#include "test_oilnet.h"

TEST(LocalResistance, DerivativeAtZeroflowIsDefined) {
    using namespace oil_transport;

    // Параметры местного сопротивления
    qsm_hydro_local_resistance_parameters params;
    params.xi = 150.0;
    params.diameter = 0.5;
    params.initial_opening = 1.0;

    // Буферы с рабочей плотностью
    std::array<endohydro_values_t, 2> buffers{};
    buffers[0].density_std.value = 850.0;
    buffers[1].density_std.value = 850.0;

    qsm_hydro_local_resistance_model_t model(buffers, params);

    const double p = 1e5;
    auto [dQ_dPin, dQ_dPout] = model.calculate_flow_derivative(p, p);

    ASSERT_GT(dQ_dPin, 0.0);
}

