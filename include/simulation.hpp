#pragma once

#include "collisions.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nbody {

std::vector<Particle> make_two_body();
std::vector<Particle> make_random_system(std::size_t count, std::uint32_t seed);

class Simulation {
public:
    explicit Simulation(std::vector<Particle> particles, CollisionSettings collisions = {});
    void advance(double dt);
    const std::vector<Particle>& particles() const { return particles_; }

private:
    std::vector<Particle> particles_;
    CollisionSettings collisions_;
    std::size_t next_id_;
};

} // namespace nbody
