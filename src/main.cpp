#include "config.hpp"
#include "simulation.hpp"
#include "trajectory.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void run_simulation(const nbody::Config& config)
{
    using Clock = std::chrono::steady_clock;
    auto start = Clock::now();
    nbody::Simulation simulation(config.two_body
        ? nbody::make_two_body()
        : nbody::make_random_system(config.particles, config.seed));
    auto simulation_time = Clock::now() - start;

    start = Clock::now();
    nbody::TrajectoryWriter writer(config.output_dir);
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
            for (const auto& simulation : nbody::load_batch(argv[2])) {
                std::cout << "\nSimulation " << simulation.name << '\n'
                          << "Particles: " << simulation.config.particles << '\n';
                try {
                    run_simulation(simulation.config);
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
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nbody_sim: " << error.what() << "\n" << nbody::usage;
        return 1;
    }
}
