#include "config.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace nbody {
namespace {

template<typename UInt>
UInt parse_unsigned(std::string_view value, std::string_view option)
{
    UInt result{};
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::invalid_argument("Invalid unsigned integer for " + std::string(option));
    }
    return result;
}

double parse_number(std::string_view value, std::string_view option, bool allow_zero)
{
    std::istringstream input{std::string(value)};
    input.imbue(std::locale::classic());
    double number{};
    if (!(input >> std::noskipws >> number) ||
        input.peek() != std::char_traits<char>::eof() ||
        !std::isfinite(number) || number < 0 || (!allow_zero && number == 0)) {
        throw std::invalid_argument(std::string(option) +
            (allow_zero ? " must be finite and nonnegative" : " must be finite and positive"));
    }
    return number;
}

void set_parameter(Config& config, std::optional<std::size_t>& particle_count,
                   std::string_view key, std::string_view value)
{
    if (key == "mode") {
        config.mode = parse_mode(value);
    } else if (key == "particles") {
        particle_count = parse_unsigned<std::size_t>(value, key);
    } else if (key == "dt") {
        config.dt = parse_number(value, key, false);
    } else if (key == "steps") {
        config.steps = parse_unsigned<std::size_t>(value, key);
    } else if (key == "seed") {
        config.seed = parse_unsigned<std::uint32_t>(value, key);
    } else if (key == "collisions") {
        if (value != "true" && value != "false") {
            throw std::invalid_argument("collisions must be true or false");
        }
        config.collisions.enabled = value == "true";
    } else if (key == "collision_type" || key == "collision-type") {
        if (value == "elastic") {
            config.collisions.type = CollisionType::elastic;
        } else if (value == "inelastic") {
            config.collisions.type = CollisionType::inelastic;
        } else if (value == "merge") {
            config.collisions.type = CollisionType::merge;
        } else {
            throw std::invalid_argument("collision_type must be elastic, inelastic, or merge");
        }
    } else if (key == "restitution") {
        config.collisions.restitution = parse_number(value, key, true);
        if (config.collisions.restitution > 1) {
            throw std::invalid_argument("restitution must be at most 1");
        }
    } else if (key == "min_radius" || key == "min-radius") {
        config.radii.min_radius = parse_number(value, key, false);
    } else if (key == "max_radius" || key == "max-radius") {
        config.radii.max_radius = parse_number(value, key, false);
    } else if (key == "central_radius" || key == "central-radius") {
        config.radii.central_radius = parse_number(value, key, false);
    } else {
        throw std::invalid_argument("Unknown setting " + std::string(key));
    }
}

void validate(Config& config, std::optional<std::size_t> particle_count)
{
    if (config.mode == Mode::custom) {
        if (particle_count) {
            throw std::invalid_argument("custom mode derives its count from [particle] sections");
        }
        config.particles = config.custom_particles.size();
    } else {
        config.particles = particle_count.value_or(default_particle_count(config.mode));
    }
    validate_particle_count(config.mode, config.particles);
    const auto& radii = config.radii;
    if (radii.min_radius > radii.max_radius) {
        throw std::invalid_argument("min_radius must not exceed max_radius");
    }
    if (radii.min_radius < 1e-9 || radii.max_radius > 1e6 ||
        radii.central_radius < 1e-9 || radii.central_radius > 1e6) {
        throw std::invalid_argument("radii must be between 1e-9 and 1e6");
    }
    if (!std::isfinite(config.dt * static_cast<double>(config.steps))) {
        throw std::invalid_argument("dt and steps yield a nonfinite end time");
    }
}

std::size_t steps_for_duration(double duration, double dt)
{
    if (duration == 0) {
        return 0;
    }
    const double ratio = duration / dt;
    const double nearest = std::round(ratio);
    const double tolerance = 4 * std::numeric_limits<double>::epsilon() * ratio;
    const double steps = nearest >= 1 && std::abs(ratio - nearest) <= tolerance
        ? nearest : std::ceil(ratio);
    // Compare against the first unrepresentable integer before converting.
    if (!std::isfinite(steps) ||
        steps >= std::ldexp(1.0, std::numeric_limits<std::size_t>::digits)) {
        throw std::invalid_argument("duration and dt require too many steps");
    }
    return steps < 1 ? 1 : static_cast<std::size_t>(steps);
}

