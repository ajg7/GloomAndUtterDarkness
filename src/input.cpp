#include "input.h"

#include "raylib.h"

void InputLatch::pollFrame() {
    m_held.moveLeft  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
    m_held.moveRight = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    m_held.fireHeld  = IsKeyDown(KEY_SPACE);

    m_reloadQueued  = m_reloadQueued  || IsKeyPressed(KEY_R);
    m_restartQueued = m_restartQueued || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
}

Input InputLatch::takeStep() {
    Input step = m_held;
    step.reload = m_reloadQueued;
    step.restart = m_restartQueued;
    m_reloadQueued = false;
    m_restartQueued = false;
    return step;
}
