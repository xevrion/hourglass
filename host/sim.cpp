// Desktop simulator. Runs the same sand, session, scene and gesture code as the
// board, so what you see here is what the panel will draw.
//
//   ./host/sim                    interactive window
//   ./host/sim --focus 40 --rest 20
//   ./host/sim --shots host/out   write PPM frames of a scripted run and exit

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gesture.hpp"
#include "menu.hpp"
#include "scene.hpp"
#include "session.hpp"

using namespace hg;

namespace {

constexpr float kStep = 1.0f / 60.0f;

struct Options {
    Durations durations;
    float speed = 1.0f;
    int scale = 2;
    std::string shots;
    std::string frames;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : "0"; };
        if (a == "--focus") o.durations.focus = float(std::atof(next()));
        else if (a == "--rest") o.durations.rest = float(std::atof(next()));
        else if (a == "--speed") o.speed = float(std::atof(next()));
        else if (a == "--scale") o.scale = std::atoi(next());
        else if (a == "--shots") o.shots = next();
        else if (a == "--frames") o.frames = next();
    }
    return o;
}

// The panel is round; black out the corners so the preview matches it.
void mask_corners(std::vector<uint16_t>& px) {
    const float c = kScreen / 2.0f - 0.5f, r = kScreen / 2.0f;
    for (int y = 0; y < kScreen; ++y) {
        for (int x = 0; x < kScreen; ++x) {
            const float dx = float(x) - c, dy = float(y) - c;
            if (dx * dx + dy * dy > r * r) px[y * kScreen + x] = 0;
        }
    }
}

struct Sim {
    Sand sand;
    Session session;
    Scene scene;
    Gesture gesture;
    Menu menu;
    std::vector<uint16_t> fb;
    Surface surface;
    Gravity gravity = Gravity::Down;
    int lateral = 0;
    int battery = 87;
    bool charging = false;
    float speed;

    explicit Sim(const Options& o)
        : session(sand, o.durations), fb(kScreen * kScreen, 0),
          surface(reinterpret_cast<uint8_t*>(fb.data()), kScreen, kScreen, kScreen * 2), speed(o.speed) {
        session.restart(gravity);
    }

    void apply(MenuAction a) {
        switch (a) {
        case MenuAction::Restart: session.restart(gravity); break;
        case MenuAction::ResetAll: session.reset_all(gravity); break;
        case MenuAction::Preset0: session.set_preset(0, 0, gravity); break;
        case MenuAction::Preset1: session.set_preset(1, 0, gravity); break;
        case MenuAction::Preset2: session.set_preset(2, 0, gravity); break;
        case MenuAction::Preset3: session.set_preset(3, 0, gravity); break;
        case MenuAction::Brightness: std::printf("brightness would change here\n"); break;
        default: break;
        }
        if (!menu.is_open()) scene.invalidate();
    }

    void advance(bool down, int x, int y) {
        const bool flipped = gravity == Gravity::Up;
        if (menu.is_open()) {
            apply(menu.update(down, x, y, 1.0f / 30.0f));
        } else {
            const Tap tap = gesture.update(down, x, y, 1.0f / 30.0f);
            if (tap == Tap::Short) session.toggle_pause();
            if (tap == Tap::Long) menu.open(flipped);
        }
        session.update(kStep * speed, gravity, lateral);
        session.update(kStep * speed, gravity, lateral);
        draw();
    }

    void draw() {
        View v{&sand, session.state(), session.phase(), gravity, session.remaining_seconds(gravity),
               session.progress(gravity), session.completed(), session.since_done(), battery, charging};
        if (menu.is_open()) {
            const bool focus = session.phase() == Phase::Focus;
            menu.draw(surface, focus ? rgb565(255, 176, 46) : rgb565(64, 224, 200));
        } else {
            scene.draw(surface, v);
        }
        Rect ignored;
        surface.take_dirty(ignored);
    }

    // Opens the menu and scrolls it to an item, the way a swipe would.
    void show_menu(int item) {
        menu.open(gravity == Gravity::Up);
        menu.update(false, 0, 0, 0.03f);
        const int y0 = kScreen / 2;
        if (item > 0) {
            menu.update(true, 233, y0, 0.03f);
            for (int i = 1; i <= 8; ++i) menu.update(true, 233, y0 - i * item * Menu::kItemH / 8, 0.03f);
            menu.update(false, 233, y0 - item * Menu::kItemH, 0.03f);
            for (int i = 0; i < 40; ++i) menu.update(false, 0, 0, 0.03f);
        }
        draw();
    }

    void write_ppm(const std::string& path) {
        std::vector<uint16_t> copy = fb;
        mask_corners(copy);
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return;
        std::fprintf(f, "P6\n%d %d\n255\n", kScreen, kScreen);
        for (uint16_t p : copy) {
            const unsigned char rgb[3] = {uint8_t((p >> 8) & 0xF8), uint8_t((p >> 3) & 0xFC), uint8_t((p << 3) & 0xF8)};
            std::fwrite(rgb, 1, 3, f);
        }
        std::fclose(f);
    }
};

