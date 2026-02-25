cmake_minimum_required(VERSION 3.10)

project(graph_solvers)

set(CMAKE_CXX_STANDARD 20)#23?
set(CMAKE_CXX_STANDARD_REQUIRED True)
message("CXX standart: " ${CMAKE_CXX_STANDARD})

if(MSVC)
    cmake_policy(SET CMP0167 NEW)
    add_compile_options("/MP")     # включает многопроцессорную компиляцию
    add_compile_options("/bigobj")
endif()
if(MINGW)
    cmake_policy(SET CMP0167 NEW)
    add_compile_options("-Wa,-mbig-obj")
endif()

find_package(pde_solvers)
find_package(Boost)




# Установка путей к файлам в папках в переменные
set(HEADER
    src/graphs.h  
    src/hydronet.h 
    src/oil_transport.h
)

set(HEADER_GRAPHS
    src/graphs/basic_graph.h
    src/graphs/general_structured_graph.h
    src/graphs/graph_bicomps.h
    src/graphs/graph_bicomps_algorithms.h
    src/graphs/graph_bindings.h
    src/graphs/graph_exception.h
    src/graphs/graph_topology.h
    src/graphs/mimo_graph.h
)
set(HEADER_GRAPHS_HELPERS
    src/graphs/helpers/invertex_index.h
    src/graphs/helpers/map_helpers.h
    src/graphs/helpers/set_helpers.h
    src/graphs/helpers/vector_helpers.h
)    


set(HEADER_HYDRONET
    src/hydronet/helpers.h
    src/hydronet/redundant_flows_corrector.h
)
set(SOURCE_HYDRONET
    src/hydronet/redundant_flows_corrector.cpp
)
set(HEADER_HYDRONET_BLOCK_TREE
    src/hydronet/block_tree/endogenous_propagator.h
    src/hydronet/block_tree/flow_propagator.h
    src/hydronet/block_tree/transport_block_solver.h
)
set(HEADER_HYDRONET_FULL_PROPAGATABLE_GRAPH
    src/hydronet/full_propagatable_graph/endogenous_propagator.h
    src/hydronet/full_propagatable_graph/flow_propagator.h
    src/hydronet/full_propagatable_graph/transport_graph_solver.h
)
set(HEADER_HYDRONET_TOPOLOGY
    src/hydronet/topology/topology.h
    src/hydronet/topology/zeroflow.h
)
set(HEADER_HYDRONET_FLOW_SOLVERS
    src/hydronet/flow_solvers/nodal_solver.h
    src/hydronet/flow_solvers/renumber_pressure_vertex.h
    src/hydronet/flow_solvers/nodal_solver_buffer_based.h
)
set(HEADER_HYDRONET_STRUCTURED_SOLVERS
    src/hydronet/structured_solvers/structured_hydro_solver.h
    src/hydronet/structured_solvers/structured_transport_solver.h
)


set(HEADER_OIL_HYDRO
)
set(HEADER_OIL_HYDRO_MODELS
    src/oil_transport/models_hydro/hydro_model_interface.h
    src/oil_transport/models_hydro/hydro_source.h
    src/oil_transport/models_hydro/hydro_local_resistance.h
    src/oil_transport/models_hydro/simple_bufferless_models.h
    src/oil_transport/models_hydro/hydro_pipe.h
)

