#pragma once

#include "csv.hpp"

namespace viewer {

class Playback {
public:
    explicit Playback(const std::vector<Frame>& frames) : frames_(frames) {}
    void update(double elapsed);
    void restart();
    void toggle_pause() { paused_ = !paused_; }
    void change_speed(double factor);
    bool paused() const { return paused_; }
    double speed() const { return speed_; }
    std::size_t index() const { return index_; }

private:
    const std::vector<Frame>& frames_;
    double time_ = 0;
    double speed_ = 1;
    std::size_t index_ = 0;
    bool paused_ = false;
};

} // namespace viewer
