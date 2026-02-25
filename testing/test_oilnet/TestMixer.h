#pragma once

#include "test_oilnet.h"


/// @brief Тест свойства - фактический расчет смешения выполняется по известным формулам для вязкости и плотности
/// (Формулы см. в коде теста)
TEST(Mixer, IsCalculationOfMixtureBasedOnFormulas) {
    using namespace oil_transport;

    pde_solvers::endogenous_selector_t endogenous_selector;
    endogenous_selector.density_std = true;
    endogenous_selector.viscosity_working = true;

    // Расходы входных потоков    
    double vol_flow1 = 2.0;
    double vol_flow2 = 3.0;

    // Эндогенные свойства входных потоков
    pde_solvers::endogenous_values_t inlet1;
    inlet1.density_std.value = 850;
    inlet1.viscosity_working.value = 8e-6;

    pde_solvers::endogenous_values_t inlet2;
    inlet2.density_std.value = 860;
    inlet2.viscosity_working.value = 27e-6;

    // Расчет эндогенных свойств смеси
    transport_mixer_t mixer;
    std::vector<graph_quantity_value_t> overwritten_by_measurements;

    pde_solvers::endogenous_values_t mixture_endogenous = mixer.mix(
        endogenous_selector,
        std::vector<double>{ 2.0, 3.0 }, 
        std::vector<pde_solvers::endogenous_values_t> { inlet1, inlet2 }, 
        std::vector<graph_quantity_value_t>(),
        &overwritten_by_measurements
    );

    // Значения эндогенных свойств, вычисленные по требуемым формулам
    double expected_density = (inlet1.density_std.value * vol_flow1 + inlet2.density_std.value * vol_flow2) / (vol_flow1 + vol_flow2);
    double expected_viscosity = pow(
        (cbrt(inlet1.viscosity_working.value) * vol_flow1 + cbrt(inlet2.viscosity_working.value) * vol_flow2) 
            / (vol_flow1 + vol_flow2), 
        3);

    ASSERT_NEAR(mixture_endogenous.density_std.value, expected_density, 1e-8);
    ASSERT_NEAR(mixture_endogenous.viscosity_working.value, expected_viscosity, 1e-8);
}


