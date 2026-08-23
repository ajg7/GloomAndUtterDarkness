#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

// A stable reference into a Pool.
//
// The slot index alone cannot distinguish "the zombie I meant" from "whoever
// occupies that slot now" — that is exactly the bug generation counters exist
// to catch. A handle only resolves when both halves match.
//
// Generation 0 is never handed out, so a default-constructed Handle{} is
// permanently stale. That makes "no target" representable without a sentinel
// index and without an extra bool.
struct Handle {
    std::uint16_t index{0};
    std::uint16_t generation{0};

    [[nodiscard]] constexpr bool isNull() const { return generation == 0u; }

    friend constexpr bool operator==(Handle, Handle) = default;
};

// Fixed-capacity storage with slot reuse and stale-handle detection.
//
// T must be default-constructible and assignable: a released slot keeps its
// object alive but dead, and spawn() overwrites it. That constraint is why
// entities in this game hold references to shared assets rather than owning
// textures themselves.
template <typename T, std::size_t Capacity>
class Pool {
public:
    static constexpr std::size_t capacity{Capacity};

    Pool() { reset(); }

    // Wipes items, alive flags, the free list, AND generation counters. The
    // generation reset is what makes a restart bit-for-bit identical rather
    // than merely "looks the same".
    void reset() {
        m_items = {};
        m_alive.fill(false);
        m_generation.fill(1u); // 1 so a live handle never carries generation 0
        m_liveCount = 0;
        m_freeCount = Capacity;
        for (std::size_t i = 0; i < Capacity; ++i) {
            // Fill descending so the first spawns take slots 0, 1, 2, ...
            m_freeSlots[i] = static_cast<std::uint16_t>(Capacity - 1 - i);
        }
    }

    // Returns a null handle when the pool is full. Callers must check; a horde
    // game hits the ceiling routinely and silently dropping a spawn is the
    // correct behaviour, not an assert.
    template <typename... Args>
    [[nodiscard]] Handle spawn(Args&&... args) {
        if (m_freeCount == 0) return Handle{};

        const std::uint16_t slot = m_freeSlots[--m_freeCount];
        m_items[slot] = T{std::forward<Args>(args)...};
        m_alive[slot] = true;
        ++m_liveCount;
        return Handle{slot, m_generation[slot]};
    }

    // Bumping the generation on release (rather than on the next spawn) is what
    // invalidates outstanding handles the instant the entity dies, instead of
    // leaving a window where a handle to a dead slot still looks plausible.
    void release(const Handle handle) {
        if (get(handle) == nullptr) return;

        m_alive[handle.index] = false;
        m_generation[handle.index] = advanceGeneration(m_generation[handle.index]);
        m_freeSlots[m_freeCount++] = handle.index;
        --m_liveCount;
    }

    void releaseSlot(const std::size_t slot) {
        release(handleAt(slot));
    }

    [[nodiscard]] T* get(const Handle handle) {
        if (!resolves(handle)) return nullptr;
        return &m_items[handle.index];
    }

    [[nodiscard]] const T* get(const Handle handle) const {
        if (!resolves(handle)) return nullptr;
        return &m_items[handle.index];
    }

    // Slot-level access, for the tight loops that walk every slot anyway
    // (integration, separation, rendering) and have no use for handle checks.
    [[nodiscard]] bool alive(const std::size_t slot) const { return m_alive[slot]; }
    [[nodiscard]] T& at(const std::size_t slot) { return m_items[slot]; }
    [[nodiscard]] const T& at(const std::size_t slot) const { return m_items[slot]; }

    [[nodiscard]] Handle handleAt(const std::size_t slot) const {
        if (slot >= Capacity || !m_alive[slot]) return Handle{};
        return Handle{static_cast<std::uint16_t>(slot), m_generation[slot]};
    }

    [[nodiscard]] std::size_t size() const { return m_liveCount; }
    [[nodiscard]] bool empty() const { return m_liveCount == 0; }
    [[nodiscard]] bool full() const { return m_freeCount == 0; }

private:
    [[nodiscard]] bool resolves(const Handle handle) const {
        return handle.index < Capacity
            && m_alive[handle.index]
            && m_generation[handle.index] == handle.generation;
    }

    // Skips 0 on wraparound so Handle{} stays stale forever. A 16-bit counter
    // wraps after 65535 reuses of one slot; at that point a very old handle
    // could alias again, but nothing in this game holds a handle across more
    // than a single simulation step, so the window is unreachable.
    [[nodiscard]] static std::uint16_t advanceGeneration(const std::uint16_t generation) {
        const std::uint16_t next = static_cast<std::uint16_t>(generation + 1u);
        return next == 0u ? std::uint16_t{1u} : next;
    }

    std::array<T, Capacity> m_items{};
    std::array<bool, Capacity> m_alive{};
    std::array<std::uint16_t, Capacity> m_generation{};
    std::array<std::uint16_t, Capacity> m_freeSlots{};
    std::size_t m_freeCount{0};
    std::size_t m_liveCount{0};
};