set(HEADER_OIL_TRANSPORT
    src/oil_transport/transport_buffer.h
    src/oil_transport/transport_mixer.h
    src/oil_transport/transport_task.h
    src/oil_transport/hydro_transport_task.h
    src/oil_transport/hydro_task.h
    src/oil_transport/graph_builder.h
    src/oil_transport/transport_batch_series.h
    src/oil_transport/transport_batch_profiles.h
    src/oil_transport/transport_ident.h
)
set(HEADER_OIL_TRANSPORT_BUFFERED_SOLVERS
    src/oil_transport/solvers_buffered_decorators/transport_graph_solver_buffered.h
    src/oil_transport/solvers_buffered_decorators/transport_block_solver_buffered.h
)
set(SOURCE_OIL_TRANSPORT
    src/oil_transport/transport_mixer.cpp
    src/oil_transport/transport_task.cpp
)
set(SOURCE_OIL_TRANSPORT_MEASUREMENTS
    src/oil_transport/measurements/graph_timeseries.cpp
    src/oil_transport/measurements/transport_measurements.cpp
    src/oil_transport/measurements/transport_measurements_funcs.cpp
)
set(HEADER_OIL_TRANSPORT_MEASUREMENTS
    src/oil_transport/measurements/graph_timeseries.h
    src/oil_transport/measurements/transport_measurements.h
    src/oil_transport/measurements/transport_measurements_funcs.h
)
set(HEADER_OIL_TRANSPORT_MODELS
    src/oil_transport/models_transport/transport_lumped.h
    src/oil_transport/models_transport/transport_model_interface.h
    src/oil_transport/models_transport/transport_pipe.h
    src/oil_transport/models_transport/transport_source.h
)
set(SOURCE_OIL_TRANSPORT_PARSERS
    src/oil_transport/serialization/graph_parser.cpp
)
set(HEADER_OIL_TRANSPORT_PARSERS
    src/oil_transport/serialization/graph_parser.h
    src/oil_transport/serialization/bounds_parser.h
    src/oil_transport/serialization/settings_parser.h
    src/oil_transport/serialization/results_serialization.h
)
set(HEADER_OIL_TRANSPORT_TRANSPORT_TOPOLOGY
    src/oil_transport/transport_topology/transport_topology.h
)


# Организация папок с исходниками в сгенерированном проекте VS
source_group("Header Files/Graphs" FILES ${HEADER_GRAPHS})
source_group("Header Files/Graphs/Helpers" FILES ${HEADER_GRAPHS_HELPERS})

source_group("Header Files/HydroNet" FILES ${HEADER_HYDRONET})
source_group("Header Files/HydroNet/BlockTree" FILES ${HEADER_HYDRONET_BLOCK_TREE})
source_group("Header Files/HydroNet/FlowSolvers" FILES ${HEADER_HYDRONET_FLOW_SOLVERS})
source_group("Header Files/HydroNet/StructuredSolvers" FILES ${HEADER_HYDRONET_STRUCTURED_SOLVERS})
source_group("Header Files/HydroNet/FullPropagatableGraph" FILES ${HEADER_HYDRONET_FULL_PROPAGATABLE_GRAPH})
source_group("Header Files/HydroNet/Topology" FILES ${HEADER_HYDRONET_TOPOLOGY})

source_group("Header Files/OilTransport" FILES ${HEADER_OIL_TRANSPORT})
source_group("Header Files/OilTransport/SolversBufferedDecorators" FILES ${HEADER_OIL_TRANSPORT_BUFFERED_SOLVERS})
source_group("Header Files/OilTransport/Measurements" FILES ${HEADER_OIL_TRANSPORT_MEASUREMENTS})
source_group("Header Files/OilTransport/ModelsTransport" FILES ${HEADER_OIL_TRANSPORT_MODELS})
source_group("Header Files/OilTransport/ModelsHydro" FILES ${HEADER_OIL_HYDRO_MODELS})
source_group("Header Files/OilTransport/TransportTopology" FILES ${HEADER_OIL_TRANSPORT_TRANSPORT_TOPOLOGY})
source_group("Header Files/OilTransport/Serialization" FILES ${HEADER_OIL_TRANSPORT_PARSERS})

source_group("Header Files/OilHydro" FILES ${HEADER_OIL_HYDRO})


source_group("Source Files/HydroNet" FILES ${SOURCE_HYDRONET})
source_group("Source Files/OilTransport" FILES ${SOURCE_OIL_TRANSPORT})
source_group("Source Files/OilTransport/Measurements" FILES ${SOURCE_OIL_TRANSPORT_MEASUREMENTS})
source_group("Source Files/OilTransport/Serialization" FILES ${SOURCE_OIL_TRANSPORT_PARSERS})


