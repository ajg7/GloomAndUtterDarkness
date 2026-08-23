#pragma once

#include "collision.h"
#include "config.h"
#include "raylib.h"

// A travelling projectile. position is the bullet's *centre*, which is what the
// swept test wants: the target box is grown by these half-extents and the centre
// is then swept through it as a point.
struct Bullet {
    Vector2 position{};
    Vector2 prevPosition{};
    Vector2 velocity{};

    static constexpr float HalfWidth{Config::Gameplay::BulletWidth * 0.5f};
    static constexpr float HalfHeight{Config::Gameplay::BulletHeight * 0.5f};

    static Bullet fired(const Vector2 origin, const Vector2 vel) {
        return Bullet{origin, origin, vel};
    }

    void update(const double simDt) {
        prevPosition = position;
        position.x += velocity.x * static_cast<float>(simDt);
        position.y += velocity.y * static_cast<float>(simDt);
    }

    // Generous margin so a bullet is never culled while its sweep for this step
    // is still being resolved against something at the arena edge.
    [[nodiscard]] bool offscreen() const {
        return position.x < -32.0f
            || position.x > Config::Virtual::WidthF + 32.0f
            || position.y < -32.0f
            || position.y > Config::Virtual::HeightF + 32.0f;
    }

    void draw() const {
        DrawRectangle(
            static_cast<int>(position.x - HalfWidth),
            static_cast<int>(position.y - HalfHeight),
            static_cast<int>(Config::Gameplay::BulletWidth),
            static_cast<int>(Config::Gameplay::BulletHeight),
            Color{255, 228, 120, 255}
        );
    }
};
