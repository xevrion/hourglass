#include <cstdio>

#include "gesture.hpp"
#include "session.hpp"

using namespace hg;

static int failures = 0;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (!(cond)) {                                       \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                        \
            std::printf("\n");                               \
            ++failures;                                      \
        }                                                    \
    } while (0)

constexpr float kDt = 1.0f / 60.0f;

// The neck passes about two grains a step, so a phase can't be shorter than roughly 15 s.
static Durations fast() { return {40.0f, 20.0f}; }

static void run(Session& s, Gravity g, float seconds, int lateral = 0) {
    for (int i = 0; i < int(seconds / kDt); ++i) s.update(kDt, g, lateral);
}

static void test_ready_until_flipped() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    run(s, Gravity::Down, 3.0f);
    CHECK(s.state() == State::Ready, "should stay ready while the sand rests");
    CHECK(s.remaining_seconds(Gravity::Down) == 40.0f, "ready shows the full phase");
    run(s, Gravity::Up, 0.2f);
    CHECK(s.state() == State::Running, "flipping starts the phase");
}

static void test_full_cycle() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    run(s, Gravity::Up, 55.0f);
    CHECK(s.state() == State::Done, "focus phase should be done after 55 s of a 40 s phase");
    CHECK(s.phase() == Phase::Break, "next phase is the break");
    CHECK(s.completed() == 1, "one focus session counted, got %d", s.completed());
    CHECK(s.total_seconds() == 20.0f, "break length");
    CHECK(sand.total() == kGrains, "grains conserved over a phase: %d", sand.total());

    run(s, Gravity::Down, 0.3f);
    CHECK(s.state() == State::Running, "flipping again starts the break");
    run(s, Gravity::Down, 30.0f);
    CHECK(s.state() == State::Done && s.phase() == Phase::Focus, "break ends and focus is next");
    CHECK(s.completed() == 1, "a break does not count as a focus session");
}

static void test_timing_follows_phase_length() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    float t = 0;
    while (s.state() != State::Done && t < 90.0f) {
        s.update(kDt, Gravity::Up, 0);
        t += kDt;
    }
    CHECK(t > 39.0f && t < 44.0f, "40 s focus took %.1f s", t);
}

static void test_pause_holds_the_sand() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    run(s, Gravity::Up, 5.0f);
    s.toggle_pause();
    CHECK(s.state() == State::Paused, "paused");
    run(s, Gravity::Up, 1.0f);
    const int held = sand.upstream(Gravity::Up);
    run(s, Gravity::Up, 10.0f);
    CHECK(sand.upstream(Gravity::Up) == held, "sand moved through the neck while paused");
    s.toggle_pause();
    run(s, Gravity::Up, 2.0f);
    CHECK(sand.upstream(Gravity::Up) < held, "sand should run again after resuming");
}

static void test_remaining_and_progress() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    run(s, Gravity::Up, 20.0f);
    const float left = s.remaining_seconds(Gravity::Up);
    CHECK(left > 17.0f && left < 23.0f, "about half left after 20 of 40 s, got %.1f", left);
    CHECK(s.progress(Gravity::Up) > 0.4f && s.progress(Gravity::Up) < 0.6f, "progress %.2f", s.progress(Gravity::Up));
}

static void test_restart_resets() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    run(s, Gravity::Up, 6.0f);
    s.restart(Gravity::Up);
    CHECK(s.state() == State::Ready, "restart goes back to ready");
    CHECK(sand.upstream(Gravity::Up) == 0 && sand.total() == kGrains, "restart refills downstream");
}

static void test_presets() {
    Sand sand;
    Session s(sand, fast());
    s.restart(Gravity::Down);
    s.cycle_preset(Gravity::Down);
    CHECK(s.total_seconds() == 50.0f * 60.0f, "second preset is 50 minutes, got %.0f", s.total_seconds());
    CHECK(s.state() == State::Ready && sand.total() == kGrains, "a new preset starts from a full, resting glass");
    for (int i = 0; i < kPresetCount; ++i) s.cycle_preset(Gravity::Down);
    CHECK(s.total_seconds() == 50.0f * 60.0f, "presets wrap around after a full lap");
}

static void test_gesture() {
    Gesture g;
    CHECK(g.update(true, 100, 100, 0.1f) == Tap::None, "no event while pressing");
    CHECK(g.update(false, 100, 100, 0.0f) == Tap::Short, "quick release is a tap");

    g = Gesture();
    Tap t = Tap::None;
    for (int i = 0; i < 20 && t == Tap::None; ++i) t = g.update(true, 50, 50, 0.1f);
    CHECK(t == Tap::Long, "holding fires a long press");
    CHECK(g.update(false, 50, 50, 0.0f) == Tap::None, "releasing after a long press is not also a tap");

    g = Gesture();
    g.update(true, 10, 10, 0.1f);
    g.update(true, 200, 10, 0.05f);
    CHECK(g.update(false, 200, 10, 0.0f) == Tap::None, "a drag is not a tap");

    g = Gesture();
    g.update(true, 10, 10, 0.5f);
    CHECK(g.update(false, 10, 10, 0.0f) == Tap::None, "a slow press that is not long enough is not a tap either");
}

int main() {
    test_ready_until_flipped();
    test_full_cycle();
    test_timing_follows_phase_length();
    test_pause_holds_the_sand();
    test_remaining_and_progress();
    test_restart_resets();
    test_presets();
    test_gesture();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all session checks passed\n");
    return 0;
}
