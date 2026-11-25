#include "simulation.hpp"

#include "physics.hpp"

#include <cmath>
#include <random>
#include <utility>

namespace nbody {

std::vector<Particle> make_two_body()
{
    // Unit masses separated by one unit, orbiting their common center of mass.
    const double speed = std::sqrt(0.5 * gravitational_constant /
                                   std::pow(1 + softening * softening, 1.5));
    return {{1, {-0.5, 0, 0}, {0, -speed, 0}},
            {1, {0.5, 0, 0}, {0, speed, 0}}};
}

std::vector<Particle> make_random_system(std::size_t count, std::uint32_t seed)
{
    std::mt19937 engine(seed);
    // Map raw engine values directly so the initial samples are portable.
    const auto unit_sample = [&engine] { return engine() / 4294967296.0; };
    std::vector<Particle> particles;
    particles.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        particles.push_back({1.0 / static_cast<double>(count),
                             {2 * unit_sample() - 1, 2 * unit_sample() - 1,
                              2 * unit_sample() - 1},
                             {0.2 * (unit_sample() - 0.5), 0.2 * (unit_sample() - 0.5),
                              0.2 * (unit_sample() - 0.5)}});
    }
    return particles;
}

Simulation::Simulation(std::vector<Particle> particles, CollisionSettings collisions)
    : particles_(std::move(particles)), collisions_(collisions), next_id_(particles_.size())
{
    for (std::size_t id = 0; id < particles_.size(); ++id) {
        particles_[id].id = id;
        if (particles_[id].fixed) {
            particles_[id].velocity = {};
        }
    }
    compute_accelerations(particles_);
}

void Simulation::advance(double dt)
{
    const double half_step = 0.5 * dt;
    for (auto& particle : particles_) {
        if (particle.fixed) {
            continue;
        }
        particle.velocity += particle.acceleration * half_step;
        particle.position += particle.velocity * dt;
    }
    if (collisions_.enabled) {
        resolve_collisions(particles_, collisions_, next_id_);
    }
    compute_accelerations(particles_);
    for (auto& particle : particles_) {
        if (!particle.fixed) {
            particle.velocity += particle.acceleration * half_step;
        }
    }
}

} // namespace nbody
