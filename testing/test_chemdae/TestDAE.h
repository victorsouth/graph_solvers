#pragma once

#include <vle/vle_solvers.h>

#include "test_chemdae.h"



/// @brief Порт узла на графе
struct port_t
{
    /// @brief Узел, для которого определен данный порт
    std::size_t vertex{std::numeric_limits<std::size_t>::max()};
    /// @brief Номер порта у узла
    int port{-1};

    bool has_vertex() const {
        return vertex != std::numeric_limits<std::size_t>::max();
    }
};

/// @brief Дуга MIMO-графа
struct mimo_arc_t {
    std::size_t id{0};
    /// @brief Коннектор (вершина, коннектор), откуда ребро выходит
    port_t from;
    /// @brief Коннектор (вершина, коннектор), куда ребро входит
    port_t to;
};

/// @brief Значения расчетных параметров порта
struct port_values_t {
    /// @brief Мольный расход
    double molar_flow;
    /// @brief Давление
    double pressure;
    /// @brief Удельная мольная энтальпия
    double enthalpy;
    /// @brief Вектор молярных концентраций
    std::vector<double> concentration;
};

struct source_parameters_t {
    std::size_t arc_id{0};
    double molar_flow{std::numeric_limits<double>::quiet_NaN()};
    double pressure{ std::numeric_limits<double>::quiet_NaN() };
    double enthalpy{ std::numeric_limits<double>::quiet_NaN() };
    double temperature{ std::numeric_limits<double>::quiet_NaN() };
    std::vector<double> concentration;
};

struct valve_paramerters_t {
    std::size_t vertex_id{ 0 };
    double kv100{ std::numeric_limits<double>::quiet_NaN() };
};

inline std::vector<std::vector<std::size_t>> build_mimo_port_incidences(const std::vector<mimo_arc_t>& arcs) 
{
    std::unordered_map<std::size_t, std::map<std::size_t, std::size_t>> result_map;
    for (std::size_t arc_index = 0; arc_index < arcs.size(); ++arc_index) {
        const mimo_arc_t& arc = arcs[arc_index];

        if (arc.from.has_vertex()) {
            result_map[arc.from.vertex][arc.from.port] = arc_index;
        }
        if (arc.to.has_vertex()) {
            result_map[arc.to.vertex][arc.to.port] = arc_index;
        }
    }

    std::vector<std::vector<std::size_t>> result(result_map.size());
    for (const auto& [vertex, port_to_arc_map] : result_map) {
        std::vector<std::size_t> port_to_arc(port_to_arc_map.size());
        for (const auto& [port, arc_index] : port_to_arc_map) {
            port_to_arc[port] = arc_index;
        }
        result[vertex] = std::move(port_to_arc);
    }

    return result;
}

struct scheme_parameters_t {
    std::vector<mimo_arc_t> arcs;
    std::vector<source_parameters_t> sources;
    std::vector<oil_transport::qsm_hydro_local_resistance_parameters> valves;
    std::unique_ptr<vlelib::fluid_rault_dalton_t> default_fluid;

private:
    mutable std::vector<std::vector<std::size_t>> port_incidences;
public:
    const std::vector<std::vector<std::size_t>>& get_mimo_port_incidences() const {
        if (port_incidences.empty()) {
            port_incidences = build_mimo_port_incidences(arcs);
        }
        return port_incidences;
    }
    std::size_t get_total_port_count() const {
        return valves.size() * 2;
    }

    const std::vector<double>& get_default_concentration() const {
        if (sources.empty()) {
            throw std::runtime_error("No sources defined");
        }
        return sources.front().concentration;
    }

    std::size_t get_components_count() const {
        return get_default_concentration().size();
    }
};


inline scheme_parameters_t prepare_scheme_two_valves() {
    scheme_parameters_t scheme;

    mimo_arc_t arc0{
        .id = 0,
        .to = port_t{ .vertex = 0, .port = 0}
    };
    mimo_arc_t arc1{
        .id = 1,
        .from = port_t{.vertex = 0, .port = 1},
        .to = port_t{.vertex = 1, .port = 0}
    };
    mimo_arc_t arc2{
        .id = 2,
        .from = port_t{.vertex = 1, .port = 1}
    };
    scheme.arcs = { arc0, arc1, arc2 };

    oil_transport::qsm_hydro_local_resistance_parameters valve1;
    valve1.external_id = 0;
    valve1.xi = 100;
    valve1.initial_opening = 1.0;
    valve1.diameter = 0.2;

    oil_transport::qsm_hydro_local_resistance_parameters valve2 = valve1;
    valve2.external_id = 1;
    valve2.xi = 40;

    scheme.valves = { valve1, valve2 };

    source_parameters_t src;
    src.arc_id = 0;
    src.molar_flow = 1.0;
    src.concentration = { 0.4, 0.6 };
    src.temperature = 280;

    source_parameters_t sink;
    sink.arc_id = 2;
    sink.concentration = { 0.5, 0.5 };
    sink.pressure = 1e5;
    sink.temperature = 270;

    scheme.sources = {src, sink};


    std::vector<std::wstring> comps{ L"C3H8", L"n_C4H10" };
    scheme.default_fluid = vlelib::create_fluid<vlelib::fluid_rault_dalton_t>(
            comps, { 0.5, 0.5 });
    return scheme;
}

