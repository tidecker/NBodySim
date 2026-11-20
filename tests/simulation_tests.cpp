#include "physics.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace nbody;

double norm(Vec3 v)
{
    return std::sqrt(dot(v, v));
}

void require_near(double actual, double expected, double tolerance, const char* message)
{
    if (!(std::abs(actual - expected) <= tolerance)) {
        std::cerr << "Expected " << expected << ", got " << actual
                  << " (tolerance " << tolerance << ")\n";
        throw std::runtime_error(message);
    }
}

Vec3 momentum(const std::vector<Particle>& particles)
{
    Vec3 total{};
    for (const auto& particle : particles) {
        total += particle.velocity * particle.mass;
    }
    return total;
}

double energy(const std::vector<Particle>& particles)
{
    double total = 0;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const auto& a = particles[i];
        total += 0.5 * a.mass * dot(a.velocity, a.velocity);
        for (std::size_t j = i + 1; j < particles.size(); ++j) {
            const auto& b = particles[j];
            const auto displacement = b.position - a.position;
            total -= gravitational_constant * a.mass * b.mass /
                     std::sqrt(dot(displacement, displacement) + softening * softening);
        }
    }
    return total;
}

void test_force()
{
    // At one softening length, softening reduces the Newtonian force by 2^(3/2).
    std::vector<Particle> particles{{2, {}, {}}, {3, {0, 0, softening}, {}}};
    const double expected_force =
        gravitational_constant * 6 / (softening * softening * std::sqrt(8.0));
    compute_accelerations(particles);
    const auto force_a = particles[0].acceleration * particles[0].mass;
    const auto force_b = particles[1].acceleration * particles[1].mass;
    require_near(norm(force_a - Vec3{0, 0, expected_force}), 0, 1e-10, "Force on A");
    require_near(norm(force_b - Vec3{0, 0, -expected_force}), 0, 1e-10, "Force on B");

    particles[1].position = particles[0].position;
    compute_accelerations(particles);
    for (const auto& particle : particles) {
        require_near(norm(particle.acceleration), 0, 0, "Coincident particle acceleration");
    }
}

void test_momentum()
{
    Simulation simulation(make_random_system(16, 42));
    const auto initial = momentum(simulation.particles());
    for (int i = 0; i < 1000; ++i) {
        simulation.advance(0.001);
        require_near(norm(momentum(simulation.particles()) - initial), 0, 1e-12,
                     "Total momentum drift");
    }
}

void test_energy()
{
    auto particles = make_two_body();
    for (auto& particle : particles) {
        particle.velocity = particle.velocity * 0.85;
    }
    Simulation simulation(particles);
    const double initial = energy(simulation.particles());
    for (int i = 0; i < 6000; ++i) {
        simulation.advance(0.001);
        const double relative_error = (energy(simulation.particles()) - initial) / initial;
        require_near(relative_error, 0, 1e-5, "Relative energy error in eccentric orbit");
    }
}

void test_orbit()
{
    const auto initial = make_two_body();
    Simulation simulation(initial);
    const double period = std::acos(-1.0) / norm(initial[0].velocity);
    constexpr int steps = 4000;
    for (int i = 0; i < steps; ++i) {
        simulation.advance(period / steps);
        const auto& particles = simulation.particles();
        require_near(norm(particles[1].position - particles[0].position), 1, 1e-5,
                     "Circular orbit separation");
        if (i + 1 == steps / 4) {
            require_near(norm(particles[0].position - Vec3{0, -0.5, 0}), 0, 1e-5,
                         "Position after a quarter orbit");
        }
    }
    require_near(norm(simulation.particles()[0].position - initial[0].position),
                 0, 1e-5, "Position after a full orbit");
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        return 1;
    }
    const std::string_view test = argv[1];
    try {
        if (test == "force") {
            test_force();
        } else if (test == "momentum") {
            test_momentum();
        } else if (test == "energy") {
            test_energy();
        } else if (test == "orbit") {
            test_orbit();
        } else {
            throw std::invalid_argument("Unknown test " + std::string(test));
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
