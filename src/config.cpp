#include "config.hpp"

#include <charconv>
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

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
        if (option == "--mode") {
            if (value != "two-body" && value != "random") {
                throw std::invalid_argument("--mode must be two-body or random");
            }
            config.two_body = value == "two-body";
        } else if (option == "--particles") {
            particle_count = parse_unsigned<std::size_t>(value, option);
        } else if (option == "--dt") {
            std::istringstream input{std::string(value)};
            input.imbue(std::locale::classic());
            if (!(input >> std::noskipws >> config.dt) ||
                input.peek() != std::char_traits<char>::eof() ||
                !std::isfinite(config.dt) || config.dt <= 0) {
                throw std::invalid_argument("--dt must be a finite positive number");
            }
        } else if (option == "--steps") {
            config.steps = parse_unsigned<std::size_t>(value, option);
        } else if (option == "--seed") {
            config.seed = parse_unsigned<std::uint32_t>(value, option);
        } else if (option == "--output-dir") {
            if (value.empty()) {
                throw std::invalid_argument("--output-dir must be nonempty");
            }
            config.output_dir = value;
        } else {
            throw std::invalid_argument("Unknown option " + std::string(option));
        }
    }
    config.particles = particle_count.value_or(config.two_body ? 2 : 100);
    if (config.particles == 0 || (config.two_body && config.particles != 2)) {
        throw std::invalid_argument("--particles must be positive and must be 2 in two-body mode");
    }
    if (!std::isfinite(config.dt * static_cast<double>(config.steps))) {
        throw std::invalid_argument("--dt and --steps yield a nonfinite end time");
    }
    return config;
}

} // namespace nbody
