#pragma once

#include "sand.hpp"

namespace hg {

enum class Phase : uint8_t { Focus, Break };
enum class State : uint8_t { Ready, Running, Paused, Done };

struct Durations {
    float focus = 25.0f * 60.0f;
    float rest = 5.0f * 60.0f;
};

// Long-press in the ready state steps through these. The last one is short enough
// to watch a whole cycle without waiting; the neck can't pass a phase faster than ~15 s.
constexpr Durations kPresets[] = {{25.0f * 60.0f, 5.0f * 60.0f}, {50.0f * 60.0f, 10.0f * 60.0f}, {15.0f * 60.0f, 3.0f * 60.0f}, {45.0f, 20.0f}};
constexpr int kPresetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));

// Drives a Sand through focus and break phases. Flipping the glass starts a phase,
// the last grain through the neck ends it, and the next flip starts the other one.
class Session {
public:
    explicit Session(Sand& sand, Durations d = {});

    void update(float dt, Gravity g, int lateral);
    void toggle_pause();
    void restart(Gravity g);
    void cycle_preset(Gravity g);
    // Back to a fresh focus phase with no finished sessions counted.
    void reset_all(Gravity g);

    State state() const { return state_; }
    Phase phase() const { return phase_; }
    int completed() const { return completed_; }
    int preset() const { return preset_; }
    // For restoring saved settings at boot.
    void set_preset(int index, int completed, Gravity g);
    float since_done() const { return since_done_; }
    float total_seconds() const { return phase_ == Phase::Focus ? dur_.focus : dur_.rest; }
    float remaining_seconds(Gravity g) const;
    float progress(Gravity g) const;

private:
    void arm(Gravity g);

    Sand& sand_;
    Durations dur_;
    Phase phase_ = Phase::Focus;
    State state_ = State::Ready;
    int completed_ = 0;
    int preset_ = 0;
    float since_done_ = 0.0f;
    bool paused_ = false;
};

}  // namespace hg