std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

template<typename Value, typename Parse>
std::array<Value, 3> parse_triplet(std::string_view value, std::string_view key, Parse parse)
{
    std::array<Value, 3> components{};
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto comma = value.find(',');
        if ((comma == std::string_view::npos) != (i + 1 == components.size())) {
            throw std::invalid_argument(std::string(key) + " requires three comma-separated values");
        }
        components[i] = parse(trim(value.substr(0, comma)));
        if (comma != std::string_view::npos) {
            value.remove_prefix(comma + 1);
        }
    }
    return components;
}

Vec3 parse_vector(std::string_view value, std::string_view key)
{
    const auto components = parse_triplet<double>(value, key, [key](std::string_view field) {
        std::istringstream input{std::string(field)};
        input.imbue(std::locale::classic());
        double number{};
        if (!(input >> std::noskipws >> number) || !std::isfinite(number) ||
            input.peek() != std::char_traits<char>::eof()) {
            throw std::invalid_argument(std::string(key) + " requires finite coordinates");
        }
        return number;
    });
    return {components[0], components[1], components[2]};
}

void set_particle_parameter(Particle& particle, std::string& name,
                            std::string_view key, std::string_view value)
{
    if (key == "name") {
        name = value;
    } else if (key == "mass") {
        particle.mass = parse_number(value, key, false);
    } else if (key == "radius") {
        particle.radius = parse_number(value, key, false);
        if (particle.radius < 1e-9 || particle.radius > 1e6) {
            throw std::invalid_argument("radius must be between 1e-9 and 1e6");
        }
    } else if (key == "position") {
        particle.position = parse_vector(value, key);
    } else if (key == "velocity") {
        particle.velocity = parse_vector(value, key);
    } else if (key == "color") {
        particle.color = parse_triplet<unsigned>(value, key, [](std::string_view field) {
            const auto channel = parse_unsigned<unsigned>(field, "color");
            if (channel > 255) {
                throw std::invalid_argument("color channels must be between 0 and 255");
            }
            return channel;
        });
    } else if (key == "fixed") {
        if (value != "true" && value != "false") {
            throw std::invalid_argument("fixed must be true or false");
        }
        particle.fixed = value == "true";
    } else {
        throw std::invalid_argument("Unknown particle setting " + std::string(key));
    }
}

std::string output_name_key(std::string name)
{
    if (name.empty() || name.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
        throw std::invalid_argument("name must contain only ASCII letters, digits, underscores, or hyphens");
    }
    for (char& ch : name) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    if (name == "con" || name == "prn" || name == "aux" || name == "nul" ||
        (name.size() == 4 && (name.compare(0, 3, "com") == 0 || name.compare(0, 3, "lpt") == 0) &&
         name.back() >= '1' && name.back() <= '9')) {
        throw std::invalid_argument("name is reserved on Windows");
    }
    return name;
}

} // namespace

Config parse_config(int argc, char* argv[])
{
    Config config;
    std::optional<std::size_t> particle_count;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--help" || option == "-h") {
            config.help = true;
            return config;
        }
        if (i + 1 == argc || std::string_view(argv[i + 1]).substr(0, 2) == "--") {
            throw std::invalid_argument("Missing value for " + std::string(option));
        }
        const std::string_view value = argv[++i];
        if (option == "--output-dir") {
            if (value.empty()) {
                throw std::invalid_argument("--output-dir must be nonempty");
            }
            config.output_dir = value;
        } else if (option == "--config") {
            throw std::invalid_argument("--config must be used by itself with a file path");
        } else if (option.substr(0, 2) == "--") {
            set_parameter(config, particle_count, option.substr(2), value);
        } else {
            throw std::invalid_argument("Unknown option " + std::string(option));
        }
    }
    validate(config, particle_count);
    return config;
}

