using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Runtime.InteropServices;

namespace GraphSolvers
{
    public class HydroTransportTask : IDisposable
    {
        private IntPtr Model;
        private bool disposed = false;



        /// <summary>
        /// Под капотом вызов C++, поэтому на всякий случай явное удаление
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (Model != IntPtr.Zero)
                {
                    GraphSolversAPI.deleteModel(Model);
                    Model = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        /// <summary>
        /// Создание пустой модели
        /// </summary>
        /// <param name="Topology"></param>
        /// <param name="Objects"></param>
        /// <param name="Settings"></param>
        /// <exception cref="ArgumentNullException"></exception>

        public HydroTransportTask(JObject Topology, JObject Objects, JObject Settings)
        {
            Model = GraphSolversAPI.createModel();
            if (Model == null)
            {
                throw new Exception("Не удалось создать расчетный контекст");
            }

            if (Topology == null)
            {
                throw new ArgumentNullException(nameof(Topology));
            }

            if (Objects == null)
            {
                throw new ArgumentNullException(nameof(Objects));
            }

            if (Settings == null)
            {
                throw new ArgumentNullException(nameof(Settings));
            }

            string topologyString = Topology.ToString();
            string objectsString = Objects.ToString();
            string settingsString = Settings.ToString();

            GraphSolversAPI.Init(Model, topologyString, objectsString, settingsString);
        }

        /// <summary>
        /// Стационарный расчет, также используется как генератор начальных условий
        /// </summary>
        /// <param name="Bounds"></param>
        /// <exception cref="ArgumentNullException"></exception>
        public void InitialSolve(JObject Bounds)
        {
            if (Bounds == null)
            {
                throw new ArgumentNullException(nameof(Bounds));
            }

            string boundsString = Bounds.ToString();
            GraphSolversAPI.Solve(Model, boundsString);
        }

        /// <summary>
        /// Шаг квазистационара. TODO в будущем сделать переменный шаг с заданием в качестве агрумента, сейчас в Settings
        /// </summary>
        /// <param name="Bounds"></param>
        /// <exception cref="ArgumentNullException"></exception>
        public void QuasistaticStep(JObject Bounds)
        {
            if (Bounds == null)
            {
                throw new ArgumentNullException(nameof(Bounds));
            }

            string boundsString = Bounds.ToString();
            GraphSolversAPI.QuasistaticStep(Model, boundsString);
        }

        /// <summary>
        /// Получить состояние
        /// </summary>
        /// <returns></returns>
        /// <exception cref="InvalidOperationException"></exception>
        public JObject GetState()
        {
            if (Model == IntPtr.Zero)
            {
                throw new InvalidOperationException("Модель не инициализирована или была удалена");
            }

           IntPtr statePtr = GraphSolversAPI.GetState(Model);

            if (statePtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Не удалось получить состояние модели. Метод GetState вернул нулевой указатель.");
            }

            try
            {
                string? jsonString = Marshal.PtrToStringAnsi(statePtr);

                if (string.IsNullOrWhiteSpace(jsonString))
                {
                    throw new InvalidOperationException("Получена пустая строка состояния");
                }

                // Преобразуем строку в JObject
                JObject stateObject = JObject.Parse(jsonString);

                return stateObject;
            }
            catch (JsonReaderException ex)
            {
                throw new InvalidOperationException("Полученный JSON имеет некорректный формат", ex);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Ошибка при получении состояния модели", ex);
            }
        }

        /// <summary>
        /// Задать состояние
        /// </summary>
        /// <param name="State"></param>
        /// <exception cref="ArgumentNullException"></exception>
        /// <exception cref="InvalidOperationException"></exception>
        /// <exception cref="ArgumentException"></exception>
        public void SetState(JObject State)
        {
            if (State == null)
            {
                throw new ArgumentNullException(nameof(State), "Состояние не может быть null");
            }

            if (Model == IntPtr.Zero)
            {
                throw new InvalidOperationException("Модель не инициализирована или была удалена");
            }

            try
            {
                string jsonString = State.ToString();

                if (string.IsNullOrWhiteSpace(jsonString))
                {
                    throw new ArgumentException("Состояние не может быть пустым", nameof(State));
                }

                GraphSolversAPI.SetState(Model, jsonString);
            }
            catch (Exception ex) when (!(ex is ArgumentException) && !(ex is InvalidOperationException))
            {
                throw new InvalidOperationException("Ошибка при установке состояния модели", ex);
            }
        }

        /// <summary>
        /// Получить результат (для пользователя и UI)
        /// </summary>
        /// <returns></returns>
        /// <exception cref="InvalidOperationException"></exception>
        public JObject GetResult()
        {
            if (Model == IntPtr.Zero)
            {
                throw new InvalidOperationException("Модель не инициализирована или была удалена");
            }

            IntPtr resultPtr = GraphSolversAPI.GetResult(Model);

            if (resultPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Не удалось получить состояние модели. Метод GetState вернул нулевой указатель.");
            }

            try
            {
                string? jsonString = Marshal.PtrToStringAnsi(resultPtr);

                if (string.IsNullOrWhiteSpace(jsonString))
                {
                    throw new InvalidOperationException("Получена пустая строка состояния");
                }

                // Преобразуем строку в JObject
                JObject stateObject = JObject.Parse(jsonString);

                return stateObject;
            }
            catch (JsonReaderException ex)
            {
                throw new InvalidOperationException("Полученный JSON имеет некорректный формат", ex);
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException("Ошибка при получении состояния модели", ex);
            }
        }
    }
}
