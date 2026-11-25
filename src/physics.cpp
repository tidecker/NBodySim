#include "physics.hpp"

#include <cmath>

namespace nbody {

void compute_accelerations(std::vector<Particle>& particles)
{
    for (auto& particle : particles) {
        particle.acceleration = {};
    }
    for (std::size_t i = 0; i < particles.size(); ++i) {
        auto& a = particles[i];
        for (std::size_t j = i + 1; j < particles.size(); ++j) {
            auto& b = particles[j];
            const Vec3 offset = b.position - a.position;
            const double distance_squared = dot(offset, offset) + softening * softening;
            const Vec3 field = offset *
                (gravitational_constant / (distance_squared * std::sqrt(distance_squared)));
            if (!a.fixed) {
                a.acceleration += field * b.mass;
            }
            if (!b.fixed) {
                b.acceleration += field * -a.mass;
            }
        }
    }
}

} // namespace nbody
