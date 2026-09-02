# Scene Stack — Design

Date: 2026-09-02
Status: Approved for planning
Context: First sub-project out of `docs/pattern-audit.md`'s backlog (item 1). The
audit found no application-level state above `World::GameState` (`Playing`/
`GameOver`) — `main.cpp` runs exactly one mode forever. The roadmap (main menu,
pause, multi-area map, dialogue, inventory) needs several distinct top-level modes,
and several of them need to layer over gameplay rather than replace it (a paused
game or an open dialogue box should still show the world behind it).

## Goal

Introduce the mechanism — a `Scene` interface and a `SceneStack` — and migrate the
existing gameplay loop onto it, with no new user-facing screens yet. This is
intentionally the smallest possible first slice: prove the mechanism against the one
scene that already exists before building new scenes on top of it.

## Non-goals (explicitly out of scope for this sub-project)

- No main menu, pause screen, dialogue, or inventory scene. Those are separate,
  later sub-projects that will each be *an instance of* this mechanism.
- No change to `World`, its `GameState` enum, or its test suite. `GameOver` stays
  exactly where it is today, inside `World`. Promoting it to its own `Scene` is a
  natural follow-up once a main-menu scene exists (so "restart" vs. "return to
  menu" is a real choice) — not part of this slice.
- No change to music, the F3 FPS-cap cycle, or window/audio-device setup. Those are
  window/app-level concerns untouched by which scene is active.

## Architecture

### Why a stack, not a heap or queue

Closing a scene must return control to whatever was open immediately before it, in
reverse order of how things were opened, however deep the nesting (gameplay → pause
→ a confirm dialog → back to pause → back to gameplay). That is LIFO order by
definition. A queue would pop in insertion order — the first scene ever pushed
would be first to leave, which is backwards. A priority structure would need a
priority to sort by, and nesting order already **is** the only ordering that
matters here; a stack gives it for free.

### Why heap allocation here, specifically

Every hot-path entity in this codebase (`Zombie`, `Bullet`, `Pickup`) lives in a
fixed-capacity `Pool` precisely to avoid allocation in the simulation's inner loop.
Scenes are the deliberate exception: they are pushed/popped on rare, discrete user
actions (open pause, start a conversation), not once per frame per entity. A
`std::vector<std::unique_ptr<Scene>>` costs one allocation per scene transition —
unmeasurable — and buys the one thing a `Pool<Scene, N>` can't: scenes are
heterogeneous types (`GameplayScene`, later `PauseScene`, `DialogueScene`, ...)
dispatched through one interface, which is what virtual dispatch is for.

### `Scene`

```cpp
class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(const Input& input, double simDt, SceneStack& stack) = 0;
    virtual void draw(const AssetStore& assets) const = 0;
};
```

`update` takes the owning `SceneStack&` so a scene can request a transition
(`stack.requestPush(std::make_unique<Foo>(...))`, `stack.requestPop()`). Requests
are recorded, not applied immediately — a scene must never destroy itself while its
own `update()` is still on the call stack. The stack applies at most one pending
request, after the update pass for the frame completes.

### `SceneStack`

Owns `std::vector<std::unique_ptr<Scene>>` plus one pending `Request` (`None`,
`Pop`, or `Push` holding a `unique_ptr<Scene>`).

- `update(const Input&, double simDt)`: calls `update()` on the **topmost** scene
  only, then applies any pending request. Only the top scene simulates — this is
  what will make "paused" mean anything later (push a pause scene, gameplay's
  `update` simply isn't called, so the world stops advancing but is still drawn).
- `draw(const AssetStore&) const`: calls `draw()` on **every** scene, bottom to
  top. This is what lets a later pause/dialogue scene render as an overlay with
  gameplay still visible underneath, rather than each overlay having to snapshot
  and redraw the frozen world itself.
- `empty() const`: for `main.cpp`'s loop condition (defense in depth — nothing in
  this slice ever empties the stack, since there is exactly one scene and nothing
  requests popping it, but the loop should not spin on an empty stack if that ever
  changes).

An empty stack is only reachable if `GameplayScene` requested a pop, which nothing
in this slice does — there is no way to leave gameplay yet since there's nowhere
else to go. Documented as a real state the type can represent, not asserted against,
consistent with `Pool::spawn` returning a null handle rather than asserting on a
full pool.

### `GameplayScene`

Wraps a `World` instance and the F1 (debug overlay toggle) / F2 (stress-test spawn)
input handling that today lives inline in `main.cpp`'s loop — both are
gameplay-specific, not window-level. `update()` calls `world.step(...)`; `draw()`
calls `world.draw(assets)` plus the existing debug-shapes/HUD drawing, matching
`main.cpp`'s current virtual-canvas pass exactly (pixel-for-pixel unchanged output).

### `main.cpp` changes

Construct `AssetStore` and a `SceneStack`, push one `GameplayScene` before the
loop starts. Replace the loop's direct `world.step(...)` / `world.draw(...)` /
`world.drawDebugShapes()` calls with `stack.update(...)` / `stack.draw(assets)`.
Music, the virtual-canvas render pass, the upscale pass, and F3's FPS-cap cycling
are untouched — they are not scene concerns.

## Data flow

```
main loop
  input.pollFrame()                     (unchanged)
  for each fixed sim step:
    stack.update(input.takeStep(), SIM_DT)
      -> top scene's update(input, dt, stack)
           (GameplayScene: world.step(...), F1/F2 handling)
      -> stack applies any pending push/pop
  render pass:
    stack.draw(assets)
      -> every scene's draw(assets), bottom to top
           (GameplayScene: world.draw(...), debug shapes, HUD)
```

## Testing

`tests.cpp` is unaffected — it drives `World` directly and none of `World`'s
public interface changes. One new headless test is added for the stack itself
(no window/GL context needed, matching the existing tests' constraint):

- A scene that calls `stack.requestPush(...)` from inside its own `update()` must
  not see the new scene take effect until *after* that `update()` call returns —
  i.e. the size of the stack and which scene is on top must be unchanged for the
  remainder of the call that requested the push. This is the deferred-apply
  guarantee the whole design leans on to avoid a scene invalidating itself mid-call.
- A pushed scene becomes the new top and receives subsequent `update()` calls; the
  scene beneath it does not.
- A popped scene is removed and destroyed; the scene beneath it (if any) resumes
  receiving `update()` calls.
- `draw()` visits every scene currently on the stack, in bottom-to-top order.

## Error handling / edge cases

- **Pool/World failure modes are unaffected** — nothing about `Pool`'s
  null-handle-on-exhaustion behavior changes; `GameplayScene` is a thin wrapper.
- **Empty stack**: `SceneStack::draw`/`update` on an empty stack are no-ops (not UB,
  not an assert) — same "represent the empty state rather than forbid it" choice
  the rest of this codebase makes (e.g. `Pool::spawn` on a full pool).
- **A scene requesting both push and pop in the same `update()`**: last request
  wins (only one `Request` is stored); not reachable by anything in this slice, but
  worth being deliberate about since a future scene could plausibly try both (e.g.
  "pop myself, then the thing under me pushes a result screen" is actually two
  scenes each requesting one transition on their own turn, not one scene doing
  both — so this case shouldn't come up in practice, but the single-slot design
  makes the answer unambiguous either way).

## Spec self-review

- Placeholders: none.
- Internal consistency: `GameOver` staying inside `World` (Non-goals) is consistent
  with `GameplayScene` being described as a thin wrapper that doesn't touch
  `World`'s public interface.
- Scope: single sub-project, bounded to the mechanism + one scene. Consistent with
  the audit's recommendation to sequence this first without over-building it.
- Ambiguity: "only one pending request, last write wins" resolves the one case that
  could otherwise be read two ways.
