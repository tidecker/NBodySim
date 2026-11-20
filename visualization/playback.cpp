#include "playback.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace viewer {

void Playback::update(double elapsed)
{
    if (paused_ || frames_.size() < 2) {
        return;
    }
    // Hold the last frame until one more sample interval has elapsed.
    const double last = frames_.back().time;
    const double duration = last + (last - frames_[frames_.size() - 2].time);
    time_ = std::fmod(time_ + elapsed * speed_, duration);
    const auto next = std::upper_bound(frames_.begin(), frames_.end(), time_,
        [](double time, const Frame& frame) { return time < frame.time; });
    index_ = static_cast<std::size_t>(std::distance(frames_.begin(), next) - 1);
}

void Playback::restart()
{
    time_ = 0;
    index_ = 0;
}

void Playback::change_speed(double factor)
{
    speed_ = std::clamp(speed_ * factor, 1.0 / 16, 16.0);
}

} // namespace viewer
