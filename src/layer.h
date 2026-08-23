#pragma once

#include "config.h"
#include "raylib.h"

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