# Создание библиотеки
add_library(${PROJECT_NAME} 
    STATIC
    ${SOURCE}
    ${HEADER}
    ${HEADER_GRAPHS}
    ${HEADER_GRAPHS_HELPERS}
    ${HEADER_HYDRONET}
    ${HEADER_HYDRONET_BLOCK_TREE}
    ${HEADER_HYDRONET_FLOW_SOLVERS}
    ${HEADER_HYDRONET_STRUCTURED_SOLVERS}
    ${HEADER_HYDRONET_FULL_PROPAGATABLE_GRAPH}
    ${HEADER_HYDRONET_TOPOLOGY}
    ${HEADER_OIL_TRANSPORT}
    ${HEADER_OIL_TRANSPORT_BUFFERED_SOLVERS}
    ${HEADER_OIL_TRANSPORT_MEASUREMENTS}
    ${HEADER_OIL_TRANSPORT_MODELS}
    ${HEADER_OIL_TRANSPORT_TRANSPORT_TOPOLOGY}
    ${HEADER_OIL_TRANSPORT_PARSERS}
    ${HEADER_OIL_HYDRO}
    ${HEADER_OIL_HYDRO_MODELS}
    ${SOURCE_HYDRONET}
    ${SOURCE_OIL_TRANSPORT}
    ${SOURCE_OIL_TRANSPORT_MEASUREMENTS}
    ${SOURCE_OIL_TRANSPORT_PARSERS}
)

# Линковка с тербуемыми библиотеками
target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
    pde_solvers::pde_solvers 
    Boost::headers
)
   
# include директории
target_include_directories(${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/graph_solvers>
)
add_library(graph_solvers::graph_solvers ALIAS graph_solvers)

# Если собирается адаптер (shared библиотека), нужно компилировать статическую библиотеку с -fPIC
option(GRAPH_SOLVERS_BUILD_ADAPTER "" OFF)
if(GRAPH_SOLVERS_BUILD_ADAPTER AND NOT WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

option(GRAPH_SOLVERS_INSTALL "" ON)

# установка
if(GRAPH_SOLVERS_INSTALL)
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)
    install(TARGETS graph_solvers
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        EXPORT graph_solversTargets
    )
    install(EXPORT graph_solversTargets NAMESPACE graph_solvers::
    FILE graph_solversTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/graph_solvers
    )


    # generate the config file that is includes the exports
    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/graph_solversConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/graph_solvers"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )
    install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/graph_solversConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/graph_solvers
    )

    # Установка заголовочных файлов (headers)
    install(FILES ${HEADER} DESTINATION include/graph_solvers/)
    install(FILES ${HEADER_GRAPHS} DESTINATION include/graph_solvers/graphs)
    install(FILES ${HEADER_GRAPHS_HELPERS} DESTINATION include/graph_solvers/graphs/helpers)
    install(FILES ${HEADER_HYDRONET} DESTINATION include/graph_solvers/hydronet)
    install(FILES ${HEADER_HYDRONET_BLOCK_TREE} DESTINATION include/graph_solvers/hydronet/block_tree)
    install(FILES ${HEADER_HYDRONET_FLOW_SOLVERS} DESTINATION include/graph_solvers/hydronet/flow_solvers)
    install(FILES ${HEADER_HYDRONET_STRUCTURED_SOLVERS} DESTINATION include/graph_solvers/hydronet/structured_solvers)

    install(FILES ${HEADER_HYDRONET_FULL_PROPAGATABLE_GRAPH} DESTINATION include/graph_solvers/hydronet/full_propagatable_graph)
    install(FILES ${HEADER_HYDRONET_TOPOLOGY} DESTINATION include/graph_solvers/hydronet/topology)
    install(FILES ${HEADER_OIL_TRANSPORT} DESTINATION include/graph_solvers/oil_transport)
    install(FILES ${HEADER_OIL_TRANSPORT_BUFFERED_SOLVERS} DESTINATION include/graph_solvers/oil_transport/solvers_buffered_decorators)
    install(FILES ${HEADER_OIL_TRANSPORT_MEASUREMENTS} DESTINATION include/graph_solvers/oil_transport/measurements)
    install(FILES ${HEADER_OIL_TRANSPORT_MODELS} DESTINATION include/graph_solvers/oil_transport/models_transport)
    install(FILES ${HEADER_OIL_TRANSPORT_TRANSPORT_TOPOLOGY} DESTINATION include/graph_solvers/oil_transport/transport_topology)
    install(FILES ${HEADER_OIL_TRANSPORT_PARSERS} DESTINATION include/graph_solvers/oil_transport/serialization)
    install(FILES ${HEADER_OIL_HYDRO} DESTINATION include/graph_solvers/oil_transport)
    install(FILES ${HEADER_OIL_HYDRO_MODELS} DESTINATION include/graph_solvers/oil_transport/models_hydro)

