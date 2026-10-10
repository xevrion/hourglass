# hourglass

A pomodoro timer that is an hourglass. It runs on the Waveshare ESP32-S3-Touch-AMOLED-1.75C, a round 466x466 AMOLED with a tilt sensor, and the sand really falls: flip the board and the sand in the new top chamber starts running through the neck, so the time left is the sand left.

## Using it

Hold it upright with the USB-C cable at the bottom. The sand rests low and the clock shows the length of the phase, 25:00 for a focus session.

- **Flip it** to start. The clock counts down while the sand runs, and the rim fills one tick at a time. The text turns around so it still reads upright.
- **When the last grain drops** the rim pulses and it says DONE. Flip it again to start the break (amber is focus, teal is break). Four dots under the label count finished focus sessions.
- **Tap** to pause and resume.
- **Long-press** to open the menu. Swipe up and down to scroll the wheel, tap the highlighted row to choose, tap a neighbouring row to step to it. It has Resume, Restart (this phase), Reset all (back to a fresh focus phase with no sessions counted), the presets 25/5, 50/10, 15/3 and a 45 second demo, and Brightness. An untouched menu closes itself after 8 seconds.
- **Shake** it to stir the sand. **Tilt** it sideways and the stream leans that way.

It works without a laptop. The battery percentage sits under the session dots (green with a plus while charging, red when low) and stays hidden if no battery is connected. The chosen preset, the finished sessions and the brightness are saved, so they are still there after a power cycle, and the screen dims after 30 seconds of being left alone.

## How the sand works

The glass is a 137x137 grid of cells inside the circle of the screen, two cones joined at a neck three cells wide. Every cell holds a grain or nothing, and each step a grain tries to move one cell along gravity, then diagonally to either side. That is the whole engine.

The pacing is a token bucket on the neck row. A grain may only enter the neck when a token is available, and tokens arrive once per `interval` seconds, so a phase of `T` seconds with `N` grains uses an interval of `T / N`. Gravity is just a direction, so flipping the board mid-run works the way a real hourglass does: whatever already fell is now the sand that is running. The neck passes about two grains a step, which puts the shortest possible phase at around 15 seconds.

Everything under `core/` (sand, session, scene, font, gestures) has no dependency on the board. The board build and the desktop simulator both run that same code, and the scene is painted incrementally, so a test can check that it always matches a from-scratch paint.

## Simulator

    make sim

Needs SDL2. F flips the glass, the arrow keys tilt it, S shakes it, and the mouse is the touchscreen. `./host/sim --focus 40 --rest 20` runs short phases, and `--shots dir` writes frames of a scripted run as PPM files.

## Tests

    make test

Needs a C++17 compiler. The tests cover the shape of the glass, that grains are conserved and cannot slip through the neck faster than the clock allows, that a phase takes the time it should, the session states, the gestures, and that the incremental painter matches a fresh one pixel for pixel while keeping its dirty box even-aligned for the panel.

## Building for the board

No toolchain needed on the host, only Docker. `scripts/idf` runs `idf.py` in Espressif's image.

    make build
    make flash PORT=/dev/ttyACM0

If the port refuses to open, check you can write to it (`dialout` group, or a udev rule). On Fedora the script also drops the container's SELinux label for the flash step, which is the only way the container can open the tty.

## License

MIT
