cmake_minimum_required(VERSION 3.20)
project(loom_core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOOM_BUILD_TESTS "Build Loom tests" ON)
option(LOOM_USE_BUNDLED_SQLITE "Compile third_party/sqlite3.c into loom_core" OFF)

add_library(loom_core
    src/db.cpp
    src/db_messages.cpp
    src/db_graph.cpp
    src/event_bus.cpp
)
add_library(loom::core ALIAS loom_core)

target_include_directories(loom_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(LOOM_USE_BUNDLED_SQLITE)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3.c")
        message(FATAL_ERROR "LOOM_USE_BUNDLED_SQLITE=ON but third_party/sqlite3.c is absent")
    endif()
    target_sources(loom_core PRIVATE third_party/sqlite3.c)
    target_include_directories(loom_core PRIVATE third_party)
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(loom_core PRIVATE SQLite::SQLite3)
endif()

if(MSVC)
    target_compile_options(loom_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(loom_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOOM_BUILD_TESTS)
    enable_testing()

    add_executable(db_compat_test tests/db_compat_test.cpp)
    target_link_libraries(db_compat_test PRIVATE loom::core)
    add_test(NAME db_compat_test COMMAND db_compat_test)

    add_executable(event_bus_test tests/event_bus_test.cpp)
    target_link_libraries(event_bus_test PRIVATE loom::core)
    add_test(NAME event_bus_test COMMAND event_bus_test)
endif()
