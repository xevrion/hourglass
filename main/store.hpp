#pragma once

// What survives a power cycle: the chosen preset, finished focus sessions and brightness.
struct Saved {
    int preset = 0;
    int completed = 0;
    int brightness = 1;
};

Saved store_load();
void store_save(const Saved& saved);
