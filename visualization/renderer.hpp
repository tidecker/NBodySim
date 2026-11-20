#pragma once

#include "csv.hpp"

namespace viewer {

class Camera;

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(const Frame& frame, const Camera& camera, int width, int height);

private:
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    std::vector<float> vertices_;
};

} // namespace viewer
