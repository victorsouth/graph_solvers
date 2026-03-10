using System;
using System.Runtime.InteropServices;

namespace GraphSolvers
{
    /// <summary>
    /// Класс для работы с DLL адаптером graph_solvers
    /// </summary>
    public class GraphSolversAPI
    {
#if USE_DLL_LIBRARY
        /// <summary>
        /// Имя файла библиотеки адаптера для DLL (Windows)
        /// </summary>
        private const string DLLName = "GraphSolversAdapter.dll";
#elif USE_SO_LIBRARY
        /// <summary>
        /// Имя файла библиотеки адаптера для SO (Linux)
        /// </summary>
        private const string DLLName = "libGraphSolversAdapter.so";
#else
        #error "Не определена константа USE_DLL_LIBRARY или USE_SO_LIBRARY. Убедитесь, что библиотека адаптера собрана и находится в netadapter/bin/tmp/"
#endif

        /// <summary>
        /// Создание модели, вызов конструкторов
        /// </summary>
        /// <returns>Дескриптор модели или IntPtr.Zero в случае ошибки</returns>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr createModel();

        /// <summary>
        /// Удаление модели, если необходимо
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern void deleteModel(IntPtr model);

        /// <summary>
        /// Инициализация расчета
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <param name="Topology">JSON строка с топологией</param>
        /// <param name="Objects">JSON строка с объектами</param>
        /// <param name="Settings">JSON строка с настройками</param>
        /// <returns>0 в случае успеха, ненулевое значение в случае ошибки</returns>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Init(
            IntPtr model,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string Topology,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string Objects,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string Settings);

        /// <summary>
        /// Начальный расчет
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <param name="Bounds">JSON строка с граничными условиями</param>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Solve(
            IntPtr model,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string Bounds);

        /// <summary>
        /// Шаг квазистационара
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <param name="Bounds">JSON строка с граничными условиями</param>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern int QuasistaticStep(
            IntPtr model,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string Bounds);

        /// <summary>
        /// Задать состояние
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <param name="State">JSON строка с состоянием</param>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetState(
            IntPtr model,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string State);

        /// <summary>
        /// Получить состояние
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <returns>Строка с состоянием или null</returns>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetState(
            IntPtr model);

        /// <summary>
        /// Получить результат расчета
        /// </summary>
        /// <param name="model">Дескриптор модели</param>
        /// <returns>Строка с результатом или null</returns>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetResult(
            IntPtr model);

        /// <summary>
        /// Освобождение строки, возвращенной из GetState или GetResult
        /// </summary>
        /// <param name="str">Указатель на строку для освобождения</param>
        [DllImport(DLLName, SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern void freeString(IntPtr str);

        /// <summary>
        /// Вспомогательный метод для получения строки из IntPtr
        /// </summary>
        /// <param name="ptr">Указатель на строку</param>
        /// <returns>Строка или null</returns>
        public static string? GetStringFromPtr(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero)
                return null;
            return Marshal.PtrToStringUTF8(ptr);
        }
    }
}

