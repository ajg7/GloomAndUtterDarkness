#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "raylib.h"

// Configuration constants for the physical application window
namespace Config::Window {
    constexpr int Width{1280};
    constexpr int Height{720};
    constexpr int TargetFPS{60};

    constexpr float WidthF{static_cast<float>(Width)};
    constexpr float HeightF{static_cast<float>(Height)};
}

// Configuration constants for the virtual canvas resolution
namespace Config::Virtual {
    // The internal coordinate space everything is authored and simulated in.
    // Every entity position and collision box downstream is measured in these
    // pixels, not window pixels.
    constexpr int Width{480};
    constexpr int Height{270};

    constexpr float WidthF{static_cast<float>(Width)};
    constexpr float HeightF{static_cast<float>(Height)};
}

// Global world layout measurements within the virtual coordinate space
namespace Config::World {
    constexpr int GroundLineY{131};
    constexpr float GroundLineYF{static_cast<float>(GroundLineY)};

    // Horizontal limits for the player's *body*, not its sprite frame. The
    // sprite is mostly transparent padding, so clamping the frame would leave a
    // visible gap at each edge of the arena.
    constexpr float ArenaMinX{0.0f};
    constexpr float ArenaMaxX{Virtual::WidthF};

    // Zombies spawn fully outside the frame and walk on, rather than popping in.
    constexpr float SpawnLeftX{-104.0f};
    constexpr float SpawnRightX{488.0f};

    // Zombies are scattered through a shallow depth band around the ground line
    // so a crowd reads as a crowd instead of one flat rank. Draw order sorts on
    // it, and crowd separation treats it as a genuine second axis.
    // Kept shallow and biased upward: the rifle fires on a flat line at a fixed
    // height, so a zombie pushed too far down the band would duck under it.
    constexpr float DepthMin{-8.0f};
    constexpr float DepthMax{2.0f};
}

// Viewport scaling and letterboxing math to fit the virtual canvas onto the physical window
namespace Config::Viewport {
    // Uniform scale factor preserving aspect ratio between virtual space and window dimensions
    constexpr float Scale = std::min(Window::WidthF / Virtual::WidthF,
                                     Window::HeightF / Virtual::HeightF);

    // Source rectangle mapping the entire virtual canvas (flipped Y for OpenGL texture coordinates)
    constexpr Rectangle SrcRect{0.0f, 0.0f, Virtual::WidthF, -Virtual::HeightF};

    // Destination rectangle centered on the physical window with letterboxing/pillarboxing applied
    constexpr Rectangle DstRect{
        (Window::WidthF - Virtual::WidthF * Scale) * 0.5f,
        (Window::HeightF - Virtual::HeightF * Scale) * 0.5f,
        Virtual::WidthF * Scale,
        Virtual::HeightF * Scale
    };
}

// Asset definitions for background parallax layers
namespace Config::Assets::Backdrop {
    // Defines metadata for a single backdrop layer image and its parallax scroll factor
    struct LayerDef {
        const char* path;
        float parallax;
    };

    constexpr LayerDef Bg{ "assets/backdrop/bg.png", 0.2f };
    constexpr LayerDef RailWall{ "assets/backdrop/rail_wall.png", 0.4f };
    constexpr LayerDef ColumnsFloor{ "assets/backdrop/columns_floor.png", 0.7f };
    constexpr LayerDef Wires{ "assets/backdrop/wires.png", 0.9f };
    constexpr LayerDef Train{ "assets/backdrop/train.png", 1.0f };

    // Consolidated array of all backdrop layer definitions in render order (back-to-front)
    inline constexpr std::array All{ Bg, RailWall, ColumnsFloor, Wires, Train };
}

// One sprite sheet, described as data. Frame size lives with the family that
// owns it, since every sheet in a family is sliced identically.
namespace Config {
    struct ClipDef {
        const char* path;
        int frameCount;
        double frameDuration;
        bool loops;
    };
}

