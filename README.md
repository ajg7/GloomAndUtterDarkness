# Gloom & Utter Darkness

**Learning objective:** raylib + C++ game loop fundamentals, and the collision math sitting underneath them.
**Stack:** C++ / raylib / CMake. No engine, no ECS library, no physics library — the parts you'd normally import are the parts you're here to learn.
**MVP:** One screen. Endless ramping waves of three zombie types, a rifle with ammo and reload, health pickups, a game over screen, and a restart that actually restarts.

## What you'll have at the end

A single binary that opens a window, draws your post-apocalyptic subway platform in parallax layers, and drops you in as a scientist with a rifle while zombies pour in from both edges. Waves get bigger and meaner on a curve you wrote. You run out of ammo, you reload, you get cornered, you die, you see how far you got, and you press a key and go again — with the second run being bit-for-bit identical in difficulty to the first.

## Status

All four phases are implemented and the MVP criteria below are met. `tests/tests.cpp` is a
headless harness that checks the parts you cannot verify by looking at the screen: handle
staleness under churn, the swept test's degenerate cases, the player's legal transitions,
the wave curve, and both reproducibility claims.

```
./build.ps1               # build
./build.ps1 -Run          # build and play
cd build && ctest         # run the simulation checks
```

**Controls:** arrows or A/D to move, Space to fire, R to reload, Enter to restart after death.
**Debug:** F1 toggles collision shapes and counters, F2 drops 200 more zombies in, F3 cycles
the framerate cap through 60 / 20 / 300 / uncapped.

Two things worth revisiting, both judgement calls rather than defects:

- **The ground line at y=131** puts the cast at the height of the train wreck's roof rather
  than on the platform floor at roughly y=205. Everything is measured off `Config::World::GroundLineY`,
  so moving it is a one-constant change - but it is your Phase 1 composition call, so it was left alone.
- **The crawler uses `wild/run.png`, not `wild/walk.png`.** Its walk pose is so low to the ground
  that a rifle shot on the flat firing line passes clean over its head; the run pose is upright
  enough to hit. There is a check in the harness that fails if any type ever falls out of the
  firing line across the whole depth band.

---

## Architecture decisions

