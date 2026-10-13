#pragma once

// Input as the simulation sees it: held states plus one-shot presses.
struct Input {
    bool moveLeft{false};
    bool moveRight{false};
    bool fireHeld{false};
    bool reload{false};
    bool restart{false};
};

// Bridges frame-rate input polling to the fixed-step simulation.
//
// Held keys can be read fresh each frame, but presses cannot: simulation steps
// and render frames are decoupled, so at 300 fps most frames run zero steps and
// a press read on such a frame would be discarded unseen, while at 20 fps a
// frame runs three steps and a press handed to all three would fire one
// keystroke three times. Presses are therefore latched here until a step
// actually consumes them.
class InputLatch {
public:
    // Once per render frame.
    void pollFrame();

    // Once per simulation step. The first step of a frame sees the queued
    // presses; the rest of that frame's steps do not.
    Input takeStep();

private:
    Input m_held{};
    bool m_reloadQueued{false};
    bool m_restartQueued{false};
};
