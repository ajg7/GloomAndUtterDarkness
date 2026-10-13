#pragma once

#include <optional>

#include "animation.h"
#include "bullet.h"
#include "config.h"
#include "raylib.h"

struct Input;
struct ScientistClips;

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
    void reset();

    [[nodiscard]] Rectangle body() const {
        const Rectangle& local = Config::Assets::Scientist::Body;
        return Rectangle{position.x + local.x, position.y + local.y, local.width, local.height};
    }

    [[nodiscard]] bool alive() const { return state != State::Dead; }
    [[nodiscard]] bool invulnerable() const { return invulnerabilityRemaining > 0.0; }
    [[nodiscard]] bool reloading() const { return state == State::Reload; }

    // The single point of assignment to `state`. Refuses illegal moves rather
    // than trusting callers to only ask for legal ones.
    bool transitionTo(State next);

    bool takeDamage(int amount);
    void heal(int amount);
    void grantAmmo(const int rounds) { reserve += rounds; }

    [[nodiscard]] bool canReload() const {
        return magazine < Config::Player::MagazineSize && reserve > 0;
    }

    std::optional<Bullet> update(const Input& input, double simDt);

    void draw(const ScientistClips& clips) const;

private:
    void advance(double simDt);
    void requestReload();
    void requestFire();
    [[nodiscard]] Bullet makeBullet() const;
    void completeReload();
};
