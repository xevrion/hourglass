#include <cstdio>
#include <vector>

#include "menu.hpp"
#include "sand.hpp"

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

constexpr float kDt = 1.0f / 30.0f;
constexpr int kMid = kScreen / 2;

// A press that starts at y and lifts again.
static MenuAction tap(Menu& m, int y) {
    m.update(true, 233, y, kDt);
    return m.update(false, 233, y, kDt);
}

static void settle(Menu& m) {
    for (int i = 0; i < 60; ++i) m.update(false, 0, 0, kDt);
}

static void open_ready(Menu& m, bool flipped = false) {
    m.open(flipped);
    m.update(false, 0, 0, kDt);
}

static void test_opening_press_is_ignored() {
    Menu m;
    m.open(false);
    CHECK(m.update(true, 233, kMid, kDt) == MenuAction::None, "the press that opened the menu must not select");
    CHECK(m.update(false, 233, kMid, kDt) == MenuAction::None, "releasing it must not select either");
    CHECK(m.is_open(), "still open");
}

static void test_tap_centre_activates() {
    Menu m;
    open_ready(m);
    CHECK(m.selected() == 0, "starts on the first item");
    CHECK(tap(m, kMid) == MenuAction::Resume, "centre tap on the first item resumes");
    CHECK(!m.is_open(), "choosing closes the menu");
}

static void test_swipe_then_choose() {
    const MenuAction expected[] = {MenuAction::Resume, MenuAction::Restart, MenuAction::ResetAll, MenuAction::Preset0,
                                   MenuAction::Preset1, MenuAction::Preset2, MenuAction::Preset3};
    for (int item = 0; item < 7; ++item) {
        Menu m;
        open_ready(m);
        if (item > 0) {
            m.update(true, 233, kMid, kDt);
            for (int i = 1; i <= 6; ++i) m.update(true, 233, kMid - i * item * Menu::kItemH / 6, kDt);
            m.update(false, 233, kMid - item * Menu::kItemH, kDt);
            settle(m);
        }
        CHECK(m.selected() == item, "swiping %d items up landed on %d", item, m.selected());
        CHECK(tap(m, kMid) == expected[item], "item %d did the wrong thing", item);
    }
}

static void test_flipped_swipe_is_mirrored() {
    Menu m;
    open_ready(m, true);
    m.update(true, 233, kMid, kDt);
    for (int i = 1; i <= 6; ++i) m.update(true, 233, kMid + i * 2 * Menu::kItemH / 6, kDt);
    m.update(false, 233, kMid + 2 * Menu::kItemH, kDt);
    settle(m);
    CHECK(m.selected() == 2, "a downward swipe on a flipped board should advance, got %d", m.selected());
}

static void test_tap_neighbour_steps() {
    Menu m;
    open_ready(m);
    CHECK(tap(m, kMid + Menu::kItemH) == MenuAction::None, "tapping the row below only steps to it");
    settle(m);
    CHECK(m.selected() == 1, "stepped to the next item, got %d", m.selected());
    CHECK(tap(m, kMid - Menu::kItemH) == MenuAction::None, "tapping the row above only steps back");
    settle(m);
    CHECK(m.selected() == 0, "stepped back, got %d", m.selected());
}

static void test_clamped_at_the_ends() {
    Menu m;
    open_ready(m);
    m.update(true, 233, kMid, kDt);
    for (int i = 1; i <= 10; ++i) m.update(true, 233, kMid + i * 40, kDt);
    m.update(false, 233, kMid + 400, kDt);
    settle(m);
    CHECK(m.selected() == 0, "scrolling past the start stays on the first item");

    m.update(true, 233, kMid + 400, kDt);
    for (int i = 1; i <= 30; ++i) m.update(true, 233, kMid + 400 - i * 40, kDt);
    m.update(false, 233, kMid - 800, kDt);
    settle(m);
    CHECK(m.selected() == Menu::kItems - 1, "scrolling past the end stays on the last item, got %d", m.selected());
}

static void test_brightness_keeps_menu_open() {
    Menu m;
    open_ready(m);
    m.update(true, 233, kMid, kDt);
    for (int i = 1; i <= 8; ++i) m.update(true, 233, kMid - i * 7 * Menu::kItemH / 8, kDt);
    m.update(false, 233, kMid - 7 * Menu::kItemH, kDt);
    settle(m);
    CHECK(tap(m, kMid) == MenuAction::Brightness, "last item is brightness");
    CHECK(m.is_open(), "brightness can be pressed repeatedly, so the menu stays open");
}

static void test_idle_close() {
    Menu m;
    open_ready(m);
    MenuAction last = MenuAction::None;
    for (int i = 0; i < int(Menu::kIdleClose / kDt) + 5 && m.is_open(); ++i) last = m.update(false, 0, 0, kDt);
    CHECK(!m.is_open() && last == MenuAction::Resume, "an untouched menu closes itself");
}

static void test_draw_stays_on_screen_and_is_deterministic() {
    std::vector<uint16_t> a(kScreen * kScreen, 1), b(kScreen * kScreen, 1);
    Surface sa(reinterpret_cast<uint8_t*>(a.data()), kScreen, kScreen, kScreen * 2);
    Surface sb(reinterpret_cast<uint8_t*>(b.data()), kScreen, kScreen, kScreen * 2);
    Menu m1, m2;
    open_ready(m1);
    open_ready(m2);
    // One menu scrolls in many small steps, the other jumps; both end on item 3.
    m1.update(true, 233, kMid, kDt);
    for (int i = 1; i <= 12; ++i) {
        m1.update(true, 233, kMid - i * 3 * Menu::kItemH / 12, kDt);
        m1.draw(sa, rgb565(255, 176, 46));
    }
    m1.update(false, 233, kMid - 3 * Menu::kItemH, kDt);
    for (int i = 0; i < 60; ++i) {
        m1.update(false, 0, 0, kDt);
        m1.draw(sa, rgb565(255, 176, 46));
    }
    m2.update(true, 233, kMid, kDt);
    m2.update(true, 233, kMid - 3 * Menu::kItemH, kDt);
    m2.update(false, 233, kMid - 3 * Menu::kItemH, kDt);
    for (int i = 0; i < 60; ++i) m2.update(false, 0, 0, kDt);
    m2.draw(sb, rgb565(255, 176, 46));
    CHECK(m1.selected() == 3 && m2.selected() == 3, "both reached item 3");
    int bad = 0;
    for (size_t i = 0; i < a.size(); ++i) bad += a[i] != b[i];
    CHECK(bad == 0, "scrolling left %d stale pixels compared with a direct draw", bad);
}

int main() {
    test_opening_press_is_ignored();
    test_tap_centre_activates();
    test_swipe_then_choose();
    test_flipped_swipe_is_mirrored();
    test_tap_neighbour_steps();
    test_clamped_at_the_ends();
    test_brightness_keeps_menu_open();
    test_idle_close();
    test_draw_stays_on_screen_and_is_deterministic();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all menu checks passed\n");
    return 0;
}
