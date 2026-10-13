#pragma once

#include "config.h"

struct Rng;

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

    void reset();

    [[nodiscard]] int wave() const { return m_wave; }
    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] double breakRemaining() const { return m_phase == Phase::Break ? m_timer : 0.0; }
    [[nodiscard]] int remainingInWave() const { return m_budget - m_spawned; }

    // --- the curve, as pure functions so they can be reasoned about and tested ---

    static int budgetFor(int wave);
    static double intervalFor(int wave);
    static int concurrentCapFor(int wave);

    // Weight per type for a given wave. Types unlock, then grow in share.
    static float weightFor(Config::Zombies::Type type, int wave);

    static Config::Zombies::Type pickType(Rng& rng, int wave);

    // activeZombies counts the living only; corpses linger in their slots and
    // must not hold a wave open.
    WaveEvent step(double simDt, int activeZombies, Rng& rng);

private:
    void beginWave(WaveEvent& event);

    int m_wave{0};
    Phase m_phase{Phase::Break};
    double m_timer{Config::Waves::FirstBreakDuration};
    int m_spawned{0};
    int m_budget{0};
    double m_interval{Config::Waves::BaseInterval};
    int m_concurrentCap{Config::Waves::BaseConcurrent};
};
