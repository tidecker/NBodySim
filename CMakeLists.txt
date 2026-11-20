cmake_minimum_required(VERSION 3.16)

project(NBodySim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nbody_core src/physics.cpp src/simulation.cpp)
target_include_directories(nbody_core PUBLIC include)

add_executable(nbody_sim src/main.cpp src/config.cpp src/trajectory.cpp)
target_link_libraries(nbody_sim PRIVATE nbody_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(nbody_tests tests/simulation_tests.cpp)
    target_link_libraries(nbody_tests PRIVATE nbody_core)
    foreach(test IN ITEMS force momentum energy orbit)
        add_test(NAME ${test} COMMAND nbody_tests ${test})
    endforeach()
    foreach(case IN ITEMS execution validation)
        add_test(NAME batch_${case} COMMAND "${CMAKE_COMMAND}"
            "-DSIMULATOR=$<TARGET_FILE:nbody_sim>" "-DCASE=${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/batch_tests.cmake")
    endforeach()
endif()

option(NBODY_BUILD_VIEWER "Build the OpenGL trajectory viewer" OFF)
if(NBODY_BUILD_VIEWER)
    enable_language(C)
    add_subdirectory(visualization)
endif()
