# NBodySim

NBodySim is a C++ N-body gravitational simulation project for USC CSCI 596.
Phase 1 provides a configurable sequential simulator and writes trajectories for future visualization.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/` | Physics, integration, command-line configuration, and CSV output |
| `include/` | Particle representation and module interfaces |
| `visualization/` | Future OpenGL visualization code |
| `tests/` | Automated physics and integration tests |
| `benchmarks/` | Future performance benchmarks |
| `scripts/discovery/` | Future Slurm job scripts for USC's Discovery cluster |

## Development setup

- Install CMake 3.16 or newer and a C++17-capable compiler.
- On Windows, use Visual Studio or Visual Studio Build Tools with the **Desktop development with C++** workload. If you use Build Tools, run the commands in a Developer PowerShell so CMake can find the compiler and build tools.
- On Linux, use GCC (`g++`) or Clang (`clang++`) and the build tool for your CMake generator, such as Make or Ninja. On Discovery, use the available compiler and CMake modules.
- VS Code is optional. Its C/C++ and CMake Tools extensions provide editor and build integration.

From the project root, configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cd build
ctest -C Release --output-on-failure
cd ..
```

Run the executable using the path for your generator:

| Generator | Command |
| --- | --- |
| Visual Studio (PowerShell) | `.\build\Release\nbody_sim.exe --mode two-body --steps 4500 --output-dir output/orbit` |
| Linux Make or single-config Ninja | `./build/nbody_sim --mode random --particles 100 --dt 0.001 --steps 1000 --seed 42 --output-dir output/random` |

## Command-line options

| Option | Default | Meaning |
| --- | --- | --- |
| `--mode` | `two-body` | Use the deterministic circular orbit or select `random` for a seeded system. |
| `--particles` | 2 in two-body mode, 100 in random mode | Positive particle count. Two-body mode requires 2. |
| `--dt` | `0.001` | Positive time step in simulation units. |
| `--steps` | `1000` | Number of integration steps. Use 0 to export the initial state alone. |
| `--seed` | `42` | Unsigned 32-bit random seed used in random mode. |
| `--output-dir` | `output` | Destination directory, created if needed. |
| `--help` | | Print usage. |

Each run writes `trajectory.csv` with columns `step,time,particle_id,x,y,z`. The file contains one row per particle per frame, starting at step 0 and time 0. IDs are zero-based and retain their order across frames. Reusing an output directory overwrites its trajectory.

## Design and limits

- The model uses dimensionless units with gravitational constant `G = 1` and fixed Plummer softening length `0.01`. The two-body mode places equal unit masses one unit apart with circular speeds calculated from the softened force. Random mode uses equal masses totaling one, with seeded positions in a cube and small random velocities.
- The force kernel evaluates each unordered pair once and applies equal and opposite forces. Velocity Verlet uses a half velocity update, a full position update, a force recalculation, and a second half velocity update. The force kernel and integrator have no dependency on command-line parsing, CSV, or visualization.
- The executable reports wall-clock simulation time for initialization and integration, and file-writing time for directory creation, CSV formatting, writes, and close. The two times are measured separately with `std::chrono::steady_clock`.
- The solver costs O(N²) per step, and writing every frame costs O(N × (steps + 1)) in output size. Softening bounds close-range acceleration for the provided masses, but the fixed time step still needs to be small enough to resolve encounters. Large steps can produce inaccurate trajectories.
- CTest covers softened forces, momentum conservation, bounded energy error in an eccentric orbit, and approximately one full circular orbit.

MPI, OpenMP, hybrid execution, OpenGL visualization, and Discovery Slurm benchmarking are planned for later phases.