endif()



option(GRAPH_SOLVERS_BUILD_TESTS "" ON)
option(GRAPH_SOLVERS_BUILD_CHEMDAE "" OFF)
# Сборка тестов
if(GRAPH_SOLVERS_BUILD_TESTS)

    find_package(Threads)
    find_package(GTest REQUIRED)

    set(TESTING_GRAPHS_HEADERS
        testing/test_graphs/test_graphs.h
        testing/test_graphs/BlockTreeTopologicalSort.h
        testing/test_graphs/BiconnectedComponentsTests.h
        testing/test_graphs/MatrixIncidences.h
    )

    set(TESTING_GRAPHS_SOURCE
        testing/test_graphs/test_graphs.cpp
    )
    
    set(TESTING_HYDRONET_HEADERS
        testing/test_hydronet/test_hydronet.h
    )
    set(TESTING_HYDRONET_SOURCE
        testing/test_hydronet/test_hydronet.cpp
    )

    set(TESTING_OIL_TRANSPORT_HEADERS
        testing/test_oilnet/test_oilnet.h
        testing/test_oilnet/TestMixer.h
        testing/test_oilnet/TestHydro.h
        testing/test_oilnet/TestGraphBuilder.h
    )

    if(GRAPH_SOLVERS_BUILD_CHEMDAE)
        find_package(Boost REQUIRED COMPONENTS serialization)
        find_package(vle_solvers REQUIRED)
        set(TESTING_CHEMDAE_HEADERS
            testing/test_chemdae/test_chemdae.h
            testing/test_chemdae/TestDAE.h
        )
        set(TESTING_CHEMDAE_SOURCE
            testing/test_chemdae/test_chemdae.cpp
        )
    endif()

    set(TESTING_OIL_TRANSPORT_SOURCE
        testing/test_oilnet/TestGGA.cpp
        testing/test_oilnet/test_oilnet.cpp        
    )
    
    set(TESTING_OIL_TRANSPORT_NODAL_SOLVER_HEADERS
        testing/test_oilnet/NodalSolver/TestNodalSolver.h
        testing/test_oilnet/NodalSolver/TestNodalSolverRenumbering.h
		testing/test_oilnet/NodalSolver/TestNodalSolverBufferBase.h
    )

    set(TESTING_OIL_TRANSPORT_BLOCK_PROPAGATION_SOLVER_HEADERS
        testing/test_oilnet/BlockPropagationSolver/BlockPropConfidence.h
        testing/test_oilnet/BlockPropagationSolver/BlockPropEndogenous.h
    )

    set(TESTING_OIL_TRANSPORT_GRAPH_PROPAGATION_SOLVER_HEADERS
        testing/test_oilnet/GraphPropagationSolver/FullPropEndogenous.h
        testing/test_oilnet/GraphPropagationSolver/FullPropFlow.h
        testing/test_oilnet/GraphPropagationSolver/FullPropConfidence.h
    )

    set(TESTING_OIL_TRANSPORT_TASK_HEADERS
        testing/test_oilnet/Task/CustomPipeSolver.h
        testing/test_oilnet/Task/MeasurementsCompleteness.h
        testing/test_oilnet/Task/TaskMeasuresHandling.h
        testing/test_oilnet/Task/TaskControlsHandling.h
        testing/test_oilnet/Task/HydroTransportTask.h
        testing/test_oilnet/Task/HydroTask.h
    )

    set(TESTING_OIL_TRANSPORT_SERIALIZATION_HEADERS
        testing/test_oilnet/Serialization/TestGraphParser.h
        testing/test_oilnet/Serialization/TestResultSerialization.h
        testing/test_oilnet/Serialization/TestBoundParserJson.h
        testing/test_oilnet/Serialization/TestSettingsParser.h
    )

    set(TESTING_OIL_TRANSPORT_UTILS_HEADERS
        testing/test_oilnet/utils/task_test_utils.h
        testing/test_oilnet/utils/scheme_loaders.h
    )

    set(RESEARCH_OIL_TRANSPORT_HEADERS
        testing/research_oilnet/research_oilnet.h
    )
    set(RESEARCH_OIL_TRANSPORT_SOURCE
        testing/research_oilnet/2025-density.cpp
        testing/research_oilnet/research_oilnet.cpp
    )

    #set(TESTING_OIL_HYDRO_HEADERS
    #    testing/test_oilhydro/research_oilnet.h
    #)
    #set(TESTING_OIL_HYDRO_SOURCE
    #    testing/test_oilhydro/2025-density.cpp
    #    testing/test_oilhydro/research_oilnet.cpp
    #)
    #

    source_group("Header Files/" FILES ${TESTING_GRAPHS_HEADERS})
    source_group("Source Files/" FILES ${TESTING_GRAPHS_SOURCE})

    source_group("Header Files/" FILES ${TESTING_HYDRONET_HEADERS})
    source_group("Source Files/" FILES ${TESTING_HYDRONET_SOURCE})

    source_group("Header Files/" FILES ${TESTING_OIL_TRANSPORT_HEADERS})
    source_group("Source Files/" FILES ${TESTING_OIL_TRANSPORT_SOURCE})
    source_group("Header Files/BlockPropagationSolver" FILES ${TESTING_OIL_TRANSPORT_BLOCK_PROPAGATION_SOLVER_HEADERS})
    source_group("Header Files/GraphPropagationSolver" FILES ${TESTING_OIL_TRANSPORT_GRAPH_PROPAGATION_SOLVER_HEADERS})
	source_group("Header Files/NodalSolver" FILES ${TESTING_OIL_TRANSPORT_NODAL_SOLVER_HEADERS})
    source_group("Header Files/Task" FILES ${TESTING_OIL_TRANSPORT_TASK_HEADERS})
    source_group("Header Files/Serialization" FILES ${TESTING_OIL_TRANSPORT_SERIALIZATION_HEADERS})
    source_group("Header Files/Utils" FILES ${TESTING_OIL_TRANSPORT_UTILS_HEADERS})

    source_group("Header Files/" FILES ${RESEARCH_OIL_TRANSPORT_HEADERS})
    source_group("Source Files/" FILES ${RESEARCH_OIL_TRANSPORT_SOURCE})

    if(GRAPH_SOLVERS_BUILD_CHEMDAE)
        source_group("Header Files/" FILES ${TESTING_CHEMDAE_HEADERS})
        source_group("Source Files/" FILES ${TESTING_CHEMDAE_SOURCE})
    endif()

      
    add_executable(testing_graphs 
        ${TESTING_GRAPHS_HEADERS} 
        ${TESTING_GRAPHS_SOURCE}
    )
    target_link_libraries(testing_graphs graph_solvers::graph_solvers GTest::gtest)
    target_include_directories(testing_graphs
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/test_graphs>
    )
    install(TARGETS testing_graphs DESTINATION bin)


    add_executable(testing_hydronet 
        ${TESTING_HYDRONET_SOURCE} 
        ${TESTING_HYDRONET_HEADERS} 
    )
    target_link_libraries(testing_hydronet graph_solvers::graph_solvers GTest::gtest)
    target_include_directories(testing_hydronet
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/test_hydronet>
    )
    install(TARGETS testing_hydronet DESTINATION bin)
    

    add_executable(testing_oiltransport 
        ${TESTING_OIL_TRANSPORT_SOURCE} 
        ${TESTING_OIL_TRANSPORT_BLOCK_PROPAGATION_SOLVER_HEADERS} 
        ${TESTING_OIL_TRANSPORT_GRAPH_PROPAGATION_SOLVER_HEADERS}
		${TESTING_OIL_TRANSPORT_NODAL_SOLVER_HEADERS}
        ${TESTING_OIL_TRANSPORT_TASK_HEADERS}
        ${TESTING_OIL_TRANSPORT_SERIALIZATION_HEADERS}
        ${TESTING_OIL_TRANSPORT_UTILS_HEADERS}
        ${TESTING_OIL_TRANSPORT_HEADERS}
    )
    target_link_libraries(testing_oiltransport graph_solvers::graph_solvers GTest::gtest)
    target_include_directories(testing_oiltransport
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/test_oilnet>
    )

    install(TARGETS testing_oiltransport DESTINATION bin)

    if(GRAPH_SOLVERS_BUILD_CHEMDAE)
        add_executable(testing_chemdae
            ${TESTING_CHEMDAE_SOURCE}
            ${TESTING_CHEMDAE_HEADERS}
        )
        target_link_libraries(testing_chemdae graph_solvers::graph_solvers vle_solvers::vle_solvers GTest::gtest)
        target_include_directories(testing_chemdae
            PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/test_chemdae>
        )
        install(TARGETS testing_chemdae DESTINATION bin)
    endif()

    add_executable(research_oiltransport 
        ${RESEARCH_OIL_TRANSPORT_HEADERS} 
        ${RESEARCH_OIL_TRANSPORT_SOURCE}
    )
    target_link_libraries(research_oiltransport graph_solvers::graph_solvers GTest::gtest)
    target_include_directories(research_oiltransport
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/research_oilnet>
    )
    install(TARGETS research_oiltransport DESTINATION bin)

