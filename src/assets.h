#pragma once

#include <array>
#include <cstddef>

#include "animation.h"
#include "config.h"

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
    AssetStore();
    ~AssetStore();

    AssetStore(const AssetStore&) = delete;
    AssetStore& operator=(const AssetStore&) = delete;
    AssetStore(AssetStore&&) = delete;
    AssetStore& operator=(AssetStore&&) = delete;

    [[nodiscard]] const ScientistClips& scientist() const { return m_scientist; }

    [[nodiscard]] const ZombieClips& zombie(const Config::Zombies::Type type) const {
        return m_zombies[static_cast<std::size_t>(type)];
    }

private:
    ScientistClips m_scientist{};
    std::array<ZombieClips, Config::Zombies::TypeCount> m_zombies{};
};
