using System;
using NUnit.Framework;
using GraphSolvers;

namespace GraphSolvers.Tests
{
    [TestFixture]
    public class GraphSolversAPITests
    {
        [Test]
        public void CreateModel_ShouldReturnNonZeroHandle()
        {
            // Arrange & Act
            IntPtr model = GraphSolversAPI.createModel();

            // Assert
            Assert.That(model, Is.Not.EqualTo(IntPtr.Zero), "createModel должен возвращать ненулевой указатель");

            // Cleanup
            if (model != IntPtr.Zero)
            {
                GraphSolversAPI.deleteModel(model);
            }
        }

        [Test]
        public void DeleteModel_ShouldNotThrow()
        {
            // Arrange
            IntPtr model = GraphSolversAPI.createModel();
            Assert.That(model, Is.Not.EqualTo(IntPtr.Zero), "Модель должна быть создана");

            // Act & Assert
            Assert.DoesNotThrow(() => GraphSolversAPI.deleteModel(model), "deleteModel не должна выбрасывать исключение");
        }

        [Test]
        public void DeleteModel_WithZeroHandle_ShouldNotThrow()
        {
            // Act & Assert
            Assert.DoesNotThrow(() => GraphSolversAPI.deleteModel(IntPtr.Zero), "deleteModel с нулевым указателем не должна выбрасывать исключение");
        }

        [Test]
        public void Init_WithNullModel_ShouldReturnError()
        {
            // Act
            int result = GraphSolversAPI.Init(IntPtr.Zero, "", "", "");

            // Assert
            Assert.That(result, Is.Not.EqualTo(0), "Init с нулевым указателем должна возвращать ошибку");
        }

        [Test]
        public void Init_WithSimpleScheme_ShouldSucceed()
        {
            // Arrange
            // Топология: поставщик (Src) -> задвижка (Vlv1) -> потребитель (Snk)
            // Такая же схема, как в GraphParser.ParsesFromStringSimpleScheme
            const string topologyJson = @"{
                ""Src"": {
                    ""to"": 0
                },
                ""Vlv1"": {
                    ""from"": 0,
                    ""to"": 1
                },
                ""Snk"": {
                    ""from"": 1
                }
            }";

            // Параметры объектов: поставщик, задвижка, потребитель
            const string objectsJson = @"{
                ""sources"": {
                    ""Src"": {
                        ""std_volflow"": 1.0
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
                    }
                }
            }";

            IntPtr model = GraphSolversAPI.createModel();
            Assert.That(model, Is.Not.EqualTo(IntPtr.Zero), "Модель должна быть создана");

            try
            {
                // Act
                int result = GraphSolversAPI.Init(model, topologyJson, objectsJson, "{}");

                // Assert
                Assert.That(result, Is.EqualTo(0), "Init должна успешно инициализировать модель с простейшей схемой");
            }
            finally
            {
                // Cleanup
                GraphSolversAPI.deleteModel(model);
            }
        }

        [Test]
        public void GetState_WithValidModel_ShouldReturnString()
        {
            // Arrange
            IntPtr model = GraphSolversAPI.createModel();
            Assert.That(model, Is.Not.EqualTo(IntPtr.Zero), "Модель должна быть создана");

            try
            {
                // Act
                IntPtr statePtr = GraphSolversAPI.GetState(model);
                string? state = GraphSolversAPI.GetStringFromPtr(statePtr);

                // Assert
                Assert.That(statePtr, Is.Not.EqualTo(IntPtr.Zero), "GetState должен возвращать ненулевой указатель");
                Assert.That(state, Is.Not.Null, "Строка состояния не должна быть null");
            }
            finally
            {
                // Cleanup
                GraphSolversAPI.deleteModel(model);
            }
        }

    }
}

