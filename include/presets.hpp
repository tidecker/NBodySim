#pragma once
#include "particle.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace nbody {
enum class Mode { two_body, random, solar_system, galaxy, collision, three_body, custom };

struct RadiusSettings {
    double min_radius = 0.02;
    double max_radius = 0.02;
    double central_radius = 0.07;
};
Mode parse_mode(std::string_view name);
std::size_t default_particle_count(Mode mode);
void validate_particle_count(Mode mode, std::size_t count);
std::vector<Particle> make_initial_conditions(Mode mode, std::size_t count, std::uint32_t seed,
                                              RadiusSettings radii = {});
}
