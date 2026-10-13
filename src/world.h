#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bullet.h"
#include "config.h"
#include "pickup.h"
#include "player.h"
#include "pool.h"
#include "rng.h"
#include "wave.h"
#include "zombie.h"

class AssetStore;
struct Input;

enum class GameState : int { Playing, GameOver };

// Everything the simulation owns.
//
// Nothing here is global and nothing is static, which is what makes reset() a
// genuine test rather than a gesture: if a value survives a restart, it is
// because it was left out of reset(), and that shows up immediately as run five
// differing from run one.
class World {
public:
    World() { reset(); }

    void reset();

    [[nodiscard]] GameState state() const { return m_state; }
    [[nodiscard]] const Player& player() const { return m_player; }
    [[nodiscard]] const WaveDirector& wave() const { return m_wave; }
    [[nodiscard]] int kills() const { return m_kills; }
    [[nodiscard]] int activeZombies() const { return m_activeZombies; }
    [[nodiscard]] std::size_t liveZombieSlots() const { return m_zombies.size(); }
    [[nodiscard]] std::size_t liveBullets() const { return m_bullets.size(); }
    [[nodiscard]] double waveBannerRemaining() const { return m_lastWaveBanner; }
    [[nodiscard]] double gameOverElapsed() const { return m_gameOverElapsed; }

    void step(const Input& input, double simDt);

    // --- rendering -------------------------------------------------------

    void draw(const AssetStore& assets);

    // Collision shapes, for the checkpoints that need to see them.
    void drawDebugShapes() const;

    // A fold of everything the simulation owns, for comparing one run against
    // another. Restarting and replaying the same inputs must reproduce the same
    // sequence of signatures; any divergence is state that reset() missed.
    [[nodiscard]] std::uint64_t stateSignature() const;

    // Drops a crowd in all at once, for the O(n^2) checkpoint.
    void spawnStressTest(int count);

private:
    static constexpr double DeathLingerDuration{1.4};
    static constexpr double WaveBannerDuration{2.0};

    struct PendingHit {
        Handle target{};
        int damage{0};
    };

    struct DrawEntry {
        float depth{0.0f};
        std::uint16_t slot{0};
    };

    // --- simulation ------------------------------------------------------

    void stepPlayer(const Input& input, double simDt);
    void stepBullets(double simDt);
    void stepZombies(double simDt);
    void resolveBulletHits();
    void applyPendingHits();
    void separateCrowd();
    void buildSeparationGrid();
    void resolveOverlap(std::uint16_t slotA, std::uint16_t slotB);
    void stepPickups(double simDt);
    void stepWaveDirector(double simDt);
    void stepDeath(double simDt);
    void spawnZombie(Config::Zombies::Type type, bool fromLeft);
    void maybeDropPickup(const Zombie& zombie);

    // --- rendering helpers -----------------------------------------------

    void buildDrawOrder();
    void drawZombie(const AssetStore& assets, std::uint16_t slot);

    Rng m_rng{};
    Player m_player{};
    Pool<Zombie, Config::Gameplay::MaxZombies> m_zombies{};
    Pool<Bullet, Config::Gameplay::MaxBullets> m_bullets{};
    Pool<Pickup, Config::Gameplay::MaxPickups> m_pickups{};
    WaveDirector m_wave{};

    GameState m_state{GameState::Playing};
    int m_kills{0};
    int m_activeZombies{0};
    int m_killsSinceDrop{0};
    double m_deathLinger{DeathLingerDuration};
    double m_lastWaveBanner{0.0};
    double m_gameOverElapsed{0.0};

    std::array<PendingHit, Config::Gameplay::MaxBullets> m_pendingHits{};
    std::size_t m_pendingHitCount{0};

    std::array<std::size_t, Config::Gameplay::SeparationCellCount> m_cellCounts{};
    std::array<std::size_t, Config::Gameplay::SeparationCellCount + 1> m_cellStart{};
    std::array<std::uint16_t, Config::Gameplay::MaxZombies> m_gridOrder{};
    std::size_t m_gridCount{0};

    std::array<DrawEntry, Config::Gameplay::MaxZombies> m_drawOrder{};
    std::size_t m_drawCount{0};
};
