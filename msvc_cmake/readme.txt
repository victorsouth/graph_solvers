Подготовка Debug

Для MSVC рекомендуется скачать предсобранный boost, файл boost_1_88_0-msvc-14.3-64.exe
Необходимо установить boost и прописать путь до папки с файлами BoostConfig.cmake в системный PATH
Вероятный путь до файлов: C:\...\boost_1_88_0\lib64-msvc-14.3\cmake\Boost-1.88.0

Также для подключения boost версии 1.88 лучше сразу обновить CMake. Проверялось на 4.1.0

Должны быть предсобраны и заинстоллены с помощью CMAKE:
- Eigen Debug
- Gtest Debug
- fixed_solvers Debug
- pde_solvers Debug

Подробности см. pde_solvers/msvc_cmake/readme.txt и fixed_solvers/msvc_cmake/readme.txt 
(ЕСЛИ В БИБЛИОТЕКАХ ПОЯВИЛИСЬ ИЗМЕНЕНИЯ, КОТОРЫЕ НЕОБХОДИМЫ ДЛЯ РАБОТЫ ТЕКУЩЕЙ БИБЛИОТЕКИ, ТО ИХ НАДО ПЕРЕСОБИРАТЬ И ПЕРЕУСТАНАВЛИВАТЬ)

При всех собранных зависимостях используем типовую команду (запускать из graph_solvers/msvc_cmake)

cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Program Files (x86)/googletest-distribution"  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL -DCMAKE_BUILD_TYPE=Debug

В папке graph_solvers/msvc_cmake будет создано решение MSVC (.sln). Его открываем, работаем с ним как обычно.
Если необходимо собирать Dll-адаптер для Net-обертки, добавить к "cmake -G ..." опцию:
-DGRAPH_SOLVERS_BUILD_ADAPTER=ON


Если в момент выполнения "cmake -G" отсутствует сборка GoogleTest в release-конфигурации, то graph_solvers подтянет зависимость gtestd.lib и далее при линковке будет ошибка несовместимости graph_solvers release и gtest debug
