cmake_minimum_required(VERSION 3.15)
project(PorcelainMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
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
