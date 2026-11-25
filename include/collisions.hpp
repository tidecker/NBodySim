#pragma once

#include "particle.hpp"

#include <vector>

namespace nbody {

enum class CollisionType { elastic, inelastic, merge };

struct CollisionSettings {
    bool enabled = false;
    CollisionType type = CollisionType::elastic;
    double restitution = 0.5;
};

void resolve_collisions(std::vector<Particle>& particles, const CollisionSettings& settings,
                        std::size_t& next_id);

} // namespace nbody
