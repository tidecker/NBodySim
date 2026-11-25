#pragma once
#include <filesystem>
#include <optional>
#include <string_view>

namespace nbody {
struct ViewSettings {
    float particle_size = 8;
    bool trails = false;
    unsigned trail_length = 32;
    unsigned sphere_quality = 10;
    unsigned trail_width = 2;
};

void set_view_setting(ViewSettings& settings, std::string_view key, std::string_view value);
void save_view_settings(const std::filesystem::path& directory, const std::optional<ViewSettings>& settings);
ViewSettings load_view_settings(const std::filesystem::path& directory);
}
