# NBodySim

NBodySim is a C++ N-body gravitational simulation project for USC CSCI 596.
The current executable prints `Welcome to NBodySim!`.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/` | C++ entry point and future simulation source files |
| `include/` | Project headers |
| `visualization/` | Future OpenGL visualization code |
| `tests/` | Future correctness tests |
| `benchmarks/` | Future performance benchmarks |
| `scripts/discovery/` | Future Slurm job scripts for USC's Discovery cluster |

## Development setup

- Install CMake 3.16 or newer and a C++17-capable compiler.
- On Windows, use Visual Studio or Visual Studio Build Tools with the **Desktop development with C++** workload. If you use Build Tools, run the commands in a Developer PowerShell so CMake can find the compiler and build tools.
- On Linux, use GCC (`g++`) or Clang (`clang++`) and the build tool for your CMake generator, such as Make or Ninja. On Discovery, use the available compiler and CMake modules.
- VS Code is optional. Its C/C++ and CMake Tools extensions provide editor and build integration.

From the project root, configure and build:

```sh
cmake -S . -B build
cmake --build build --config Release
```

Run the executable using the path for your generator:

| Generator | Command |
| --- | --- |
| Visual Studio (PowerShell) | `.\build\Release\nbody_sim.exe` |
| Linux Make or single-config Ninja | `./build/nbody_sim` |

## Planned features

1. Implement a sequential gravitational simulation as the baseline.
2. Add MPI, OpenMP, and hybrid MPI + OpenMP execution.
3. Visualize particle motion with OpenGL.
4. Benchmark performance on USC's Discovery Linux cluster using Slurm.

MPI, OpenMP, and OpenGL will be introduced in later stages. The current build requires only the toolchain above.
