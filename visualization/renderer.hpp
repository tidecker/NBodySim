#pragma once

#include "csv.hpp"
#include "view_settings.hpp"

namespace viewer {

class Camera;

class Renderer {
public:
    explicit Renderer(unsigned sphere_quality);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(const std::vector<Frame>& frames, std::size_t index, const Camera& camera,
              const nbody::ViewSettings& settings, int width, int height);

private:
    struct Sphere {
        Position center;
        float radius;
        Position color;
    };
    struct Trail {
        Position start;
        Position end;
        Position color;
    };

    unsigned program_;
    unsigned arrays_[3]{};
    unsigned buffers_[5]{};
    int sphere_indices_ = 0;
    int eye_, right_, up_, forward_, lens_, size_, kind_, color_, viewport_, trail_width_;
    std::size_t last_index_ = static_cast<std::size_t>(-1);
    std::size_t last_trail_index_ = static_cast<std::size_t>(-1);
    unsigned last_trail_length_ = 0;
    std::vector<Sphere> spheres_;
    std::vector<Trail> trails_;
};

} // namespace viewer