int run_shots(const Options& o) {
    Sim sim(o);
    struct Beat {
        float at;
        const char* what;
    };
    // Rest, flip to start, run, finish, flip for the break, pause, flip back.
    const std::vector<Beat> beats = {
        {0.5f, "ready"}, {1.5f, "flip"},  {8.0f, "run"},     {20.0f, "run"},     {32.0f, "run"},
        {44.0f, "run"},  {56.0f, "done"}, {58.0f, "flipdown"}, {68.0f, "break"},  {76.0f, "pause"},
        {79.0f, "shot"}, {80.0f, "resume"}, {110.0f, "done"}, {111.0f, "menu0"}, {112.0f, "menu3"}, {113.0f, "menuclose"},
    };
    float t = 0.0f;
    int shot = 0;
    for (const Beat& b : beats) {
        while (t < b.at) {
            sim.advance(false, 0, 0);
            t += 2.0f * kStep * o.speed;
        }
        const std::string what = b.what;
        if (what == "flip") sim.gravity = Gravity::Up;
        if (what == "flipdown") sim.gravity = Gravity::Down;
        if (what == "pause") sim.session.toggle_pause();
        if (what == "resume") sim.session.toggle_pause();
        if (what == "menu0") sim.show_menu(0);
        else if (what == "menu3") sim.show_menu(3);
        else if (what == "menuclose") {
            sim.menu.close();
            sim.scene.invalidate();
            sim.advance(false, 0, 0);
        } else sim.advance(false, 0, 0);
        char name[256];
        std::snprintf(name, sizeof name, "%s/shot%02d_%s.ppm", o.shots.c_str(), shot++, b.what);
        sim.write_ppm(name);
        std::printf("%-26s t=%6.1f state=%d phase=%d\n", name, t, int(sim.session.state()), int(sim.session.phase()));
    }
    return 0;
}

// One focus phase from rest to DONE, a frame every few steps, for an animated preview.
int run_frames(const Options& o) {
    Sim sim(o);
    int n = 0, tick = 0;
    auto grab = [&]() {
        char name[256];
        std::snprintf(name, sizeof name, "%s/f%04d.ppm", o.frames.c_str(), n++);
        sim.write_ppm(name);
    };
    for (int i = 0; i < 20; ++i) sim.advance(false, 0, 0), grab();
    sim.gravity = Gravity::Up;
    int guard = 0;
    while (sim.session.state() != State::Done && ++guard < 20000) {
        sim.advance(false, 0, 0);
        if (++tick % 3 == 0) grab();
    }
    for (int i = 0; i < 40; ++i) sim.advance(false, 0, 0), grab();
    std::printf("%d frames\n", n);
    return 0;
}

int run_window(const Options& o) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    const int size = kScreen * o.scale;
    SDL_Window* win = SDL_CreateWindow("hourglass", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size, size, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kScreen, kScreen);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);

    Sim sim(o);
    bool run = true, down = false;
    int mx = 0, my = 0;
    std::printf("F flip   Left/Right tilt   S shake   hold mouse: touch (tap pauses, hold restarts)   Esc quit\n");

    Uint32 last = SDL_GetTicks();
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) run = false;
            if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) down = e.type == SDL_MOUSEBUTTONDOWN;
            if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) mx = e.button.x / o.scale, my = e.button.y / o.scale;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: run = false; break;
                case SDLK_f: sim.gravity = sim.gravity == Gravity::Down ? Gravity::Up : Gravity::Down; break;
                case SDLK_LEFT: sim.lateral = sim.lateral == -1 ? 0 : -1; break;
                case SDLK_RIGHT: sim.lateral = sim.lateral == 1 ? 0 : 1; break;
                case SDLK_s: sim.sand.stir(60); break;
                case SDLK_c: sim.charging = !sim.charging; break;
                case SDLK_b: sim.battery = sim.battery > 60 ? 50 : sim.battery > 20 ? 12 : 100; break;
                default: break;
                }
            }
        }
        sim.advance(down, mx, my);

        void* pixels;
        int pitch;
        SDL_LockTexture(tex, nullptr, &pixels, &pitch);
        std::vector<uint16_t> copy = sim.fb;
        mask_corners(copy);
        for (int y = 0; y < kScreen; ++y) std::memcpy(static_cast<uint8_t*>(pixels) + y * pitch, &copy[y * kScreen], kScreen * 2);
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);

        const Uint32 now = SDL_GetTicks();
        if (now - last < 33) SDL_Delay(33 - (now - last));
        last = SDL_GetTicks();
    }
    SDL_Quit();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const Options o = parse(argc, argv);
    if (!o.frames.empty()) return run_frames(o);
    return o.shots.empty() ? run_window(o) : run_shots(o);
}