std::vector<NamedSimulation> load_batch(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open configuration file: " + path.string());
    }

    std::vector<NamedSimulation> simulations;
    std::unordered_set<std::string> names;
    std::unordered_set<std::string> keys;
    std::unordered_set<std::string> particle_names;
    std::unordered_set<std::string> particle_keys;
    Particle particle{};
    std::string particle_name;
    std::size_t particle_line = 0;
    NamedSimulation current;
    std::optional<std::size_t> particle_count;
    std::optional<double> duration;
    std::size_t section_line = 0;
    std::size_t line_number = 0;

    const auto error_at = [&path](std::size_t line, const std::string& message) {
        return std::runtime_error(path.string() + ":" + std::to_string(line) + ": " + message);
    };
    const auto finish_particle = [&] {
        if (particle_line == 0) {
            return;
        }
        for (const char* key : {"name", "mass", "radius", "position", "velocity", "color", "fixed"}) {
            if (particle_keys.count(key) == 0) {
                throw error_at(particle_line, std::string("missing particle setting: ") + key);
            }
        }
        if (!particle_names.insert(particle_name).second) {
            throw error_at(particle_line, "duplicate particle name: " + particle_name);
        }
        current.config.custom_particles.push_back(particle);
        particle_line = 0;
    };
    const auto finish_section = [&] {
        finish_particle();
        try {
            const auto name_key = output_name_key(current.name);
            if ((keys.count("steps") != 0) == duration.has_value()) {
                throw std::invalid_argument("specify exactly one of steps or duration");
            }
            if (duration) {
                current.config.steps = steps_for_duration(*duration, current.config.dt);
            }
            validate(current.config, particle_count);
            if (!names.insert(name_key).second) {
                throw std::invalid_argument("duplicate simulation name: " + current.name);
            }
            current.config.output_dir /= current.name;
            simulations.push_back(std::move(current));
        } catch (const std::invalid_argument& error) {
            throw error_at(section_line, error.what());
        }
    };

    std::string line;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);
        }
        const auto text = trim(std::string_view(line).substr(0, line.find_first_of("#;")));
        if (text.empty()) {
            continue;
        }
        if (text == "[simulation]") {
            if (section_line != 0) {
                finish_section();
            }
            section_line = line_number;
            current = {};
            particle_count.reset();
            duration.reset();
            keys.clear();
            particle_names.clear();
            continue;
        }
        if (text == "[particle]") {
            if (section_line == 0 || current.config.mode != Mode::custom || current.view) {
                throw error_at(line_number, "expected [particle] after custom simulation settings and before [visualization]");
            }
            finish_particle();
            particle_line = line_number;
            particle = {};
            particle_keys.clear();
            continue;
        }
        if (text == "[visualization]") {
            if (section_line == 0 || current.view) {
                throw error_at(line_number, "expected one [visualization] after a [simulation]");
            }
            finish_particle();
            current.view.emplace();
            continue;
        }
        const auto equals = text.find('=');
        if (section_line == 0 || equals == std::string_view::npos || text.front() == '[') {
            throw error_at(line_number, "expected [simulation] or a key = value within a section");
        }
        const std::string key(trim(text.substr(0, equals)));
        const auto value = trim(text.substr(equals + 1));
        if (key.empty() || value.empty()) {
            throw error_at(line_number, "expected a nonempty key and value");
        }
        auto& section_keys = particle_line != 0 ? particle_keys : keys;
        if (!section_keys.insert(current.view ? "view." + key : key).second) {
            throw error_at(line_number, "duplicate setting: " + key);
        }
        try {
            if (particle_line != 0) {
                set_particle_parameter(particle, particle_name, key, value);
            } else if (current.view) {
                set_view_setting(*current.view, key, value);
            } else if (key == "name") {
                current.name = value;
            } else if (key == "duration") {
                duration = parse_number(value, key, true);
            } else {
                set_parameter(current.config, particle_count, key, value);
            }
        } catch (const std::invalid_argument& error) {
            throw error_at(line_number, error.what());
        }
    }
    if (input.bad()) {
        throw error_at(line_number, "error reading configuration");
    }
    if (section_line == 0) {
        throw error_at(1, "expected at least one [simulation] section");
    }
    finish_section();
    return simulations;
}

} // namespace nbody
