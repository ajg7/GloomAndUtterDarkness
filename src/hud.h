#pragma once

#include <algorithm>
#include <format>
#include <string>

#include "config.h"
#include "player.h"
#include "raylib.h"
#include "wave.h"
#include "world.h"

namespace Hud {

namespace Detail {

constexpr Color Ink{226, 232, 226, 255};
constexpr Color Dim{150, 160, 152, 255};
constexpr Color Panel{12, 16, 14, 170};
constexpr Color Good{120, 220, 140, 255};
constexpr Color Warn{240, 190, 90, 255};
constexpr Color Bad{226, 92, 82, 255};

inline void bar(const int x, const int y, const int width, const int height,
                const float fill, const Color colour) {
    DrawRectangle(x - 1, y - 1, width + 2, height + 2, Panel);
    DrawRectangleLines(x - 1, y - 1, width + 2, height + 2, Color{70, 80, 72, 200});
    const int filled = static_cast<int>(static_cast<float>(width) * std::clamp(fill, 0.0f, 1.0f));
    if (filled > 0) DrawRectangle(x, y, filled, height, colour);
}

inline void textRight(const std::string& text, const int right, const int y,
                      const int size, const Color colour) {
    const int width = MeasureText(text.c_str(), size);
    DrawText(text.c_str(), right - width, y, size, colour);
}

inline void textCentred(const std::string& text, const int y, const int size, const Color colour) {
    const int width = MeasureText(text.c_str(), size);
    DrawText(text.c_str(), (Config::Virtual::Width - width) / 2, y, size, colour);
}

} // namespace Detail

inline void draw(const World& world) {
    using namespace Detail;

    const Player& player = world.player();

    // Health.
    const float healthFraction =
        static_cast<float>(player.health) / static_cast<float>(Config::Player::MaxHealth);
    const Color healthColour = healthFraction > 0.55f ? Good : (healthFraction > 0.25f ? Warn : Bad);
    bar(7, 7, 64, 5, healthFraction, healthColour);

    // Ammo. Reserve greys out at zero so running dry is visible before it bites.
    const auto ammo = std::format("{:>2} / {}", player.magazine, player.reserve);
    DrawText(ammo.c_str(), 7, 16, 10, player.magazine == 0 ? Bad : Ink);

    if (player.reloading()) {
        const float progress = 1.0f - static_cast<float>(player.reloadRemaining
                                                       / Config::Player::ReloadDuration);
        bar(7, 29, 44, 3, progress, Warn);
        DrawText("RELOADING", 7, 34, 10, Dim);
    } else if (player.emptyFeedbackRemaining > 0.0
               && static_cast<int>(player.emptyFeedbackRemaining * 10.0) % 2 == 0) {
        DrawText("EMPTY - R TO RELOAD", 7, 29, 10, Bad);
    }

    // Wave and score.
    const int right = Config::Virtual::Width - 7;
    textRight(std::format("WAVE {}", std::max(1, world.wave().wave())), right, 7, 10, Ink);
    textRight(std::format("KILLS {}", world.kills()), right, 18, 10, Dim);

    // Between waves, count the break down; during one, show what is left to come.
    const WaveDirector& wave = world.wave();
    if (wave.phase() == WaveDirector::Phase::Break) {
        textRight(std::format("NEXT IN {:.1f}", std::max(0.0, wave.breakRemaining())), right, 29, 10, Warn);
    } else {
        textRight(std::format("{} LEFT", world.activeZombies() + std::max(0, wave.remainingInWave())),
                  right, 29, 10, Dim);
    }

    if (world.waveBannerRemaining() > 0.0) {
        textCentred(std::format("WAVE {}", wave.wave()), 44, 20, Ink);
    }

    if (world.state() == GameState::GameOver) {
        DrawRectangle(0, 0, Config::Virtual::Width, Config::Virtual::Height, Color{6, 8, 7, 190});
        textCentred("YOU DIED", 88, 20, Bad);
        textCentred(std::format("REACHED WAVE {}", std::max(1, wave.wave())), 118, 10, Ink);
        textCentred(std::format("{} ZOMBIES KILLED", world.kills()), 132, 10, Ink);
        textCentred("PRESS ENTER TO RESTART", 156, 10, Dim);
    }
}

// Controls and counters, toggled with F1.
inline void drawDebug(const World& world, const int simTicksPerSecond) {
    using namespace Detail;

    DrawText(std::format("zombies {} (live slots {})", world.activeZombies(), world.liveZombieSlots()).c_str(),
             7, Config::Virtual::Height - 46, 10, Dim);
    DrawText(std::format("bullets {}", world.liveBullets()).c_str(),
             7, Config::Virtual::Height - 36, 10, Dim);
    DrawText(std::format("sim hz {}", simTicksPerSecond).c_str(),
             7, Config::Virtual::Height - 26, 10, Dim);
    DrawText("F1 debug  F2 +200 zombies  F3 cycle fps cap",
             7, Config::Virtual::Height - 14, 10, Dim);
}

} // namespace Hud
