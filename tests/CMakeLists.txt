add_executable(nbody_tests simulation_tests.cpp)
target_link_libraries(nbody_tests PRIVATE nbody_core)
foreach(test IN ITEMS force momentum energy orbit)
    add_test(NAME ${test} COMMAND nbody_tests ${test})
endforeach()

add_executable(nbody_preset_tests preset_tests.cpp)
target_link_libraries(nbody_preset_tests PRIVATE nbody_core)
add_test(NAME presets COMMAND nbody_preset_tests)

add_executable(nbody_collision_tests collision_tests.cpp)
target_link_libraries(nbody_collision_tests PRIVATE nbody_core)
add_test(NAME collisions COMMAND nbody_collision_tests)

add_executable(nbody_csv_tests csv_tests.cpp ../visualization/csv.cpp)
target_include_directories(nbody_csv_tests PRIVATE ../visualization)
add_test(NAME trajectory_reader COMMAND nbody_csv_tests)

set_target_properties(nbody_tests nbody_preset_tests nbody_collision_tests nbody_csv_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

foreach(case IN ITEMS execution validation)
    add_test(NAME batch_${case} COMMAND "${CMAKE_COMMAND}"
        "-DSIMULATOR=$<TARGET_FILE:nbody_sim>" "-DCASE=${case}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/batch_tests.cmake"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endforeach()