namespace Config::Assets::Scientist {
    constexpr int FrameWidth{128};
    constexpr int FrameHeight{128};

    inline constexpr ClipDef Idle{"assets/scientist/idle.png", 6, 0.15, true};
    inline constexpr ClipDef Walk{"assets/scientist/walk.png", 12, 0.08, true};
    inline constexpr ClipDef Special{"assets/scientist/special.png", 14, 0.03, false};
    inline constexpr ClipDef Hurt{"assets/scientist/hurt.png", 3, 0.12, false};
    inline constexpr ClipDef Dead{"assets/scientist/dead.png", 4, 0.16, false};

    // Opaque body within the 128x128 frame, measured off the sheets' own alpha.
    // Collision uses this; the rest of the frame is empty padding.
    inline constexpr Rectangle Body{46.0f, 60.0f, 37.0f, 68.0f};

    constexpr float MoveSpeed{78.0f};

    // The Special sheet's muzzle flash. The bullet spawns when playback reaches
    // this frame, never on the keypress.
    // Measured off the sheet: the barrel sits at y 73..79 and its tip reaches
    // x 95, with the flash blooming out to x 110 on frames 2..4.
    constexpr int MuzzleFlashFrame{2};
    constexpr float MuzzleOffsetX{100.0f}; // from the left of the frame, facing right
    constexpr float MuzzleOffsetY{76.0f};  // from the top of the frame, barrel centre
}

namespace Config::Player {
    constexpr int MaxHealth{100};
    constexpr double InvulnerabilityDuration{0.9};

    constexpr int MagazineSize{12};
    constexpr int StartingReserve{48};
    constexpr double ReloadDuration{1.15};
    constexpr double FireCooldown{0.10};
    constexpr double EmptyFeedbackDuration{0.6};
}

namespace Config::Gameplay {
    constexpr float BulletSpeed{300.0f};
    constexpr int BulletDamage{1};
    constexpr float BulletWidth{4.0f};
    constexpr float BulletHeight{2.0f};

    // Crowd separation. A cell must be at least as wide as the largest
    // separation diameter (2 * 14) or a genuinely overlapping pair could
    // straddle two non-adjacent cells and never get tested.
    constexpr float SeparationCellSize{32.0f};
    constexpr float SeparationGridMinX{-272.0f};
    constexpr std::size_t SeparationCellCount{32};
    constexpr int SeparationIterations{2};

    constexpr std::size_t MaxZombies{512};
    constexpr std::size_t MaxBullets{256};
    constexpr std::size_t MaxPickups{32};
}

namespace Config::Pickups {
    constexpr float Width{11.0f};
    constexpr float Height{11.0f};
    constexpr int HealAmount{25};
    constexpr double Lifetime{9.0};
    constexpr double BlinkBelow{2.5}; // starts flashing with this long left
}

// ---------------------------------------------------------------------------
// Zombie types, entirely as data.
//
// One update function reads this table; nothing about a type lives in code.
// Adding a fourth type is a new enumerator plus a new row - including its
// *movement*, because the lunge is parameters (range, multiplier, durations)
// rather than a branch. A row with SurgeRange 0 simply never lunges.
// ---------------------------------------------------------------------------
namespace Config::Zombies {
    constexpr int FrameWidth{96};
    constexpr int FrameHeight{96};

    enum class Type : int { Man = 0, Wild, Woman, Count };
    constexpr int TypeCount{static_cast<int>(Type::Count)};

    struct Stats {
        const char* name;

        ClipDef walk;
        ClipDef attack;
        ClipDef dead;

        float speed;
        int maxHp;
        int contactDamage;

        Rectangle body;         // opaque body within the 96x96 frame
        float separationRadius; // crowd shoving circle, independent of the hit box
        float attackReach;      // how far the body box inflates to start a swing

