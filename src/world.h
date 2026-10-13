#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "assets.h"
#include "bullet.h"
#include "collision.h"
#include "config.h"
#include "input.h"
#include "pickup.h"
#include "player.h"
#include "pool.h"
#include "rng.h"
#include "sprite.h"
#include "wave.h"
#include "zombie.h"

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

    void reset() {
        m_rng.reseed();
        m_player.reset();
        m_zombies.reset();
        m_bullets.reset();
        m_pickups.reset();
        m_wave.reset();
        m_state = GameState::Playing;
        m_kills = 0;
        m_activeZombies = 0;
        m_killsSinceDrop = 0;
        m_deathLinger = DeathLingerDuration;
        m_lastWaveBanner = 0.0;
        m_gameOverElapsed = 0.0;
    }

    [[nodiscard]] GameState state() const { return m_state; }
    [[nodiscard]] const Player& player() const { return m_player; }
    [[nodiscard]] const WaveDirector& wave() const { return m_wave; }
    [[nodiscard]] int kills() const { return m_kills; }
    [[nodiscard]] int activeZombies() const { return m_activeZombies; }
    [[nodiscard]] std::size_t liveZombieSlots() const { return m_zombies.size(); }
    [[nodiscard]] std::size_t liveBullets() const { return m_bullets.size(); }
    [[nodiscard]] double waveBannerRemaining() const { return m_lastWaveBanner; }
    [[nodiscard]] double gameOverElapsed() const { return m_gameOverElapsed; }

    void step(const Input& input, const double simDt) {
        if (m_state == GameState::GameOver) {
            // Kept ticking while the simulation is frozen: the epitaph writes
            // itself on over a few seconds, and it needs a clock that is still
            // running. Cosmetic only, so it stays out of stateSignature().
            m_gameOverElapsed += simDt;
            if (input.restart) reset();
            return;
        }

        if (m_lastWaveBanner > 0.0) m_lastWaveBanner -= simDt;

        stepPlayer(input, simDt);
        stepBullets(simDt);
        stepZombies(simDt);
        resolveBulletHits();
        separateCrowd();
        stepPickups(simDt);
        stepWaveDirector(simDt);
        stepDeath(simDt);
    }

    // --- rendering -------------------------------------------------------

    void draw(const AssetStore& assets) {
        for (std::size_t slot = 0; slot < m_pickups.capacity; ++slot) {
            if (m_pickups.alive(slot)) m_pickups.at(slot).draw();
        }

        buildDrawOrder();

        // Zombies behind the player, then the player, then those in front of
        // him. Depth is the same axis crowd separation pushes along, so what
        // you see matches what the shoving is doing.
        std::size_t index = 0;
        for (; index < m_drawCount && m_zombies.at(m_drawOrder[index].slot).depth < 0.0f; ++index) {
            drawZombie(assets, m_drawOrder[index].slot);
        }

        m_player.draw(assets.scientist());

        for (; index < m_drawCount; ++index) {
            drawZombie(assets, m_drawOrder[index].slot);
        }

        for (std::size_t slot = 0; slot < m_bullets.capacity; ++slot) {
            if (m_bullets.alive(slot)) m_bullets.at(slot).draw();
        }
    }

    // Collision shapes, for the checkpoints that need to see them.
    void drawDebugShapes() const {
        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;
            const Zombie& zombie = m_zombies.at(slot);
            if (zombie.isDead()) continue;

            const Rectangle box = zombie.body();
            DrawRectangleLines(static_cast<int>(box.x), static_cast<int>(box.y),
                               static_cast<int>(box.width), static_cast<int>(box.height),
                               Color{255, 80, 80, 160});

            const Vector2 centre = zombie.separationCentre();
            DrawCircleLines(static_cast<int>(centre.x), static_cast<int>(centre.y),
                            zombie.stats().separationRadius, Color{80, 160, 255, 160});
        }

        const Rectangle body = m_player.body();
        DrawRectangleLines(static_cast<int>(body.x), static_cast<int>(body.y),
                           static_cast<int>(body.width), static_cast<int>(body.height),
                           Color{120, 255, 120, 200});
    }

    // A fold of everything the simulation owns, for comparing one run against
    // another. Restarting and replaying the same inputs must reproduce the same
    // sequence of signatures; any divergence is state that reset() missed.
    [[nodiscard]] std::uint64_t stateSignature() const {
        std::uint64_t hash = 1469598103934665603ull;
        const auto mix = [&hash](const std::uint64_t value) {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        const auto mixFloat = [&mix](const float value) {
            // Quantised so the fold compares positions, not float noise.
            mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(value * 64.0f)));
        };

        mix(static_cast<std::uint64_t>(m_kills));
        mix(static_cast<std::uint64_t>(m_wave.wave()));
        mix(static_cast<std::uint64_t>(m_activeZombies));
        mix(static_cast<std::uint64_t>(m_state == GameState::GameOver ? 1 : 0));
        mix(static_cast<std::uint64_t>(m_player.health));
        mix(static_cast<std::uint64_t>(m_player.magazine));
        mix(static_cast<std::uint64_t>(m_player.reserve));
        mix(static_cast<std::uint64_t>(m_player.state));
        mixFloat(m_player.position.x);
        mix(m_rng.state);
        mix(static_cast<std::uint64_t>(m_killsSinceDrop));

        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;
            const Zombie& zombie = m_zombies.at(slot);
            mix(static_cast<std::uint64_t>(slot));
            mix(static_cast<std::uint64_t>(zombie.type));
            mix(static_cast<std::uint64_t>(zombie.hp));
            mixFloat(zombie.x);
            mixFloat(zombie.depth);
        }
        return hash;
    }

    // Drops a crowd in all at once, for the O(n^2) checkpoint.
    void spawnStressTest(const int count) {
        for (int i = 0; i < count; ++i) {
            const auto type = static_cast<Config::Zombies::Type>(i % Config::Zombies::TypeCount);
            const bool fromLeft = (i % 2) == 0;
            spawnZombie(type, fromLeft);
        }
    }

