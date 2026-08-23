#pragma once

#include "animation.h"
#include "raylib.h"

namespace Sprite {

// Source rectangle for the current frame. Horizontal flip is a negative width
// on the source rect — one subtraction instead of a mirrored copy of every sheet.
inline Rectangle frameRect(const AnimClip& clip, const AnimState& state, const bool flipX) {
    const float w = static_cast<float>(clip.frameWidth);
    return Rectangle{
        flipX ? w * static_cast<float>(state.frameIndex + 1) : w * static_cast<float>(state.frameIndex),
        0.0f,
        flipX ? -w : w,
        static_cast<float>(clip.frameHeight)
    };
}

inline void draw(const AnimClip& clip, const AnimState& state, const Vector2 topLeft,
                 const bool flipX, const Color tint = WHITE) {
    const Rectangle src = frameRect(clip, state, flipX);
    const Rectangle dst{
        topLeft.x, topLeft.y,
        static_cast<float>(clip.frameWidth),
        static_cast<float>(clip.frameHeight)
    };
    DrawTexturePro(clip.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

} // namespace Sprite
