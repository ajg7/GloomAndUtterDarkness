# Gameplay Pattern & C++ Practice Audit

Audit date: 2026-09-02. Scope: `src/` as of commit `d7f9ec9`. Reference: Robert Nystrom,
*Game Programming Patterns* (gameprogrammingpatterns.com); C++ Core Guidelines
(isocpp.github.io/CppCoreGuidelines).

Goal: identify where the MVP already matches canonical patterns/practice, where it
diverges, and which divergences actually matter given the stated roadmap (more zombie
types incl. bosses, a multi-area map with per-area wave spins, UI/UX, dialogue,
inventory, save state).

## Already applied, and applied well

| Pattern | Where | Note |
|---|---|---|
| Game Loop (fixed-timestep, decoupled sim/render) | `main.cpp`, `frameRateMonitor.h` | Textbook: accumulator, capped catch-up, input latched per sim step not per frame. |
| Object Pool | `pool.h` | Generation-counter handles catch stale-reference bugs the book's naive version doesn't. Better than the reference implementation. |
| State | `player.h` (`PlayerStates::Legal` table), `zombie.h` | Explicit legality table instead of a pile of booleans — exactly the pattern's motivating example. |
| Update Method | `World::step` → per-entity `update()` | Clean. |
| Type Object (the book's, not "Component") | `Config::Zombies::Table` / `Stats` | A 4th zombie type is a data row, not a subclass. This is the right call over an inheritance hierarchy or a full ECS at this entity count. |
| Flyweight | `assets.h` (`AssetStore`) | Shared textures across pooled entities; entities hold a type tag, not a texture handle. |
| Spatial Partition | `world.h` (`separateCrowd`, uniform grid on x) | Correctly reasons about cell width vs. separation diameter. |
| RAII / ownership (C++ Core Guidelines R.1) | `scoped*.h` family | Every raylib C handle (window, audio device, music, render texture, texture mode) gets an owning wrapper. No manual `Unload*` scattered through `main`. |
| Determinism discipline | `rng.h`, `World::stateSignature()` | No `rand()`/`GetRandomValue()` anywhere; the state-signature fold is a genuinely unusual and good regression net for a hobby project. |
| Const-correctness / `[[nodiscard]]` | throughout | Consistently applied, not just on the "important" functions. |

This codebase is meaningfully above typical MVP quality already — the audit below is
about what the *next* phase of scope will strain, not about fixing bad code.

## Gaps, ranked by how much the roadmap needs them

### 1. No Scene / app-state stack — blocks almost everything else
`World` has a `GameState` enum (`Playing` / `GameOver`), but there is no notion of
*application* state above that: no main menu, no pause, no way to overlay a dialogue
box or an inventory screen over (or instead of) gameplay. `main.cpp` hardcodes exactly
one scene forever running.

The standard answer is the **Scene stack** (push/pop; a paused game is "gameplay
scene with a pause scene pushed on top of it," a dialogue box is "gameplay scene with
a dialogue scene pushed on top of it"). This is not one of Nystrom's 19 named patterns
but it's the universal architecture for exactly this problem, and it composes with
State (each scene *is* one).

**Why it's first:** main menu, pause, inventory screen, and dialogue overlay are four
separate roadmap items that are all "a scene" once this exists, and are each a
one-off special case without it.

### 2. No data-driven Area/Level table
The backdrop (`Config::Assets::Backdrop::All`) and wave curve (`Config::Waves`) are
each a single fixed global config. A "map with unique wave spins per area" wants the
same move already made for zombies: an `Area`/`Level` struct with its own backdrop
set, wave-curve parameters, and (once bosses exist) boss/unlock table, indexed the
same way `Config::Zombies::Table` is. This is Type Object again, one level up.

### 3. No decoupling between systems (Observer / Event Queue)
`World::step` calls concrete methods on `Player`, `Hud` reads `World` directly, audio
is driven by hand in `main.cpp`. This is *fine* at current scope — nothing here is
actually tangled — but every new cross-system reaction the roadmap implies (a boss
death triggers a cutscene; a kill triggers a UI floater; a story beat opens a
dialogue scene) is another direct call site if nothing changes. Worth introducing a
small **Event Queue** (not a general pub/sub framework — a fixed-capacity array of
tagged events per frame, in keeping with this codebase's no-allocation style) once
scenes exist, so gameplay code can *announce* things without knowing who's listening.

**Not urgent on its own** — only worth doing when the first cross-system reaction
(boss intro, first dialogue trigger) actually needs it. Doing it speculatively now
would be exactly the premature abstraction the project's own conventions warn against.

### 4. Boss battles have no home yet
Bosses aren't "a bigger zombie" — they want multi-phase behaviour (State pattern,
same tool already in use for `Player`/`Zombie`, just with more states and probably
its own `BossStates::Legal` table) and likely their own draw/HUD hooks (a boss health
bar). The existing `Pool<Zombie, N>` + `Type Object` approach can probably still hold
the boss *entity*, but its behavior almost certainly needs to stop being "one
`update()` reads a stats row" and become a real state machine per boss type. Worth
scoping as its own sub-project once the first boss is actually being designed, not
generalized in advance.

### 5. No save/load system
Nothing exists yet, but the news is good: most of this game's state is already
plain, flat, default-constructible structs (`Player`, `Zombie`, `Pool`'s backing
arrays) — the "DTOs" a save system usually has to be built to produce already exist
as the live representation. The main design decision when this becomes real is a
versioned save envelope (schema version + checksum) and *which* subset of `World`
is savable (probably not mid-air bullets or crowd-separation scratch buffers — save
at wave boundaries or on quit, not mid-frame).

### 6. Inventory has no home yet
Same shape as pickups (`pickup.h`) but persistent and player-facing rather than
world-facing and transient. Natural fit: a small fixed-capacity array (this
codebase already avoids `std::vector` in hot state for exactly this reason) owned by
`Player`, with its own scene (#1) for the UI.

### 7. Dialogue has no home yet
Wants a data-driven script format — a `Bytecode`-lite (Nystrom's actual `Bytecode`
pattern is probably overkill; a flat array of `{speaker, line, next}` records is
likely enough) — displayed through a scene (#1) and triggered through the event
queue (#3) once those exist.

### 8. `World` is doing a lot, and the roadmap only adds to it
502 lines, seven owned subsystems (player, zombies, bullets, pickups, wave, crowd
grid, draw order), all as private methods on one class. Not a problem *today* — it's
still one clear read top-to-bottom — but multi-area maps, bosses, inventory, and
dialogue are each a new subsystem `World` would otherwise absorb. Worth watching, not
worth splitting pre-emptively: the project's own stated convention is smaller units
with one clear purpose, so the trigger to actually split it is "the next subsystem
that doesn't obviously belong to combat," not a line-count threshold.

## What this audit is *not* flagging

- **No ECS.** At these entity counts (`MaxZombies = 512`), with behavior already
  correctly data-driven via `Type Object`, a full ECS would be a rewrite in search of
  a problem this code doesn't have. Revisit only if profiling ever says otherwise.
- **No Singleton/Service Locator for `AssetStore`.** It's passed explicitly through
  `draw()` calls instead of hidden behind a global accessor — that's the *better*
  practice (explicit dependencies), not a gap.
- **Command pattern for input.** `InputLatch` is a fine hand-rolled solution for a
  single fixed action set. Worth revisiting only if/when key rebinding or multiple
  input contexts (e.g. dialogue advancing on the same key that fires the rifle)
  become real requirements — likely alongside the scene stack (#1), not before it.

## Proposed order of work

1. **Scene / app-state stack** — foundational; menu, pause, inventory screen, and
   dialogue overlay all become instances of it rather than one-offs.
2. **Data-driven Area/Level table** — unlocks the multi-area map with per-area wave
   spins, mirroring the existing zombie-table pattern.
3. **Event queue** — introduced when the first cross-system trigger (boss
   intro/death, first dialogue hook) actually needs it, not before.
4. **Boss battle support** — multi-phase state machine per boss type, scoped when
   the first boss is designed.
5. **Save/load system** — versioned envelope over the already-flat entity structs.
6. **Inventory** — fixed-capacity array on `Player` + a scene for its UI.
7. **Dialogue** — data-driven script format, a scene, and an event trigger.
8. **`World` decomposition** — only if/when a new subsystem stops obviously
   belonging to combat.

Items 1–2 are sequenced first because nearly everything else either sits on top of
them or is meaningfully easier once they exist. 3–8 are scoped but intentionally not
designed in detail yet — each becomes its own brainstorm → design → spec → plan
cycle when picked up, per the project's usual process.
