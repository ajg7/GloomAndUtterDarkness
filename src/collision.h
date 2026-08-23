#pragma once

#include <algorithm>
#include <limits>
#include <optional>

#include "raylib.h"

namespace Collision {

// One axis of the slab test.
//
// Returns false when the segment can never overlap this slab. The delta == 0
// case has to be handled explicitly: a bullet fired dead horizontally has
// exactly zero y-movement, and dividing by it produces either infinities that
// happen to work or a 0/0 NaN that silently defeats every rejection test below.
inline bool slab(const float start, const float delta, const float lo, const float hi,
                 float& enter, float& exit) {
    constexpr float infinity = std::numeric_limits<float>::infinity();

    if (delta == 0.0f) {
        if (start < lo || start > hi) return false; // parallel and outside: never overlaps
        enter = -infinity;                          // parallel and inside: always overlaps
        exit = infinity;
        return true;
    }

    const float t0 = (lo - start) / delta;
    const float t1 = (hi - start) / delta;
    enter = std::min(t0, t1);
    exit = std::max(t0, t1);
    return true;
}

// Sweeps the segment start -> end against box, returning the fraction of the
// segment at which contact begins, or nothing if there is no contact.
//
// The moving object is treated as a point, so the caller must hand in a box
// already expanded by the mover's half-extents (see expand() below). That is
// the trick the whole method rests on: growing the target by the mover's size
// turns "box vs box over time" into "ray vs box", which is four divisions.
//
// A negative entry time means contact began before this step started, i.e. the
// two were already overlapping. That is a legitimate hit, so it is clamped to 0
// rather than rejected — a bullet spawned inside a body should still connect.
inline std::optional<float> sweptAABB(const Vector2 start, const Vector2 end, const Rectangle box) {
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;

    float enterX{}, exitX{}, enterY{}, exitY{};
    if (!slab(start.x, deltaX, box.x, box.x + box.width, enterX, exitX)) return std::nullopt;
    if (!slab(start.y, deltaY, box.y, box.y + box.height, enterY, exitY)) return std::nullopt;

    const float entry = std::max(enterX, enterY);
    const float exit = std::min(exitX, exitY);

    // entry > exit: the axes overlap at different times, so the box is missed.
    // exit < 0: contact finished before the step began.
    // entry > 1: contact begins after the step ends.
    if (entry > exit || exit < 0.0f || entry > 1.0f) return std::nullopt;

    return std::max(entry, 0.0f);
}

// Grows a box outward by the mover's half-extents, so the mover can be swept
// as a point. Same routine serves bullets, pickups, and any wall ever added.
inline Rectangle expand(const Rectangle box, const float halfWidth, const float halfHeight) {
    return Rectangle{
        box.x - halfWidth,
        box.y - halfHeight,
        box.width + 2.0f * halfWidth,
        box.height + 2.0f * halfHeight
    };
}

// Static overlap. Correct only when neither box can pass through the other
// within one step — true for zombie-vs-player contact at walking speed, false
// for anything bullet-shaped.
inline bool overlaps(const Rectangle a, const Rectangle b) {
    return a.x < b.x + b.width
        && b.x < a.x + a.width
        && a.y < b.y + b.height
        && b.y < a.y + a.height;
}

inline Vector2 center(const Rectangle box) {
    return Vector2{box.x + box.width * 0.5f, box.y + box.height * 0.5f};
}

} // namespace Collision
