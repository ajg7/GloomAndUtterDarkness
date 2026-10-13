#include "assets.h"

#include "raylib.h"

namespace {

AnimClip load(const Config::ClipDef& def, const int frameWidth, const int frameHeight) {
    return AnimClip{
        LoadTexture(def.path),
        frameWidth,
        frameHeight,
        def.frameCount,
        def.frameDuration,
        def.loops
    };
}

void unload(const AnimClip& clip) {
    UnloadTexture(clip.texture);
}

} // namespace

AssetStore::AssetStore() {
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

AssetStore::~AssetStore() {
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
