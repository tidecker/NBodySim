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
| `saved_sim/` | Local archive for simulation results you keep manually |

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
| `--progress true\|false` | `true` | Show the progress bar and ETA. Use `--progress false` for benchmarking. |

Each run writes `trajectory.csv` with columns `step,time,particle_id,x,y,z,radius`.
Runs with manual particles append `red,green,blue` as integers from 0 to 255.
Generated particles in a mixed run have three empty color fields. The viewer
uses the assigned colors for spheres and trails and its ID-based palette for
particles without an assigned color.
Frames start at step 0 and time 0. Each contains one row per live body in
ascending ID order. Initial IDs start at zero. Generated particles come first,
followed by manual particles in declaration order. When two movable particles
merge, both original IDs retire and the new body gets a fresh ID. It inherits
an assigned color if either particle has one, using the lower original ID if
both have assigned colors.
A fixed body keeps its ID and color when it absorbs another body.
Reusing an output directory
overwrites its trajectory.

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

### Radii and contact physics

Set these keys in `[simulation]`. They also have single-run CLI equivalents
with hyphens in place of underscores.

| Setting | Default | Meaning |
| --- | --- | --- |
| `collisions` | `false` | Enable sphere contact handling with `true`. |
| `collision_type` | `elastic` | Select `elastic`, `inelastic`, or `merge`. |
| `restitution` | `0.5` | Normal restitution from 0 to 1, used for inelastic impacts. |
| `min_radius` / `max_radius` | `0.02` / `0.02` | Bounds for ordinary body radii in simulation distance units. |
| `central_radius` | `0.07` | Radius of the central body in solar-system and galaxy modes. |

Radii must be from `1e-9` to `1e6`, with `min_radius <= max_radius`. Unequal bounds
produce seeded uniform radii. Masses scale with the cube of radius within each
group while preserving the preset's total mass, including each cluster's mass
in collision mode. The central body's mass stays constant during preset initialization. Two-body and three-body use
the midpoint radius for each body and keep their original masses and velocities.
Equal bounds keep the original preset masses in every mode. The `collision`
preset describes how the clusters start. Use `collisions = true` to enable contact.

The solver checks overlaps after the Verlet drift and before recomputing
gravity. Between movable particles, elastic and inelastic contacts conserve linear momentum and leave
tangential velocities alone. Elastic impacts conserve kinetic energy at the
instant of impact. Inelastic impacts reduce the normal relative speed by the
restitution factor. Merges between movable particles conserve mass, linear momentum, and sphere volume and
place the new body at the center of mass. Bodies carry no spin or friction.

Contact detection is discrete. Fast bodies can pass through each other between
steps, and up to four passes per step may leave residual overlap in dense
systems. Choose a time step small enough to resolve encounters. Overlap
correction can change gravitational potential energy.

### Manually defined particles

Add `[particle]` sections after the batch `[simulation]` settings and before
its optional `[visualization]` section. In preset modes, `particles` counts only
generated bodies, including any preset central body. Manual bodies add to that
count. Use `mode = custom` for a manual-only run and omit `particles`.
Each manual particle requires all seven
fields. Names are unique and case-sensitive within a run. Mass must be finite
and positive, and radius must be between `1e-9` and `1e6` in simulation distance
units. Position and velocity each take three finite, comma-separated numbers.
Color takes three comma-separated integers from 0 to 255. Preset radius and
seed settings apply to generated bodies.

All bodies share the same gravity solver and collision settings. Initial
placement keeps generated spheres clear of manual spheres, even with collisions
disabled. In `random` mode, overlapping generated positions are resampled from
the usual cube using the configured seed. If placement fails after 1,024
retries for a particle, the batch stops before cleaning up previous results.
Other presets shift the generated group along positive X when needed, preserving
its internal layout and velocities. Manual positions stay as configured.
The batch file includes mixed random and galaxy examples.

```ini
[simulation]
name = custom_orbit
mode = custom
dt = 0.005
duration = 7
collisions = false

[particle]
name = Anchor
mass = 1
radius = 0.12
position = 0, 0, 0
velocity = 0, 0, 0
color = 255, 186, 68
fixed = true

[particle]
name = Orbiter
mass = 0.005
radius = 0.045
position = 1, 0, 0
velocity = 0, 1, 0
color = 70, 174, 255
fixed = false
```

A fixed particle stays at its configured position with zero velocity even if
its input velocity is nonzero. It still exerts gravity on movable particles.
With `collisions = false` all overlap handling is disabled. With
`collisions = true`, contacts involving fixed particles follow these rules:

| Contact | Result |
| --- | --- |
| Elastic | The movable body takes the full overlap correction and its normal velocity reverses. |
| Inelastic | The movable body takes the full overlap correction and rebounds with its normal speed scaled by `restitution`. Tangential velocity stays the same in both bounce modes. |
| Merge | The fixed body absorbs the movable body's mass and sphere volume. Its position, zero velocity, ID, and color are preserved. The absorbed ID retires. |
| Two fixed bodies | The solver ignores the contact in every mode. |

The fixed position is an external constraint. Total momentum and energy
conservation may not apply to the movable-particle system, particularly
when contacts transfer momentum to that constraint or remove a movable body.
The batch file includes an orbit with contact disabled and an impact demo
with merging enabled.

## Batch simulations

Use the included `simulations.txt` from the project root:

```powershell
.\build\Release\nbody_sim.exe --config simulations.txt
```

On Linux, run `./build/nbody_sim --config simulations.txt`. Batch mode takes the
configuration file as its only argument. The existing single-run flags remain available.

### Run and view the batch on Windows

