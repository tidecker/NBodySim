cmake_minimum_required(VERSION 3.16)

project(NBodySim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nbody_core src/physics.cpp src/simulation.cpp src/presets.cpp src/collisions.cpp)
target_include_directories(nbody_core PUBLIC include)

add_library(nbody_view_settings src/view_settings.cpp)
target_include_directories(nbody_view_settings PUBLIC include)

add_executable(nbody_sim src/main.cpp src/config.cpp src/trajectory.cpp)
target_link_libraries(nbody_sim PRIVATE nbody_core nbody_view_settings)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(NBODY_BUILD_VIEWER "Build the 3D OpenGL trajectory viewer" OFF)
if(NBODY_BUILD_VIEWER)
    enable_language(C)
    add_subdirectory(visualization)
endif()
