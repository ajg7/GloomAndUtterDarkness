#include "player.h"

#include <algorithm>

#include "assets.h"
#include "collision.h"
#include "input.h"
#include "sprite.h"

namespace {

using PlayerStates::State;

constexpr const Config::ClipDef& clipDef(const State state) {
    switch (state) {
        case State::Walk:   return Config::Assets::Scientist::Walk;
        case State::Shoot:  return Config::Assets::Scientist::Special;
        case State::Hurt:   return Config::Assets::Scientist::Hurt;
        case State::Dead:   return Config::Assets::Scientist::Dead;
        case State::Idle:
        case State::Reload: // rooted, so the idle pose is the honest one
        default:            return Config::Assets::Scientist::Idle;
    }
}

const AnimClip& clip(const ScientistClips& clips, const State state) {
    switch (state) {
        case State::Walk:   return clips.walk;
        case State::Shoot:  return clips.special;
        case State::Hurt:   return clips.hurt;
        case State::Dead:   return clips.dead;
        case State::Idle:
        case State::Reload:
        default:            return clips.idle;
    }
}

} // namespace

void Player::reset() {
    position = Vector2{frameXForBodyCentre(Config::Virtual::WidthF * 0.5f), FrameY};
    prevBodyCentre = Collision::center(body());
    state = State::Idle;
    anim.reset();
    facingLeft = false;
    health = Config::Player::MaxHealth;
    invulnerabilityRemaining = 0.0;
    magazine = Config::Player::MagazineSize;
    reserve = Config::Player::StartingReserve;
    reloadRemaining = 0.0;
    fireCooldownRemaining = 0.0;
    bulletSpawnedThisShot = false;
    emptyFeedbackRemaining = 0.0;
}

bool Player::transitionTo(const State next) {
    if (!PlayerStates::legal(state, next)) return false;

    if (state == State::Reload) reloadRemaining = 0.0; // interrupted: no rounds moved

    state = next;
    anim.reset();

    if (next == State::Reload) reloadRemaining = Config::Player::ReloadDuration;
    if (next == State::Shoot) bulletSpawnedThisShot = false;
    return true;
}

bool Player::takeDamage(const int amount) {
    if (!alive() || invulnerable()) return false;

    health = std::max(0, health - amount);
    invulnerabilityRemaining = Config::Player::InvulnerabilityDuration;
    transitionTo(health <= 0 ? State::Dead : State::Hurt);
    return true;
}

void Player::heal(const int amount) {
    if (!alive()) return;
    health = std::min(Config::Player::MaxHealth, health + amount);
}

std::optional<Bullet> Player::update(const Input& input, const double simDt) {
    prevBodyCentre = Collision::center(body());

    if (invulnerabilityRemaining > 0.0) invulnerabilityRemaining -= simDt;
    if (fireCooldownRemaining > 0.0) fireCooldownRemaining -= simDt;
    if (emptyFeedbackRemaining > 0.0) emptyFeedbackRemaining -= simDt;

    if (state == State::Dead) {
        advance(simDt);
        return std::nullopt;
    }

    const bool goLeft = input.moveLeft && !input.moveRight;
    const bool goRight = input.moveRight && !input.moveLeft;
    const bool moving = (goLeft || goRight) && PlayerStates::allowsMovement(state);

    if (moving) {
        facingLeft = goLeft;
        const float delta = (goLeft ? -1.0f : 1.0f)
                          * Config::Assets::Scientist::MoveSpeed * static_cast<float>(simDt);
        // Clamped on the body, not the frame, so the sprite's transparent
        // padding does not hold the player away from the arena edge.
        const float minX = Config::World::ArenaMinX - Config::Assets::Scientist::Body.x;
        const float maxX = Config::World::ArenaMaxX - Config::Assets::Scientist::Body.x
                         - Config::Assets::Scientist::Body.width;
        position.x = std::clamp(position.x + delta, minX, maxX);
    }

    if (input.reload) requestReload();

    std::optional<Bullet> spawned;

    switch (state) {
        case State::Hurt: {
            advance(simDt);
            if (anim.finished) transitionTo(moving ? State::Walk : State::Idle);
            break;
        }
        case State::Reload: {
            advance(simDt);
            reloadRemaining -= simDt;
            if (reloadRemaining <= 0.0) {
                completeReload();
                transitionTo(State::Idle);
            }
            break;
        }
        case State::Shoot: {
            advance(simDt);
            // The shot leaves on the muzzle-flash frame, never on the
            // keypress. `reached` rather than `==` so an oversized
            // simulation step that jumps past the frame still fires,
            // latched by bulletSpawnedThisShot to exactly once per shot.
            if (!bulletSpawnedThisShot && anim.reached(Config::Assets::Scientist::MuzzleFlashFrame)) {
                bulletSpawnedThisShot = true;
                --magazine;
                spawned = makeBullet();
            }
            if (anim.finished) {
                fireCooldownRemaining = Config::Player::FireCooldown;
                transitionTo(moving ? State::Walk : State::Idle);
            }
            break;
        }
        case State::Idle:
        case State::Walk:
        default: {
            if (input.fireHeld) requestFire();

            if (state != State::Shoot && state != State::Reload) {
                const State locomotion = moving ? State::Walk : State::Idle;
                if (locomotion != state) transitionTo(locomotion);
                advance(simDt);
            }
            break;
        }
    }

    return spawned;
}

void Player::draw(const ScientistClips& clips) const {
    // Blink through the invulnerability window so the player can see it.
    if (invulnerable() && static_cast<int>(invulnerabilityRemaining * 16.0) % 2 == 0) return;

    Sprite::draw(clip(clips, state), anim, position, facingLeft);
}

void Player::advance(const double simDt) {
    const Config::ClipDef& def = clipDef(state);
    anim.advance(def.frameCount, def.frameDuration, def.loops, simDt);
}

void Player::requestReload() {
    if (!canReload()) return;
    transitionTo(State::Reload); // refused by the table from Shoot and Hurt
}

void Player::requestFire() {
    if (fireCooldownRemaining > 0.0) return;

    if (magazine <= 0) {
        // Feedback rather than silence, and roll straight into a reload if
        // there is anything left to load.
        emptyFeedbackRemaining = Config::Player::EmptyFeedbackDuration;
        requestReload();
        return;
    }

    transitionTo(State::Shoot);
}

Bullet Player::makeBullet() const {
    // The frame is flipped about its own centre, so the mirrored muzzle
    // offset is frameWidth - offset.
    const float frameWidth = static_cast<float>(Config::Assets::Scientist::FrameWidth);
    const float offsetX = facingLeft
        ? frameWidth - Config::Assets::Scientist::MuzzleOffsetX
        : Config::Assets::Scientist::MuzzleOffsetX;

    return Bullet::fired(
        Vector2{position.x + offsetX, position.y + Config::Assets::Scientist::MuzzleOffsetY},
        Vector2{(facingLeft ? -1.0f : 1.0f) * Config::Gameplay::BulletSpeed, 0.0f}
    );
}

void Player::completeReload() {
    const int wanted = Config::Player::MagazineSize - magazine;
    const int loaded = std::min(wanted, reserve);
    magazine += loaded;
    reserve -= loaded;
}
