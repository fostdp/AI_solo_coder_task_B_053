cmake_minimum_required(VERSION 3.15)
project(PorcelainMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
    add_definitions(-D_USE_MATH_DEFINES)
    set(Boost_USE_STATIC_LIBS ON)
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS
    system
    thread
    chrono
    regex
    filesystem
)

find_package(PostgreSQL REQUIRED)

find_package(spdlog REQUIRED)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/backend/include
    ${CMAKE_SOURCE_DIR}/backend/algorithms
)

set(BACKEND_SOURCES
    backend/src/main.cpp
    backend/src/profinet_parser.cpp
    backend/src/tcp_server.cpp
    backend/src/database.cpp
    backend/src/database_extensions.cpp
    backend/src/alert_manager.cpp
    backend/src/websocket_server.cpp
    backend/src/http_server.cpp
    backend/algorithms/crack_propagation.cpp
    backend/algorithms/dem_simulation.cpp
    backend/algorithms/stress_analysis.cpp
    backend/algorithms/washburn_model.cpp
    backend/algorithms/four_point_bending.cpp
)

add_executable(porcelain_monitor ${BACKEND_SOURCES})

target_link_libraries(porcelain_monitor
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(porcelain_monitor
        ws2_32
        mswsock
    )
endif()

set(SIMULATOR_SOURCES
    simulator/main.cpp
    backend/src/profinet_parser.cpp
)

add_executable(profinet_simulator ${SIMULATOR_SOURCES})

target_link_libraries(profinet_simulator
    ${Boost_LIBRARIES}
)

if(WIN32)
    target_link_libraries(profinet_simulator
        ws2_32
        mswsock
    )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

install(TARGETS porcelain_monitor profinet_simulator
    RUNTIME DESTINATION bin
)

install(DIRECTORY frontend/
    DESTINATION share/porcelain_monitor/frontend
)

install(FILES database/init.sql
    DESTINATION share/porcelain_monitor/database
)

include_directories(
    ${CMAKE_SOURCE_DIR}/tests
)

set(TEST_SOURCES
    tests/test_stress_analysis.cpp
    backend/algorithms/stress_analysis.cpp
)

add_executable(test_stress_analysis ${TEST_SOURCES})

target_link_libraries(test_stress_analysis
    ${Boost_LIBRARIES}
)

if(WIN32)
    target_link_libraries(test_stress_analysis
        ws2_32
        mswsock
    )
endif()

set(WASHBURN_TEST_SOURCES
    tests/test_washburn_model.cpp
    backend/algorithms/washburn_model.cpp
)

add_executable(test_washburn_model ${WASHBURN_TEST_SOURCES})

target_link_libraries(test_washburn_model
    ${Boost_LIBRARIES}
)

if(WIN32)
    target_link_libraries(test_washburn_model
        ws2_32
        mswsock
    )
endif()

set(FOUR_POINT_BENDING_TEST_SOURCES
    tests/test_four_point_bending.cpp
    backend/algorithms/four_point_bending.cpp
)

add_executable(test_four_point_bending ${FOUR_POINT_BENDING_TEST_SOURCES})

target_link_libraries(test_four_point_bending
    ${Boost_LIBRARIES}
)

if(WIN32)
    target_link_libraries(test_four_point_bending
        ws2_32
        mswsock
    )
endif()

set(ALL_TEST_SOURCES
    tests/test_main.cpp
    tests/test_stress_analysis.cpp
    tests/test_washburn_model.cpp
    tests/test_four_point_bending.cpp
    backend/algorithms/stress_analysis.cpp
    backend/algorithms/washburn_model.cpp
    backend/algorithms/four_point_bending.cpp
    backend/algorithms/crack_propagation.cpp
)

add_executable(porcelain_tests ${ALL_TEST_SOURCES})

target_link_libraries(porcelain_tests
    ${Boost_LIBRARIES}
)

if(WIN32)
    target_link_libraries(porcelain_tests
        ws2_32
        mswsock
    )
endif()

enable_testing()
add_test(NAME AllTests COMMAND porcelain_tests)
add_test(NAME StressAnalysisTests COMMAND porcelain_tests --suite StressFEMTests)
add_test(NAME WashburnModelTests COMMAND porcelain_tests --suite WashburnModelTests)
add_test(NAME FourPointBendingTests COMMAND porcelain_tests --suite BendingTestTests)
add_test(NAME StressAcceptance COMMAND porcelain_tests --suite StressFEMTests --test Acceptance)
add_test(NAME WashburnAcceptance COMMAND porcelain_tests --suite WashburnModelTests --test Acceptance)
add_test(NAME BendingAcceptance COMMAND porcelain_tests --suite BendingTestTests --test Acceptance)
