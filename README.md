# NBodySim

NBodySim is a C++ N-body gravitational simulation project for USC CSCI 596.
The sequential simulator writes trajectories for playback in the standalone 3D OpenGL viewer.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/` | Physics, integration, command-line configuration, and CSV output |
| `include/` | Particle representation and module interfaces |
| `visualization/` | Standalone OpenGL trajectory viewer |
| `tests/` | Automated physics and integration tests |
| `benchmarks/` | Future performance benchmarks |
| `scripts/discovery/` | Future Slurm job scripts for USC's Discovery cluster |
| `simulations.txt` | Batch scenarios, including the preset demos and stress test |
| `build/` | Generated CMake build tree on Windows and Linux |
| `output/` | Default location for generated trajectories |

## Build and test

Use CMake 3.16 or newer and a C++17 compiler. Run these commands from the project
root. Both workflows use `build/`.

### Windows with Visual Studio

Use Developer PowerShell with the **Desktop development with C++** workload.
The build below includes the simulator, tests, and 3D viewer.

```powershell
cmake -S . -B build -DNBODY_BUILD_VIEWER=ON
cmake --build build --config Release
Push-Location build
ctest -C Release --output-on-failure
Pop-Location

.\build\Release\nbody_sim.exe --mode solar-system --dt 0.005 --steps 1000 --output-dir output/solar_example
.\build\visualization\Release\nbody_viewer.exe .\output\solar_example\trajectory.csv
```

### Linux or Discovery command-line build

Use GCC or Clang with Make or Ninja. On Discovery, use the available compiler and
CMake modules. With the viewer disabled, the simulator and tests need only the C++ toolchain.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNBODY_BUILD_VIEWER=OFF
cmake --build build
(cd build && ctest --output-on-failure)
./build/nbody_sim --mode random --particles 100 --steps 1000 --seed 42 --output-dir output/random
```

## Command-line options

| Option | Default | Meaning |
| --- | --- | --- |
| `--mode` | `two-body` | Select any of the six initial-condition presets below. |
| `--particles` | Depends on mode | Total count, including any central body. |
| `--dt` | `0.001` | Positive time step in simulation units. |
| `--steps` | `1000` | Number of integration steps. Use 0 to export the initial state alone. |
| `--seed` | `42` | Unsigned 32-bit seed for random, solar-system, galaxy, and collision modes. |
| `--output-dir` | `output` | Destination directory, created if needed. |
| `--help` | | Print usage. |

Each run writes `trajectory.csv` with columns `step,time,particle_id,x,y,z`. The file contains one row per particle per frame, starting at step 0 and time 0. IDs are zero-based and retain their order across frames. Reusing an output directory overwrites its trajectory.

### Initial-condition presets

All presets use the same dimensionless distance, mass, and time units, with
`G = 1` and Plummer softening length `0.01`. Their positions and velocities
have three components. The original two-body case intentionally remains in XY.

| Mode | Particle count | Initial conditions |
| --- | --- | --- |
| `two-body` | Exactly 2 (default 2) | Two unit masses separated by one unit, with equal and opposite circular velocities from the softened force. |
| `random` | At least 1 (default 100) | Seeded uniform positions in the cube [−1, 1]³, small random velocities, and equal masses totaling one. |
| `solar-system` | At least 2 (default 9) | Central mass 1 and orbiters totaling 0.003, on nearly circular orbits with seeded phases and small inclinations. Radii are spaced from 0.35 toward 2. This is an idealized system. |
| `galaxy` | At least 2 (default 100) | Central mass 3 with a disk of mass 0.3. Disk radii are sampled uniformly by area from 0.4 to 3, with seeded phases and inclinations. |
| `collision` | At least 2 (default 100) | Two seeded spherical clusters, each of mass 1 and radius 0.65, approach at an initial relative speed of roughly 1.1 with a 0.5 transverse offset. |
| `three-body` | Exactly 3 (default 3) | Three unit masses with fixed perturbed figure-eight starting positions and velocities, producing a complex encounter with nonzero Z motion. |

