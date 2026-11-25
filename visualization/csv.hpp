#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

namespace viewer {

using Position = std::array<float, 3>;

struct Body {
    Position position;
    float radius;
    std::size_t id;
    std::optional<std::array<float, 3>> color;
};

struct Frame {
    double time;
    std::vector<Body> bodies;
};

std::vector<Frame> load_trajectory(const std::filesystem::path& path);

} // namespace viewer
