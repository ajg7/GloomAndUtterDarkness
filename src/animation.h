#pragma once

#include "raylib.h"

// Immutable description of a sprite sheet: which texture, how it is sliced, how
// fast it runs. Shared by every entity that plays it — a hundred zombies of the
// same type point at one of these, they do not each carry a copy of the texture.
struct AnimClip {
    Texture2D texture{};
    int frameWidth{0};
    int frameHeight{0};
    int frameCount{1};
    double frameDuration{0.1};
    bool loops{true};
};

// Per-entity playback position within a clip. Small and trivially copyable so it
// can live inside a pooled entity.
struct AnimState {
    int frameIndex{0};
    double elapsed{0.0};
    bool finished{false};

    void reset() {
        frameIndex = 0;
        elapsed = 0.0;
        finished = false;
    }

    // Advances on simulation dt, never on frame time. Animation frames are the
    // clock that gameplay events (the muzzle flash, the zombie's swing landing)
    // are scheduled against, so they have to tick on the same clock as the
    // physics those events feed into.
    //
    // Takes the pacing directly rather than a clip, so an entity can advance
    // playback knowing only its stats row - no texture needed until draw time.
    void advance(const int frameCount, const double frameDuration, const bool loops, const double simDt) {
        if (finished || frameCount <= 1 || frameDuration <= 0.0) return;

        elapsed += simDt;
        while (elapsed >= frameDuration) {
            elapsed -= frameDuration;

            if (frameIndex + 1 < frameCount) {
                ++frameIndex;
                continue;
            }

            if (loops) {
                frameIndex = 0;
            } else {
                frameIndex = frameCount - 1;
                finished = true;
                elapsed = 0.0;
                break;
            }
        }
    }

    void advance(const AnimClip& clip, const double simDt) {
        advance(clip.frameCount, clip.frameDuration, clip.loops, simDt);
    }

    // "Playback has reached or passed frame n." Deliberately >= rather than ==:
    // a large simulation step can advance several frames at once, and an event
    // gated on equality would be skipped entirely. Callers latch the event so it
    // still fires exactly once per playthrough of the clip.
    [[nodiscard]] bool reached(const int frame) const { return frameIndex >= frame; }
};
