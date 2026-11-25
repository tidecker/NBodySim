#include "collisions.hpp"

#include <algorithm>
#include <cmath>

namespace nbody {
namespace {

void bounce(Particle& a, Particle& b, Vec3 normal, double overlap, double restitution,
            double closing_speed)
{
    const double mass = a.mass + b.mass;
    const double share_a = a.fixed ? 0 : b.fixed ? 1 : b.mass / mass;
    const double share_b = b.fixed ? 0 : a.fixed ? 1 : a.mass / mass;
    const double change = -(1 + restitution) * std::min(closing_speed, 0.0);
    if (!a.fixed) {
        a.position += normal * (-overlap * share_a);
        a.velocity += normal * (-change * share_a);
    }
    if (!b.fixed) {
        b.position += normal * (overlap * share_b);
        b.velocity += normal * (change * share_b);
    }
}

void merge(Particle& a, const Particle& b, std::size_t id)
{
    const double total = a.mass + b.mass;
    const double share = b.mass / total;
    if (!a.fixed) {
        a.position += (b.position - a.position) * share;
        a.velocity += (b.velocity - a.velocity) * share;
        if (b.color && (!a.color || b.id < a.id)) {
            a.color = b.color;
        }
        a.id = id;
    }
    a.mass = total;
    const double largest = std::max(a.radius, b.radius);
    const double ratio = std::min(a.radius, b.radius) / largest;
    a.radius = largest * std::cbrt(1 + ratio * ratio * ratio);
}

} // namespace

CollisionCounts resolve_collisions(std::vector<Particle>& particles, const CollisionSettings& settings,
                        std::size_t& next_id)
{
    const bool merging = settings.type == CollisionType::merge;
    const double restitution = settings.type == CollisionType::elastic ? 1 : settings.restitution;
    CollisionCounts counts;
    // A bounded number of passes also addresses contacts created by a correction.
    for (int pass = 0; pass < 4; ++pass) {
        bool contact = false;
        for (std::size_t i = 0; i < particles.size(); ++i) {
            for (std::size_t j = i + 1; j < particles.size();) {
                auto& a = particles[i];
                auto& b = particles[j];
                if (a.fixed && b.fixed) {
                    ++j;
                    continue;
                }
                const Vec3 separation = b.position - a.position;
                const double distance_squared = dot(separation, separation);
                const double limit = a.radius + b.radius;
                if (distance_squared >= limit * limit) {
                    ++j;
                    continue;
                }
                contact = true;
                ++counts.collisions;
                if (merging) {
                    if (b.fixed) {
                        std::swap(a, b);
                    }
                    merge(a, b, a.fixed ? a.id : next_id++);
                    particles.erase(particles.begin() + j);
                    ++counts.merges;
                } else {
                    const double distance = std::sqrt(distance_squared);
                    const Vec3 relative = (a.fixed ? Vec3{} : a.velocity) -
                                          (b.fixed ? Vec3{} : b.velocity);
                    const double relative_speed = std::sqrt(dot(relative, relative));
                    const Vec3 normal = distance > 0 ? separation * (1 / distance)
                        : relative_speed > 0 ? relative * (1 / relative_speed) : Vec3{1, 0, 0};
                    bounce(a, b, normal, limit - distance, restitution, -dot(relative, normal));
                    ++j;
                }
            }
        }
        if (!contact) {
            break;
        }
    }
    if (counts.merges != 0) {
        std::sort(particles.begin(), particles.end(),
            [](const Particle& a, const Particle& b) { return a.id < b.id; });
    }
    return counts;
}

} // namespace nbody
