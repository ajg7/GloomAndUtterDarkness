#include "zombie.h"

#include <cmath>

#include "assets.h"
#include "collision.h"
#include "sprite.h"

Zombie Zombie::spawned(const Config::Zombies::Type type, const float x, const float depth,
                       const bool facingLeft) {
    Zombie z{};
    z.type = type;
    z.x = x;
    z.depth = depth;
    z.hp = Config::Zombies::statsFor(type).maxHp;
    z.facingLeft = facingLeft;
    return z;
}

bool Zombie::takeDamage(const int amount) {
    if (isDead()) return false;

    hp -= amount;
    hurtFlash = 0.10;
    if (hp > 0) return false;

    state = State::Dead;
    anim.reset();
    deathTimer = stats().deathLinger;
    return true;
}

ZombieStep Zombie::update(const double simDt, const Rectangle& playerBody, const bool playerAlive) {
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

void Zombie::draw(const ZombieClips& clips) const {
    const AnimClip& clip = (state == State::Dead) ? clips.dead
                         : (state == State::Attack) ? clips.attack
                         : clips.walk;

    const Color tint = hurtFlash > 0.0 ? Color{255, 140, 140, 255} : WHITE;
    Sprite::draw(clip, anim, Vector2{x, frameY()}, facingLeft, tint);
}

// Playback only needs frame count and duration, both of which live in the
// stats row. The texture is not needed until draw time.
void Zombie::advanceClip(const Config::ClipDef& def, const double simDt) {
    anim.advance(def.frameCount, def.frameDuration, def.loops, simDt);
}