private:
    static constexpr double DeathLingerDuration{1.4};
    static constexpr float DespawnMinX{-260.0f};
    static constexpr float DespawnMaxX{Config::Virtual::WidthF + 260.0f};

    struct PendingHit {
        Handle target{};
        int damage{0};
    };

    struct DrawEntry {
        float depth{0.0f};
        std::uint16_t slot{0};
    };

    // --- simulation ------------------------------------------------------

    void stepPlayer(const Input& input, const double simDt) {
        if (auto spawned = m_player.update(input, simDt)) {
            (void)m_bullets.spawn(*spawned);
        }
    }

    void stepBullets(const double simDt) {
        for (std::size_t slot = 0; slot < m_bullets.capacity; ++slot) {
            if (m_bullets.alive(slot)) m_bullets.at(slot).update(simDt);
        }
    }

    void stepZombies(const double simDt) {
        const Rectangle playerBody = m_player.body();
        const bool playerAlive = m_player.alive();

        m_activeZombies = 0;

        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;

            Zombie& zombie = m_zombies.at(slot);
            const ZombieStep result = zombie.update(simDt, playerBody, playerAlive);

            if (result.dealsDamage) {
                m_player.takeDamage(zombie.stats().contactDamage);
            }

            if (result.expired || zombie.x < DespawnMinX || zombie.x > DespawnMaxX) {
                m_zombies.releaseSlot(slot);
                continue;
            }

            if (!zombie.isDead()) ++m_activeZombies;
        }
    }

    // Bullets are swept against every living zombie and resolved at the
    // earliest entry time, so a shot that would cross two bodies in one step
    // hits the near one. Hits are recorded as handles and applied afterwards
    // rather than inline: mutating the zombie pool while sweeping it is exactly
    // the churn that stale references are born in.
    void resolveBulletHits() {
        m_pendingHitCount = 0;

        for (std::size_t bulletSlot = 0; bulletSlot < m_bullets.capacity; ++bulletSlot) {
            if (!m_bullets.alive(bulletSlot)) continue;

            const Bullet& bullet = m_bullets.at(bulletSlot);

            float earliest = 2.0f;
            std::size_t hitSlot = m_zombies.capacity;

            for (std::size_t zombieSlot = 0; zombieSlot < m_zombies.capacity; ++zombieSlot) {
                if (!m_zombies.alive(zombieSlot)) continue;

                const Zombie& zombie = m_zombies.at(zombieSlot);
                if (zombie.isDead()) continue;

                const Rectangle expanded =
                    Collision::expand(zombie.body(), Bullet::HalfWidth, Bullet::HalfHeight);

                if (const auto entry = Collision::sweptAABB(bullet.prevPosition, bullet.position, expanded)) {
                    if (*entry < earliest) {
                        earliest = *entry;
                        hitSlot = zombieSlot;
                    }
                }
            }

            if (hitSlot < m_zombies.capacity) {
                if (m_pendingHitCount < m_pendingHits.size()) {
                    m_pendingHits[m_pendingHitCount++] =
                        PendingHit{m_zombies.handleAt(hitSlot), Config::Gameplay::BulletDamage};
                }
                m_bullets.releaseSlot(bulletSlot);
            } else if (bullet.offscreen()) {
                m_bullets.releaseSlot(bulletSlot);
            }
        }

        applyPendingHits();
    }

    void applyPendingHits() {
        for (std::size_t i = 0; i < m_pendingHitCount; ++i) {
            // The generation check earns its keep here: between recording the
            // hit and applying it, the target may have died and had its slot
            // reused by a fresh spawn. A bare index would damage the newcomer.
            Zombie* zombie = m_zombies.get(m_pendingHits[i].target);
            if (zombie == nullptr) continue;

            if (zombie->takeDamage(m_pendingHits[i].damage)) {
                ++m_kills;
                --m_activeZombies;
                maybeDropPickup(*zombie);
            }
        }
        m_pendingHitCount = 0;
    }

    // Pairwise penetration resolution over a uniform grid along x.
    //
    // All-pairs was free at N = 1 and is not at N = 500. Bucketing by x means
    // each zombie is only tested against its own cell and the next one, which
    // is sound precisely because a cell is wider than the largest separation
    // diameter - two circles that overlap can never be more than one cell apart.
    void separateCrowd() {
        buildSeparationGrid();
        if (m_gridCount < 2) return;

        for (int iteration = 0; iteration < Config::Gameplay::SeparationIterations; ++iteration) {
            for (std::size_t cell = 0; cell < Config::Gameplay::SeparationCellCount; ++cell) {
                const std::size_t begin = m_cellStart[cell];
                const std::size_t end = m_cellStart[cell + 1];

                for (std::size_t i = begin; i < end; ++i) {
                    for (std::size_t j = i + 1; j < end; ++j) {
                        resolveOverlap(m_gridOrder[i], m_gridOrder[j]);
                    }

                    if (cell + 1 < Config::Gameplay::SeparationCellCount) {
                        const std::size_t nextEnd = m_cellStart[cell + 2];
                        for (std::size_t j = end; j < nextEnd; ++j) {
                            resolveOverlap(m_gridOrder[i], m_gridOrder[j]);
                        }
                    }
                }
            }
        }
    }

    void buildSeparationGrid() {
        m_cellCounts.fill(0);
        m_gridCount = 0;

        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;
            if (m_zombies.at(slot).isDead()) continue; // corpses do not shove
            ++m_cellCounts[cellOf(m_zombies.at(slot).separationCentre().x)];
            ++m_gridCount;
        }

        std::size_t running = 0;
        for (std::size_t cell = 0; cell < Config::Gameplay::SeparationCellCount; ++cell) {
            m_cellStart[cell] = running;
            running += m_cellCounts[cell];
        }
        m_cellStart[Config::Gameplay::SeparationCellCount] = running;

        auto cursor = m_cellStart;
        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;
            if (m_zombies.at(slot).isDead()) continue;
            const std::size_t cell = cellOf(m_zombies.at(slot).separationCentre().x);
            m_gridOrder[cursor[cell]++] = static_cast<std::uint16_t>(slot);
        }
    }

    static std::size_t cellOf(const float x) {
        const float local = (x - Config::Gameplay::SeparationGridMinX) / Config::Gameplay::SeparationCellSize;
        if (local <= 0.0f) return 0;
        const auto cell = static_cast<std::size_t>(local);
        return std::min(cell, Config::Gameplay::SeparationCellCount - 1);
    }

    void resolveOverlap(const std::uint16_t slotA, const std::uint16_t slotB) {
        Zombie& a = m_zombies.at(slotA);
        Zombie& b = m_zombies.at(slotB);

        const Vector2 centreA = a.separationCentre();
        const Vector2 centreB = b.separationCentre();
        const float minDistance = a.stats().separationRadius + b.stats().separationRadius;

        float dx = centreB.x - centreA.x;
        float dy = centreB.y - centreA.y;
        const float squared = dx * dx + dy * dy;
        if (squared >= minDistance * minDistance) return;

        float distance = std::sqrt(squared);
        if (distance < 1e-4f) {
            // Exactly coincident. Break the tie on slot order rather than
            // randomly, so the resolution stays reproducible across runs.
            dx = (slotA < slotB) ? -1.0f : 1.0f;
            dy = 0.0f;
            distance = 1.0f;
        }

        // Half the penetration each, along the line between centres.
        const float push = (minDistance - distance) * 0.5f;
        const float nx = dx / distance;
        const float ny = dy / distance;

        a.x -= nx * push;
        b.x += nx * push;
        a.depth = std::clamp(a.depth - ny * push, Config::World::DepthMin, Config::World::DepthMax);
        b.depth = std::clamp(b.depth + ny * push, Config::World::DepthMin, Config::World::DepthMax);
    }

    void stepPickups(const double simDt) {
        const Rectangle body = m_player.body();
        const Vector2 centreNow = Collision::center(body);
        const Vector2 centreBefore = m_player.prevBodyCentre;

        for (std::size_t slot = 0; slot < m_pickups.capacity; ++slot) {
            if (!m_pickups.alive(slot)) continue;

            Pickup& pickup = m_pickups.at(slot);
            if (!pickup.update(simDt)) {
                m_pickups.releaseSlot(slot);
                continue;
            }

            if (!m_player.alive()) continue;

            // Same sweep the rifle uses. The player crosses MoveSpeed * dt per
            // step; an overlap check would only sample the end of that motion
            // and could step clean over a drop narrower than the stride.
            const Rectangle expanded = Collision::expand(pickup.box(), body.width * 0.5f, body.height * 0.5f);
            if (Collision::sweptAABB(centreBefore, centreNow, expanded)) {
                m_player.heal(Config::Pickups::HealAmount);
                m_pickups.releaseSlot(slot);
            }
        }
    }

    void stepWaveDirector(const double simDt) {
        const WaveEvent event = m_wave.step(simDt, m_activeZombies, m_rng);

        if (event.spawnZombie) spawnZombie(event.type, event.fromLeft);
        if (event.waveStarted) m_lastWaveBanner = WaveBannerDuration;
        if (event.waveCleared) {
            m_player.grantAmmo(Config::Waves::AmmoRewardBase
                             + Config::Waves::AmmoRewardPerWave * m_wave.wave());
        }
    }

    void stepDeath(const double simDt) {
        if (m_player.alive()) return;

        m_deathLinger -= simDt;
        if (m_deathLinger <= 0.0) m_state = GameState::GameOver;
    }

    void spawnZombie(const Config::Zombies::Type type, const bool fromLeft) {
        const float depth = m_rng.nextRange(Config::World::DepthMin, Config::World::DepthMax);
        const float x = fromLeft ? Config::World::SpawnLeftX : Config::World::SpawnRightX;
        (void)m_zombies.spawn(Zombie::spawned(type, x, depth, !fromLeft));
    }

    void maybeDropPickup(const Zombie& zombie) {
        // Always roll, so the RNG stream does not depend on the pity counter.
        const bool rolled = m_rng.chance(zombie.stats().dropChance);
        if (!rolled && ++m_killsSinceDrop < Config::Pickups::PityKills) return;
        m_killsSinceDrop = 0;

        const Rectangle box = zombie.body();
        (void)m_pickups.spawn(Pickup::dropped(
            box.x + box.width * 0.5f,
            Config::World::GroundLineYF - Config::Pickups::Height * 0.5f - 2.0f));
    }

    // --- rendering helpers -----------------------------------------------

    void buildDrawOrder() {
        m_drawCount = 0;
        for (std::size_t slot = 0; slot < m_zombies.capacity; ++slot) {
            if (!m_zombies.alive(slot)) continue;
            m_drawOrder[m_drawCount++] =
                DrawEntry{m_zombies.at(slot).depth, static_cast<std::uint16_t>(slot)};
        }

        std::sort(m_drawOrder.begin(), m_drawOrder.begin() + static_cast<std::ptrdiff_t>(m_drawCount),
                  [](const DrawEntry& a, const DrawEntry& b) {
                      if (a.depth != b.depth) return a.depth < b.depth;
                      return a.slot < b.slot; // stable and reproducible
                  });
    }

    void drawZombie(const AssetStore& assets, const std::uint16_t slot) {
        const Zombie& zombie = m_zombies.at(slot);
        zombie.draw(assets.zombie(zombie.type));
    }

    static constexpr double WaveBannerDuration{2.0};

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
