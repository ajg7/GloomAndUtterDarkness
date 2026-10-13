#include "hud.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "config.h"
#include "player.h"
#include "raylib.h"
#include "wave.h"
#include "world.h"

namespace Hud {

namespace {

constexpr Color Ink{226, 232, 226, 255};
constexpr Color Dim{150, 160, 152, 255};
constexpr Color Panel{12, 16, 14, 170};
constexpr Color Good{120, 220, 140, 255};
constexpr Color Warn{240, 190, 90, 255};
constexpr Color Bad{226, 92, 82, 255};
constexpr Color Verse{136, 144, 139, 255};    // ash, for the passage
constexpr Color VerseLit{214, 222, 216, 255}; // the line the game is named from

void bar(const int x, const int y, const int width, const int height,
         const float fill, const Color colour) {
    DrawRectangle(x - 1, y - 1, width + 2, height + 2, Panel);
    DrawRectangleLines(x - 1, y - 1, width + 2, height + 2, Color{70, 80, 72, 200});
    const int filled = static_cast<int>(static_cast<float>(width) * std::clamp(fill, 0.0f, 1.0f));
    if (filled > 0) DrawRectangle(x, y, filled, height, colour);
}

void textRight(const std::string& text, const int right, const int y,
               const int size, const Color colour) {
    const int width = MeasureText(text.c_str(), size);
    DrawText(text.c_str(), right - width, y, size, colour);
}

void textCentred(const std::string& text, const int y, const int size, const Color colour) {
    const int width = MeasureText(text.c_str(), size);
    DrawText(text.c_str(), (Config::Virtual::Width - width) / 2, y, size, colour);
}

// Centred text with explicit letter spacing.
//
// There is no TTF to bundle and a smooth outline font would sit badly against
// 480x270 pixel art, so the epitaph gets its own voice from the built-in font by
// opening the tracking right up. Wide-tracked small caps read as inscription
// rather than as UI, which is the whole point of the screen.
void textCentredSpaced(const std::string& text, const float y, const float size,
                       const float spacing, const Color colour) {
    const Font font = GetFontDefault();
    const Vector2 measured = MeasureTextEx(font, text.c_str(), size, spacing);
    DrawTextEx(font, text.c_str(),
               Vector2{std::round((Config::Virtual::WidthF - measured.x) * 0.5f), std::round(y)},
               size, spacing, colour);
}

void drawGameOver(const World& world) {
    DrawRectangle(0, 0, Config::Virtual::Width, Config::Virtual::Height, Color{5, 7, 6, 216});

    const WaveDirector& wave = world.wave();

    textCentredSpaced("YOU DIED", 30.0f, 20.0f, 4.0f, Bad);
    DrawRectangle(160, 58, 160, 1, Color{52, 58, 54, 255});

    textCentredSpaced(std::format("REACHED WAVE {}", std::max(1, wave.wave())), 66.0f, 10.0f, 2.0f, Ink);
    textCentredSpaced(std::format("{} ZOMBIES KILLED", world.kills()), 78.0f, 10.0f, 2.0f, Dim);

    // The passage writes itself on a character at a time. Each line is centred
    // on its *finished* width, so the text grows rightward from a fixed left
    // edge instead of sliding around as it types.
    const Font font = GetFontDefault();
    double budget = (world.gameOverElapsed() - Epitaph::StartDelay) * Epitaph::CharactersPerSecond;
    bool complete = true;

    for (std::size_t i = 0; i < Epitaph::Lines.size(); ++i) {
        if (budget <= 0.0) {
            complete = false;
            break;
        }

        const std::string full{Epitaph::Lines[i]};
        const auto shown = static_cast<std::size_t>(
            std::min(budget, static_cast<double>(full.size())));
        const std::string visible = full.substr(0, shown);

        const float y = Epitaph::TopY + Epitaph::LineHeight * static_cast<float>(i);
        const Vector2 fullSize = MeasureTextEx(font, full.c_str(), Epitaph::Size, Epitaph::Spacing);
        const float x = std::round((Config::Virtual::WidthF - fullSize.x) * 0.5f);

        const Color colour = (i == Epitaph::TitleLine) ? VerseLit : Verse;
        DrawTextEx(font, visible.c_str(), Vector2{x, y}, Epitaph::Size, Epitaph::Spacing, colour);

        if (shown < full.size()) {
            const Vector2 visibleSize =
                MeasureTextEx(font, visible.c_str(), Epitaph::Size, Epitaph::Spacing);
            DrawRectangle(static_cast<int>(x + visibleSize.x), static_cast<int>(y) + 1, 4, 8, VerseLit);
            complete = false;
            break;
        }

        budget -= static_cast<double>(full.size());
    }

    if (!complete) return;

    textCentredSpaced(Epitaph::Attribution, 196.0f, 10.0f, 1.0f, Color{78, 86, 81, 255});

    // Restart is accepted from the moment you die - this is only the prompt.
    if (static_cast<int>(world.gameOverElapsed() * 1.6) % 2 == 0) {
        textCentredSpaced("PRESS ENTER TO RESTART", 230.0f, 10.0f, 2.0f, Dim);
    }
}

} // namespace

void draw(const World& world) {
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

    if (world.state() == GameState::GameOver) drawGameOver(world);
}

void drawDebug(const World& world, const int simTicksPerSecond) {
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
