#include "config.hpp"
#include "simulation.hpp"
#include "trajectory.hpp"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <system_error>
#include <string_view>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
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

class OutputLock {
public:
    explicit OutputLock(const fs::path& path);
    ~OutputLock()
    {
#ifdef _WIN32
        CloseHandle(handle_);
#else
        close(descriptor_);
#endif
    }
    OutputLock(const OutputLock&) = delete;
    OutputLock& operator=(const OutputLock&) = delete;

private:
#ifdef _WIN32
    HANDLE handle_;
#else
    int descriptor_;
#endif
};

OutputLock::OutputLock(const fs::path& path)
{
#ifdef _WIN32
    const auto handle = CreateFileW(path.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto code = GetLastError();
        if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION) {
            throw std::runtime_error("Output file is locked: " + path.string() +
                                     ". Close the application using it and retry.");
        }
        throw fs::filesystem_error("Cannot remove previous output", path,
                                   {static_cast<int>(code), std::system_category()});
    }
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_READONLY) != 0) {
        CloseHandle(handle);
        throw std::runtime_error("Cannot remove read-only or inaccessible output: " + path.string());
    }
    handle_ = handle;
#else
    const int descriptor = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (descriptor == -1) {
        throw fs::filesystem_error("Cannot open previous output for lock check", path,
                                   {errno, std::generic_category()});
    }
    const int status = flock(descriptor, LOCK_EX | LOCK_NB);
    if (status != 0) {
        const int code = errno;
        close(descriptor);
        if (code == EWOULDBLOCK || code == EAGAIN) {
            throw std::runtime_error("Output file is locked: " + path.string() +
                                     ". Close the application using it and retry.");
        }
        throw fs::filesystem_error("Cannot check previous output lock", path,
                                   {code, std::generic_category()});
    }
    descriptor_ = descriptor;
#endif
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
    std::vector<fs::path> files;
    std::vector<fs::path> run_directories;
    const auto collect_results = [&files](const fs::path& directory) {
        const auto before = files.size();
        for (const auto* name : result_files) {
            const auto path = directory / name;
            const auto status = fs::symlink_status(path);
            if (fs::is_regular_file(status) && !is_link(path, status)) {
                files.push_back(path);
            }
        }
        return files.size() != before;
    };
    if (fs::exists(output_status)) {
        collect_results(output);
        for (const auto& entry : fs::directory_iterator(output)) {
            const auto status = entry.symlink_status();
            if (fs::is_directory(status) && !is_link(entry.path(), status) &&
                collect_results(entry.path())) {
                run_directories.push_back(entry.path());
            }
        }
    }
    {
        std::vector<std::unique_ptr<OutputLock>> locks;
        locks.reserve(files.size());
        for (const auto& path : files) {
            locks.push_back(std::make_unique<OutputLock>(path));
        }
        for (const auto& path : files) {
            fs::remove(path);
        }
    }
    for (const auto& directory : run_directories) {
        if (fs::is_empty(directory)) {
            fs::remove(directory);
        }
    }
}

using Clock = std::chrono::steady_clock;
using Seconds = std::chrono::duration<double>;

struct RunStats {
    std::size_t particles;
    nbody::CollisionCounts contacts;
    Clock::duration calculation;
    Clock::duration writing;
    Clock::duration total;
};

void print_start(const nbody::Config& config, std::string_view name,
                 std::size_t index, std::size_t count)
{
    const auto& collisions = config.collisions;
    const char* type = collisions.type == nbody::CollisionType::elastic ? "elastic" :
        collisions.type == nbody::CollisionType::inelastic ? "inelastic" : "merge";
    std::cout << "\nSimulation " << name << " [" << index << '/' << count << "]\n"
              << "Particles: " << config.particles + config.manual_particles.size()
              << " | Time step: " << config.dt << " | Steps: " << config.steps
              << " | Duration: " << config.dt * static_cast<double>(config.steps)
              << "\nCollisions: " << (collisions.enabled ? "enabled" : "disabled")
              << " | Type: " << type << " | Restitution: " << collisions.restitution << '\n'
              << std::flush;
}

