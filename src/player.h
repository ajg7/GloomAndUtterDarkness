#pragma once

#include <algorithm>
#include <optional>

#include "animation.h"
#include "assets.h"
#include "bullet.h"
#include "collision.h"
#include "config.h"
#include "input.h"
#include "raylib.h"
#include "sprite.h"

// ---------------------------------------------------------------------------
// The player as one explicit state, and the legal moves between states.
//
// The alternative - a pile of booleans - has 2^n combinations, most of them
// never intended: shooting while reloading, hurt while shooting, dead but still
// firing. Making state a single value means those combinations are not
// unreachable by discipline, they are unrepresentable.
//
// Whether the player can walk is a *function of* the state, resolved in one
// place below. That is not a boolean sneaking back in: there is still exactly
// one state variable, and locomotion is derived from it rather than tracked
// alongside it.
// ---------------------------------------------------------------------------
namespace PlayerStates {

enum class State : int { Idle = 0, Walk, Shoot, Reload, Hurt, Dead, Count };

constexpr int Count{static_cast<int>(State::Count)};

// Rows are the state being left, columns the state being entered. Every
// transition in the game goes through this table; there is nowhere else that
// assigns to Player::state.
inline constexpr bool Legal[Count][Count] = {
//   to:        Idle   Walk   Shoot  Reload Hurt   Dead
/* Idle   */ {  false, true,  true,  true,  true,  true  },
/* Walk   */ {  true,  false, true,  true,  true,  true  },
/* Shoot  */ {  true,  true,  false, false, true,  true  }, // no re-entry, no reload mid-shot
/* Reload */ {  true,  true,  false, false, true,  true  }, // firing is refused while reloading
/* Hurt   */ {  true,  true,  false, false, false, true  },
/* Dead   */ {  false, false, false, false, false, false }  // only reset() leaves Dead
};

constexpr bool legal(const State from, const State to) {
    return Legal[static_cast<int>(from)][static_cast<int>(to)];
}

// Rooted while reloading, reeling from a hit, or dead. Shooting still allows a
// walk so the player is not glued in place every time they pull the trigger.
constexpr bool allowsMovement(const State state) {
    return state == State::Idle || state == State::Walk || state == State::Shoot;
}

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

inline const AnimClip& clip(const ScientistClips& clips, const State state) {
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

} // namespace PlayerStates

struct Player {
    using State = PlayerStates::State;

    Vector2 position{};      // top-left of the 128x128 sprite frame
    Vector2 prevBodyCentre{};
    State state{State::Idle};
    AnimState anim{};
    bool facingLeft{false};

    int health{Config::Player::MaxHealth};
    double invulnerabilityRemaining{0.0};

    int magazine{Config::Player::MagazineSize};
    int reserve{Config::Player::StartingReserve};
    double reloadRemaining{0.0};
    double fireCooldownRemaining{0.0};
    bool bulletSpawnedThisShot{false};
    double emptyFeedbackRemaining{0.0};

    static constexpr float FrameY{
        Config::World::GroundLineYF - static_cast<float>(Config::Assets::Scientist::FrameHeight)};

    // Puts the player's body centre at a given x.
    static constexpr float frameXForBodyCentre(const float centreX) {
        return centreX - Config::Assets::Scientist::Body.x - Config::Assets::Scientist::Body.width * 0.5f;
    }

    // Everything the player owns, restored. Anything missed here survives a
    // restart and shows up as drift on run five.
    void reset() {
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

    [[nodiscard]] Rectangle body() const {
        const Rectangle& local = Config::Assets::Scientist::Body;
        return Rectangle{position.x + local.x, position.y + local.y, local.width, local.height};
    }

    [[nodiscard]] bool alive() const { return state != State::Dead; }
    [[nodiscard]] bool invulnerable() const { return invulnerabilityRemaining > 0.0; }
    [[nodiscard]] bool reloading() const { return state == State::Reload; }

    // The single point of assignment to `state`. Refuses illegal moves rather
    // than trusting callers to only ask for legal ones.
    bool transitionTo(const State next) {
        if (!PlayerStates::legal(state, next)) return false;

        if (state == State::Reload) reloadRemaining = 0.0; // interrupted: no rounds moved

        state = next;
        anim.reset();

        if (next == State::Reload) reloadRemaining = Config::Player::ReloadDuration;
        if (next == State::Shoot) bulletSpawnedThisShot = false;
        return true;
    }

    bool takeDamage(const int amount) {
        if (!alive() || invulnerable()) return false;

        health = std::max(0, health - amount);
        invulnerabilityRemaining = Config::Player::InvulnerabilityDuration;
        transitionTo(health <= 0 ? State::Dead : State::Hurt);
        return true;
    }

    void heal(const int amount) {
        if (!alive()) return;
        health = std::min(Config::Player::MaxHealth, health + amount);
    }

    void grantAmmo(const int rounds) { reserve += rounds; }

    [[nodiscard]] bool canReload() const {
        return magazine < Config::Player::MagazineSize && reserve > 0;
    }

    std::optional<Bullet> update(const Input& input, const double simDt) {
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

    void draw(const ScientistClips& clips) const {
        // Blink through the invulnerability window so the player can see it.
        if (invulnerable() && static_cast<int>(invulnerabilityRemaining * 16.0) % 2 == 0) return;

        Sprite::draw(PlayerStates::clip(clips, state), anim, position, facingLeft);
    }

private:
    void advance(const double simDt) {
        const Config::ClipDef& def = PlayerStates::clipDef(state);
        anim.advance(def.frameCount, def.frameDuration, def.loops, simDt);
    }

    void requestReload() {
        if (!canReload()) return;
        transitionTo(State::Reload); // refused by the table from Shoot and Hurt
    }

    void requestFire() {
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

    [[nodiscard]] Bullet makeBullet() const {
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

    void completeReload() {
        const int wanted = Config::Player::MagazineSize - magazine;
        const int loaded = std::min(wanted, reserve);
        magazine += loaded;
        reserve -= loaded;
    }
};
