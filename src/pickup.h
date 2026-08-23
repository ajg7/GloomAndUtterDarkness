#pragma once

#include <cmath>

#include "config.h"
#include "raylib.h"

// A health drop sitting on the ground line.
//
// It is collected with the same swept AABB the rifle uses, not an overlap test.
// An overlap check samples only where the player ended the step, so it starts
// missing drops once one step carries the player further than the player's and
// the drop's half-widths combined. At the current walking pace that threshold is
// nowhere near - it would take roughly 1400 px/s at 60 Hz - but it is a real
// edge, it moves with any change to speed or step rate, and reusing the sweep
// costs nothing. One primitive, three consumers: bullets, pickups, and anything
// solid added later.
struct Pickup {
    Vector2 position{};      // centre
    double remaining{0.0};

    static Pickup dropped(const float x, const float y) {
        return Pickup{Vector2{x, y}, Config::Pickups::Lifetime};
    }

    [[nodiscard]] Rectangle box() const {
        return Rectangle{
            position.x - Config::Pickups::Width * 0.5f,
            position.y - Config::Pickups::Height * 0.5f,
            Config::Pickups::Width,
            Config::Pickups::Height
        };
    }

    // Returns false once the drop has expired and its slot should be recycled.
    bool update(const double simDt) {
        remaining -= simDt;
        return remaining > 0.0;
    }

    void draw() const {
        // Flash on the way out so a drop about to vanish is readable.
        if (remaining < Config::Pickups::BlinkBelow) {
            const int blink = static_cast<int>(remaining * 8.0);
            if (blink % 2 == 0) return;
        }

        const Rectangle b = box();
        const int x = static_cast<int>(b.x);
        const int y = static_cast<int>(b.y);
        const int w = static_cast<int>(b.width);
        const int h = static_cast<int>(b.height);

        DrawRectangle(x, y, w, h, Color{28, 40, 32, 220});
        DrawRectangleLines(x, y, w, h, Color{120, 220, 140, 255});
        DrawRectangle(x + w / 2 - 1, y + 2, 3, h - 4, Color{140, 240, 160, 255});
        DrawRectangle(x + 2, y + h / 2 - 1, w - 4, 3, Color{140, 240, 160, 255});
    }
};
