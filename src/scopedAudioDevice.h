#pragma once

#include "raylib.h"

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
