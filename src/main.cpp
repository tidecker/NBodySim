#include "config.hpp"
#include "simulation.hpp"
#include "trajectory.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

constexpr const char* result_files[] = {"trajectory.csv", "trajectory.view.ini"};

bool is_link(const fs::path& path, fs::file_status status)
{
    if (fs::is_symlink(status)) {
        return true;
    }
#ifdef _WIN32
    if (fs::exists(status)) {
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            throw std::runtime_error("Cannot inspect output path: " + path.string());
        }
        // Junctions and other reparse points can redirect filesystem operations.
        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }
#endif
    return false;
}

void prepare_batch_output(const std::vector<nbody::NamedSimulation>& simulations)
{
    const fs::path output = "output";
    const auto output_status = fs::symlink_status(output);
    if (is_link(output, output_status) ||
        (fs::exists(output_status) && !fs::is_directory(output_status))) {
        throw std::runtime_error("Batch output must be an ordinary directory: " + output.string());
    }

    for (const auto& simulation : simulations) {
        const auto& directory = simulation.config.output_dir;
        const auto status = fs::symlink_status(directory);
        if (is_link(directory, status) || (fs::exists(status) && !fs::is_directory(status))) {
            throw std::runtime_error("Batch destination must be an ordinary directory: " + directory.string());
        }
        for (const auto* name : result_files) {
            const auto path = directory / name;
            const auto file_status = fs::symlink_status(path);
            if (is_link(path, file_status) ||
                (fs::exists(file_status) && !fs::is_regular_file(file_status))) {
                throw std::runtime_error("Batch destination must be an ordinary file: " + path.string());
            }
        }
    }

    const fs::path saved = "saved_sim";
    if (!fs::exists(fs::symlink_status(saved))) {
        fs::create_directory(saved);
    }
    if (!fs::exists(output_status)) {
        return;
    }

    const auto remove_results = [](const fs::path& directory) {
        bool removed = false;
        for (const auto* name : result_files) {
            const auto path = directory / name;
            const auto status = fs::symlink_status(path);
            if (fs::is_regular_file(status) && !is_link(path, status)) {
                removed = fs::remove(path) || removed;
            }
        }
        return removed;
    };
    remove_results(output);
    std::vector<fs::path> run_directories;
    for (const auto& entry : fs::directory_iterator(output)) {
        const auto status = entry.symlink_status();
        if (fs::is_directory(status) && !is_link(entry.path(), status)) {
            run_directories.push_back(entry.path());
        }
    }
    for (const auto& directory : run_directories) {
        if (remove_results(directory) && fs::is_empty(directory)) {
            fs::remove(directory);
        }
    }
}

void run_simulation(const nbody::Config& config)
{
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    const bool custom = config.mode == nbody::Mode::custom;
    nbody::Simulation simulation(custom ? config.custom_particles : nbody::make_initial_conditions(
        config.mode, config.particles, config.seed, config.radii), config.collisions);
    auto simulation_time = Clock::now() - start;

    start = Clock::now();
    nbody::TrajectoryWriter writer(config.output_dir, custom);
    writer.write_frame(0, 0, simulation.particles());
    auto output_time = Clock::now() - start;

    for (std::size_t step = 0; step < config.steps; ++step) {
        start = Clock::now();
        simulation.advance(config.dt);
        simulation_time += Clock::now() - start;

        start = Clock::now();
        writer.write_frame(step + 1, static_cast<double>(step + 1) * config.dt,
                           simulation.particles());
        output_time += Clock::now() - start;
    }

    start = Clock::now();
    writer.close();
    output_time += Clock::now() - start;

    std::cout << "Trajectory: " << config.output_dir / "trajectory.csv" << '\n'
              << std::fixed << std::setprecision(6)
              << "Simulation time (s): "
              << std::chrono::duration<double>(simulation_time).count() << '\n'
              << "File-writing time (s): "
              << std::chrono::duration<double>(output_time).count() << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--config") {
            const auto simulations = nbody::load_batch(argv[2]);
            prepare_batch_output(simulations);
            for (const auto& simulation : simulations) {
                std::cout << "\nSimulation " << simulation.name << '\n'
                          << "Particles: " << simulation.config.particles << '\n';
                try {
                    run_simulation(simulation.config);
                    nbody::save_view_settings(simulation.config.output_dir, simulation.view);
                } catch (const std::exception& error) {
                    throw std::runtime_error(simulation.name + ": " + error.what());
                }
            }
        } else {
            const auto config = nbody::parse_config(argc, argv);
            if (config.help) {
                std::cout << nbody::usage;
                return 0;
            }
            run_simulation(config);
            nbody::save_view_settings(config.output_dir, std::nullopt);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nbody_sim: " << error.what() << "\n" << nbody::usage;
        return 1;
    }
}
