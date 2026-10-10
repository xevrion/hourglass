#include <cstdio>
#include <cstring>
#include <vector>

#include "scene.hpp"

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

static View view_of(const Sand& sand, const Session& s, Gravity g) {
    return {&sand, s.state(), s.phase(), g, s.remaining_seconds(g), s.progress(g), s.completed(), s.since_done()};
}

static int differing_pixels(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b, int* first) {
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == b[i]) continue;
        if (!n) *first = int(i);
        ++n;
    }
    return n;
}

// The incremental painter must always end up with the same picture as painting
// from scratch. A stale pixel here is exactly the "line left on screen" bug.
static void test_incremental_matches_fresh() {
    Sand sand;
    Session session(sand, {10.0f, 4.0f});
    session.restart(Gravity::Down);

    std::vector<uint16_t> live(kScreen * kScreen, 0), fresh(kScreen * kScreen, 0);
    Surface live_sf(reinterpret_cast<uint8_t*>(live.data()), kScreen, kScreen, kScreen * 2);
    Scene scene;

    Gravity g = Gravity::Down;
    int compared = 0;
    for (int frame = 0; frame < 1500; ++frame) {
        // Script: rest, flip, pause for a moment, resume, run to the end, flip for the break.
        if (frame == 30) g = Gravity::Up;
        if (frame == 200) session.toggle_pause();
        if (frame == 260) session.toggle_pause();
        if (frame == 800 || frame == 1100) g = g == Gravity::Up ? Gravity::Down : Gravity::Up;
        const int lateral = (frame / 150) % 3 - 1;
        session.update(kDt, g, lateral);
        session.update(kDt, g, lateral);

        const View v = view_of(sand, session, g);
        scene.draw(live_sf, v);

        Rect r;
        if (live_sf.take_dirty(r)) {
            CHECK((r.x0 & 1) == 0 && (r.y0 & 1) == 0, "dirty origin %d,%d is not even", r.x0, r.y0);
            CHECK((r.x1 & 1) == 1 && (r.y1 & 1) == 1, "dirty end %d,%d is not odd", r.x1, r.y1);
            CHECK(r.x0 >= 0 && r.y0 >= 0 && r.x1 < kScreen && r.y1 < kScreen, "dirty box out of bounds");
        }

        if (frame % 20 == 0) {
            Surface fresh_sf(reinterpret_cast<uint8_t*>(fresh.data()), kScreen, kScreen, kScreen * 2);
            Scene full;
            full.draw(fresh_sf, v);
            int first = 0;
            const int bad = differing_pixels(live, fresh, &first);
            CHECK(bad == 0, "frame %d: %d pixels differ from a fresh paint, first at %d,%d", frame, bad, first % kScreen, first / kScreen);
            ++compared;
        }
    }
    CHECK(compared > 50, "compared only %d frames", compared);
}

static void test_surface_dirty_alignment() {
    std::vector<uint16_t> buf(100 * 100, 0);
    Surface sf(reinterpret_cast<uint8_t*>(buf.data()), 100, 100, 200);
    Rect r;
    CHECK(!sf.take_dirty(r), "fresh surface is clean");
    sf.set(5, 7, 1);
    CHECK(sf.take_dirty(r) && r.x0 == 4 && r.y0 == 6 && r.x1 == 5 && r.y1 == 7, "single pixel widens to %d,%d..%d,%d", r.x0, r.y0, r.x1, r.y1);
    CHECK(!sf.take_dirty(r), "taking clears the box");
    sf.set(99, 99, 1);
    CHECK(sf.take_dirty(r) && r.x1 == 99 && r.y1 == 99, "edge pixel stays inside the surface");
    sf.set(-3, 4, 1);
    CHECK(!sf.take_dirty(r), "off-surface writes are ignored");
}

int main() {
    test_surface_dirty_alignment();
    test_incremental_matches_fresh();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all scene checks passed\n");
    return 0;
}
