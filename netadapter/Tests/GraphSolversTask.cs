using System;
using System.Linq;
using NUnit.Framework;
using GraphSolvers;
using Newtonsoft.Json.Linq;

namespace GraphSolvers.Tests
{
    /// <summary>
    /// Тесты для класса HydroTransportTask
    /// </summary>
    [TestFixture]
    public class GraphSolversTaskTests
    {
        /// <summary>
        /// Тест для HydroTransportTask на основе C++ теста StepWithFlowPropagation
        /// Проверяет инициализацию схемы, выполнение InitialSolve и получение состояния
        /// </summary>
        [Test]
        public void HydroTransportTask_StepWithFlowPropagation_ShouldSucceed()
        {
            // Arrange - Подготавливаем схему с задвижкой и измерения
            // Топология: Src -> Vlv1 -> Vlv2 -> Snk (аналогично valve_control_test/scheme)
            // TODO: в отдельную функцию.
            JObject topology = JObject.Parse(@"
            {
                ""Src"": {
                    ""to"": 0
                },
                ""Vlv1"": {
                    ""from"": 0,
                    ""to"": 1
                },
                ""Vlv2"": {
                    ""from"": 1,
                    ""to"": 2
                },
                ""Snk"": {
                    ""from"": 2
                }
            }");

            // Параметры объектов: источники, задвижки, потребитель
            // В C++ тесте: source_pressure = 90e3, std_volflow = NaN (не задан)
            JObject objects = JObject.Parse(@"
            {
                ""sources"": {
                    ""Src"": {
                        ""pressure"": 110000.0
                    },
                    ""Snk"": {
                        ""pressure"": 1e5
                    }
                },
                ""valves"": {
                    ""Vlv1"": {
                        ""initial_opening"": 100,
                        ""xi"": 0.03,
                        ""diameter"": 0.2
                    },
                    ""Vlv2"": {
                        ""initial_opening"": 100,
                        ""xi"": 0.03,
                        ""diameter"": 0.2
                    }
                }
            }");

            // Настройки: solution_method = FindFlowFromMeasurements, density = true
            JObject settings = JObject.Parse(@"
            {
                ""solution_method"": ""CalcFlowWithHydraulicSolver"",
                ""structured_transport_solver"": {
                    ""endogenous_options"": {
                        ""density_std"": true
                    }
                }
            }");

            // Измерения (Bounds): 
            // - vol_flow_std для Vlv1 = 0.9
            // - density_std для вершины 1 = 886.0
            // Формат: массив измерений с привязкой к объекту/вершине
            // Примечание: Формат Bounds может потребовать корректировки после реализации Solve в адаптере.
            // В C++ коде используется graph_quantity_value_t с привязкой через name_to_binding для объектов
            // и graph_binding_t для вершин. Возможно, потребуется использовать имена объектов вместо edge_id.
            JObject bounds = JObject.Parse(@"
            {
                ""transport_measurements"": [
                    {
                        ""edge"": ""Vlv1"",
                        ""type"": ""vol_flow_std"",
                        ""value"": 0.9
                    },
                    {
                        ""vertex"": 1,
                        ""type"": ""density_std"",
                        ""value"": 886.0
                    }
                ]
            }");

            
            using (var task = new HydroTransportTask(topology, objects, settings))
            {
                // Act
                task.InitialSolve(bounds);
                task.QuasistaticStep(bounds);
                JObject result = task.GetResult();

                // Assert
                JToken? densityConfidence = ((JObject)result)["layers"]?
                    .LastOrDefault()?["buffers"]?["sources"]?["Snk"]?["parameters"]?["density_std_confidence"];
                
                Assert.That(densityConfidence?.ToString(), Is.EqualTo("true"), 
                    "На потребителе плотность должна быть достоверной");
            }
        }
    }
}

