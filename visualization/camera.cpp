#include "camera.hpp"
#include "playback.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace viewer {

Camera::Camera(std::vector<Frame>& frames, const LoadProgress& progress)
{
    std::array<double, 3> low{}, high{};
    const auto& initial = frames.front().bodies.front();
    for (std::size_t axis = 0; axis < low.size(); ++axis) {
        low[axis] = high[axis] = initial.position[axis];
    }
    std::size_t bodies_seen = 0;
    std::size_t frame_index = 0;
    for (const auto& frame : frames) {
        for (const auto& body : frame.bodies) {
            if (progress && ++bodies_seen % 4096 == 0) {
                progress(0.5 * frame_index / frames.size());
            }
            for (std::size_t axis = 0; axis < low.size(); ++axis) {
                const double coordinate = body.position[axis];
                low[axis] = std::min(low[axis], coordinate - body.radius);
                high[axis] = std::max(high[axis], coordinate + body.radius);
            }
        }
        ++frame_index;
    }
    double radius_squared = 0;
    for (std::size_t axis = 0; axis < center_.size(); ++axis) {
        center_[axis] = (low[axis] + high[axis]) / 2;
        const auto half_span = high[axis] - center_[axis];
        radius_squared += half_span * half_span;
    }
    radius_ = radius_squared == 0 ? 1 : std::sqrt(radius_squared);
    frame_index = 0;
    for (auto& frame : frames) {
        for (auto& body : frame.bodies) {
            if (progress && ++bodies_seen % 4096 == 0) {
                progress(0.5 + 0.5 * frame_index / frames.size());
            }
            body.position = scene_position({body.position[0], body.position[1], body.position[2]});
            body.radius = body.radius > 0 ? static_cast<float>(body.radius / radius_) : 0.025f;
        }
        ++frame_index;
    }
    reset();
    if (progress) {
        progress(1);
    }
}

Position Camera::scene_position(const std::array<double, 3>& world) const
{
    Position scene{};
    for (std::size_t axis = 0; axis < scene.size(); ++axis) {
        scene[axis] = static_cast<float>((world[axis] - center_[axis]) / radius_);
    }
    return scene;
}

CameraView Camera::view(double aspect) const
{
    const auto sy = static_cast<float>(std::sin(yaw_));
    const auto cy = static_cast<float>(std::cos(yaw_));
    const auto sp = static_cast<float>(std::sin(pitch_));
    const auto cp = static_cast<float>(std::cos(pitch_));
    const Position forward{-cp * cy, -cp * sy, -sp};
    const auto depth = static_cast<float>(distance(aspect));
    Position eye{};
    for (std::size_t axis = 0; axis < eye.size(); ++axis) {
        eye[axis] = target_[axis] - depth * forward[axis];
    }
    const float near_plane = std::max(1e-7f, depth * 0.02f);
    const float far_plane = depth + 10 + static_cast<float>(
        std::hypot(static_cast<double>(target_[0]), target_[1], target_[2]));
    return {eye, {-sy, cy, 0}, {-sp * cy, -sp * sy, cp}, forward,
            static_cast<float>(aspect), near_plane, far_plane};
}

double Camera::distance(double aspect) const
{
    return 3.2 / (zoom_ * std::min(1.0, aspect));
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
    const double units_per_pixel = 2 * 0.41421356237 * distance(aspect) / height;
    for (std::size_t axis = 0; axis < target_.size(); ++axis) {
        target_[axis] += static_cast<float>(
            units_per_pixel * (-dx * current.right[axis] + dy * current.up[axis]));
    }
}

void Camera::zoom(double factor)
{
    zoom_ = std::clamp(zoom_ * factor, 0.001, 1000000.0);
}

void Camera::focus(const Frame& frame, double x, double y, int width, int height, float sphere_scale)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const double aspect = static_cast<double>(width) / height;
    const auto camera = view(aspect);
    constexpr double tan_half_fov = 0.41421356237;
    const double horizontal = (2 * x / width - 1) * aspect * tan_half_fov;
    const double vertical = (1 - 2 * y / height) * tan_half_fov;
    const double ray_length = std::hypot(1.0, horizontal, vertical);
    std::array<double, 3> ray{};
    for (std::size_t i = 0; i < ray.size(); ++i) {
        ray[i] = (camera.forward[i] + horizontal * camera.right[i] + vertical * camera.up[i]) / ray_length;
    }

    const Body* selected = nullptr;
    double nearest = std::numeric_limits<double>::infinity();
    for (const auto& body : frame.bodies) {
        std::array<double, 3> offset{};
        double along = 0;
        for (std::size_t i = 0; i < offset.size(); ++i) {
            offset[i] = static_cast<double>(body.position[i]) - camera.eye[i];
            along += offset[i] * ray[i];
        }
        double off_axis_squared = 0;
        for (std::size_t i = 0; i < offset.size(); ++i) {
            const double perpendicular = offset[i] - along * ray[i];
            off_axis_squared += perpendicular * perpendicular;
        }
        const double radius = body.radius * sphere_scale;
        const double chord_squared = radius * radius - off_axis_squared;
        if (chord_squared >= 0) {
            const auto half_chord = std::sqrt(chord_squared);
            const auto entry = along - half_chord;
            const auto hit = (entry > 0 ? entry : along + half_chord);
            if (hit > 0 && hit < nearest) {
                nearest = hit;
                selected = &body;
            }
        }
    }
    if (selected) {
        target_ = selected->position;
        zoom_ = 1;
        zoom(3.2 / (3.5 * selected->radius * sphere_scale));
    }
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
        case GLFW_KEY_C: {
            double x, y;
            int width, height;
            glfwGetCursorPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);
            controls.camera.focus(controls.playback.frame(), x, y, width, height,
                                  controls.settings.particle_size / 8);
            break;
        }
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
