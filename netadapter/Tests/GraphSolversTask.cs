using System;
using System.Linq;
using System.Text;
using NUnit.Framework;
using GraphSolvers;
using Newtonsoft.Json.Linq;
using System.IO;

namespace GraphSolvers.Tests
{
    /// <summary>
    /// Тесты для класса HydroTransportTask
    /// </summary>
    [TestFixture]
    public class GraphSolversTaskTests
    {
        /// <summary>
        /// Возвращает данные для схемы из одной трубы
        /// </summary>
        private static (JObject topology, JObject objects) GetPipeScheme(string pipeEdgeName, double pipeLength)
        {
            var topology = new JObject(
                new JProperty(pipeEdgeName, new JObject(new JProperty("from", 0), new JProperty("to", 1))),
                new JProperty("Inlet", new JObject(new JProperty("to", 0))),
                new JProperty("Outlet", new JObject(new JProperty("from", 1))));

            var pipeParams = new JObject(
                new JProperty("X_start", 0.0),
                new JProperty("X_end", pipeLength),
                new JProperty("Diameter", 0.8));
            var pipes = new JObject(new JProperty(pipeEdgeName, pipeParams));
            var sources = new JObject(
                new JProperty("Inlet", new JObject(new JProperty("std_volflow", 1), new JProperty("density", 843))),
                new JProperty("Outlet", new JObject(new JProperty("pressure", 1e5), new JProperty("density", 850))));
            var objects = new JObject(
                new JProperty("pipes", pipes),
                new JProperty("sources", sources),
                new JProperty("valves", new JObject()));

            return (topology, objects);
        }

        /// <summary>
        /// Тест для HydroTransportTask на основе C++ теста StepWithFlowPropagation
        /// Проверяет инициализацию схемы, выполнение InitialSolve, длительный расчет и получение состояния
        /// </summary>
        [Test]
        public void HydroTransportTask_StepWithFlowPropagation_ShouldSucceed()
        {
            // Arrange - Подготавливаем схему с задвижкой и измерения
            // Топология: Src -> Pipe1 -> Snk
            var (topology, objects) = GetPipeScheme("Pipe1", 10000.0);

            var settings = JObject.Parse(
            """
            {
              "solution_method": "CalcFlowWithHydraulicSolver",
              "structured_transport_solver": {
                "endogenous_options": {
                  "density_std": true
                }
              }
            }
            """);

            // Измерения (Bounds): 
            // - density_std для вершины 1 = 886.0
            // Формат: массив измерений с привязкой к объекту/вершине
            JObject bounds = JObject.Parse(@"
            {
              ""transport_measurements"": [
                {
                        ""vertex"": 0,
                        ""type"": ""density_std"",
                        ""value"": 886.0
                    }
],
              ""controls"": {
                ""sources"": {
                  ""Inlet"": {
                    ""std_vol_flow"": 1,
                    ""density_std_out"": 843
                  },
                  ""Outlet"": {
                    ""pressure"": 100000,
                    ""density_std_out"": 850
                  }
                },
                ""improvers"": {},
                ""valves"": {}
              }
            }");

            using (var task = new HydroTransportTask(topology, objects, settings))
            {
                // Act - Расчет до вымещения недостоверной партии из трубы
                task.InitialSolve(bounds);
                const int stepCount = 200;
                for (int step = 0; step < stepCount; step++)
                {
                    task.QuasistaticStep(bounds);
                }
                
                JObject result = task.GetResult();

                // Assert
                JToken? densityConfidence = ((JObject)result)["layers"]?
                    .LastOrDefault()?["buffers"]?["sources"]?["Outlet"]?["parameters"]?["density_std_confidence"];
                
                Assert.That(densityConfidence?.ToString(), Is.EqualTo("true"), 
                    "На потребителе плотность должна быть достоверной");
            }
        }

        
        /// <summary>
        /// Проверка возможности задания данных схемы в кодировке UTF-8
        /// </summary>
        [Test]
        public void HydroTransportTask_Utf8StringInScheme_IsPreservedInResult()
        {
            const string Utf8TestString = "😀 Привет 🎉 ∑ ∞ → … — 中文 日本語 🚀";
            var (topology, objects) = GetPipeScheme(Utf8TestString, 50000.0);

            var settings = JObject.Parse(
            """
            {
              "solution_method": "CalcFlowWithHydraulicSolver",
              "structured_transport_solver": {
                "endogenous_options": {
                  "density_std": true
                }
              }
            }
            """);

            JObject bounds = JObject.Parse(@"{}");

            using (var task = new HydroTransportTask(topology, objects, settings))
            {
                // Act - Расчет
                task.InitialSolve(bounds);
                task.QuasistaticStep(bounds);
                JObject result = task.GetResult();

                // Assert результаты содержат Unicode название ребра без искажения
                JObject? pipes = result["layers"]?[0]?["buffers"]?["pipes"] as JObject;
                Assert.That(pipes, Is.Not.Null, "В результате должен быть узел buffers.pipes");
                Assert.That(pipes!.ContainsKey(Utf8TestString), Is.True,
                    "Расчётный модуль должен сохранять UTF-8 имя объекта в результате");
            }
        }

    }
}