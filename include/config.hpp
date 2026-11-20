#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nbody {

struct Config {
    bool two_body = true;
    bool help = false;
    std::size_t particles = 2;
    double dt = 0.001;
    std::size_t steps = 1000;
    std::uint32_t seed = 42;
    std::filesystem::path output_dir = "output";
};

Config parse_config(int argc, char* argv[]);

struct NamedSimulation {
    std::string name;
    Config config;
};

std::vector<NamedSimulation> load_batch(const std::filesystem::path& path);

inline constexpr char usage[] =
    "Usage: nbody_sim [--mode two-body|random] [--particles N] [--dt DT]\n"
    "                 [--steps N] [--seed N] [--output-dir PATH] [--help]\n"
    "       nbody_sim --config simulations.txt\n";

} // namespace nbody
