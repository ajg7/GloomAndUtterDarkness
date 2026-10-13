#pragma once

#include "raylib.h"

// ---------------------------------------------------------------------------
// RAII owners for raylib's C handles and begin/end pairs.
//
// Each one is the same shape - acquire in the constructor, release in the
// destructor, no copies, no moves - so they live together rather than one per
// file. Declaration order in main() is teardown order in reverse, which is
// what keeps the audio device alive until the music stream is gone.
// ---------------------------------------------------------------------------

struct ScopedWindow {
    ScopedWindow(const int width, const int height, const char* title) {
        InitWindow(width, height, title);
    }

    ~ScopedWindow() {
        CloseWindow();
    }

    ScopedWindow(const ScopedWindow&) = delete;
    ScopedWindow& operator=(const ScopedWindow&) = delete;
    ScopedWindow(ScopedWindow&&) = delete;
    ScopedWindow& operator=(ScopedWindow&&) = delete;
};

struct ScopedAudioDevice {
    ScopedAudioDevice() {
        InitAudioDevice();
    }

    ~ScopedAudioDevice() {
        CloseAudioDevice();
    }

    ScopedAudioDevice(const ScopedAudioDevice&) = delete;
    ScopedAudioDevice& operator=(const ScopedAudioDevice&) = delete;
    ScopedAudioDevice(ScopedAudioDevice&&) = delete;
    ScopedAudioDevice& operator=(ScopedAudioDevice&&) = delete;
};

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

struct ScopedRenderTexture {
    RenderTexture2D texture{};

    ScopedRenderTexture(const int width, const int height)
        : texture(LoadRenderTexture(width, height)) {}

    ~ScopedRenderTexture() {
        UnloadRenderTexture(texture);
    }

    operator RenderTexture2D() const { return texture; }

    const Texture2D& getTexture() const { return texture.texture; }

    ScopedRenderTexture(const ScopedRenderTexture&) = delete;
    ScopedRenderTexture& operator=(const ScopedRenderTexture&) = delete;
    ScopedRenderTexture(ScopedRenderTexture&&) = delete;
    ScopedRenderTexture& operator=(ScopedRenderTexture&&) = delete;
};

class ScopedDrawing {
public:
    ScopedDrawing() {
        BeginDrawing();
    }

    ~ScopedDrawing() {
        EndDrawing();
    }

    ScopedDrawing(const ScopedDrawing&) = delete;
    ScopedDrawing& operator=(const ScopedDrawing&) = delete;
    ScopedDrawing(ScopedDrawing&&) = delete;
    ScopedDrawing& operator=(ScopedDrawing&&) = delete;
};

class ScopedTextureMode {
public:
    explicit ScopedTextureMode(const RenderTexture2D& target) {
        BeginTextureMode(target);
    }

    ~ScopedTextureMode() {
        EndTextureMode();
    }

    ScopedTextureMode(const ScopedTextureMode&) = delete;
    ScopedTextureMode& operator=(const ScopedTextureMode&) = delete;
    ScopedTextureMode(ScopedTextureMode&&) = delete;
    ScopedTextureMode& operator=(ScopedTextureMode&&) = delete;
};