struct arc_parameters_t {
    double molar_flow;
    double pressure;
    double enthalpy;
    Eigen::VectorXd concentration;
};

class scheme_equations : public fixed_system_t<-1> 
{
    const scheme_parameters_t& scheme;
public:
    scheme_equations(const scheme_parameters_t& scheme) 
        : scheme(scheme)
    {

    }

    std::size_t get_port_vector_size() const {
        return 3 + scheme.get_components_count();
    }

    std::size_t get_algebraic_variable_count() const {
        std::size_t var_count = 
            scheme.arcs.size() * 
            get_port_vector_size();
        return var_count;
    }

    Eigen::VectorXd estimation() const {
        
        std::vector<double> default_concentration = scheme.get_default_concentration();

        std::size_t c_size = get_port_vector_size();

        Eigen::VectorXd default_portvector(c_size);
        default_portvector(0) = 1.0;
        default_portvector(1) = 1e5;
        default_portvector(2) = 0;
        default_portvector.tail(scheme.get_components_count()) =
            Eigen::VectorXd::Map(default_concentration.data(), default_concentration.size());

        Eigen::VectorXd est(get_algebraic_variable_count());
        for (std::size_t i = 0; i < scheme.arcs.size(); ++i) {
            est.segment(i * c_size, c_size) = default_portvector;
        }

        
        return est;

    }

    std::vector<Eigen::VectorXd> unpack(const Eigen::VectorXd& x) const {
        std::size_t c_size = get_port_vector_size();
        std::vector<Eigen::VectorXd> arcs_vectors(scheme.arcs.size());
        for (std::size_t i = 0; i < scheme.arcs.size(); ++i) {
            arcs_vectors[i] = x.segment(i * c_size, c_size);
        }

        const std::vector<std::vector<std::size_t>>& port_incidences 
            = scheme.get_mimo_port_incidences();

        std::vector<std::vector<Eigen::VectorXd>> port_vectors(port_incidences.size());
        for (std::size_t vertex = 0; vertex < port_incidences.size(); ++vertex) {
            //std::vector<Eigen::VectorXd>& object_vectors = port_vectors[]
        }

        return arcs_vectors;
    }

    static arc_parameters_t unpack_arc(const Eigen::VectorXd& c) {
        arc_parameters_t result;
        result.molar_flow = c(0);
        result.pressure = c(1);
        result.enthalpy = c(2);
        result.concentration = c.tail(c.size() - 3);


        return result;
    }




