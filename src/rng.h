#pragma once

#include <cstdint>

// Deterministic pseudo-random source.
//
// Every random decision in the simulation (spawn side, type mix, drop rolls,
// crowd depth) draws from one of these. Restart reseeds it to a fixed value,
// which is the whole reason wave 1 of the fifth run is identical to wave 1 of
// the first. Nothing in the game may call rand() or GetRandomValue().
struct Rng {
    static constexpr std::uint32_t DefaultSeed{0x9E3779B9u};

    std::uint32_t state{DefaultSeed};

    void reseed(const std::uint32_t seed = DefaultSeed) {
        state = (seed == 0u) ? DefaultSeed : seed; // xorshift is dead at zero
    }

    // xorshift32 — small, fast, and fully reproducible across platforms because
    // it only uses shifts and xors on a fixed-width unsigned type.
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform in [0, 1). Uses the top 24 bits, which are the well-mixed ones.
    float nextFloat() {
        return static_cast<float>(next() >> 8) * 0x1.0p-24f;
    }

    float nextRange(const float lo, const float hi) {
        return lo + (hi - lo) * nextFloat();
    }

    // Uniform in [lo, hi], inclusive on both ends.
    int nextInt(const int lo, const int hi) {
        if (hi <= lo) return lo;
        const std::uint32_t span = static_cast<std::uint32_t>(hi - lo) + 1u;
        return lo + static_cast<int>(next() % span);
    }

    bool chance(const float probability) {
        return nextFloat() < probability;
    }
};
