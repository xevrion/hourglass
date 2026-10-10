#include <cstdio>
#include <string>

#include "sand.hpp"

using namespace hg;

static void write_ppm(const Sand& s, const std::string& path) {
    static const unsigned char sand[4][3] = {{255, 176, 46}, {244, 160, 38}, {255, 192, 72}, {232, 146, 30}};
    constexpr int cell = 3;
    constexpr int size = kGrid * cell;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", size, size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int cx = x / cell, cy = y / cell;
            unsigned char px[3] = {0, 0, 0};
            if (s.shade(cx, cy)) {
                const unsigned char* c = sand[s.shade(cx, cy) - 1];
                px[0] = c[0], px[1] = c[1], px[2] = c[2];
            } else if (s.inside(cx, cy)) {
                const bool edge = !s.inside(cx - 1, cy) || !s.inside(cx + 1, cy) || !s.inside(cx, cy - 1) || !s.inside(cx, cy + 1);
                if (edge) px[0] = px[1] = px[2] = 70;
            }
            std::fwrite(px, 1, 3, f);
        }
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    Sand s;
    s.set_interval(25.0f * 60.0f / float(kGrains));
    s.fill(Gravity::Down);
    const float dt = 1.0f / 60.0f;
    const float marks[] = {0.0f, 10.0f, 300.0f, 750.0f, 1200.0f, 1500.0f};
    int shot = 0;
    float t = 0.0f;
    for (float mark : marks) {
        while (t < mark) {
            s.step(dt, Gravity::Up);
            t += dt;
        }
        char name[64];
        std::snprintf(name, sizeof name, "/frame%d.ppm", shot++);
        write_ppm(s, dir + name);
        std::printf("t=%6.0fs  upstream=%4d  remaining=%.0fs\n", t, s.upstream(Gravity::Up), s.remaining(Gravity::Up));
    }
    return 0;
}
