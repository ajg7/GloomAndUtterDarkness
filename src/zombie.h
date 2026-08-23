#pragma once

#include <algorithm>
#include <cmath>

#include "animation.h"
#include "assets.h"
#include "collision.h"
#include "config.h"
#include "raylib.h"
#include "sprite.h"

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

    static Zombie spawned(const Config::Zombies::Type type, const float x, const float depth,
                          const bool facingLeft) {
        Zombie z{};
        z.type = type;
        z.x = x;
        z.depth = depth;
        z.hp = Config::Zombies::statsFor(type).maxHp;
        z.facingLeft = facingLeft;
        return z;
    }

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
    bool takeDamage(const int amount) {
        if (isDead()) return false;

        hp -= amount;
        hurtFlash = 0.10;
        if (hp > 0) return false;

        state = State::Dead;
        anim.reset();
        deathTimer = stats().deathLinger;
        return true;
    }

    ZombieStep update(const double simDt, const Rectangle& playerBody, const bool playerAlive) {
        const Config::Zombies::Stats& s = stats();
        ZombieStep result{};

        if (hurtFlash > 0.0) hurtFlash -= simDt;

        if (state == State::Dead) {
            advanceClip(s.dead, simDt);
            deathTimer -= simDt;
            result.expired = deathTimer <= 0.0;
            return result;
        }

        if (attackCooldownRemaining > 0.0) attackCooldownRemaining -= simDt;
        if (surgeRemaining > 0.0) surgeRemaining -= simDt;
        if (surgeCooldownRemaining > 0.0) surgeCooldownRemaining -= simDt;

        const Rectangle self = body();
        const float selfCentreX = self.x + self.width * 0.5f;
        const float targetCentreX = playerBody.x + playerBody.width * 0.5f;
        const bool inReach = playerAlive
            && Collision::overlaps(Collision::expand(self, s.attackReach, s.attackReach), playerBody);

        if (state == State::Attack) {
            advanceClip(s.attack, simDt);

            // The swing lands on an animation frame, not after a timer, so the
            // damage and the sprite agree at any simulation step size.
            if (!swingLanded && anim.reached(s.attackDamageFrame)) {
                swingLanded = true;
                result.dealsDamage = inReach;
            }

            if (anim.finished) {
                state = State::Walk;
                anim.reset();
                attackCooldownRemaining = s.attackCooldown;
            }
            return result;
        }

        // Walking.
        facingLeft = targetCentreX < selfCentreX;

        if (inReach && attackCooldownRemaining <= 0.0) {
            state = State::Attack;
            anim.reset();
            swingLanded = false;
            return result;
        }

        // The lunge is pure data: a type whose surgeRange is 0 can never enter
        // this branch, so adding a non-lunging fourth type changes nothing here.
        const float distance = std::fabs(targetCentreX - selfCentreX);
        if (s.surgeRange > 0.0f && playerAlive
            && surgeRemaining <= 0.0 && surgeCooldownRemaining <= 0.0
            && distance < s.surgeRange) {
            surgeRemaining = s.surgeDuration;
            surgeCooldownRemaining = s.surgeCooldown;
        }

        const float speed = s.speed * (surgeRemaining > 0.0 ? s.surgeSpeedScale : 1.0f);
        const float direction = facingLeft ? -1.0f : 1.0f;
        x += direction * speed * static_cast<float>(simDt);

        advanceClip(s.walk, simDt);
        return result;
    }

    void draw(const ZombieClips& clips) const {
        const AnimClip& clip = (state == State::Dead) ? clips.dead
                             : (state == State::Attack) ? clips.attack
                             : clips.walk;

        const Color tint = hurtFlash > 0.0 ? Color{255, 140, 140, 255} : WHITE;
        Sprite::draw(clip, anim, Vector2{x, frameY()}, facingLeft, tint);
    }

private:
    // Playback only needs frame count and duration, both of which live in the
    // stats row. The texture is not needed until draw time.
    void advanceClip(const Config::ClipDef& def, const double simDt) {
        anim.advance(def.frameCount, def.frameDuration, def.loops, simDt);
    }
};
