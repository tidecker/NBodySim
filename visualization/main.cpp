#include "camera.hpp"
#include "csv.hpp"
#include "playback.hpp"
#include "renderer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

int main(int argc, char* argv[])
{
    constexpr char usage[] = "Usage: nbody_viewer <trajectory.csv>\n";
    if (argc != 2) {
        std::cerr << usage;
        return 1;
    }
    if (std::string_view(argv[1]) == "--help") {
        std::cout << usage;
        return 0;
    }

    try {
        const auto frames = viewer::load_trajectory(argv[1]);
        viewer::Playback playback(frames);
        viewer::Camera camera(frames);
        viewer::Controls controls{playback, camera};

        glfwSetErrorCallback([](int, const char* message) {
            std::cerr << "GLFW: " << message << '\n';
        });
        if (!glfwInit()) {
            throw std::runtime_error("Could not initialize GLFW");
        }
        struct GlfwSession {
            ~GlfwSession() { glfwTerminate(); }
        } session;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window(
            glfwCreateWindow(960, 720, "NBodySim Viewer", nullptr, nullptr), glfwDestroyWindow);
        if (!window) {
            throw std::runtime_error("Could not create an OpenGL 3.3 core window");
        }
        glfwMakeContextCurrent(window.get());
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) ||
            !GLAD_GL_VERSION_3_3) {
            throw std::runtime_error("Could not load OpenGL 3.3 functions");
        }
        glfwSwapInterval(1);
        viewer::Renderer renderer;
        controls.bind(window.get());

        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window.get())) {
            const double now = glfwGetTime();
            playback.update(now - previous);
            previous = now;

            const auto index = playback.index();
            const auto& frame = frames[index];
            std::ostringstream title;
            title << "NBodySim | Frame " << index << '/' << frames.size() - 1
                  << " | t=" << std::fixed << std::setprecision(3) << frame.time
                  << " | " << std::defaultfloat << std::setprecision(4) << playback.speed()
                  << "x" << (playback.paused() ? " | Paused" : "");
            glfwSetWindowTitle(window.get(), title.str().c_str());

            int width, height;
            glfwGetFramebufferSize(window.get(), &width, &height);
            renderer.draw(frame, camera, width, height);
            glfwSwapBuffers(window.get());
            glfwPollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "nbody_viewer: " << error.what() << '\n';
        return 1;
    }
}
