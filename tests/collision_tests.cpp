#include "presets.hpp"
#include "simulation.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
using namespace nbody;

void expect(bool matches, const char* message)
{
    if (!matches) throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) < 1e-12;
}

double energy(const Particle& p)
{
    return 0.5 * p.mass * dot(p.velocity, p.velocity);
}

void check_bounce(CollisionType type, double restitution)
{
    std::vector<Particle> bodies{{2, {-0.15, 0, 0}, {1, 2, 0}, {}, 0.2, 0},
                                 {1, {0.15, 0, 0}, {-2, 0, 0}, {}, 0.2, 1}};
    const auto before = energy(bodies[0]) + energy(bodies[1]);
    std::size_t next_id = 2;
    resolve_collisions(bodies, {true, type, restitution}, next_id);
    const auto& a = bodies[0];
    const auto& b = bodies[1];
    expect(near(a.mass * a.velocity.x + b.mass * b.velocity.x, 0), "bounce momentum");
    expect(near(b.position.x - a.position.x, 0.4), "overlap correction");
    expect(near(a.mass * a.position.x + b.mass * b.position.x, -0.15), "position center of mass");
    expect(near(a.velocity.y, 2) && near(b.velocity.y, 0), "unchanged tangential velocity");
    const double coefficient = type == CollisionType::elastic ? 1 : restitution;
    expect(near(b.velocity.x - a.velocity.x, 3 * coefficient), "normal restitution");
    expect(near(energy(a) + energy(b), before - 3 * (1 - coefficient * coefficient)),
           "kinetic energy after impact");
    expect(a.id == 0 && b.id == 1 && next_id == 2, "bounce changes IDs");
}

void check_merge()
{
    const std::vector<Particle> bodies{{2, {}, {1, 0, 0}, {}, 0.2, 0, false, {1, 2, 3}},
                                       {1, {}, {-2, 1, 0}, {}, 0.1, 1, false, {4, 5, 6}}};
    Simulation simulation(bodies, {true, CollisionType::merge, 0.5});
    simulation.advance(0.001);
    expect(simulation.particles().size() == 1, "integration did not merge");
    const auto& merged = simulation.particles().front();
    expect(merged.id == 2, "merged body lacks a new ID");
    expect(merged.color == bodies[0].color, "merged body did not inherit the lower ID color");
    expect(near(merged.mass, 3), "mass not conserved");
    expect(near(merged.velocity.x * merged.mass, 0) && near(merged.velocity.y * merged.mass, 1),
           "merge momentum");
    expect(near(merged.position.y, 0.001 / 3), "merge position center of mass");
    expect(near(std::pow(merged.radius, 3), 0.009), "volume not conserved");
    Simulation baseline(bodies);
    baseline.advance(0.001);
    expect(baseline.particles().size() == 2, "collisions must default to off");

    auto coincident = bodies;
    coincident.front().velocity = coincident.back().velocity;
    std::size_t next_id = 2;
    resolve_collisions(coincident, {true, CollisionType::elastic, 1}, next_id);
    expect(near(coincident[1].position.x - coincident[0].position.x, 0.3), "coincident contact");
}

void check_radii()
{
    const RadiusSettings settings{0.01, 0.04, 0.12};
    const auto a = make_initial_conditions(Mode::galaxy, 32, 19, settings);
    const auto b = make_initial_conditions(Mode::galaxy, 32, 19, settings);
    const auto c = make_initial_conditions(Mode::galaxy, 32, 20, settings);
    expect(a.front().radius == 0.12 && a.front().mass == 3, "central body settings");
    expect(a[1].radius != c[1].radius && a[1].radius != a[2].radius, "radii must vary");
    double mass = 0;
    for (std::size_t i = 1; i < a.size(); ++i) {
        expect(a[i].radius >= 0.01 && a[i].radius <= 0.04, "radius bounds");
        expect(a[i].radius == b[i].radius && a[i].mass == b[i].mass, "seed repeatability");
        expect(near(a[i].mass / a[1].mass, std::pow(a[i].radius / a[1].radius, 3)), "constant density");
        mass += a[i].mass;
    }
    expect(near(mass, 0.3), "disk mass changed");
}

void check_fixed_gravity()
{
    std::vector<Particle> bodies{{4, {0, 0, 2}, {10, 5, -1}}, {1, {1, 0, 2}, {0, 0.5, 0}}};
    bodies[0].fixed = true;
    bodies[0].color = {255, 60, 0};
    bodies[1].color = {0, 100, 255};
    Simulation simulation(bodies);
    for (int step = 0; step < 10; ++step) {
        simulation.advance(0.01);
    }
    const auto& fixed = simulation.particles()[0];
    const auto& moving = simulation.particles()[1];
    expect(dot(fixed.position - bodies[0].position, fixed.position - bodies[0].position) == 0 &&
           dot(fixed.velocity, fixed.velocity) == 0, "fixed particle moved");
    expect(moving.position.x < 1 && moving.velocity.x < 0 && moving.position.y > 0,
           "movable particle did not respond to fixed gravity");
    expect(fixed.id == 0 && moving.id == 1 && fixed.color == bodies[0].color &&
           moving.color == bodies[1].color, "integration changed identity or color");
}

void check_fixed_collisions()
{
    std::vector<Particle> bodies{{2, {-0.3, 0, 0}, {2, 1, 0}, {}, 0.2, 0},
                                 {5, {}, {}, {}, 0.2, 1}};
    bodies[0].color = {40, 100, 230};
    bodies[1].fixed = true;
    bodies[1].color = {250, 70, 30};
    for (auto type : {CollisionType::elastic, CollisionType::inelastic}) {
        for (bool fixed_first : {false, true}) {
            auto contact = bodies;
            if (fixed_first) {
                std::swap(contact[0], contact[1]);
            }
            std::size_t next_id = 2;
            resolve_collisions(contact, {true, type, 0.25}, next_id);
            const auto& fixed = contact[fixed_first ? 0 : 1];
            const auto& moving = contact[fixed_first ? 1 : 0];
            expect(fixed.position.x == 0 && fixed.velocity.x == 0 && fixed.id == 1 &&
                   fixed.color == bodies[1].color, "bounce changed fixed particle");
            expect(near(moving.position.x, -0.4) &&
                   near(moving.velocity.x, type == CollisionType::elastic ? -2 : -0.5) &&
                   moving.velocity.y == 1 && moving.id == 0 && moving.color == bodies[0].color,
                   "incorrect fixed bounce");
        }
    }
    auto merged = bodies;
    std::size_t next_id = 2;
    resolve_collisions(merged, {true, CollisionType::merge, 0.5}, next_id);
    expect(merged.size() == 1 && merged[0].fixed && merged[0].id == 1 && next_id == 2 &&
           merged[0].color == bodies[1].color && merged[0].position.x == 0 &&
           dot(merged[0].velocity, merged[0].velocity) == 0 && merged[0].mass == 7 &&
           near(std::pow(merged[0].radius, 3), 0.016), "incorrect fixed merge");
    bodies[0].fixed = true;
    resolve_collisions(bodies, {true, CollisionType::merge, 0.5}, next_id);
    expect(bodies.size() == 2 && bodies[0].position.x == -0.3 && bodies[1].position.x == 0,
           "two fixed bodies should ignore contact");
}
}

int main()
{
    try {
        check_bounce(CollisionType::elastic, 0);
        check_bounce(CollisionType::inelastic, 0.4);
        check_merge();
        check_radii();
        check_fixed_gravity();
        check_fixed_collisions();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
