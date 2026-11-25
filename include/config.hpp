#pragma once

#include "presets.hpp"
#include "collisions.hpp"
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
    bool progress = true;
    std::size_t particles = 2;
    double dt = 0.001;
    std::size_t steps = 1000;
    std::uint32_t seed = 42;
    CollisionSettings collisions;
    RadiusSettings radii;
    std::filesystem::path output_dir = "output";
    std::vector<Particle> manual_particles;
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
    "                 [--steps N] [--seed N] [--output-dir PATH] [--progress true|false] [--help]\n"
    "                 [--collisions true|false] [--collision-type elastic|inelastic|merge]\n"
    "                 [--restitution E] [--min-radius R] [--max-radius R] [--central-radius R]\n"
    "       nbody_sim --config simulations.txt\n"
    "Modes: two-body, random, solar-system, galaxy, collision, three-body\n"
    "Use [particle] sections in a batch configuration to add manual particles.\n";

} // namespace nbody
