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
       NOT header STREQUAL "step,time,particle_id,x,y,z,radius" OR
       NOT last_row MATCHES "${last_row_pattern}")
        message(FATAL_ERROR "Unexpected trajectory at ${path}: ${line_count} lines, last row ${last_row}")
    endif()
endfunction()

set(custom_header "mode=custom\nsteps=1\n")
set(anchor "[particle]\nname=anchor\nmass=5\nradius=0.25\nposition=0,0,2\nvelocity=99,0,0\ncolor=255,128,0\nfixed=true\n")
set(mover "[particle]\nname=mover\nmass=1\nradius=0.25\nposition=0.375,0,2\nvelocity=0,1,0\ncolor=0,10,255\nfixed=false\n")

if(CASE STREQUAL "execution")
    file(MAKE_DIRECTORY "${work}/output/stale" "${work}/output/mixed/nested")
    file(WRITE "${work}/output/trajectory.csv" "old single run")
    file(WRITE "${work}/output/stale/trajectory.csv" "old batch run")
    file(WRITE "${work}/output/stale/trajectory.view.ini" "old settings")
    file(WRITE "${work}/output/mixed/trajectory.csv" "old mixed run")
    file(WRITE "${work}/output/mixed/notes.txt" "keep these notes")
    file(WRITE "${work}/output/mixed/nested/trajectory.csv" "keep this nested file")
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

[simulation]
name = triple
mode = three-body
steps = 1
[visualization]
particle_size = 12
trails = true
trail_length = 42
sphere_quality = 12
trail_width = 3

[simulation]
name = merging
mode = two-body
steps = 2
collisions = true
collision_type = merge
min_radius = 0.75
max_radius = 0.75
]=])
    file(APPEND "${config}"
        "\n[simulation]\nname=custom_off\n${custom_header}collisions=false\ncollision_type=merge\n${anchor}${mover}"
        "\n[simulation]\nname=custom_on\n${custom_header}collisions=true\ncollision_type=merge\n${anchor}${mover}"
        "[visualization]\ntrails=true\n"
        "\n[simulation]\nname=custom_single\n${custom_header}${anchor}")
    foreach(mode IN ITEMS random galaxy solar-system collision two-body three-body)
        set(count_setting "")
        if(mode STREQUAL "random")
            set(count_setting "particles=7\n")
        endif()
        file(APPEND "${config}"
            "\n[simulation]\nname=mixed-${mode}\nmode=${mode}\n${count_setting}steps=1\n${anchor}${mover}")
    endforeach()
    file(APPEND "${config}" [=[

[simulation]
name=mixed-impact
mode=two-body
particles=2
dt=0.02
steps=1
collisions=true
collision_type=merge
[particle]
name=projectile
mass=0.01
radius=0.1
position=-0.37,0,0
velocity=-1,0,0
color=200,100,0
fixed=false
]=])
    run_batch(0)
    if(NOT IS_DIRECTORY "${work}/saved_sim" OR EXISTS "${work}/output/stale" OR
       EXISTS "${work}/output/mixed/trajectory.csv" OR
       NOT EXISTS "${work}/output/mixed/notes.txt" OR
       NOT EXISTS "${work}/output/mixed/nested/trajectory.csv")
        message(FATAL_ERROR "Unexpected batch cleanup scope")
    endif()
    if(NOT summary MATCHES "Simulation short_orbit.*Simulation cloud-3.*Simulation roundoff.*Simulation initial_only" OR
       NOT summary MATCHES "Particles: 3" OR NOT summary MATCHES "Simulation time" OR
       NOT summary MATCHES "trajectory.csv")
        message(FATAL_ERROR "Missing or unordered batch summaries: ${summary}")
    endif()
    if(NOT summary MATCHES "Simulation short_orbit .1/" OR
       NOT summary MATCHES "Time step: 0[.]125 . Steps: 2 . Duration: 0[.]25" OR
       NOT summary MATCHES "100% . Steps 2/2 . Elapsed .*ETA" OR
       NOT summary MATCHES "Final particles: 1 . Collisions: 1 . Merges: 1" OR
       NOT summary MATCHES "Total execution time" OR NOT summary MATCHES "Batch summary")
        message(FATAL_ERROR "Missing run progress or statistics: ${summary}")
    endif()
    check_trajectory(short_orbit 7 "^2,0[.]25,1,")
    check_trajectory(cloud-3 7 "^1,0[.]001,2,")
    check_trajectory(roundoff 9 "^3,")
    check_trajectory(initial_only 3 "^0,0,1,")
    check_trajectory(triple 7 "^1,0[.]001,2,")
    file(STRINGS "${work}/output/triple/trajectory.view.ini" settings)
    if(NOT settings STREQUAL "particle_size=12;trails=true;trail_length=42;sphere_quality=12;trail_width=3")
        message(FATAL_ERROR "Unexpected viewer settings: ${settings}")
    endif()
    check_trajectory(merging 5 "^2,0[.]002,2,0,0,0,0[.]94")
    foreach(entry IN ITEMS "random,7" "galaxy,100" "solar-system,9" "collision,100" "two-body,2" "three-body,3")
        string(REPLACE "," ";" fields "${entry}")
        list(GET fields 0 mode)
        list(GET fields 1 count)
        file(STRINGS "${work}/output/mixed-${mode}/trajectory.csv" mixed)
        list(LENGTH mixed lines)
        math(EXPR anchor_index "${count} + 1")
        math(EXPR expected_lines "2 * (${count} + 2) + 1")
        list(GET mixed 1 first_generated)
        list(GET mixed ${anchor_index} first_manual)
        if(NOT lines EQUAL expected_lines OR NOT first_generated MATCHES "^0,0,0,.*,,,$" OR
           NOT first_manual STREQUAL "0,0,${count},0,0,2,0.25,255,128,0")
            message(FATAL_ERROR "Incorrect mixed count, IDs, or RGB for ${mode}")
        endif()
    endforeach()
    file(STRINGS "${work}/output/mixed-impact/trajectory.csv" impact)
    list(LENGTH impact impact_lines)
    list(GET impact -1 merged_impact)
    if(NOT impact_lines EQUAL 6 OR NOT merged_impact MATCHES "^1,0[.]02,3,.*,200,100,0$")
        message(FATAL_ERROR "Generated and manual bodies did not merge with the assigned color: ${impact}")
    endif()
    file(STRINGS "${work}/output/custom_off/trajectory.csv" custom_off)
    file(STRINGS "${work}/output/custom_on/trajectory.csv" custom_on)
    file(STRINGS "${work}/output/custom_single/trajectory.csv" custom_single)
    list(LENGTH custom_off off_count)
    list(LENGTH custom_on on_count)
    list(LENGTH custom_single single_count)
    list(GET custom_off 0 custom_columns)
    list(GET custom_off 3 fixed_row)
    list(GET custom_off 4 moving_row)
    list(GET custom_on 3 merged_row)
    if(NOT off_count EQUAL 5 OR NOT on_count EQUAL 4 OR NOT single_count EQUAL 3 OR
       NOT custom_columns STREQUAL "step,time,particle_id,x,y,z,radius,red,green,blue" OR
       NOT fixed_row STREQUAL "1,0.001,0,0,0,2,0.25,255,128,0" OR
       NOT moving_row MATCHES "^1,0[.]001,1,0[.]374[^,]*,0[.]001,2,0[.]25,0,10,255$" OR
       NOT merged_row MATCHES "^1,0[.]001,0,0,0,2,0[.]31[^,]*,255,128,0$")
        message(FATAL_ERROR "Unexpected custom trajectories: ${custom_off} / ${custom_on} / ${custom_single}")
    endif()
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

    file(WRITE "${work}/saved_sim/short_orbit/trajectory.csv" "saved trajectory")
    file(WRITE "${work}/saved_sim/short_orbit/trajectory.view.ini" "saved settings")
    file(WRITE "${config}" "[simulation]\nname=replacement\nsteps=0\nprogress=false\n")
    run_batch(0)
    if(summary MATCHES "100%" OR NOT summary MATCHES "Batch summary")
        message(FATAL_ERROR "Disabling progress changed the summary or left progress enabled")
    endif()
    file(READ "${work}/saved_sim/short_orbit/trajectory.csv" saved_csv)
    file(READ "${work}/saved_sim/short_orbit/trajectory.view.ini" saved_settings)
    if(EXISTS "${work}/output/short_orbit" OR EXISTS "${work}/output/triple" OR
       NOT saved_csv STREQUAL "saved trajectory" OR NOT saved_settings STREQUAL "saved settings")
        message(FATAL_ERROR "Replacement batch failed to clean old runs or preserve saved results")
    endif()
