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
    if (key != "particle_size" && key != "trail_length" && key != "sphere_quality" &&
        key != "trail_width") {
        throw std::invalid_argument("Unknown visualization setting " + std::string(key));
    }
    unsigned number{};
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, number);
    unsigned maximum = 256;
    if (key == "particle_size") maximum = 64;
    if (key == "sphere_quality") maximum = 32;
    if (key == "trail_width") maximum = 12;
    const unsigned minimum = key == "sphere_quality" ? 4 : 1;
    if (parsed.ec != std::errc{} || parsed.ptr != end || number < minimum || number > maximum) {
        throw std::invalid_argument(std::string(key) + " must be an integer from " +
                                    std::to_string(minimum) + " to " + std::to_string(maximum));
    }
    if (key == "particle_size") {
        settings.particle_size = static_cast<float>(number);
    } else if (key == "sphere_quality") {
        settings.sphere_quality = number;
    } else if (key == "trail_width") {
        settings.trail_width = number;
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
           << "trail_length=" << settings->trail_length << '\n'
           << "sphere_quality=" << settings->sphere_quality << '\n'
           << "trail_width=" << settings->trail_width << '\n';
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
