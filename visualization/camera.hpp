#pragma once

#include "csv.hpp"
#include "view_settings.hpp"

struct GLFWwindow;

namespace viewer {

class Playback;

struct CameraView {
    Position eye;
    Position right;
    Position up;
    Position forward;
    float aspect;
    float near_plane;
    float far_plane;
};

class Camera {
public:
    // Center and scale the trajectory once so camera movement uses consistent units.
    explicit Camera(std::vector<Frame>& frames);
    CameraView view(double aspect) const;
    void orbit(double dx, double dy);
    void pan(double dx, double dy, int width, int height);
    void zoom(double factor);
    void reset();

private:
    Position target_{};
    double yaw_ = 0;
    double pitch_ = 0;
    double zoom_ = 1;
};

struct Controls {
    Playback& playback;
    Camera& camera;
    nbody::ViewSettings& settings;
    double cursor_x = 0;
    double cursor_y = 0;

    void bind(GLFWwindow* window);
};

} // namespace viewer
