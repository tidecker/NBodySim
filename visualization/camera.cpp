#include "camera.hpp"

#include "playback.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace viewer {

Camera::Camera(const std::vector<Frame>& frames)
{
    double min_x = frames.front().positions.front()[0];
    double max_x = min_x;
    double min_y = frames.front().positions.front()[1];
    double max_y = min_y;
    for (const auto& frame : frames) {
        for (const auto& position : frame.positions) {
            min_x = std::min(min_x, static_cast<double>(position[0]));
            max_x = std::max(max_x, static_cast<double>(position[0]));
            min_y = std::min(min_y, static_cast<double>(position[1]));
            max_y = std::max(max_y, static_cast<double>(position[1]));
        }
    }
    center_x_ = (min_x + max_x) / 2;
    center_y_ = (min_y + max_y) / 2;
    half_span_ = 0.55 * std::max(max_x - min_x, max_y - min_y);
    if (half_span_ == 0) {
        half_span_ = 1;
    }
}

void Camera::zoom(double factor)
{
    zoom_ = std::clamp(zoom_ * factor, 0.05, 100.0);
}

std::array<float, 2> Camera::project(const Position& position, double aspect) const
{
    const double scale = zoom_ / half_span_;
    return {static_cast<float>((position[0] - center_x_) * scale / std::max(1.0, aspect)),
            static_cast<float>((position[1] - center_y_) * scale * std::min(1.0, aspect))};
}

void Controls::bind(GLFWwindow* window)
{
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int) {
        if (action != GLFW_PRESS) {
            return;
        }
        auto& controls = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
        switch (key) {
        case GLFW_KEY_SPACE: controls.playback.toggle_pause(); break;
        case GLFW_KEY_R: controls.playback.restart(); break;
        case GLFW_KEY_UP: controls.playback.change_speed(2); break;
        case GLFW_KEY_DOWN: controls.playback.change_speed(0.5); break;
        case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD: controls.camera.zoom(1.25); break;
        case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT: controls.camera.zoom(0.8); break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double, double y) {
        auto& controls = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
        controls.camera.zoom(std::pow(1.2, y));
    });
}

} // namespace viewer
