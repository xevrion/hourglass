#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "sand.hpp"

using namespace hg;

static int failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
            ++failures;                                   \
        }                                                 \
    } while (0)

constexpr float kDt = 1.0f / 60.0f;

static int run_until_empty(Sand& s, Gravity g, float limit_seconds) {
    int steps = 0;
    const int max_steps = int(limit_seconds / kDt);
    while (s.upstream(g) > 0 && steps < max_steps) {
        s.step(kDt, g);
        ++steps;
    }
    return steps;
}

static void test_shape() {
    Sand s;
    CHECK(s.inside(kCenter, kCenter), "neck centre must be open");
    CHECK(s.inside(kCenter - 1, kCenter) && s.inside(kCenter + 1, kCenter), "neck is three cells wide");
    CHECK(!s.inside(kCenter - 2, kCenter), "neck must be closed beyond three cells");
    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            if (!s.inside(x, y)) continue;
            const float dx = float(x - kCenter), dy = float(y - kCenter);
            CHECK(std::sqrt(dx * dx + dy * dy) <= float(kRadius), "cell %d,%d outside the circle", x, y);
            CHECK(s.inside(2 * kCenter - x, y) && s.inside(x, 2 * kCenter - y), "shape not symmetric at %d,%d", x, y);
        }
    }
}

static void test_fill() {
    for (Gravity g : {Gravity::Down, Gravity::Up}) {
        Sand s;
        s.fill(g);
        CHECK(s.total() == kGrains, "fill placed %d grains", s.total());
        CHECK(s.upstream(g) == 0, "nothing starts upstream, got %d", s.upstream(g));
    }
}

static void test_rests_without_flip() {
    Sand s;
    s.set_interval(0.05f);
    s.fill(Gravity::Down);
    for (int i = 0; i < 1200; ++i) s.step(kDt, Gravity::Down);
    CHECK(s.upstream(Gravity::Down) == 0, "settled sand leaked through the neck");
    CHECK(s.total() == kGrains, "grains lost: %d", s.total());
}

static void test_pace_and_conservation() {
    Sand s;
    const float interval = 0.05f;
    s.set_interval(interval);
    s.fill(Gravity::Down);
    // Flip: gravity reverses, the filled chamber is now upstream.
    const Gravity g = Gravity::Up;
    CHECK(s.upstream(g) == kGrains, "flipped hourglass should have everything upstream");

    for (int i = 0; i < 600; ++i) {
        s.step(kDt, g);
        CHECK(s.total() == kGrains, "grains not conserved at step %d: %d", i, s.total());
    }
    const int passed = kGrains - s.upstream(g);
    const float expected = 10.0f / interval;
    CHECK(std::fabs(float(passed) - expected) <= expected * 0.06f, "passed %d grains in 10 s, expected about %.0f", passed, expected);
}

static void test_total_time() {
    Sand s;
    const float interval = 0.02f;
    s.set_interval(interval);
    s.fill(Gravity::Down);
    const Gravity g = Gravity::Up;
    const float ideal = float(kGrains) * interval;
    const int steps = run_until_empty(s, g, ideal * 2.0f);
    const float took = float(steps) * kDt;
    CHECK(s.upstream(g) == 0, "never emptied, %d grains left", s.upstream(g));
    CHECK(took >= ideal * 0.97f && took <= ideal * 1.05f, "took %.1f s, ideal %.1f s", took, ideal);
    CHECK(s.total() == kGrains, "grains lost: %d", s.total());
}

static void test_reflip_mid_run() {
    Sand s;
    s.set_interval(0.02f);
    s.fill(Gravity::Down);
    for (int i = 0; i < 1500; ++i) s.step(kDt, Gravity::Up);
    const int downstream = kGrains - s.upstream(Gravity::Up);
    // Flipping back makes the sand that already fell the new upstream.
    CHECK(s.upstream(Gravity::Down) >= downstream - 12, "flip lost sand: %d of %d upstream", s.upstream(Gravity::Down), downstream);
    for (int i = 0; i < 600; ++i) {
        s.step(kDt, Gravity::Down);
        CHECK(s.total() == kGrains, "grains not conserved after reflip: %d", s.total());
    }
}

static void test_interval_scales_rate() {
    int slow = 0, fast = 0;
    for (float interval : {0.10f, 0.02f}) {
        Sand s;
        s.set_interval(interval);
        s.fill(Gravity::Down);
        for (int i = 0; i < 600; ++i) s.step(kDt, Gravity::Up);
        (interval > 0.05f ? slow : fast) = kGrains - s.upstream(Gravity::Up);
    }
    CHECK(fast > slow * 4, "5x shorter interval should be about 5x faster: %d vs %d", fast, slow);
}

static void test_edges() {
    Sand s;
    s.set_interval(0.05f);
    s.fill(Gravity::Down);
    s.step(0.0f, Gravity::Up);
    s.step(100.0f, Gravity::Up);
    int in_neck = 0;
    for (int x = 0; x < kGrid; ++x) in_neck += s.shade(x, kCenter) != 0;
    CHECK(in_neck <= 2, "a huge dt must not bank more than two tokens, %d in neck row", in_neck);
    CHECK(s.total() == kGrains, "grains lost on edge steps");
}

int main() {
    test_shape();
    test_fill();
    test_rests_without_flip();
    test_pace_and_conservation();
    test_total_time();
    test_reflip_mid_run();
    test_interval_scales_rate();
    test_edges();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all sand checks passed\n");
    return 0;
}
