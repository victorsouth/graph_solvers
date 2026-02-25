#pragma once

#include "hydronet.h"
#include <fixed/fixed.h>
#include "pde_solvers/pde_solvers.h"

// Здесь совсем простые объекты - примерный код под обучение студентов
namespace oil_hydro_simple {
    ;

    /// @brief Базовые свойства - содрано полностью из transport_object_parameters_t. 
    /// Точно нужна еще одна структура? Или вообще все свойства во всех случаях будут такими?
    struct oil_hydro_model_properties {
        /// @brief Внешний идентификтор объекта (от внешнего пользователя библиотеки)
        long long external_id{ -1 };
        /// @brief Чтобы в наследниках были виртуальные деструктуры, а то память в unique_ptr течет
        virtual ~oil_hydro_model_properties() = default;
    };

    /// @brief Структура для описания параметров 
    struct oil_hydro_source_properties : public oil_hydro_model_properties {
        /// @brief Стандартный объемный расход
        double std_volflow{ std::numeric_limits<double>::quiet_NaN() };
        /// @brief Давление
        double pressure{ std::numeric_limits<double>::quiet_NaN() };
    };

    /// @brief Наследует функциональность графовых замыкающих соотношений и обеспечивает базовую реализацию
    class oil_hydro_source : public graphlib::closing_relation1 {
        oil_hydro_source_properties properties;
    public:
        /// @brief Конструктор класса oil_hydro_source
        /// @param properties Свойства для инициализации
        oil_hydro_source(const oil_hydro_source_properties& properties)
            : properties(properties)
        {
        }
        /// @brief Возвращает известное давление для P-притоков. 
        /// Если объект не является P-притоком, возвращает NaN
        virtual double get_known_pressure() {
            return properties.pressure;
        }
        /// @brief Возвращает известный расход для Q-притоков. 
        /// Если объект не является Q-притоком, возвращает NaN
        /// Тип расхода (объемный, массовый) не регламентируется и решается на уровне реализации замыкающих соотношений
        /// Нужно, чтобы все замыкающие соотношения подразумевали один и тот же вид расхода
        virtual double get_known_flow() {
            return properties.std_volflow;
        }
        /// @brief Расход висячей дуги при заданном давлении
        virtual double calculate_flow(double pressure) override {
            throw std::runtime_error("not supported");
        }
        /// @brief Производная расход висячей дуги по давлению
        virtual double calculate_flow_derivative(double pressure) override {
            throw std::runtime_error("not supported");
        }

        /// @brief Запись значения расхода
        virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
            throw std::runtime_error("No buffer to update");
        }

        /// @brief Для одностороннего ребра запись двух давлений не определена
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Запись давления
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
            throw std::runtime_error("No buffer to update");
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            throw std::runtime_error("No buffer to read from");
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        /// @param incidence +1 - выход ребра. -1 - вход ребра.
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) {
            throw std::runtime_error("Must not be called for edge type I");
        }

        /// @brief Возвращает рассчитанный ранее объемный расход через дугу
        virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            throw std::runtime_error("No buffer to read from");
        }
    };

    /// @brief Структура, представляющая свойства местного сопротивления
    struct oil_hydro_local_resistance_properties : public oil_hydro_model_properties {
        /// @brief Сопротивление
        double resistance{ std::numeric_limits<double>::quiet_NaN() };
    };

    /// @brief Класс, представляющий местное гидравлическое сопротивление
    class oil_hydro_local_resistance : public graphlib::closing_relation2 {
        oil_hydro_local_resistance_properties properties;
    public:
        /// @brief Конструктор класса oil_hydro_local_resistance
        /// @param properties Свойства местного сопротивления для инициализации
        oil_hydro_local_resistance(const oil_hydro_local_resistance_properties& properties)
            : properties(properties)
        {
        }
    public:
        /// @brief Расчет расхода через местное сопротивление 
        /// Вычисляет расход на основе перепада давления между входом и выходом
        /// по формуле : flow = ssqrt((pressure_in - pressure_out) / resistance)
        /// @param pressure_in давление на входе
        /// @param pressure_out давление на выходе
        /// @return расход
        virtual double calculate_flow(double pressure_in, double pressure_out) override {
            double dp = pressure_in - pressure_out;
            double flow = fixed_solvers::ssqrt(dp / properties.resistance);
            return flow;
        }
        /// @brief Расчет производных расхода по давлениям на входе и выходе
        /// @param pressure_in  давление на входе
        /// @param pressure_out давление на выходе
        /// @return массив производных
        virtual std::array<double, 2> calculate_flow_derivative(double pressure_in, double pressure_out) override {
            double dp = pressure_in - pressure_out;
            double denom = properties.resistance;
            // dq/dx for ssqrt(x) is 1/(2*ssqrt(x)); handle dp==0 by returning zeros
            if (denom == 0 || dp == 0) {
                return { 0.0, 0.0 };
            }
            double root = fixed_solvers::ssqrt(dp / denom);
            double common = 1.0 / (2.0 * denom * root);
            return { common, -common };
        }

        /// @brief Запись значения расхода
        virtual void update_vol_flow(graphlib::layer_offset_t layer_offset, double flow) override {
            throw std::runtime_error("No buffer to update");
        }

        /// @brief Запись давлений на входе и выходе
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure_in, double pressure_out) override {
            throw std::runtime_error("No buffer to update");
        }

        /// @brief Для двустороннего ребра запись одного давления не определена
        virtual void update_pressure(graphlib::layer_offset_t layer_offset, double pressure) override {
            throw std::runtime_error("Must not be called for edge type II");
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            throw std::runtime_error("Must not be called for edge type II");
        }

        /// @brief Возвращает рассчитанное ранее давление в узле, инцидентном ребру
        /// @param incidence +1 - выход ребра. -1 - вход ребра.
        virtual double get_pressure_from_buffer(graphlib::solver_estimation_type_t estimation_type, bool is_end_side) {
            throw std::runtime_error("No buffer to read from");
        }

        /// @brief Возвращает рассчитанный ранее объемный расход через дугу
        virtual double get_vol_flow_from_buffer(graphlib::solver_estimation_type_t estimation_type) override {
            throw std::runtime_error("No buffer to read from");
        }
    };

}

