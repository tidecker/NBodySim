#pragma once

#include "csv.hpp"

struct GLFWwindow;

namespace viewer {

class Playback;

class Camera {
public:
    explicit Camera(const std::vector<Frame>& frames);
    void zoom(double factor);
    std::array<float, 2> project(const Position& position, double aspect) const;

private:
    double center_x_;
    double center_y_;
    double half_span_;
    double zoom_ = 1;
};

struct Controls {
    Playback& playback;
    Camera& camera;

    void bind(GLFWwindow* window);
};

} // namespace viewer
