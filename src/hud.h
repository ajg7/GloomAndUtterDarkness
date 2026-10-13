#pragma once

#include <array>
#include <cstddef>

class World;

namespace Hud {

// ---------------------------------------------------------------------------
// Job 10:20-22, New International Version - the passage the game is named from.
//
// Pre-wrapped rather than measured and reflowed at runtime, so the line breaks
// land where they should: verse 21 ends on the title phrase, alone on its line.
// ---------------------------------------------------------------------------
namespace Epitaph {

constexpr std::array<const char*, 7> Lines{
    "Are not my few days almost over?",
    "Turn away from me so I can have a moment's joy",
    "before I go to the place of no return,",
    "to the land of gloom and utter darkness,",
    "to the land of deepest night,",
    "of utter darkness and disorder,",
    "where even the light is like darkness."
};

// The line the game takes its name from, lit a shade brighter than the rest.
constexpr std::size_t TitleLine{3};

constexpr const char* Attribution{"JOB 10:20-22 (NIV)"};

// Longest line the layout can take before it runs into the margins. Guarded by
// the test harness so editing the passage cannot silently overflow the screen.
constexpr std::size_t MaxLineLength{48};

constexpr double StartDelay{1.1};       // a beat after YOU DIED lands
constexpr double CharactersPerSecond{42.0};
constexpr float LineHeight{13.0f};
constexpr float TopY{92.0f};
constexpr float Size{10.0f};
constexpr float Spacing{2.0f};

} // namespace Epitaph

void draw(const World& world);

// Controls and counters, toggled with F1.
void drawDebug(const World& world, int simTicksPerSecond);

} // namespace Hud
