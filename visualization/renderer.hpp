#pragma once

#include "csv.hpp"
#include "view_settings.hpp"

namespace viewer {

class Camera;

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(const std::vector<Frame>& frames, std::size_t index, const Camera& camera,
              const nbody::ViewSettings& settings, int width, int height);

private:
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned buffers_[3]{};
    int eye_ = -1;
    int right_ = -1;
    int up_ = -1;
    int forward_ = -1;
    int lens_ = -1;
    int point_size_ = -1;
    int points_ = -1;
    int line_color_ = -1;
    std::size_t last_index_ = static_cast<std::size_t>(-1);
    std::size_t last_trail_index_ = static_cast<std::size_t>(-1);
    unsigned last_trail_length_ = 0;
    int trail_vertices_ = 0;
    std::vector<Position> trails_;
};

} // namespace viewer