bool is_terminal()
{
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

void print_progress(std::size_t done, std::size_t steps, Clock::duration elapsed, bool interactive)
{
    const double fraction = steps == 0 ? 1 : static_cast<double>(done) / steps;
    const auto filled = static_cast<std::size_t>(fraction * 20);
    const double seconds = Seconds(elapsed).count();
    std::ostringstream line;
    line << '[' << std::string(filled, '=') << std::string(20 - filled, ' ') << "] "
         << static_cast<int>(fraction * 100) << "% | Steps " << done << '/' << steps
         << std::fixed << std::setprecision(1) << " | Elapsed " << seconds << " s | ETA ";
    if (done == 0 && steps != 0) {
        line << "--";
    } else {
        line << (fraction < 1 ? seconds * (1 - fraction) / fraction : 0) << " s";
    }
    std::cout << (interactive ? "\r" : "") << line.str() << "        "
              << (done == steps ? "\n" : "") << std::flush;
}

RunStats run_simulation(const nbody::Config& config, const std::optional<nbody::ViewSettings>& view,
                        std::vector<nbody::Particle> initial = {}, Clock::duration preparation = {})
{
    const auto run_start = Clock::now();
    auto start = Clock::now();
    if (initial.empty()) {
        initial = config.mode == nbody::Mode::custom ? config.manual_particles :
            nbody::make_initial_conditions(config.mode, config.particles, config.seed, config.radii);
    }
    nbody::Simulation simulation(std::move(initial), config.collisions);
    auto simulation_time = Clock::now() - start + preparation;

    start = Clock::now();
    nbody::TrajectoryWriter writer(config.output_dir, !config.manual_particles.empty());
    writer.write_frame(0, 0, simulation.particles());
    auto output_time = Clock::now() - start;
    const bool interactive = config.progress && is_terminal();
    auto next_update = Clock::now() + std::chrono::milliseconds(200);
    if (interactive && config.steps != 0) {
        print_progress(0, config.steps, Clock::now() - run_start + preparation, true);
    }

    for (std::size_t step = 0; step < config.steps; ++step) {
        start = Clock::now();
        simulation.advance(config.dt);
        simulation_time += Clock::now() - start;

        start = Clock::now();
        writer.write_frame(step + 1, static_cast<double>(step + 1) * config.dt,
                           simulation.particles());
        const auto now = Clock::now();
        output_time += now - start;
        if (interactive && now >= next_update && step + 1 < config.steps) {
            print_progress(step + 1, config.steps, now - run_start + preparation, true);
            next_update = now + std::chrono::milliseconds(200);
        }
    }

    start = Clock::now();
    writer.close();
    nbody::save_view_settings(config.output_dir, view);
    output_time += Clock::now() - start;
    const auto elapsed = Clock::now() - run_start + preparation;
    if (config.progress) {
        print_progress(config.steps, config.steps, elapsed, interactive);
    }

    const auto contacts = simulation.collision_counts();
    std::cout << "Trajectory: " << config.output_dir / "trajectory.csv" << '\n'
              << "Final particles: " << simulation.particles().size()
              << " | Collisions: " << contacts.collisions << " | Merges: " << contacts.merges << '\n'
              << std::fixed << std::setprecision(6)
              << "Simulation time (s): " << Seconds(simulation_time).count() << '\n'
              << "File-writing time (s): " << Seconds(output_time).count() << '\n'
              << "Total execution time (s): " << Seconds(elapsed).count() << '\n'
              << std::defaultfloat;
    return {simulation.particles().size(), contacts, simulation_time, output_time, elapsed};
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--config") {
            const auto batch_start = Clock::now();
            const auto simulations = nbody::load_batch(argv[2]);
            std::vector<std::vector<nbody::Particle>> mixed_initial(simulations.size());
            std::vector<Clock::duration> preparation(simulations.size());
            for (std::size_t i = 0; i < simulations.size(); ++i) {
                const auto& simulation = simulations[i];
                const auto& config = simulation.config;
                if (config.mode != nbody::Mode::custom && !config.manual_particles.empty()) {
                    try {
                        const auto start = Clock::now();
                        mixed_initial[i] = nbody::make_mixed_initial_conditions(
                            config.mode, config.particles, config.seed, config.radii, config.manual_particles);
                        preparation[i] = Clock::now() - start;
                    } catch (const std::exception& error) {
                        throw std::runtime_error(simulation.name + ": " + error.what());
                    }
                }
            }
            prepare_batch_output(simulations);
            RunStats batch{};
            for (std::size_t i = 0; i < simulations.size(); ++i) {
                const auto& simulation = simulations[i];
                print_start(simulation.config, simulation.name, i + 1, simulations.size());
                try {
                    const auto stats = run_simulation(simulation.config, simulation.view,
                                                      std::move(mixed_initial[i]), preparation[i]);
                    batch.particles += stats.particles;
                    batch.contacts.collisions += stats.contacts.collisions;
                    batch.contacts.merges += stats.contacts.merges;
                    batch.calculation += stats.calculation;
                    batch.writing += stats.writing;
                } catch (const std::exception& error) {
                    throw std::runtime_error(simulation.name + ": " + error.what());
                }
            }
            std::cout << "\nBatch summary | Completed: " << simulations.size() << '/' << simulations.size()
                      << " | Final particles: " << batch.particles
                      << " | Collisions: " << batch.contacts.collisions
                      << " | Merges: " << batch.contacts.merges << '\n'
                      << std::fixed << std::setprecision(6)
                      << "Calculation (s): " << Seconds(batch.calculation).count()
                      << " | File writing (s): " << Seconds(batch.writing).count()
                      << " | Total execution (s): " << Seconds(Clock::now() - batch_start).count() << '\n';
        } else {
            const auto config = nbody::parse_config(argc, argv);
            if (config.help) {
                std::cout << nbody::usage;
                return 0;
            }
            const auto name = config.output_dir.filename().string();
            print_start(config, name.empty() ? "single run" : name, 1, 1);
            run_simulation(config, std::nullopt);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "nbody_sim: " << error.what() << "\n" << nbody::usage;
        return 1;
    }
}