- **Fixed timestep with an accumulator** _(recommendation, taken)_ — variable frame deltas mean objects move different distances on a stutter, which silently corrupts every collision test you write.
- **Fixed virtual resolution rendered to a `RenderTexture`, scaled to the window** _(recommendation, taken)_ — your art comes in at mismatched scales; one internal coordinate space means collision math never has to ask which pixels it's talking about.
- **Travelling bullets resolved with swept AABB** _(the plan's call — you deferred)_ — hitscan is a single ray test you write once and stop thinking about. Travelling bullets force the tunneling problem and its fix, which is the whole math objective.
- **Fixed-capacity pools with generation handles** _(the plan's call — you deferred)_ — an ECS teaches architecture, which isn't the objective. A `std::vector` you erase from mid-frame invalidates every index a bullet holds as its target. Pools with handles are the C++ fundamental underneath both.
- **Two collision shapes, two jobs** _(recommendation, taken)_ — AABB for bullets and hits because your sprites are boxy and axis-aligned; circles for zombie-vs-zombie shoving because penetration depth between circles is one subtraction.
- **One screen, no scrolling camera** _(your call)_ — wave survival means the arena is the frame. This removes an entire class of coordinate bugs from the MVP.

---

## Phase 1 — The loop

**Goal:** A window is open, running a simulation whose tick rate you control independently of the framerate, with your backdrop drawn in layers and the scientist walking around on it.
**Concept in focus:** Separating simulation time from render time.

### Milestone 1.1 — A loop that ticks at a rate you control

Get raylib linking and a window opening, then immediately replace the naive "update once per frame" structure with an accumulator. This is the foundation everything else in the plan assumes, and it's much cheaper to put in now than to retrofit.

1. Project builds, links raylib, opens and closes a blank window cleanly.
2. Create a `RenderTexture` at a fixed virtual resolution (something in the 480×270 to 640×360 range suits your art). Draw everything into it, then draw it scaled to the window with nearest-neighbour filtering.
3. Add the accumulator: each frame, add elapsed time to a running total, then run fixed-size simulation steps while the total exceeds your step size, keeping the remainder.
4. Draw two counters on screen — simulation ticks elapsed, and render frames elapsed.

- **Concept:** Fixed timestep — because every collision test from Phase 2 onward assumes an object moved a _known_ distance since it was last checked. Variable deltas make that assumption a lie.
- **Checkpoint:** Cap the framerate to 20, then uncap it to 300. Before you run either, predict what each counter does. If simulation ticks drift with framerate, your accumulator is wrong — find out why before continuing.

### Milestone 1.2 — The platform, in layers

Compose your backdrop assets into a scene with a clear ground line. The ground line is not decoration — it becomes the y-coordinate that every entity position and every collision box in the game is measured against.

1. Pick the ground line as a constant in virtual pixels.
2. Draw the layers in order: far backdrop, rail wall, columns and floor, hanging wires, train wreck.
3. Give the far layers a parallax offset factor now, even though nothing scrolls yet.
4. Confirm no bilinear smearing anywhere — if edges look soft, your filtering or your scale factor is wrong.

- **Concept:** Draw order and a single coordinate space — everything downstream measures against this one line, so getting it wrong here is expensive later.
- **Checkpoint:** Change the virtual resolution to a different value. If more than one constant needs editing to keep the scene composed, your layout is hardcoded in the wrong places. Fix that before moving on.

### Milestone 1.3 — Scientist animating and moving

Slice the sprite sheets and build the smallest animation abstraction that works: frame list, frame duration, loop flag, elapsed time. Wire input to horizontal movement and clamp to the arena.

1. Define frame rectangles for the idle and walk sheets.
2. Build an animation struct that advances on **simulation dt**, not frame time.
3. Left/right input sets velocity; position integrates in the fixed step; clamp to screen bounds.
4. Flip the sprite by negating the source rectangle width rather than duplicating the sheet.

- **Concept:** Animation as a function of accumulated simulation time — so animation frames stay locked to the physics that Phase 2 layers on top.
- **Checkpoint:** Without looking at your code, explain why advancing animation during the render step instead of the update step would make the shooting animation desync from the bullet spawn you're about to build. If the explanation is fuzzy, the coupling isn't clear in your head yet.

---

## Phase 2 — Bullets and bodies

**Goal:** The rifle fires travelling bullets, one zombie walks toward you, takes hits, and dies.
**Concept in focus:** Swept AABB, and why moving objects can't use point tests.

### Milestone 2.1 — Firing, driven by the animation

Your `Special` sheet has a muzzle flash on specific frames. The bullet should spawn on that frame — not on the keypress. This inverts the usual instinct and it's the right instinct for game code.

1. Add a shooting state to the player that plays the special sheet once, then returns to idle.
2. Spawn the bullet when the animation reaches the flash frame.
3. Bullet carries position and velocity; it integrates in the fixed step like everything else.
4. Fire cooldown counted in simulation time.

- **Concept:** Animation-driven events — the sprite sheet is authoring your gameplay timing, which is how the shot ends up feeling connected to the sprite instead of layered over it.
- **Checkpoint:** Drop your simulation step to something absurd like 1/10 second and watch in slow motion. The bullet must still leave exactly at the flash frame. If it leaves early, you're spawning off input.

### Milestone 2.2 — One zombie, tested naively on purpose

Build a single zombie that walks at you and dies, using the simplest possible hit test — is the bullet's current point inside the zombie's box. This is deliberately wrong, and the next milestone exists because of it.

1. Zombie holds position, velocity, hp, and an AABB derived from its sprite.
2. It walks toward the player's x each step.
3. On death, switch to a death animation and stop updating it.
4. Hit test: point-in-box, checked once per simulation step.

- **Concept:** Establishing the naive baseline, so you can see the failure mode with your own eyes rather than reading about it.
- **Checkpoint:** Raise bullet speed until shots start passing through the zombie. Before you test, compute from your step size and the zombie's width the speed at which this _should_ start failing. Your prediction and the observed number should match closely — if they don't, you've misunderstood one of the two.

### Milestone 2.3 — Swept AABB

Replace the point test with a sweep of the bullet's movement segment against the zombie box. This is the piece of math the whole project is built around, and you'll reuse it three more times before the MVP is done.

1. Expand the target box by the bullet's half-extents so you can treat the bullet as a point.
2. Slab test the bullet's segment against the expanded box, producing entry and exit times.
3. Reject when entry exceeds exit, or when entry falls outside the step.
4. Against multiple targets, take the earliest valid entry time and resolve the hit at that position.

- **Concept:** The slab method — the same routine handles bullets, pickups, and any wall you ever add, so it's worth understanding rather than pasting.
- **Checkpoint:** Rebuild it faster: on paper, write the 1D version from scratch with no reference. Then explain what a negative entry time means and what an entry-greater-than-exit result means, physically, in terms of where the bullet was. If either explanation is hand-wavy, you copied a formula instead of learning one.

---

## Phase 3 — The horde

**Goal:** Three zombie types, dozens on screen at once, shoving against each other, arriving in waves that ramp.
**Concept in focus:** Stable entity references under churn, and crowd separation.

This phase exists to break what Phase 2 got away with at N = 1. Both the storage and the collision loop from Phase 2 are about to stop being adequate, on purpose.

### Milestone 3.1 — Pools and generation handles

Spawning and killing mid-frame is exactly where raw indices betray you: a bullet holds index 4, zombie 4 dies, a new zombie takes slot 4, and your bullet kills the wrong thing.

1. Fixed-capacity arrays for zombies and bullets, with an alive flag per slot.
2. Free list of dead slots for reuse.
3. Handle is a slot index plus a generation counter; bump the generation whenever a slot is released.
4. Resolving a handle to a pointer checks the generation and returns nothing if stale.

- **Concept:** Stable references under churn — this is the C++ lesson that most cheap tutorials skip, and it's the one that separates code that survives feature growth from code that doesn't.
- **Checkpoint:** Deliberately kill a zombie in the same simulation step that a bullet tracking it resolves. Predict what happens before you run it. If it crashes or hits a recycled zombie, your generation check isn't guarding what you thought it was.

### Milestone 3.2 — Three types from one update function

Your three zombie sheets differ in size and read as different threats. Make the difference data, not code.

1. Type enum plus a stats table: speed, hp, contact damage, collision size, sprite sheet, animation timings.
2. One shared update function reads from the table.
3. Give at least one type a genuinely distinct movement behaviour rather than just different numbers.

- **Concept:** Data-driven variation instead of inheritance — because the alternative is a virtual call per zombie per step, and you'd be paying it for no gain.
- **Checkpoint:** Add a fourth type without editing the update function. If you can't, your stats aren't data yet — they're constants hiding in logic.

### Milestone 3.3 — Crowd separation

A horde that overlaps into a single blob doesn't read as a horde. Push overlapping zombies apart each step.

1. Give each zombie a separation circle, independent of its hit AABB.
2. Each step, for each overlapping pair, push both apart by half the penetration depth along the line between centres.
3. Cap the resolution iterations per step so a dense pile can't stall the frame.
4. When the pairwise loop gets slow, add a spatial partition — a uniform grid or a sort along x.

- **Concept:** Penetration resolution and the cost of O(n²) — Phase 2's all-pairs loop was free at N = 1, and this is where you find out what it actually costs.
- **Checkpoint:** Spawn 500 zombies and find your framerate cliff. Then explain which loop dominates the cost and predict, before implementing it, whether halving your grid cell size helps or hurts. Measure and see if you were right.

### Milestone 3.4 — Waves that ramp

1. Wave state: number, spawn budget, spawn interval, break duration between waves.
2. Spawn just off the left and right edges so zombies walk on rather than appear.
3. Derive count and type mix from the wave number as a formula, not a hand-authored table.

- **Concept:** Difficulty as a curve with a couple of tunable knobs — a table would let you fake balance for ten waves and then run out, which endless waves don't allow.
- **Checkpoint:** Defend the choice: three sentences on why your ramp has the shape it does, and at roughly which wave it becomes unwinnable. Then play to that wave and see whether you were right.

---

## Phase 4 — Consequence

**Goal:** Ammo and reloading, contact damage, health pickups, death, a game over screen, and a restart that leaves nothing behind.
**Concept in focus:** Explicit state machines, and reset as a test of state ownership.

### Milestone 4.1 — Ammo and reload as a state machine

By now the player has accumulated flags — moving, shooting, and soon reloading. Booleans multiply into illegal combinations. Replace them with one explicit state.

1. Magazine count and reserve ammo, displayed simply on screen.
2. Reload is a state with a duration measured in simulation steps.
3. Firing is refused while reloading; an empty magazine gives feedback rather than silence.
4. Legal transitions are declared in one place.

- **Concept:** The player as a state machine — a pile of booleans has 2ⁿ states, most of which you never intended and some of which you'll accidentally reach.
- **Checkpoint:** Try to force an illegal combination — reloading while shooting, firing mid-reload with a fast input. If any input reaches one, your states are still booleans wearing a costume.

### Milestone 4.2 — Damage and pickups on the same primitive

1. Zombies deal contact damage on a per-zombie cooldown so a crowd can't delete you in one step.
2. Player health, with a brief invulnerability window after a hit.
3. Health pickups drop occasionally and are collected using the swept test from Milestone 2.3 against the player's box.

- **Concept:** One collision primitive, three consumers — this is the payoff for having built the sweep properly rather than special-casing bullets.
- **Checkpoint:** Explain cold why a pickup should use the same sweep as a bullet rather than a simple overlap check, and identify the exact condition under which the overlap check would start missing pickups.

### Milestone 4.3 — Game over, and a restart that's actually clean

1. Top-level game state: playing, game over.
2. On death, freeze the simulation and show wave reached and zombies killed.
3. Restart wipes pools, free lists, generation counters, wave state, and player state.

- **Concept:** Reset as a diagnostic — anything that survives a restart is state you accidentally made global, and this is the cheapest way to find all of it at once.
- **Checkpoint:** Restart five times without relaunching the binary. Compare wave 1 of the fifth run against wave 1 of the first — same spawn counts, same type mix, same ammo, same everything. Any drift at all is leaked state. Track it down before you call the MVP finished.

---

## MVP is done when

- The simulation runs identically at 20 fps and 300 fps.
- Bullets hit reliably at any speed you can set, with no tunneling.
- A hundred-plus zombies of three types can be on screen, jostling, without the frame collapsing.
- Ammo, reload, health, and pickups all work, and no input sequence produces an illegal player state.
- You can die, see your result, restart, and get a run that starts genuinely identically.

## Deliberately not in the MVP

Parked, not forgotten — most of these are one evening each once the MVP is solid:

- Particles and juice: muzzle flash sparks, blood, hit-stop, screen shake
- Sound of any kind
- Title screen, pause, options
- High score persistence
- A second arena, or a scrolling camera
- Gamepad support
- Shaders, outlines, dynamic lighting
- Weapon variety beyond the rifle