    virtual Eigen::VectorXd residuals(const Eigen::VectorXd& x) override {
        Eigen::VectorXd g(get_algebraic_variable_count());

        
        
        std::vector<Eigen::VectorXd> arcs_vectors = unpack(x);


        std::size_t index = 0;
        for (const source_parameters_t& src : scheme.sources) {
            const Eigen::VectorXd& c = arcs_vectors[src.arc_id];
            arc_parameters_t arc_parameters = unpack_arc(c);

            const mimo_arc_t& scheme_arc = scheme.arcs[src.arc_id];

            if (std::isfinite(src.pressure)) {
                double c_pressure = c(1);
                g(index++) = src.pressure - arc_parameters.pressure;
            }
            
            if (std::isfinite(src.molar_flow)) {
                double c_molar_flow = c(0);
                g(index++) = src.molar_flow - arc_parameters.molar_flow;
            }
            
            
            bool is_really_producer =
                (scheme_arc.to.has_vertex() && arc_parameters.molar_flow >= 0) ||
                (scheme_arc.from.has_vertex() && arc_parameters.molar_flow < 0);
            if (is_really_producer)
            {
                for (std::size_t component_index = 0; component_index < scheme.get_components_count(); ++component_index) {
                    double src_concentration = src.concentration[component_index];
                    double arc_concentration = arc_parameters.concentration(component_index);
                    g(index++) = src_concentration - arc_concentration;
                }
                if (std::isfinite(src.temperature)) {
                    auto fluid = scheme.default_fluid->create_copy(
                        arc_parameters.concentration);
                    auto vle = fluid->flash(arc_parameters.pressure, src.temperature);
                    double src_enthalpy = vle.enthalpy.molar.mix;
                    double arc_enthalpy = arc_parameters.enthalpy;
                    g(index++) = src_enthalpy - arc_enthalpy;
                }
            }
                
        }

        const std::vector<std::vector<std::size_t>>& port_incidences = scheme.get_mimo_port_incidences();

        for (std::size_t valve_index = 0; valve_index < scheme.valves.size(); ++valve_index) {
            const oil_transport::qsm_hydro_local_resistance_parameters& valve = scheme.valves[valve_index];

            double S = M_PI * valve.diameter * valve.diameter / 4.0;  // площадь, м²
            double kv = S * sqrt(2.0 / valve.xi);  // Kv пропускная способность

            std::size_t arc_in = port_incidences[valve_index][0];
            std::size_t arc_out = port_incidences[valve_index][1];

            // По arc_in/arc_out получаем вектор параметров на порту (расход, P, h, концентрации)
            const Eigen::VectorXd& c_in = arcs_vectors[arc_in];
            const Eigen::VectorXd& c_out = arcs_vectors[arc_out];
            arc_parameters_t in_params = unpack_arc(c_in);
            arc_parameters_t out_params = unpack_arc(c_out);
            

            g(index++) = out_params.molar_flow - in_params.molar_flow;
            g(index++) = out_params.enthalpy - in_params.enthalpy;

            
            for (std::size_t component_index = 0; component_index < scheme.get_components_count(); ++component_index) {
                double in_concentration = in_params.concentration(component_index);
                double out_concentration = out_params.concentration(component_index);
                g(index++) = out_concentration - in_concentration;
            }
            
            
            auto fluid = scheme.default_fluid->create_copy(in_params.concentration);
            
            double molar_mass = fluid->get_molar_mass();
            double h_molar = in_params.enthalpy / molar_mass;

            fixed_bisection_result_t<1> res;
            vlelib::desired_enthalpy ph(fluid.get(), h_molar, in_params.pressure);
            double Tcalc = ph.solve(&res);

            auto vle2 = fluid->flash(in_params.pressure, Tcalc);

            double q_mass = kv / ( 3.57 * 1e4)
                   * sqrt(vle2.density.mix
                          * (in_params.pressure - out_params.pressure));

            // осталось найти/вычислить/понять Q и дописать g(index++) = Q - q_molar
            double q_molar = q_mass / vle2.molar_mass.mix;

            g(index++) = out_params.molar_flow - q_molar;
            

        }
        
        return g;
    }


};

/// @brief Тест для разработки ДАЕ-решателя: решение scheme_equations методом Ньютона (fixed_solvers)
TEST(DAE, Develop)
{
    scheme_parameters_t scheme = prepare_scheme_two_valves();
    scheme_equations equations(scheme);

    Eigen::VectorXd initial = equations.estimation();

    fixed_solver_parameters_t<-1, 0, golden_section_search> params;
    params.line_search.iteration_count = 20;
    params.line_search.function_decrement_factor = 1e-4;
    params.iteration_count = 100;
    params.argument_increment_norm = 1e-10;
    params.residuals_norm = 1e-8;

    fixed_solver_result_t<-1> solver_result;
    fixed_newton_raphson<-1>::solve(equations, initial, params, &solver_result, nullptr);

    EXPECT_EQ(solver_result.result_code, numerical_result_code_t::Converged)
        << "Newton did not converge; iterations=" << solver_result.iteration_count;

    if (solver_result.result_code == numerical_result_code_t::Converged) {
        Eigen::VectorXd x = solver_result.argument;
        Eigen::VectorXd r = equations.residuals(x);
        EXPECT_LT(r.norm(), 1e-6) << "Residual norm at solution should be small";
    }
}


TEST(Fluid, PT_flash) {
    std::vector<std::wstring> comps{ L"C3H8", L"n_C4H10" };
    auto fluid = vlelib::create_fluid<vlelib::fluid_rault_dalton_t>(
        comps, {0.5, 0.5});

    double P = 1e6;
    double T = 300;

    auto vle1 = fluid->flash(P, T);

    auto vle2 = fluid->flash(P, 1.1 * T);
}

TEST(Fluid, PH_flash) {
    std::vector<std::wstring> comps{ L"C3H8", L"n_C4H10" };
    auto fluid = vlelib::create_fluid<vlelib::fluid_rault_dalton_t>(
        comps, { 0.5, 0.5 });

    double P = 1e6;
    double T = 300;

    auto vle2 = fluid->flash(P, 1.1 * T);
    double Htarget = vle2.enthalpy.mass.mix;

    fixed_bisection_result_t<1> res;
    vlelib::desired_enthalpy ph(fluid.get(), Htarget, P, T);
    double Tcalc = ph.solve(&res);

}