endif()

# Сборка DLL-адаптера для .NET
if(GRAPH_SOLVERS_BUILD_ADAPTER)
    
    set(ADAPTER_HEADERS
    )
    
    set(ADAPTER_SOURCES
        netadapter/GraphSolversAdapter/GraphSolversAdapter.cpp
    )
    
    source_group("Header Files" FILES ${ADAPTER_HEADERS})
    source_group("Source Files" FILES ${ADAPTER_SOURCES})
    
    add_library(graph_solvers_adapter SHARED
        ${ADAPTER_HEADERS}
        ${ADAPTER_SOURCES}
    )
    
    target_link_libraries(graph_solvers_adapter
        PRIVATE
        graph_solvers::graph_solvers
        pde_solvers::pde_solvers
        Boost::headers
    )
    
    target_include_directories(graph_solvers_adapter
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/netadapter/GraphSolversAdapter>
        PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    
    # Определение макроса для экспорта функций
    target_compile_definitions(graph_solvers_adapter
        PRIVATE
        GRAPHSOLVERSADAPTER_EXPORTS
    )
    
    # Имя DLL должно соответствовать ожидаемому в C# коде
    set_target_properties(graph_solvers_adapter PROPERTIES
        OUTPUT_NAME "GraphSolversAdapter"
    )
    
    install(TARGETS graph_solvers_adapter
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    
    # Автоматическое копирование библиотеки в netadapter/bin после сборки
    # Библиотека будет подтянута в тесты через PreBuild event в Tests.csproj
    if(WIN32)
        set(ADAPTER_LIB_NAME "GraphSolversAdapter.dll")
    else()
        set(ADAPTER_LIB_NAME "libGraphSolversAdapter.so")
    endif()
    add_custom_command(TARGET graph_solvers_adapter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            "${CMAKE_CURRENT_SOURCE_DIR}/netadapter/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:graph_solvers_adapter>"
            "${CMAKE_CURRENT_SOURCE_DIR}/netadapter/bin/${ADAPTER_LIB_NAME}"
        COMMENT "Copy GraphSolversAdapter library to netadapter/bin"
    )
    
endif()

