#pragma once

#include <array>
#include <filesystem>
#include <functional>
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

using LoadProgress = std::function<void(double)>;
std::vector<Frame> load_trajectory(const std::filesystem::path& path, const LoadProgress& progress = {});

} // namespace viewer