The disk presets estimate starting speeds from the softened field of the central
mass. Every particle then evolves under the full solver. The disk and collision
presets start in their center-of-mass frame. The galaxy and clusters are
demonstration initial conditions with no equilibrium or long-term stability guarantee.

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

An optional `[visualization]` section immediately after the simulation settings
applies to that simulation only. Omitted keys use viewer defaults:

```ini
[simulation]
name = orbit_demo
mode = solar-system
dt = 0.005
duration = 5
seed = 42

[visualization]
particle_size = 9
trails = true
trail_length = 80
```

`particle_size` is an integer from 1 to 64 (default 8), `trails` is
`true` or `false` (default false), and `trail_length` is 1 to 256 recorded
frame intervals (default 32). Batch mode saves these settings in
`trajectory.view.ini` beside the CSV. The viewer automatically loads the sidecar
if present. A new run in the same directory without view settings clears the
generated sidecar. View options never enter the gravitational solver.

## Design and limits

- The force kernel evaluates each unordered pair once and applies equal and opposite forces. Velocity Verlet uses a half velocity update, a full position update, a force recalculation, and a second half velocity update. The force kernel and integrator have no dependency on command-line parsing, CSV, or visualization.
- The executable reports wall-clock simulation time for initialization and integration, and file-writing time for directory creation, CSV formatting, writes, and close. The two times are measured separately with `std::chrono::steady_clock`.
- The solver costs O(N²) per step, and writing every frame costs O(N × (steps + 1)) in output size. Softening bounds close-range acceleration for the provided masses, but the fixed time step still needs to be small enough to resolve encounters. Large steps can produce inaccurate trajectories.
- CTest covers softened forces, momentum and energy conservation, circular orbital behavior, configuration validation, batch execution, and output naming.

## OpenGL trajectory viewer

The optional 3D viewer uses GLFW 3.4 for window and input handling and GLAD 1 to
load OpenGL functions. Running it requires a desktop session and a graphics driver
that supports OpenGL 3.3 core. The simulator and its tests build with the viewer disabled by default.

CMake defaults to `C:/libs/glfw-3.4/glfw-3.4` and `C:/libs/glad` on Windows.
For other locations, including a Linux desktop build, pass
`-DGLFW_SOURCE_DIR="path/to/glfw"` and `-DGLAD_ROOT="path/to/glad"` with
`-DNBODY_BUILD_VIEWER=ON` when configuring.
The GLAD tree must contain `include/glad/glad.h`, `src/glad.c`, and the generated
support headers for OpenGL 3.3 core or later.
Pass a trajectory CSV as the viewer's sole argument. With a single-config
generator such as Make or Ninja, the viewer is `build/visualization/nbody_viewer`.

| Control | Action |
| --- | --- |
| Space | Play or pause |
| R | Restart at frame zero |
| Up / Down | Double or halve playback speed |
| Left mouse drag | Orbit the view |
| Right or middle mouse drag | Pan |
| Mouse wheel or + / − | Zoom in or out |
| [ / ] | Decrease or increase particle size |
| T | Toggle trails |
| F | Reset the camera to the fitted view |
| Esc or window close button | Close the viewer |

The viewer expects the simulator's `step,time,particle_id,x,y,z` CSV header.
Rows must be grouped by contiguous steps starting at zero. Each frame must have
one timestamp and the same count of ascending, zero-based particle IDs. Timestamps
start at zero and increase between frames. At 1× speed, one simulation time unit
plays in one real second. Playback loops and the window title shows the displayed frame and time.

The perspective camera fits the full trajectory on opening. It draws depth-tested,
ID-colored points and a reference grid on the centered XY plane. Red, green,
and blue axes indicate positive X, Y, and Z. Point sizes vary with distance.
Trails connect the recorded positions leading up to the displayed frame and
restart at the playback loop boundary. The viewer reduces trail history as
particle count increases to limit drawing to 200,000 trail segments per frame.
It loads the full CSV into memory, so startup time and memory use increase
with the number of recorded particles and frames.

MPI, OpenMP, hybrid execution, and Discovery Slurm benchmarking are planned for later phases.
