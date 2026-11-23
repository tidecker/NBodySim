#include "presets.hpp"
#include "physics.hpp"
#include "simulation.hpp"

#include <cmath>
#include <random>
#include <stdexcept>

namespace nbody {
namespace {
constexpr double tau = 6.283185307179586;

double sample(std::mt19937& engine)
{
    return engine() / 4294967296.0;
}

void center_system(std::vector<Particle>& particles)
{
    Vec3 position{}, velocity{};
    double mass = 0;
    for (const auto& p : particles) {
        position += p.position * p.mass;
        velocity += p.velocity * p.mass;
        mass += p.mass;
    }
    for (auto& p : particles) {
        p.position = p.position - position * (1 / mass);
        p.velocity = p.velocity - velocity * (1 / mass);
    }
}

std::vector<Particle> make_disk(std::size_t count, std::mt19937& engine, bool solar)
{
    const double central_mass = solar ? 1 : 3;
    const double disk_mass = solar ? 0.003 : 0.3;
    std::vector<Particle> particles{{central_mass, {}, {}}};
    particles.reserve(count);
    for (std::size_t i = 1; i < count; ++i) {
        const double fraction = static_cast<double>(i - 1) / (count - 1);
        const double radius = solar ? 0.35 + 1.65 * fraction : std::sqrt(0.16 + 8.84 * sample(engine));
        const double angle = tau * sample(engine);
        const double tilt = (sample(engine) - 0.5) * (solar ? 0.22 : 0.12);
        const double c = std::cos(angle), s = std::sin(angle);
        const double speed = radius * std::sqrt(gravitational_constant * central_mass /
            std::pow(radius * radius + softening * softening, 1.5));
        particles.push_back({disk_mass / (count - 1),
            {radius * c, radius * s * std::cos(tilt), radius * s * std::sin(tilt)},
            {-speed * s, speed * c * std::cos(tilt), speed * c * std::sin(tilt)}});
    }
    center_system(particles);
    return particles;
}

std::vector<Particle> make_collision(std::size_t count, std::mt19937& engine)
{
    std::vector<Particle> particles;
    particles.reserve(count);
    for (int side : {-1, 1}) {
        const auto cluster_count = side == -1 ? count / 2 : count - count / 2;
        for (std::size_t i = 0; i < cluster_count; ++i) {
            const double z = 2 * sample(engine) - 1;
            const double angle = tau * sample(engine);
            const double radius = 0.65 * std::cbrt(sample(engine));
            const double xy = std::sqrt(1 - z * z);
            const double x = radius * xy * std::cos(angle);
            const double y = radius * xy * std::sin(angle);
            particles.push_back({1.0 / cluster_count,
                {side * 1.7 + x, side * 0.25 + y, radius * z},
                {-side * 0.55 - 0.45 * y, 0.45 * x, 0.12 * (sample(engine) - 0.5)}});
        }
    }
    center_system(particles);
    return particles;
}
}

Mode parse_mode(std::string_view name)
{
    if (name == "two-body") return Mode::two_body;
    if (name == "random") return Mode::random;
    if (name == "solar-system") return Mode::solar_system;
    if (name == "galaxy") return Mode::galaxy;
    if (name == "collision") return Mode::collision;
    if (name == "three-body") return Mode::three_body;
    throw std::invalid_argument("mode must be two-body, random, solar-system, galaxy, collision, or three-body");
}

std::size_t default_particle_count(Mode mode)
{
    switch (mode) {
    case Mode::two_body: return 2;
    case Mode::three_body: return 3;
    case Mode::solar_system: return 9;
    default: return 100;
    }
}

void validate_particle_count(Mode mode, std::size_t count)
{
    if (mode == Mode::three_body && count != 3) {
        throw std::invalid_argument("particles must be 3 in three-body mode");
    }
    if (count == 0 || (mode == Mode::two_body && count != 2)) {
        throw std::invalid_argument("particles must be positive and must be 2 in two-body mode");
    }
    if (count < 2 && mode != Mode::random) {
        throw std::invalid_argument("particles must be at least 2 in solar-system, galaxy, and collision modes");
    }
}

std::vector<Particle> make_initial_conditions(Mode mode, std::size_t count, std::uint32_t seed)
{
    validate_particle_count(mode, count);
    std::mt19937 engine(seed);
    switch (mode) {
    case Mode::two_body: return make_two_body();
    case Mode::random: return make_random_system(count, seed);
    case Mode::solar_system: return make_disk(count, engine, true);
    case Mode::galaxy: return make_disk(count, engine, false);
    case Mode::collision: return make_collision(count, engine);
    case Mode::three_body:
        // Perturbed figure-eight starting configuration with nonzero Z components.
        return {{1, {-0.97000436, 0.2306151, 0.1345122}, {0.466203685, 0.3710279, 0.2129754}},
                {1, {0.97000436, -0.198, -0.099}, {0.466203685, 0.376, 0.201}},
                {1, {0, -0.0326151, -0.0355122}, {-0.93240737, -0.7470279, -0.4139754}}};
    }
    throw std::invalid_argument("Unknown mode");
}
}
