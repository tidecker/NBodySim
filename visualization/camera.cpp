#include "camera.hpp"
#include "playback.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace viewer {

Camera::Camera(std::vector<Frame>& frames)
{
    auto low = frames.front().positions.front();
    auto high = low;
    for (const auto& frame : frames) {
        for (const auto& point : frame.positions) {
            for (std::size_t axis = 0; axis < point.size(); ++axis) {
                low[axis] = std::min(low[axis], point[axis]);
                high[axis] = std::max(high[axis], point[axis]);
            }
        }
    }
    std::array<double, 3> center{};
    double radius_squared = 0;
    for (std::size_t axis = 0; axis < center.size(); ++axis) {
        center[axis] = (static_cast<double>(low[axis]) + high[axis]) / 2;
        const auto half_span = high[axis] - center[axis];
        radius_squared += half_span * half_span;
    }
    const auto radius = radius_squared == 0 ? 1 : std::sqrt(radius_squared);
    for (auto& frame : frames) {
        for (auto& point : frame.positions) {
            for (std::size_t axis = 0; axis < point.size(); ++axis) {
                point[axis] = static_cast<float>((point[axis] - center[axis]) / radius);
            }
        }
    }
    reset();
}

CameraView Camera::view(double aspect) const
{
    const auto sy = static_cast<float>(std::sin(yaw_));
    const auto cy = static_cast<float>(std::cos(yaw_));
    const auto sp = static_cast<float>(std::sin(pitch_));
    const auto cp = static_cast<float>(std::cos(pitch_));
    const Position forward{-cp * cy, -cp * sy, -sp};
    const auto distance = static_cast<float>(3.2 / (zoom_ * std::min(1.0, aspect)));
    Position eye{};
    for (std::size_t axis = 0; axis < eye.size(); ++axis) {
        eye[axis] = target_[axis] - distance * forward[axis];
    }
    return {eye, {-sy, cy, 0}, {-sp * cy, -sp * sy, cp}, forward,
            static_cast<float>(aspect), 0.005f, distance + 10};
}

void Camera::orbit(double dx, double dy)
{
    yaw_ = std::remainder(yaw_ - 0.006 * dx, 6.283185307179586);
    pitch_ = std::clamp(pitch_ + 0.006 * dy, -1.55, 1.55);
}

void Camera::pan(double dx, double dy, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const auto aspect = static_cast<double>(width) / height;
    const auto current = view(aspect);
    const double units_per_pixel = 2 * 0.41421356237 * 3.2 /
        (zoom_ * std::min(1.0, aspect) * height);
    for (std::size_t axis = 0; axis < target_.size(); ++axis) {
        target_[axis] += static_cast<float>(
            units_per_pixel * (-dx * current.right[axis] + dy * current.up[axis]));
    }
}

void Camera::zoom(double factor)
{
    zoom_ = std::clamp(zoom_ * factor, 0.1, 30.0);
}

void Camera::reset()
{
    target_ = {};
    yaw_ = -1.1;
    pitch_ = 0.55;
    zoom_ = 1;
}

void Controls::bind(GLFWwindow* window)
{
    glfwSetWindowUserPointer(window, this);
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
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
        case GLFW_KEY_LEFT_BRACKET:
            controls.settings.particle_size = std::max(1.0f, controls.settings.particle_size - 1);
            break;
        case GLFW_KEY_RIGHT_BRACKET:
            controls.settings.particle_size = std::min(64.0f, controls.settings.particle_size + 1);
            break;
        case GLFW_KEY_T: controls.settings.trails = !controls.settings.trails; break;
        case GLFW_KEY_F: controls.camera.reset(); break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double, double y) {
        auto& controls = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
        controls.camera.zoom(std::pow(1.2, y));
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {
        auto& controls = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
        const auto dx = x - controls.cursor_x;
        const auto dy = y - controls.cursor_y;
        controls.cursor_x = x;
        controls.cursor_y = y;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ||
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            controls.camera.pan(dx, dy, width, height);
        } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            controls.camera.orbit(dx, dy);
        }
    });
}

} // namespace viewer