else()
    file(WRITE "${work}/output/previous/trajectory.csv" "keep on validation failure")
    function(check_invalid content pattern)
        file(WRITE "${config}" "[simulation]\nname=validate_before_writing\nsteps=0\n${content}")
        run_batch(1)
        file(READ "${work}/output/previous/trajectory.csv" previous)
        if(NOT error MATCHES "simulations[.]txt:[0-9]+: ${pattern}" OR
           EXISTS "${work}/output/validate_before_writing" OR
           NOT previous STREQUAL "keep on validation failure")
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
    check_invalid("[simulation]\nname=bad\nsteps=0\n[visualization]\ntrail_length=0\n" "trail_length must be an integer")
    check_invalid("[simulation]\nname=bad\nsteps=0\nmin_radius=0.1\n" "min_radius must not exceed max_radius")
    check_invalid("[simulation]\nname=bad\nsteps=0\nrestitution=1.1\n" "restitution must be at most 1")
    set(custom "[simulation]\nname=bad\n${custom_header}")
    check_invalid("${custom}" "custom mode requires at least one")
    check_invalid("${custom}${anchor}${anchor}" "duplicate particle name")
    check_invalid("${custom}particles=1\n${anchor}" "custom mode derives its count")
    check_invalid("[simulation]\nname=bad\nsteps=1\n[visualization]\n${anchor}" "expected .particle. after simulation")
    check_invalid("${custom}[particle]\nname=incomplete\n" "missing particle setting: mass")
    foreach(key IN ITEMS mass radius position velocity color fixed)
        if(key STREQUAL "mass" OR key STREQUAL "radius")
            set(bad_value 0)
        elseif(key STREQUAL "position")
            set(bad_value "1,2")
        elseif(key STREQUAL "velocity")
            set(bad_value "1,2,nan")
        elseif(key STREQUAL "color")
            set(bad_value "0,256,0")
        else()
            set(bad_value yes)
        endif()
        string(REGEX REPLACE "${key}=[^\n]+" "${key}=${bad_value}" invalid_particle "${anchor}")
        check_invalid("${custom}${invalid_particle}" "${key}")
    endforeach()
    string(REPLACE "radius=0.25" "radius=10" blocked "${anchor}")
    file(WRITE "${config}" "[simulation]\nname=blocked\nmode=random\nparticles=1\nsteps=0\n${blocked}")
    run_batch(1)
    if(NOT error MATCHES "cannot place a random particle" OR
       NOT EXISTS "${work}/output/previous/trajectory.csv")
        message(FATAL_ERROR "Placement failure triggered cleanup or lacked a diagnostic: ${error}")
    endif()
endif()
