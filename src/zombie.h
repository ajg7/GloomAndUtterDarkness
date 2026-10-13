#pragma once

#include "animation.h"
#include "config.h"
#include "raylib.h"

struct ZombieClips;

// What one simulation step of a zombie produced, for the caller to act on.
struct ZombieStep {
    bool dealsDamage{false}; // the swing landed on the player this step
    bool expired{false};     // the corpse has lingered long enough; recycle the slot
};

// A single zombie.
//
// It holds no textures and no per-type behaviour - only a type tag, its
// position, and its playback state. Everything that distinguishes a shambler
// from a crawler is read out of Config::Zombies::Table by the one update()
// below, which is why a fourth type needs no code here at all.
//
// It is a plain, default-constructible, assignable aggregate because that is
// what Pool requires of the objects it recycles.
//
// The small geometry accessors stay inline here: crowd separation and the
// bullet sweep call them for every zombie, several times a step.
struct Zombie {
    enum class State : int { Walk, Attack, Dead };

    Config::Zombies::Type type{Config::Zombies::Type::Man};
    float x{0.0f};      // left edge of the 96x96 sprite frame
    float depth{0.0f};  // offset from the ground line: crowd depth and draw order
    int hp{0};
    bool facingLeft{false};
    State state{State::Walk};
    AnimState anim{};

    double attackCooldownRemaining{0.0};
    bool swingLanded{false};
    double deathTimer{0.0};
    double surgeRemaining{0.0};
    double surgeCooldownRemaining{0.0};
    double hurtFlash{0.0};

    static Zombie spawned(Config::Zombies::Type type, float x, float depth, bool facingLeft);

    [[nodiscard]] const Config::Zombies::Stats& stats() const {
        return Config::Zombies::statsFor(type);
    }

    [[nodiscard]] float frameY() const {
        return Config::World::GroundLineYF - static_cast<float>(Config::Zombies::FrameHeight) + depth;
    }

    // The hit box: the opaque body within the frame, not the frame itself.
    [[nodiscard]] Rectangle body() const {
        const Rectangle& local = stats().body;
        return Rectangle{x + local.x, frameY() + local.y, local.width, local.height};
    }

    // The shoving circle, deliberately separate from the hit box. It sits at
    // foot level so crowding is resolved where the bodies actually stand, while
    // bullets still test against the full torso.
    [[nodiscard]] Vector2 separationCentre() const {
        const Rectangle b = body();
        return Vector2{b.x + b.width * 0.5f, Config::World::GroundLineYF + depth};
    }

    [[nodiscard]] bool isDead() const { return state == State::Dead; }

    // Returns true if this hit was the killing one.
    bool takeDamage(int amount);

    ZombieStep update(double simDt, const Rectangle& playerBody, bool playerAlive);

    void draw(const ZombieClips& clips) const;

private:
    void advanceClip(const Config::ClipDef& def, double simDt);
};
