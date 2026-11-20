#include "renderer.hpp"

#include "camera.hpp"

#include <glad/glad.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace viewer {
namespace {

constexpr char vertex_source[] = R"(
#version 330 core
layout(location = 0) in vec2 position;
flat out int particle_id;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    particle_id = gl_VertexID;
}
)";

constexpr char fragment_source[] = R"(
#version 330 core
flat in int particle_id;
out vec4 pixel;
void main() {
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

} // namespace

Renderer::Renderer() : program_(create_program())
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glClearColor(0.035f, 0.05f, 0.08f, 1.0f);
    glPointSize(8.0f);
}

Renderer::~Renderer()
{
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
}

void Renderer::draw(const Frame& frame, const Camera& camera, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    if (frame.positions.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::runtime_error("Too many particles to render in one frame");
    }
    vertices_.clear();
    vertices_.reserve(2 * frame.positions.size());
    const double aspect = static_cast<double>(width) / height;
    for (const auto& position : frame.positions) {
        const auto projected = camera.project(position, aspect);
        vertices_.push_back(projected[0]);
        vertices_.push_back(projected[1]);
    }

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(float), vertices_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(frame.positions.size()));
}

} // namespace viewer
