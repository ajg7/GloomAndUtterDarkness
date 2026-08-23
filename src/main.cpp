#include <array>
#include <cstdlib>

#include "raylib.h"

#include "assets.h"
#include "config.h"
#include "frameRateMonitor.h"
#include "hud.h"
#include "input.h"
#include "scopedDrawing.h"
#include "scopedLayers.h"
#include "scopedRenderTexture.h"
#include "scopedTextureMode.h"
#include "scopedWindow.h"
#include "world.h"

namespace {

// Framerate caps the MVP has to survive unchanged. F3 cycles them so the
// "identical at 20 and at 300" claim can be checked without a rebuild.
constexpr std::array<int, 4> FpsCaps{60, 20, 300, 0};

} // namespace

int main(void)
{
    // Window
    const ScopedWindow window{Config::Window::Width, Config::Window::Height, "Gloom and Utter Darkness"};
    SetTargetFPS(Config::Window::TargetFPS);
    SetExitKey(KEY_ESCAPE);

    // Virtual Canvas
    const ScopedRenderTexture target{Config::Virtual::Width, Config::Virtual::Height};
    SetTextureFilter(target.getTexture(), TEXTURE_FILTER_POINT);

    // Background Layers
    const ScopedLayers backgroundLayers{Config::Assets::Backdrop::All};

    // Sprite sheets, loaded once and shared by every entity that plays them.
    const AssetStore assets{};

    World world{};
    InputLatch input{};
    FrameRateMonitor frameRateMonitor{};

    bool debugOverlay{false};
    std::size_t fpsCapIndex{0};

    // Game Loop
    while (not WindowShouldClose())
    {
        input.pollFrame();

        if (IsKeyPressed(KEY_F1)) debugOverlay = !debugOverlay;
        if (IsKeyPressed(KEY_F2)) world.spawnStressTest(200);
        if (IsKeyPressed(KEY_F3)) {
            fpsCapIndex = (fpsCapIndex + 1) % FpsCaps.size();
            SetTargetFPS(FpsCaps[fpsCapIndex]);
        }

        // Simulation, at a fixed rate independent of how fast we are drawing.
        // Each step consumes its own input snapshot so a single keypress cannot
        // be applied twice on a slow frame, or dropped on a fast one.
        const int simSteps = frameRateMonitor.update(GetFrameTime());
        for (int step = 0; step < simSteps; ++step) {
            world.step(input.takeStep(), FrameRateMonitor::SIM_DT);
        }

        // -------------------------------------------------------------------------
        // Virtual Canvas Pass
        // Render all game content into the low-resolution virtual render texture.
        // -------------------------------------------------------------------------
        {
            const ScopedTextureMode textureMode{target};
            ClearBackground(BLACK);

            for (const auto& layer : backgroundLayers) {
                layer.draw();
            }

            world.draw(assets);

            if (debugOverlay) {
                world.drawDebugShapes();
                DrawLine(0, Config::World::GroundLineY, Config::Virtual::Width, Config::World::GroundLineY,
                         Color{200, 60, 60, 140});
                frameRateMonitor.drawFrameRateText();
                Hud::drawDebug(world, static_cast<int>(FrameRateMonitor::SIM_HZ));
            }

            Hud::draw(world);
        }

        // -------------------------------------------------------------------------
        // Main Window Upscale Pass
        // Scales the virtual canvas up to the physical window size with letterboxing applied
        // -------------------------------------------------------------------------
        {
            const ScopedDrawing drawing;
            ClearBackground(BLACK);

            DrawTexturePro(
                target.getTexture(),
                Config::Viewport::SrcRect,
                Config::Viewport::DstRect,
                {},
                0.0f,
                WHITE
            );
        }
    }

    return EXIT_SUCCESS;
}