Build the Release simulator and 3D viewer using the Windows commands above.
Then, from PowerShell in the project root, run:

```powershell
.\run_all.ps1
.\run_all.ps1 -ViewOnly
```

The first command runs the full batch before opening its trajectories. Use
`-ViewOnly` to replay existing CSVs without running the simulator or cleaning up output. The script
follows the order in `simulations.txt`, shows the current name and how many
entries remain, and waits for each viewer window to close. Missing CSVs are
reported and skipped.

If your local PowerShell session blocks scripts, you can run this local script
in a separate process with `powershell.exe -ExecutionPolicy RemoteSigned -File .\run_all.ps1 -ViewOnly`.
Omit `-ViewOnly` to run the batch first.

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

The entire configuration is validated before cleanup. A valid batch removes
previous `trajectory.csv` and `trajectory.view.ini` files from `output/` and its
immediate run folders, including runs omitted from the new configuration. It
also removes run folders emptied by cleanup. These paths are relative to the
working directory. Cleanup preserves unrelated files and skips links and
junctions. Single-run CLI invocations bypass batch cleanup.
Before removing any result, the simulator checks all candidates for Windows
delete-sharing restrictions or Unix advisory `flock` locks. If a result is
locked, the batch reports its path and leaves the previous results in place.

Move any run you want to keep into `saved_sim/` before starting another batch.
The simulator creates that directory if needed and leaves its contents untouched.
Git ignores the archive.

Simulations run sequentially in file order. Each writes
`output/<name>/trajectory.csv` and reports its particle count, output path,
simulation time, and file-writing time. If a run fails, the batch stops and
results already written by that batch remain.

Each run prints its batch position, name, particle count, time step, total
steps, simulated duration, and collision settings. The terminal progress line
updates at most five times per second during steady execution and shows step
count, elapsed time, and estimated time remaining. Redirected output gets one
completion line per run. Add `progress = false` to a `[simulation]` section to
disable its progress line while keeping the start and final statistics.

The final statistics include the live particle count, resolved collisions,
merges, calculation time, file-writing time, and elapsed execution time. A
collision is a contact resolved by the solver, including merges. A pair can
contribute more than once if the solver corrects it in multiple passes.
Calculation and file-writing timers exclude progress display. The batch
summary totals these counters and reports elapsed time for the entire batch.

An optional `[visualization]` section after the simulation settings and any manual particles
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
sphere_quality = 12
trail_width = 2
```

`particle_size` is an integer from 1 to 64 (default 8). It scales the drawn
radius by `particle_size / 8`. `trails` is `true` or `false` (default false),
`trail_length` is 1 to 256 recorded frame intervals (default 32),
`trail_width` is 1 to 12 pixels (default 2), and `sphere_quality` is 4 to 32
latitude subdivisions (default 10). Batch mode saves these settings in
`trajectory.view.ini` beside the CSV. The viewer automatically loads the sidecar
if present. A new run in the same directory without view settings clears the
generated sidecar. View options never enter the gravitational solver.

## Design and limits

- The force kernel evaluates each unordered pair once and applies equal and opposite forces to movable pairs. A fixed body contributes to the field while its acceleration stays zero. Velocity Verlet uses a half velocity update, a full position update, a force recalculation, and a second half velocity update. The force kernel and integrator have no dependency on command-line parsing, CSV, or visualization.
- The executable reports wall-clock simulation time for initialization and integration, and file-writing time for directory creation, CSV formatting, writes, and close. The two times are measured separately with `std::chrono::steady_clock`.
- The solver costs O(N²) per step, and writing every frame costs O(N × (steps + 1)) in output size. Softening bounds close-range acceleration for the provided masses, but the fixed time step still needs to be small enough to resolve encounters. Large steps can produce inaccurate trajectories.
- CTest covers softened gravity, orbits, collision conservation and restitution, overlap resolution, seeded radii, batch configuration, and reading old and new trajectories.

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
| C | Center the camera on the sphere under the pointer and zoom in for inspection |
| F | Reset the camera to the fitted view |
| Esc or window close button | Close the viewer |

The viewer reads the new radius column and accepts the old six-column header,
using a uniform display radius for legacy files. Steps start at zero and are
contiguous. IDs within a frame must be unique and ascending, and all rows in a
frame share a timestamp. Timestamps start at zero and increase between frames.
At 1× speed, one simulation time unit plays in one real second.

The viewer opens a window before reading the trajectory. The loading screen
shows a progress bar, with the simulation name, stage, and percentage in the
title bar. The window stays responsive while reading the CSV and fitting the
camera. Press Esc to cancel. Playback starts automatically after loading, and
the CSV closes as soon as it has been read. Loading errors appear in the
window title and terminal.

The perspective camera fits the full trajectory on opening. Zoom ranges from
0.001× to 1,000,000× relative to the fitted view, with clipping planes that
adjust to viewing distance. To inspect a moving body at its current position,
pause playback, point at it, and press C. The viewer uses instanced
rendering for shaded, depth-tested spheres and draws a reference grid on the
world XY plane (`z = 0`). The red, green, and blue axes extend from the world
origin in the positive X, Y, and Z directions.
Trails use the sphere's base color and connect matching IDs in consecutive
frames. A merge between movable bodies starts a new trail. An absorbing fixed
body retains its ID across the merge. Playback loops without
connecting the last frame to the first. Trail geometry is capped at 200,000
segments per displayed frame.
It loads the full CSV into memory, so startup time and memory use increase
with the number of recorded particles and frames.

MPI, OpenMP, hybrid execution, and Discovery Slurm benchmarking are planned for later phases.
