set(work "${CMAKE_CURRENT_BINARY_DIR}/batch-test-${CASE}")
file(MAKE_DIRECTORY "${work}")
set(config "${work}/simulations.txt")

function(run_batch expected_status)
    execute_process(COMMAND "${SIMULATOR}" --config "${config}"
        WORKING_DIRECTORY "${work}" RESULT_VARIABLE status
        OUTPUT_VARIABLE summary ERROR_VARIABLE error)
    if(NOT status STREQUAL "${expected_status}")
        message(FATAL_ERROR "Expected exit code ${expected_status}, got ${status}\n${summary}\n${error}")
    endif()
    set(summary "${summary}" PARENT_SCOPE)
    set(error "${error}" PARENT_SCOPE)
endfunction()

function(check_trajectory name expected_lines last_row_pattern)
    set(path "${work}/output/${name}/trajectory.csv")
    file(STRINGS "${path}" rows)
    list(LENGTH rows line_count)
    list(GET rows 0 header)
    list(GET rows -1 last_row)
    if(NOT line_count EQUAL expected_lines OR
       NOT header STREQUAL "step,time,particle_id,x,y,z" OR
       NOT last_row MATCHES "${last_row_pattern}")
        message(FATAL_ERROR "Unexpected trajectory at ${path}: ${line_count} lines, last row ${last_row}")
    endif()
endfunction()

if(CASE STREQUAL "execution")
    file(WRITE "${config}" [=[
# Repeated sections use independent defaults.
[simulation]
name = short_orbit
duration = 0.14 ; rounds up
dt = 0.125

[simulation] # A different mode and step count
name = cloud-3
mode = random
particles = 3
steps = 1
seed = 17

[simulation]
name = roundoff
dt = 0.1
duration = 0.30000000000000004

[simulation]
name = initial_only
duration = 0
]=])
    run_batch(0)
    if(NOT summary MATCHES "Simulation short_orbit.*Simulation cloud-3.*Simulation roundoff.*Simulation initial_only" OR
       NOT summary MATCHES "Particles: 3" OR NOT summary MATCHES "Simulation time" OR
       NOT summary MATCHES "trajectory.csv")
        message(FATAL_ERROR "Missing or unordered batch summaries: ${summary}")
    endif()
    check_trajectory(short_orbit 7 "^2,0[.]25,1,")
    check_trajectory(cloud-3 7 "^1,0[.]001,2,")
    check_trajectory(roundoff 9 "^3,")
    check_trajectory(initial_only 3 "^0,0,1,")
    if(EXISTS "${work}/output/trajectory.csv")
        message(FATAL_ERROR "Batch unexpectedly wrote the single-run trajectory")
    endif()

    execute_process(COMMAND "${SIMULATOR}" --mode random --particles 3 --steps 1 --seed 17
        --output-dir cli_output WORKING_DIRECTORY "${work}" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Single-run CLI failed with status ${status}")
    endif()
    file(SHA256 "${work}/cli_output/trajectory.csv" cli_hash)
    file(SHA256 "${work}/output/cloud-3/trajectory.csv" batch_hash)
    if(NOT cli_hash STREQUAL batch_hash)
        message(FATAL_ERROR "Batch and CLI produced different output with the same parameters")
    endif()
else()
    function(check_invalid content pattern)
        file(WRITE "${config}" "[simulation]\nname=validate_before_writing\nsteps=0\n${content}")
        run_batch(1)
        if(NOT error MATCHES "simulations[.]txt:[0-9]+: ${pattern}" OR
           EXISTS "${work}/output/validate_before_writing")
            message(FATAL_ERROR "Expected validation before writing and diagnostic '${pattern}', got: ${error}")
        endif()
    endfunction()
    check_invalid("[simulation]\nname=VALIDATE_BEFORE_WRITING\nsteps=0\n" "duplicate simulation name")
    check_invalid("[simulation]\nname=bad\ndt=-1\nsteps=1\n" "dt must be finite and positive")
    check_invalid("[simulation]\nname=bad\nsteps=1\nduration=1\n" "specify exactly one of steps or duration")
    check_invalid("[simulation]\nname=bad\n" "specify exactly one of steps or duration")
    check_invalid("[simulation]\nname=../outside\nsteps=1\n" "name must contain only")
    check_invalid("[simulation]\nname=bad\nsteps=0\nsteps=1\n" "duplicate setting: steps")
    check_invalid("[simulation]\nname=bad\nsteps=1\nparticles=3\n" "particles must be positive")
    check_invalid("[simulation]\nname=bad\ndt=0.0001\nduration=1e200\n" "duration and dt require too many steps")
endif()
