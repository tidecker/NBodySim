#pragma once

#include <array>
#include <filesystem>
#include <vector>

namespace viewer {

using Position = std::array<float, 3>;

struct Frame {
    double time;
    std::vector<Position> positions;
};

std::vector<Frame> load_trajectory(const std::filesystem::path& path);

} // namespace viewer
