#include "session.hpp"

namespace hg {

Session::Session(Sand& sand, Durations d) : sand_(sand), dur_(d) {
    sand_.set_interval(total_seconds() / float(kGrains));
}

void Session::arm(Gravity g) {
    sand_.set_interval(total_seconds() / float(kGrains));
    sand_.fill(g);
}

void Session::restart(Gravity g) {
    paused_ = false;
    state_ = State::Ready;
    arm(g);
}

void Session::cycle_preset(Gravity g) {
    preset_ = (preset_ + 1) % kPresetCount;
    dur_ = kPresets[preset_];
    reset_all(g);
}

void Session::set_preset(int index, int completed, Gravity g) {
    preset_ = (index % kPresetCount + kPresetCount) % kPresetCount;
    dur_ = kPresets[preset_];
    phase_ = Phase::Focus;
    restart(g);
    completed_ = completed < 0 ? 0 : completed;
}

void Session::reset_all(Gravity g) {
    phase_ = Phase::Focus;
    completed_ = 0;
    restart(g);
}

void Session::toggle_pause() {
    if (state_ == State::Running) {
        paused_ = true;
        state_ = State::Paused;
    } else if (state_ == State::Paused) {
        paused_ = false;
        state_ = State::Running;
    }
}

void Session::update(float dt, Gravity g, int lateral) {
    sand_.step(paused_ ? 0.0f : dt, g, lateral);

    const int up = sand_.upstream(g);
    switch (state_) {
    case State::Ready:
        if (up > 0) state_ = State::Running;
        break;
    case State::Running:
        if (up == 0) {
            if (phase_ == Phase::Focus) ++completed_;
            phase_ = phase_ == Phase::Focus ? Phase::Break : Phase::Focus;
            sand_.set_interval(total_seconds() / float(kGrains));
            since_done_ = 0.0f;
            state_ = State::Done;
        }
        break;
    case State::Paused:
        break;
    case State::Done:
        since_done_ += dt;
        if (up > 0) state_ = State::Running;
        break;
    }
}

float Session::remaining_seconds(Gravity g) const {
    if (state_ == State::Ready || state_ == State::Done) return total_seconds();
    return sand_.remaining(g);
}

float Session::progress(Gravity g) const {
    if (state_ == State::Ready || state_ == State::Done) return 0.0f;
    return 1.0f - float(sand_.upstream(g)) / float(kGrains);
}

}  // namespace hg
