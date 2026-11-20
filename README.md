# NBodySim

NBodySim is a C++ N-body gravitational simulation project for USC CSCI 596.
Phase 1 provides a configurable sequential simulator and writes trajectories for future visualization.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/` | Physics, integration, command-line configuration, and CSV output |
| `include/` | Particle representation and module interfaces |
| `visualization/` | Standalone OpenGL trajectory viewer |
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

## Batch simulations

Use the included `simulations.txt` from the project root:

```powershell
.\build\Release\nbody_sim.exe --config simulations.txt
```

On Linux, run `./build/nbody_sim --config simulations.txt`. Batch mode takes the
configuration file as its only argument. The existing single-run flags remain available.

Repeat the `[simulation]` section for each run. Use one `key = value` per line.
Blank lines are allowed, and `#` and `;` start comments anywhere on a line.
Each section requires a unique `name` and exactly one of `steps` or `duration`.
Names use ASCII letters, digits, underscores, or hyphens. Name uniqueness is
case-insensitive, and Windows device names such as `CON` are rejected.
The optional `mode`, `particles`, `dt`, and `seed` settings have the same defaults
and validation as their single-run flags.

`duration` is a finite, nonnegative amount of simulated time. The simulator
rounds `duration / dt` up to a whole step so the final time reaches or exceeds
the requested duration. A ratio within floating-point roundoff of a positive
integer uses that integer. For example, `duration = 0.14` with `dt = 0.125`
uses two steps and ends at time 0.25. A zero duration writes only the initial frame.

The entire configuration is validated before any output is written. Simulations
then run sequentially in file order. Each writes `output/<name>/trajectory.csv`
relative to the working directory and reports its particle count, output path,
simulation time, and file-writing time. Reusing a name in a later batch overwrites
that trajectory. If a run fails, the batch stops and earlier outputs remain.

## Design and limits

- The model uses dimensionless units with gravitational constant `G = 1` and fixed Plummer softening length `0.01`. The two-body mode places equal unit masses one unit apart with circular speeds calculated from the softened force. Random mode uses equal masses totaling one, with seeded positions in a cube and small random velocities.
- The force kernel evaluates each unordered pair once and applies equal and opposite forces. Velocity Verlet uses a half velocity update, a full position update, a force recalculation, and a second half velocity update. The force kernel and integrator have no dependency on command-line parsing, CSV, or visualization.
- The executable reports wall-clock simulation time for initialization and integration, and file-writing time for directory creation, CSV formatting, writes, and close. The two times are measured separately with `std::chrono::steady_clock`.
- The solver costs O(N²) per step, and writing every frame costs O(N × (steps + 1)) in output size. Softening bounds close-range acceleration for the provided masses, but the fixed time step still needs to be small enough to resolve encounters. Large steps can produce inaccurate trajectories.
- CTest covers softened forces, momentum and energy conservation, circular orbital behavior, configuration validation, batch execution, and output naming.

## OpenGL trajectory viewer

The optional C++17 viewer uses GLFW 3.4 for window and input handling and GLAD 1 to
load OpenGL functions. Running it requires a desktop session and a graphics driver
that supports OpenGL 3.3 core. The simulator and its tests build with the viewer disabled by default.

From the project root in PowerShell, enable the viewer and build it with Visual Studio:

```powershell
cmake -S . -B build -DNBODY_BUILD_VIEWER=ON
cmake --build build --config Release --target nbody_viewer
.\build\visualization\Release\nbody_viewer.exe .\output\trajectory.csv
```

CMake uses the existing GLFW source tree at `C:/libs/glfw-3.4/glfw-3.4` and the
generated GLAD files at `C:/libs/glad`. For different locations, pass
`-DGLFW_SOURCE_DIR="path/to/glfw"` and `-DGLAD_ROOT="path/to/glad"` when configuring.
The GLAD tree must contain `include/glad/glad.h`, `src/glad.c`, and the generated
support headers for OpenGL 3.3 core or later.

To generate a sample orbit from the project root:

```powershell
cmake --build build --config Release --target nbody_sim
.\build\Release\nbody_sim.exe --mode two-body --steps 4500 --output-dir output/orbit
.\build\visualization\Release\nbody_viewer.exe .\output\orbit\trajectory.csv
```

| Control | Action |
| --- | --- |
| Space | Play or pause |
| R | Restart at frame zero |
| Up / Down | Double or halve playback speed |
| Mouse wheel or + / − | Zoom in or out |
| Esc or window close button | Close the viewer |

The viewer expects the simulator's `step,time,particle_id,x,y,z` CSV header.
Rows must be grouped by contiguous steps starting at zero. Each frame must have
one timestamp and the same count of ascending, zero-based particle IDs. Timestamps
start at zero and increase between frames. At 1× speed, one simulation time unit
plays in one real second. Playback loops and the window title shows the displayed frame and time.

The viewer loads the full file into memory and draws recorded samples as
ID-colored points on an auto-fitted XY projection. This is intended for local
demonstrations with modest trajectories.

MPI, OpenMP, hybrid execution, and Discovery Slurm benchmarking are planned for later phases.
