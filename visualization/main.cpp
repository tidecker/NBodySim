#include "camera.hpp"
#include "csv.hpp"
#include "playback.hpp"
#include "renderer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

struct LoadingCancelled {};

class LoadingScreen {
public:
    LoadingScreen(GLFWwindow* window, std::string name)
        : window_(window), name_(std::move(name)) {}

    void update(double fraction, std::string_view stage, bool force = false)
    {
        const auto now = glfwGetTime();
        if (!force && now < next_update_) {
            return;
        }
        next_update_ = now + 0.033;
        glfwPollEvents();
        if (glfwWindowShouldClose(window_) || glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            throw LoadingCancelled{};
        }
        fraction = std::clamp(fraction, 0.0, 1.0);
        const auto title = name_ + " | " + std::string(stage) + " | " +
            std::to_string(static_cast<int>(100 * fraction)) + "%";
        glfwSetWindowTitle(window_, title.c_str());
        draw(fraction, false);
    }

    void show_error(const std::string& message)
    {
        const auto title = name_ + " | Loading failed | " + message;
        glfwSetWindowTitle(window_, title.c_str());
        const double until = glfwGetTime() + 4;
        while (!glfwWindowShouldClose(window_) && glfwGetTime() < until &&
               glfwGetKey(window_, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
            draw(1, true);
            glfwWaitEventsTimeout(0.05);
        }
    }

private:
    void draw(double fraction, bool error)
    {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        if (width <= 0 || height <= 0) {
            return;
        }
        glViewport(0, 0, width, height);
        glClearColor(0.035f, 0.05f, 0.08f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        const int bar_width = width * 3 / 5;
        const int bar_height = std::max(2, height / 36);
        const int x = (width - bar_width) / 2;
        const int y = (height - bar_height) / 2;
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, bar_width, bar_height);
        glClearColor(0.14f, 0.20f, 0.29f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(x, y, static_cast<int>(bar_width * fraction), bar_height);
        glClearColor(error ? 0.8f : 0.22f, error ? 0.22f : 0.65f, error ? 0.22f : 0.93f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.035f, 0.05f, 0.08f, 1);
        glfwSwapBuffers(window_);
    }

    GLFWwindow* window_;
    std::string name_;
    double next_update_ = 0;
};

} // namespace

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
        const std::filesystem::path trajectory = argv[1];
        auto name = trajectory.parent_path().filename().u8string();
        if (name.empty() || name == ".") {
            name = trajectory.stem().u8string();
        }

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
        LoadingScreen loading(window.get(), name);
        std::vector<viewer::Frame> frames;
        nbody::ViewSettings settings;
        std::optional<viewer::Camera> camera;
        std::unique_ptr<viewer::Renderer> renderer;
        try {
            loading.update(0, "Opening trajectory", true);
            frames = viewer::load_trajectory(trajectory, [&](double fraction) {
                loading.update(0.85 * fraction, "Reading trajectory");
            });
            loading.update(0.85, "Reading viewer settings", true);
            settings = nbody::load_view_settings(trajectory.parent_path());
            loading.update(0.87, "Fitting camera", true);
            camera.emplace(frames, [&](double fraction) {
                loading.update(0.87 + 0.10 * fraction, "Fitting camera");
            });
            loading.update(0.97, "Preparing renderer", true);
            renderer = std::make_unique<viewer::Renderer>(settings.sphere_quality, *camera);
            loading.update(1, "Ready", true);
        } catch (const LoadingCancelled&) {
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "nbody_viewer: " << trajectory << ": " << error.what() << '\n';
            loading.show_error(error.what());
            return 1;
        }
        viewer::Playback playback(frames);
        viewer::Controls controls{playback, *camera, settings};
        controls.bind(window.get());

        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window.get())) {
            const double now = glfwGetTime();
            playback.update(now - previous);
            previous = now;

            const auto index = playback.index();
            std::ostringstream title;
            title << name << " | Frame " << index << '/' << frames.size() - 1
                  << " | t=" << std::fixed << std::setprecision(3) << frames[index].time
                  << " | " << std::defaultfloat << std::setprecision(4) << playback.speed()
                  << "x" << (playback.paused() ? " | Paused" : "");
            glfwSetWindowTitle(window.get(), title.str().c_str());

            int width, height;
            glfwGetFramebufferSize(window.get(), &width, &height);
            renderer->draw(frames, index, *camera, settings, width, height);
            glfwSwapBuffers(window.get());
            glfwPollEvents();
        }
    } catch (const std::exception& error) {
        std::cerr << "nbody_viewer: " << error.what() << '\n';
        return 1;
    }
}
