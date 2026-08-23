#pragma once

#include "raylib.h"

// Streamed audio. Needs a live ScopedAudioDevice declared before it, and
// UpdateMusicStream called once per frame to keep the buffer filled.
struct ScopedMusic {
    Music music{};

    explicit ScopedMusic(const char* path)
        : music(LoadMusicStream(path)) {
        music.looping = true;
    }

    ~ScopedMusic() {
        UnloadMusicStream(music);
    }

    operator Music() const { return music; }

    ScopedMusic(const ScopedMusic&) = delete;
    ScopedMusic& operator=(const ScopedMusic&) = delete;
    ScopedMusic(ScopedMusic&&) = delete;
    ScopedMusic& operator=(ScopedMusic&&) = delete;
};
