#include "renderer.hpp"
#include "camera.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace viewer {
namespace {

constexpr char vertex_source[] = R"(
#version 330 core
layout(location = 0) in vec3 vertex;
layout(location = 1) in vec4 body; // center/radius for spheres, second endpoint for trails
layout(location = 2) in vec3 color;
uniform vec3 eye, right, up, forward;
uniform vec3 lens;
uniform int kind;
uniform float sphere_scale, trail_width;
uniform vec2 viewport;
uniform vec3 reference_color;
out vec3 normal;
out vec3 base_color;

vec4 project_position(vec3 position) {
    vec3 offset = position - eye;
    float depth = dot(offset, forward);
    float f = 2.41421356237;
    float n = lens.y, far_plane = lens.z;
    return vec4(f * dot(offset, right) / lens.x, f * dot(offset, up),
        ((far_plane + n) * depth - 2.0 * far_plane * n) / (far_plane - n), depth);
}

void main() {
    base_color = kind == 0 ? reference_color : color;
    normal = vertex;
    if (kind == 1) {
        gl_Position = project_position(body.xyz + vertex * (body.w * sphere_scale));
    } else if (kind == 2) {
        vec4 a = project_position(vertex);
        vec4 b = project_position(body.xyz);
        if (max(a.w, b.w) <= lens.y) {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }
        if (a.w < lens.y) a = mix(a, b, (lens.y - a.w) / (b.w - a.w));
        if (b.w < lens.y) b = mix(b, a, (lens.y - b.w) / (a.w - b.w));
        vec2 direction = (b.xy / b.w - a.xy / a.w) * viewport;
        vec2 perpendicular = vec2(-direction.y, direction.x) / max(length(direction), 0.001);
        gl_Position = gl_VertexID < 2 ? a : b;
        float side = gl_VertexID % 2 == 0 ? -1.0 : 1.0;
        gl_Position.xy += perpendicular * side * trail_width * gl_Position.w / viewport;
    } else {
        gl_Position = project_position(vertex);
    }
}
)";

constexpr char fragment_source[] = R"(
#version 330 core
uniform int kind;
in vec3 normal;
in vec3 base_color;
out vec4 pixel;
void main() {
    float shade = 1.0;
    if (kind == 1) {
        vec3 light = normalize(vec3(0.35, -0.55, 0.76));
        shade = 0.33 + 0.67 * max(dot(normalize(normal), light), 0.0);
    }
    pixel = vec4(base_color * shade, 1.0);
}
)";

GLuint compile_shader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compilation failed: ") + log);
    }
    return shader;
}

GLuint create_program()
{
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment;
    try {
        fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    } catch (...) {
        glDeleteShader(vertex);
        throw;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof log, nullptr, log);
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Shader link failed: ") + log);
    }
    return program;
}

template<typename T>
void upload(GLenum target, GLuint buffer, const std::vector<T>& data, GLenum usage)
{
    glBindBuffer(target, buffer);
    glBufferData(target, data.size() * sizeof(T), data.data(), usage);
}

void attribute(GLuint location, GLint size, GLsizei stride, std::size_t offset, GLuint divisor)
{
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offset));
    glVertexAttribDivisor(location, divisor);
}

Position palette(std::size_t id)
{
    constexpr Position colors[] = {{0.39f, 0.82f, 1}, {1, 0.62f, 0.39f}, {0.53f, 0.95f, 0.66f},
        {0.83f, 0.67f, 1}, {1, 0.88f, 0.48f}, {1, 0.54f, 0.72f}};
    return colors[id % 6];
}

constexpr int grid_vertices = 44;
enum DrawKind { reference, spheres, trails };
enum Buffer { grid_buffer, mesh_buffer, index_buffer, sphere_buffer, trail_buffer };

} // namespace

