#pragma once

#include <algorithm>
#include <cmath>

#include "config.h"
#include "rng.h"

// What the director wants the world to do this step.
struct WaveEvent {
    bool spawnZombie{false};
    Config::Zombies::Type type{Config::Zombies::Type::Man};
    bool fromLeft{false};
    bool waveStarted{false};
    bool waveCleared{false};
};

// Endless waves derived from the wave number, never from a table.
//
// A hand-authored table lets you fake balance for ten waves and then run out,
// which endless waves do not allow. The shape here: count climbs linearly, the
// gap between spawns decays geometrically onto a floor, and the concurrent cap
// climbs linearly so the early waves stay readable instead of arriving all at
// once. Past roughly wave 18 the spawn interval is pinned at its floor while
// the budget keeps growing, so pressure stops being about pacing and becomes
// about whether you can kill faster than they arrive - which is where a run
// ends.
class WaveDirector {
public:
    enum class Phase : int { Break, Spawning };

    void reset() {
        m_wave = 0;
        m_phase = Phase::Break;
        m_timer = Config::Waves::FirstBreakDuration;
        m_spawned = 0;
        m_budget = 0;
        m_interval = Config::Waves::BaseInterval;
        m_concurrentCap = Config::Waves::BaseConcurrent;
    }

    [[nodiscard]] int wave() const { return m_wave; }
    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] double breakRemaining() const { return m_phase == Phase::Break ? m_timer : 0.0; }
    [[nodiscard]] int remainingInWave() const { return m_budget - m_spawned; }

    // --- the curve, as pure functions so they can be reasoned about and tested ---

    static int budgetFor(const int wave) {
        return static_cast<int>(Config::Waves::BaseBudget
                              + Config::Waves::BudgetGrowth * static_cast<float>(wave - 1));
    }

    static double intervalFor(const int wave) {
        return std::max(Config::Waves::MinInterval,
                        Config::Waves::BaseInterval * std::pow(Config::Waves::IntervalDecay, wave - 1));
    }

    static int concurrentCapFor(const int wave) {
        const int cap = Config::Waves::BaseConcurrent + Config::Waves::ConcurrentGrowth * (wave - 1);
        return std::min(cap, static_cast<int>(Config::Gameplay::MaxZombies));
    }

    // Weight per type for a given wave. Types unlock, then grow in share.
    static float weightFor(const Config::Zombies::Type type, const int wave) {
        switch (type) {
            case Config::Zombies::Type::Man:
                return Config::Waves::ManBaseWeight;
            case Config::Zombies::Type::Woman:
                return std::max(0, wave - Config::Waves::WomanUnlockWave + 1)
                     * Config::Waves::WomanWeightGrowth;
            case Config::Zombies::Type::Wild:
                return std::max(0, wave - Config::Waves::WildUnlockWave + 1)
                     * Config::Waves::WildWeightGrowth;
            default:
                return 0.0f;
        }
    }

    static Config::Zombies::Type pickType(Rng& rng, const int wave) {
        float total = 0.0f;
        for (int i = 0; i < Config::Zombies::TypeCount; ++i) {
            total += weightFor(static_cast<Config::Zombies::Type>(i), wave);
        }
        if (total <= 0.0f) return Config::Zombies::Type::Man;

        float roll = rng.nextFloat() * total;
        for (int i = 0; i < Config::Zombies::TypeCount; ++i) {
            roll -= weightFor(static_cast<Config::Zombies::Type>(i), wave);
            if (roll <= 0.0f) return static_cast<Config::Zombies::Type>(i);
        }
        return Config::Zombies::Type::Man;
    }

    // activeZombies counts the living only; corpses linger in their slots and
    // must not hold a wave open.
    WaveEvent step(const double simDt, const int activeZombies, Rng& rng) {
        WaveEvent event{};

        if (m_phase == Phase::Break) {
            m_timer -= simDt;
            if (m_timer <= 0.0) beginWave(event);
            return event;
        }

        if (m_spawned < m_budget) {
            if (m_timer > 0.0) m_timer -= simDt;

            // Holding at or below zero until there is room means a full field
            // does not silently burn through the wave's spawn budget.
            if (m_timer <= 0.0 && activeZombies < m_concurrentCap) {
                m_timer = m_interval;
                ++m_spawned;
                event.spawnZombie = true;
                event.type = pickType(rng, m_wave);
                event.fromLeft = rng.chance(0.5f);
            }
            return event;
        }

        if (activeZombies == 0) {
            m_phase = Phase::Break;
            m_timer = Config::Waves::BreakDuration;
            event.waveCleared = true;
        }
        return event;
    }

private:
    void beginWave(WaveEvent& event) {
        ++m_wave;
        m_phase = Phase::Spawning;
        m_spawned = 0;
        m_budget = budgetFor(m_wave);
        m_interval = intervalFor(m_wave);
        m_concurrentCap = concurrentCapFor(m_wave);
        m_timer = 0.0; // first zombie of a wave arrives immediately
        event.waveStarted = true;
    }

    int m_wave{0};
    Phase m_phase{Phase::Break};
    double m_timer{Config::Waves::FirstBreakDuration};
    int m_spawned{0};
    int m_budget{0};
    double m_interval{Config::Waves::BaseInterval};
    int m_concurrentCap{Config::Waves::BaseConcurrent};
};
