#pragma once

#include <array>
#include <cstddef>
#include <tuple>

#include "config.h"
#include "raylib.h"

// One parallax layer of the backdrop. Plain data: the owning ScopedLayers
// below is what loads and unloads the texture.
struct Layer
{
    Texture2D texture;
    float parallax;

    static Layer load(const Config::Assets::Backdrop::LayerDef& def) {
        return Layer{ LoadTexture(def.path), def.parallax };
    }

    void draw(const float cameraOffsetX = 0.0f) const {
        const float x = -cameraOffsetX * parallax; // Horizontal offset scaled by the layer's parallax factor
        const Rectangle src{ 0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height) };
        const Rectangle dst{ x, 0.0f, Config::Virtual::WidthF, Config::Virtual::HeightF };

        DrawTexturePro(texture, src, dst, {}, 0.0f, WHITE);
    }
};

template <std::size_t N>
struct ScopedLayers {
    std::array<Layer, N> layers;

    template <typename T>
    constexpr explicit ScopedLayers(const std::array<T, N>& defArray)
        : layers(std::apply([](const auto&... defs) {
              return std::array{ Layer::load(defs)... };
          }, defArray)) {}

    ~ScopedLayers() {
        for (const auto& layer : layers) {
            UnloadTexture(layer.texture);
        }
    }

    ScopedLayers(const ScopedLayers&) = delete;
    ScopedLayers& operator=(const ScopedLayers&) = delete;
    ScopedLayers(ScopedLayers&&) = delete;
    ScopedLayers& operator=(ScopedLayers&&) = delete;

    [[nodiscard]] auto begin() const { return layers.begin(); }
    [[nodiscard]] auto end() const { return layers.end(); }
};
