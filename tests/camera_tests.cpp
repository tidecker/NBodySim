#include "camera.hpp"

#include <cmath>
#include <iostream>

int main()
{
    using namespace viewer;
    std::vector<Frame> frames{
        {0, {{{0, 0, 0}, 0.1f, 0}, {{8, 2, 4}, 0.2f, 1}}},
        {1, {{{0, 0, 0}, 0.1f, 0}, {{10, 3, 5}, 0.2f, 1}}}
    };
    Camera camera(frames);
    const auto origin = camera.scene_position({0, 0, 0});
    if (origin == Position{} || frames[0].bodies[0].position != origin ||
        frames[1].bodies[0].position != origin ||
        frames[0].bodies[1].position != camera.scene_position({8, 2, 4}) ||
        frames[1].bodies[1].position != camera.scene_position({10, 3, 5}) ||
        camera.scene_position({8, 2, 0})[2] != origin[2]) {
        std::cerr << "Asymmetric camera fit misaligned world references and trajectory positions\n";
        return 1;
    }
    const Frame inspection{0, {{{0, 0, 0}, 0.001f, 0}}};
    camera.focus(inspection, 320, 240, 640, 480, 1);
    const auto close = camera.view(640.0 / 480);
    const auto eye_distance = std::hypot(close.eye[0], close.eye[1], close.eye[2]);
    if (std::abs(eye_distance - 0.0035) > 1e-6 ||
        !(close.near_plane < eye_distance - 0.001 && close.far_plane > eye_distance + 0.001)) {
        std::cerr << "A small focused sphere is clipped or poorly framed\n";
        return 1;
    }
    camera.zoom(0.000001);
    const auto wide = camera.view(1);
    if (!(wide.far_plane > 3200 && wide.near_plane < 3199)) {
        std::cerr << "Wide view has incorrect clipping planes\n";
        return 1;
    }
}
