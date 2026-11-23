#pragma once

#include "presets.hpp"
#include "view_settings.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nbody {

struct Config {
    Mode mode = Mode::two_body;
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
    std::optional<ViewSettings> view;
};

std::vector<NamedSimulation> load_batch(const std::filesystem::path& path);

inline constexpr char usage[] =
    "Usage: nbody_sim [--mode MODE] [--particles N] [--dt DT]\n"
    "                 [--steps N] [--seed N] [--output-dir PATH] [--help]\n"
    "       nbody_sim --config simulations.txt\n"
    "Modes: two-body, random, solar-system, galaxy, collision, three-body\n";

} // namespace nbody