Renderer::Renderer(unsigned quality, const Camera& camera) : program_(create_program())
{
    eye_ = glGetUniformLocation(program_, "eye");
    right_ = glGetUniformLocation(program_, "right");
    up_ = glGetUniformLocation(program_, "up");
    forward_ = glGetUniformLocation(program_, "forward");
    lens_ = glGetUniformLocation(program_, "lens");
    size_ = glGetUniformLocation(program_, "sphere_scale");
    kind_ = glGetUniformLocation(program_, "kind");
    color_ = glGetUniformLocation(program_, "reference_color");
    viewport_ = glGetUniformLocation(program_, "viewport");
    trail_width_ = glGetUniformLocation(program_, "trail_width");

    glGenVertexArrays(3, arrays_);
    glGenBuffers(5, buffers_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.035f, 0.05f, 0.08f, 1.0f);

    std::vector<Position> grid;
    const double extent = camera.scene_radius();
    const auto add_line = [&](const std::array<double, 3>& start, const std::array<double, 3>& end) {
        grid.push_back(camera.scene_position(start));
        grid.push_back(camera.scene_position(end));
    };
    for (int i = -5; i <= 5; ++i) {
        const double coordinate = i * extent / 5;
        add_line({coordinate, -extent, 0}, {coordinate, extent, 0});
        add_line({-extent, coordinate, 0}, {extent, coordinate, 0});
    }
    const double axis_length = 1.15 * extent;
    add_line({0, 0, 0}, {axis_length, 0, 0});
    add_line({0, 0, 0}, {0, axis_length, 0});
    add_line({0, 0, 0}, {0, 0, axis_length});
    glBindVertexArray(arrays_[reference]);
    upload(GL_ARRAY_BUFFER, buffers_[grid_buffer], grid, GL_STATIC_DRAW);
    attribute(0, 3, sizeof(Position), 0, 0);

    constexpr double pi = 3.141592653589793;
    const unsigned columns = 2 * quality;
    std::vector<Position> mesh;
    std::vector<unsigned> indices;
    for (unsigned row = 0; row <= quality; ++row) {
        const double latitude = pi * row / quality;
        for (unsigned column = 0; column < columns; ++column) {
            const double longitude = 2 * pi * column / columns;
            mesh.push_back({static_cast<float>(std::sin(latitude) * std::cos(longitude)),
                            static_cast<float>(std::sin(latitude) * std::sin(longitude)),
                            static_cast<float>(std::cos(latitude))});
            if (row < quality) {
                const unsigned a = row * columns + column;
                const unsigned b = row * columns + (column + 1) % columns;
                indices.insert(indices.end(), {a, a + columns, b, b, a + columns, b + columns});
            }
        }
    }
    glBindVertexArray(arrays_[spheres]);
    upload(GL_ARRAY_BUFFER, buffers_[mesh_buffer], mesh, GL_STATIC_DRAW);
    attribute(0, 3, sizeof(Position), 0, 0);
    upload(GL_ELEMENT_ARRAY_BUFFER, buffers_[index_buffer], indices, GL_STATIC_DRAW);
    sphere_indices_ = static_cast<int>(indices.size());
    glBindBuffer(GL_ARRAY_BUFFER, buffers_[sphere_buffer]);
    attribute(1, 4, sizeof(Sphere), offsetof(Sphere, center), 1);
    attribute(2, 3, sizeof(Sphere), offsetof(Sphere, color), 1);

    glBindVertexArray(arrays_[trails]);
    glBindBuffer(GL_ARRAY_BUFFER, buffers_[trail_buffer]);
    attribute(0, 3, sizeof(Trail), offsetof(Trail, start), 1);
    attribute(1, 3, sizeof(Trail), offsetof(Trail, end), 1);
    attribute(2, 3, sizeof(Trail), offsetof(Trail, color), 1);
}

Renderer::~Renderer()
{
    glDeleteBuffers(5, buffers_);
    glDeleteVertexArrays(3, arrays_);
    glDeleteProgram(program_);
}

void Renderer::draw(const std::vector<Frame>& frames, std::size_t index, const Camera& camera,
                    const nbody::ViewSettings& settings, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const auto& bodies = frames[index].bodies;
    if (bodies.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::runtime_error("Too many particles to render in one frame");
    }
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program_);
    const auto view = camera.view(static_cast<double>(width) / height);
    glUniform3fv(eye_, 1, view.eye.data());
    glUniform3fv(right_, 1, view.right.data());
    glUniform3fv(up_, 1, view.up.data());
    glUniform3fv(forward_, 1, view.forward.data());
    glUniform3f(lens_, view.aspect, view.near_plane, view.far_plane);
    glUniform1f(size_, settings.particle_size / 8);
    glUniform1f(trail_width_, static_cast<float>(settings.trail_width));
    glUniform2f(viewport_, static_cast<float>(width), static_cast<float>(height));

    glUniform1i(kind_, reference);
    glBindVertexArray(arrays_[reference]);
    glUniform3f(color_, 0.17f, 0.22f, 0.30f);
    glDrawArrays(GL_LINES, 0, grid_vertices);
    glUniform3f(color_, 0.9f, 0.3f, 0.3f);
    glDrawArrays(GL_LINES, grid_vertices, 2);
    glUniform3f(color_, 0.3f, 0.8f, 0.4f);
    glDrawArrays(GL_LINES, grid_vertices + 2, 2);
    glUniform3f(color_, 0.3f, 0.55f, 1.0f);
    glDrawArrays(GL_LINES, grid_vertices + 4, 2);

    if (settings.trails && index > 0) {
        if (last_trail_index_ != index || last_trail_length_ != settings.trail_length) {
            trails_.clear();
            const auto history = std::min(index, static_cast<std::size_t>(settings.trail_length));
            constexpr std::size_t budget = 200000;
            for (std::size_t offset = 0; offset < history && trails_.size() < budget; ++offset) {
                const auto& previous = frames[index - offset - 1].bodies;
                const auto& current = frames[index - offset].bodies;
                auto a = previous.begin();
                auto b = current.begin();
                // IDs are sorted. A newly merged body has no match in the previous frame.
                while (a != previous.end() && b != current.end() && trails_.size() < budget) {
                    if (a->id < b->id) {
                        ++a;
                    } else if (b->id < a->id) {
                        ++b;
                    } else {
                        trails_.push_back({a->position, b->position, a->color.value_or(palette(a->id))});
                        ++a;
                        ++b;
                    }
                }
            }
            upload(GL_ARRAY_BUFFER, buffers_[trail_buffer], trails_, GL_STREAM_DRAW);
            last_trail_index_ = index;
            last_trail_length_ = settings.trail_length;
        }
        glUniform1i(kind_, trails);
        glBindVertexArray(arrays_[trails]);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(trails_.size()));
    }

    if (last_index_ != index) {
        spheres_.clear();
        spheres_.reserve(bodies.size());
        for (const auto& body : bodies) {
            spheres_.push_back({body.position, body.radius, body.color.value_or(palette(body.id))});
        }
        upload(GL_ARRAY_BUFFER, buffers_[sphere_buffer], spheres_, GL_STREAM_DRAW);
        last_index_ = index;
    }
    glUniform1i(kind_, spheres);
    glBindVertexArray(arrays_[spheres]);
    glDrawElementsInstanced(GL_TRIANGLES, sphere_indices_, GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(spheres_.size()));
}

} // namespace viewer
