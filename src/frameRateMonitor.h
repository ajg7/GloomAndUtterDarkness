#pragma once

#include <cstdint>

#include "raylib.h"

class FrameRateMonitor {
public:
    // Screen coordinate pair for UI positioning
    struct Position {
        int x;
        int y;
    };

    // Typography options for frame rate rendering
    struct FontOptions {
        int fontSize;
        Color color;
        int lineSpacing;
    };

    // Fixed-timestep target (60 Hz / ~16.6ms per tick)
    static constexpr double SIM_HZ{60.0};
    static constexpr double SIM_DT{1.0 / SIM_HZ};

    // Ceiling on how much simulation one frame may be asked to catch up on.
    // Without it a single long stall (a breakpoint, a window drag) hands back a
    // huge step count, that frame takes even longer, and the accumulator never
    // drains again.
    static constexpr int MAX_STEPS_PER_FRAME{8};

    // Accumulates delta time and returns how many fixed-size simulation steps
    // the caller should run this frame (0, 1, or several, depending on framerate).
    int update(double deltaTime) noexcept;

    // Renders the current tick and frame stats to the screen
    void drawFrameRateText(
        Position position = {.x = 4, .y = 4},
        FontOptions font = {.fontSize = 10, .color = GREEN, .lineSpacing = 2}
    ) const;

private:
    double m_accumulator{0.0};       // Unprocessed time buffer awaiting the next sim tick
    std::uint64_t m_simTicks{0};     // Total fixed-step updates executed
    std::uint64_t m_renderFrames{0}; // Total render frames executed
};
