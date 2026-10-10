# hourglass

A pomodoro timer that is an hourglass. It runs on the Waveshare ESP32-S3-Touch-AMOLED-1.75C, a round 466x466 AMOLED with a tilt sensor, and the sand really falls: flip the board and the sand in the new top chamber starts running through the neck, so the time left is the sand left.

Work in progress. The simulation and its tests run on a laptop; the board side is next.

## How the sand works

The glass is a 155x155 grid of cells inside the circle of the screen, two cones joined at a neck three cells wide. Every cell holds a grain or nothing, and each step a grain tries to move one cell along gravity, then diagonally to either side. That is the whole engine.

The pacing is a token bucket on the neck row. A grain may only enter the neck when a token is available, and tokens arrive once per `interval` seconds, so a session of `T` seconds with `N` grains uses an interval of `T / N`. Gravity is just a direction, so flipping the board mid-run works the way a real hourglass does: whatever already fell is now the sand that is running.

## Run the tests

    make test

Needs a C++17 compiler. The tests check the shape, that grains are conserved, that a settled pile does not leak, and that a full run takes the time it should (within a few percent). `make preview` writes a few frames of a 25 minute run as PPM files into `host/frames/`.

## License

MIT
