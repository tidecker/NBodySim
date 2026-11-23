#include "renderer.hpp"
#include "camera.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace viewer {
namespace {

constexpr char vertex_source[] = R"(
#version 330 core
layout(location = 0) in vec3 position;
uniform vec3 eye, right, up, forward;
uniform vec3 lens; // aspect, near plane, far plane
uniform float point_size;
flat out int particle_id;
void main() {
    vec3 offset = position - eye;
    float depth = dot(offset, forward);
    float f = 2.41421356237;
    float n = lens.y, far_plane = lens.z;
    gl_Position = vec4(f * dot(offset, right) / lens.x, f * dot(offset, up),
        ((far_plane + n) * depth - 2.0 * far_plane * n) / (far_plane - n), depth);
    gl_PointSize = clamp(3.2 * point_size / max(depth, n), 1.0, 64.0);
    particle_id = gl_VertexID;
}
)";

constexpr char fragment_source[] = R"(
#version 330 core
flat in int particle_id;
uniform bool points;
uniform vec3 line_color;
out vec4 pixel;
void main() {
    if (!points) {
        pixel = vec4(line_color, 1.0);
        return;
    }
    const vec3 colors[6] = vec3[6](
        vec3(0.39, 0.82, 1.0), vec3(1.0, 0.62, 0.39), vec3(0.53, 0.95, 0.66),
        vec3(0.83, 0.67, 1.0), vec3(1.0, 0.88, 0.48), vec3(1.0, 0.54, 0.72));
    vec2 offset = gl_PointCoord - vec2(0.5);
    if (dot(offset, offset) > 0.25) discard;
    pixel = vec4(colors[particle_id % 6], 1.0);
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

void upload(GLuint buffer, const std::vector<Position>& positions, GLenum usage)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(Position), positions.data(), usage);
}

void bind_positions(GLuint buffer)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Position), nullptr);
}

constexpr int grid_vertices = 44;
enum Buffer { reference, particles, trails };

} // namespace

Renderer::Renderer() : program_(create_program())
{
    eye_ = glGetUniformLocation(program_, "eye");
    right_ = glGetUniformLocation(program_, "right");
    up_ = glGetUniformLocation(program_, "up");
    forward_ = glGetUniformLocation(program_, "forward");
    lens_ = glGetUniformLocation(program_, "lens");
    point_size_ = glGetUniformLocation(program_, "point_size");
    points_ = glGetUniformLocation(program_, "points");
    line_color_ = glGetUniformLocation(program_, "line_color");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(3, buffers_);
    glBindVertexArray(vao_);
    glEnableVertexAttribArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glClearColor(0.035f, 0.05f, 0.08f, 1.0f);

    std::vector<Position> grid;
    for (int i = -5; i <= 5; ++i) {
        const float coordinate = i * 0.2f;
        grid.insert(grid.end(), {{coordinate, -1, 0}, {coordinate, 1, 0},
                                {-1, coordinate, 0}, {1, coordinate, 0}});
    }
    grid.insert(grid.end(), {{0, 0, 0}, {1.15f, 0, 0}, {0, 0, 0}, {0, 1.15f, 0},
                            {0, 0, 0}, {0, 0, 1.15f}});
    upload(buffers_[reference], grid, GL_STATIC_DRAW);
}

Renderer::~Renderer()
{
    glDeleteBuffers(3, buffers_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
}

void Renderer::draw(const std::vector<Frame>& frames, std::size_t index, const Camera& camera,
                    const nbody::ViewSettings& settings, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const auto& positions = frames[index].positions;
    if (positions.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::runtime_error("Too many particles to render in one frame");
    }

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program_);
    glBindVertexArray(vao_);

    const auto view = camera.view(static_cast<double>(width) / height);
    glUniform3fv(eye_, 1, view.eye.data());
    glUniform3fv(right_, 1, view.right.data());
    glUniform3fv(up_, 1, view.up.data());
    glUniform3fv(forward_, 1, view.forward.data());
    glUniform3f(lens_, view.aspect, view.near_plane, view.far_plane);
    glUniform1f(point_size_, settings.particle_size);

    glUniform1i(points_, GL_FALSE);
    bind_positions(buffers_[reference]);
    glUniform3f(line_color_, 0.17f, 0.22f, 0.30f);
    glDrawArrays(GL_LINES, 0, grid_vertices);
    glUniform3f(line_color_, 0.9f, 0.3f, 0.3f);
    glDrawArrays(GL_LINES, grid_vertices, 2);
    glUniform3f(line_color_, 0.3f, 0.8f, 0.4f);
    glDrawArrays(GL_LINES, grid_vertices + 2, 2);
    glUniform3f(line_color_, 0.3f, 0.55f, 1.0f);
    glDrawArrays(GL_LINES, grid_vertices + 4, 2);

    if (settings.trails && index > 0) {
        if (last_trail_index_ != index || last_trail_length_ != settings.trail_length) {
            // Keep the work bounded for dense trajectories.
            const auto history = std::min({index, static_cast<std::size_t>(settings.trail_length),
                                            200000 / positions.size()});
            trails_.clear();
            trails_.reserve(2 * history * positions.size());
            for (std::size_t step = index - history; step < index; ++step) {
                for (std::size_t id = 0; id < positions.size(); ++id) {
                    trails_.push_back(frames[step].positions[id]);
                    trails_.push_back(frames[step + 1].positions[id]);
                }
            }
            upload(buffers_[trails], trails_, GL_STREAM_DRAW);
            trail_vertices_ = static_cast<int>(trails_.size());
            last_trail_index_ = index;
            last_trail_length_ = settings.trail_length;
        }
        bind_positions(buffers_[trails]);
        glUniform3f(line_color_, 0.37f, 0.51f, 0.70f);
        glDrawArrays(GL_LINES, 0, trail_vertices_);
    }

    if (last_index_ != index) {
        upload(buffers_[particles], positions, GL_STREAM_DRAW);
        last_index_ = index;
    }
    bind_positions(buffers_[particles]);
    glUniform1i(points_, GL_TRUE);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions.size()));
}

} // namespace viewer
