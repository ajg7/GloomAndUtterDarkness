#pragma once

#include <array>

#include "animation.h"
#include "config.h"
#include "raylib.h"

// Every clip a zombie type can play. One of these per type, not per zombie.
struct ZombieClips {
    AnimClip walk{};
    AnimClip attack{};
    AnimClip dead{};
};

struct ScientistClips {
    AnimClip idle{};
    AnimClip walk{};
    AnimClip special{};
    AnimClip hurt{};
    AnimClip dead{};
};

// Load-once, shared, outlives every entity.
//
// This exists because pooled entities cannot own textures. A 512-slot zombie
// array that each loaded its own sheet would upload the same PNG 512 times, and
// releasing a slot would unload a texture that other live slots are still
// drawing from. Entities hold nothing but a type tag and read their clips from
// here.
class AssetStore {
public:
    AssetStore() {
        m_scientist.idle    = load(Config::Assets::Scientist::Idle,    Config::Assets::Scientist::FrameWidth, Config::Assets::Scientist::FrameHeight);
        m_scientist.walk    = load(Config::Assets::Scientist::Walk,    Config::Assets::Scientist::FrameWidth, Config::Assets::Scientist::FrameHeight);
        m_scientist.special = load(Config::Assets::Scientist::Special, Config::Assets::Scientist::FrameWidth, Config::Assets::Scientist::FrameHeight);
        m_scientist.hurt    = load(Config::Assets::Scientist::Hurt,    Config::Assets::Scientist::FrameWidth, Config::Assets::Scientist::FrameHeight);
        m_scientist.dead    = load(Config::Assets::Scientist::Dead,    Config::Assets::Scientist::FrameWidth, Config::Assets::Scientist::FrameHeight);

        for (int i = 0; i < Config::Zombies::TypeCount; ++i) {
            const auto& stats = Config::Zombies::Table[static_cast<std::size_t>(i)];
            m_zombies[static_cast<std::size_t>(i)] = ZombieClips{
                load(stats.walk,   Config::Zombies::FrameWidth, Config::Zombies::FrameHeight),
                load(stats.attack, Config::Zombies::FrameWidth, Config::Zombies::FrameHeight),
                load(stats.dead,   Config::Zombies::FrameWidth, Config::Zombies::FrameHeight)
            };
        }
    }

    ~AssetStore() {
        unload(m_scientist.idle);
        unload(m_scientist.walk);
        unload(m_scientist.special);
        unload(m_scientist.hurt);
        unload(m_scientist.dead);

        for (auto& clips : m_zombies) {
            unload(clips.walk);
            unload(clips.attack);
            unload(clips.dead);
        }
    }

    AssetStore(const AssetStore&) = delete;
    AssetStore& operator=(const AssetStore&) = delete;
    AssetStore(AssetStore&&) = delete;
    AssetStore& operator=(AssetStore&&) = delete;

    [[nodiscard]] const ScientistClips& scientist() const { return m_scientist; }

    [[nodiscard]] const ZombieClips& zombie(const Config::Zombies::Type type) const {
        return m_zombies[static_cast<std::size_t>(type)];
    }

private:
    static AnimClip load(const Config::ClipDef& def, const int frameWidth, const int frameHeight) {
        return AnimClip{
            LoadTexture(def.path),
            frameWidth,
            frameHeight,
            def.frameCount,
            def.frameDuration,
            def.loops
        };
    }

    static void unload(const AnimClip& clip) {
        UnloadTexture(clip.texture);
    }

    ScientistClips m_scientist{};
    std::array<ZombieClips, Config::Zombies::TypeCount> m_zombies{};
};
