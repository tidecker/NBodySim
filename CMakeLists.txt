cmake_minimum_required(VERSION 3.16)

project(NBodySim LANGUAGES CXX)

add_executable(nbody_sim src/main.cpp)
set_target_properties(nbody_sim PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
