#pragma once

#include "particle.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace nbody {

class TrajectoryWriter {
public:
    explicit TrajectoryWriter(const std::filesystem::path& directory, bool write_color = false);
    void write_frame(std::size_t step, double time, const std::vector<Particle>& particles);
    void close();

private:
    std::ofstream file_;
    bool write_color_;
};

} // namespace nbody
