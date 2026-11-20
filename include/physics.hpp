#pragma once

#include "particle.hpp"

#include <vector>

namespace nbody {

inline constexpr double gravitational_constant = 1.0;
inline constexpr double softening = 0.01;

void compute_accelerations(std::vector<Particle>& particles);

} // namespace nbody
