# .NET Adapter для graph_solvers

Этот каталог содержит компоненты для использования библиотеки graph_solvers из .NET приложений.

## Структура

- **GraphSolversAdapter/** - C++ DLL-адаптер, предоставляющий C-интерфейс для P/Invoke
- **GraphSolversNet/** - C# библиотека с P/Invoke обертками
- **Tests/** - Тесты для C# библиотеки на NUnit3

## Сборка

### C++ адаптер

Для сборки адаптера необходимо включить опцию `GRAPH_SOLVERS_BUILD_ADAPTER` в CMake:

```bash
cmake -DGRAPH_SOLVERS_BUILD_ADAPTER=ON ..
cmake --build .
```

Или в CMake GUI установить опцию `GRAPH_SOLVERS_BUILD_ADAPTER` в `ON`.

Адаптер будет собран как DLL с именем `GraphSolversAdapter.dll`.

### C# библиотека и тесты

```bash
cd GraphSolversNet
dotnet build

cd ../Tests
dotnet test
```

## Использование

Подключите проект `GraphSolversNet` к вашему .NET приложению и используйте класс `GraphSolversAPI`:

```csharp
using GraphSolvers;

IntPtr model = GraphSolversAPI.createModel();
// ... работа с моделью ...
GraphSolversAPI.deleteModel(model);
```

