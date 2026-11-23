#include "presets.hpp"
#include "simulation.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using namespace nbody;

bool finite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool equal(Vec3 a, Vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

void check_preset(Mode mode, std::size_t count, bool seeded)
{
    const auto first = make_initial_conditions(mode, count, 42);
    const auto repeat = make_initial_conditions(mode, count, 42);
    const auto different = make_initial_conditions(mode, count, 17);
    if (first.size() != count || repeat.size() != count || different.size() != count) {
        throw std::runtime_error("Wrong preset particle count");
    }
    bool seed_changes_positions = false;
    bool has_depth = false;
    Vec3 momentum{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto& p = first[i];
        if (!(p.mass > 0) || !std::isfinite(p.mass) || !finite(p.position) || !finite(p.velocity)) {
            throw std::runtime_error("Invalid preset mass, position or velocity");
        }
        if (p.mass != repeat[i].mass || !equal(p.position, repeat[i].position) ||
            !equal(p.velocity, repeat[i].velocity)) {
            throw std::runtime_error("Fixed seed did not reproduce the preset");
        }
        seed_changes_positions |= !equal(p.position, different[i].position);
        has_depth |= p.position.z != 0 && p.velocity.z != 0;
        momentum += p.velocity * p.mass;
    }
    if (seed_changes_positions != seeded || !has_depth || std::sqrt(dot(momentum, momentum)) > 1e-12) {
        throw std::runtime_error("Unexpected seed behavior, depth or total momentum");
    }
    if ((mode == Mode::solar_system || mode == Mode::galaxy) && first[0].mass <= first[1].mass) {
        throw std::runtime_error("Missing dominant central body");
    }
    if (mode == Mode::collision && !(first.front().velocity.x > 0 && first.back().velocity.x < 0)) {
        throw std::runtime_error("Clusters are not approaching");
    }
    Simulation simulation(first);
    simulation.advance(0.001);
    for (const auto& p : simulation.particles()) {
        if (!finite(p.position) || !finite(p.velocity) || !finite(p.acceleration)) {
            throw std::runtime_error("Nonfinite state after integration");
        }
    }
}

void check_invalid_count(Mode mode, std::size_t count)
{
    try {
        make_initial_conditions(mode, count, 42);
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Invalid count was accepted");
}

} // namespace

int main()
{
    try {
        check_preset(Mode::solar_system, 9, true);
        check_preset(Mode::galaxy, 256, true);
        check_preset(Mode::collision, 101, true);
        check_preset(Mode::three_body, 3, false);
        check_invalid_count(Mode::solar_system, 1);
        check_invalid_count(Mode::galaxy, 1);
        check_invalid_count(Mode::collision, 1);
        check_invalid_count(Mode::three_body, 4);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
