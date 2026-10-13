#include "frameRateMonitor.h"

#include <format>

int FrameRateMonitor::update(const double deltaTime) noexcept {
    m_accumulator += deltaTime;
    int steps = 0;
    while (m_accumulator >= SIM_DT && steps < MAX_STEPS_PER_FRAME) {
        ++steps;
        ++m_simTicks;
        m_accumulator -= SIM_DT;
    }

    // Anything still owed after the ceiling is dropped, not banked - the
    // simulation runs slow for one frame rather than spiralling.
    if (m_accumulator >= SIM_DT) m_accumulator = 0.0;

    ++m_renderFrames;
    return steps;
}

void FrameRateMonitor::drawFrameRateText(const Position position, const FontOptions font) const {
    const auto simTicksText = std::format("sim ticks: {}", m_simTicks);
    const auto renderFramesText = std::format("render frames: {}", m_renderFrames);

    DrawText(simTicksText.c_str(), position.x, position.y, font.fontSize, font.color);
    DrawText(renderFramesText.c_str(), position.x, position.y + font.fontSize + font.lineSpacing, font.fontSize, font.color);
}