        int attackDamageFrame;  // the swing lands on this frame of the attack clip
        double attackCooldown;

        float surgeRange;       // 0 disables the lunge entirely
        float surgeSpeedScale;
        double surgeDuration;
        double surgeCooldown;

        double deathLinger;     // how long a corpse stays before its slot is recycled
        float dropChance;       // odds of a health pickup on death
    };

    inline constexpr std::array<Stats, TypeCount> Table{{
        // Man - the baseline shambler. Steady, never lunges.
        Stats{
            "shambler",
            ClipDef{"assets/zombie/man/walk.png",     8, 0.11, true},
            ClipDef{"assets/zombie/man/attack_1.png", 5, 0.09, false},
            ClipDef{"assets/zombie/man/dead.png",     5, 0.13, false},
            28.0f, 3, 9,
            Rectangle{34.0f, 32.0f, 30.0f, 64.0f}, 9.0f, 6.0f,
            3, 0.75,
            0.0f, 1.0f, 0.0, 0.0,
            2.2, 0.10f
        },
        // Wild - a crawler. Fast, fragile, closes the last stretch in a lunge.
        // Uses the run sheet rather than walk: the walk pose is so low to the
        // ground that a flat rifle shot passes clean over its head. The hit box
        // is a few pixels taller than the art, which errs in the player's favour.
        Stats{
            "crawler",
            ClipDef{"assets/zombie/wild/run.png",       8, 0.07,  true},
            ClipDef{"assets/zombie/wild/attack_1.png",  4, 0.075, false},
            ClipDef{"assets/zombie/wild/dead.png",      5, 0.12,  false},
            42.0f, 2, 6,
            Rectangle{24.0f, 38.0f, 54.0f, 58.0f}, 14.0f, 4.0f,
            2, 0.55,
            78.0f, 2.6f, 0.32, 1.5,
            1.8, 0.06f
        },
        // Woman - slow, tanky, hits hard. The one you have to back away from.
        Stats{
            "screamer",
            ClipDef{"assets/zombie/woman/walk.png",     7, 0.10, true},
            ClipDef{"assets/zombie/woman/attack_1.png", 4, 0.10, false},
            ClipDef{"assets/zombie/woman/dead.png",     5, 0.13, false},
            20.0f, 6, 15,
            Rectangle{30.0f, 33.0f, 40.0f, 63.0f}, 11.0f, 6.0f,
            2, 0.95,
            0.0f, 1.0f, 0.0, 0.0,
            2.4, 0.16f
        }
    }};

    constexpr const Stats& statsFor(const Type type) {
        return Table[static_cast<std::size_t>(type)];
    }
}

// ---------------------------------------------------------------------------
// Difficulty as a curve, not a hand-authored table.
//
// Count grows linearly, the gap between spawns decays geometrically toward a
// floor, and the concurrent cap grows linearly so early waves stay readable.
// Two knobs - BudgetGrowth and IntervalDecay - shape the whole ramp.
// ---------------------------------------------------------------------------
namespace Config::Waves {
    constexpr double BreakDuration{3.5};
    constexpr double FirstBreakDuration{1.5};

    constexpr float BaseBudget{5.0f};
    constexpr float BudgetGrowth{2.5f};

    constexpr double BaseInterval{1.15};
    constexpr double IntervalDecay{0.93};
    constexpr double MinInterval{0.28};

    constexpr int BaseConcurrent{10};
    constexpr int ConcurrentGrowth{3};

    // Type mix: the shambler is always available, the others unlock and then
    // grow in weight as the wave number climbs.
    constexpr float ManBaseWeight{100.0f};
    constexpr int WomanUnlockWave{3};
    constexpr float WomanWeightGrowth{22.0f};
    constexpr int WildUnlockWave{5};
    constexpr float WildWeightGrowth{18.0f};

    constexpr int AmmoRewardBase{16};
    constexpr int AmmoRewardPerWave{2};
}
