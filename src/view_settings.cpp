#include "view_settings.hpp"
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string>

namespace nbody {
namespace {
constexpr char filename[] = "trajectory.view.ini";
}

void set_view_setting(ViewSettings& settings, std::string_view key, std::string_view value)
{
    if (key == "trails") {
        if (value != "true" && value != "false") {
            throw std::invalid_argument("trails must be true or false");
        }
        settings.trails = value == "true";
        return;
    }
    if (key != "particle_size" && key != "trail_length") {
        throw std::invalid_argument("Unknown visualization setting " + std::string(key));
    }
    unsigned number{};
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, number);
    const unsigned maximum = key == "particle_size" ? 64 : 256;
    if (parsed.ec != std::errc{} || parsed.ptr != end || number < 1 || number > maximum) {
        throw std::invalid_argument(std::string(key) + " must be an integer from 1 to " +
                                    std::to_string(maximum));
    }
    if (key == "particle_size") {
        settings.particle_size = static_cast<float>(number);
    } else {
        settings.trail_length = number;
    }
}

void save_view_settings(const std::filesystem::path& directory, const std::optional<ViewSettings>& settings)
{
    const auto path = directory / filename;
    if (!settings) {
        if (std::filesystem::is_regular_file(path)) {
            std::filesystem::remove(path);
        }
        return;
    }
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path);
    output << "particle_size=" << settings->particle_size << '\n'
           << "trails=" << (settings->trails ? "true" : "false") << '\n'
           << "trail_length=" << settings->trail_length << '\n';
    output.close();
}

ViewSettings load_view_settings(const std::filesystem::path& directory)
{
    ViewSettings settings;
    const auto path = directory / filename;
    if (!std::filesystem::exists(path)) {
        return settings;
    }
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open " + path.string());
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("Invalid setting in " + path.string());
        }
        set_view_setting(settings, line.substr(0, equals), line.substr(equals + 1));
    }
    if (input.bad()) {
        throw std::runtime_error("Cannot read " + path.string());
    }
    return settings;
}
}
