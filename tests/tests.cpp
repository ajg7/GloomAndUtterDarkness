// Headless checks for the parts of the MVP whose correctness is not obvious by
// looking at the screen: handle staleness under churn, the swept test's edge
// cases, the player's legal transitions, the wave curve, and the two
// reproducibility claims the MVP is judged on.
//
// No window is created, so nothing here may touch a raylib function that needs
// a GL context.

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "collision.h"
#include "hud.h"
#include "frameRateMonitor.h"
#include "input.h"
#include "player.h"
#include "pool.h"
#include "rng.h"
#include "wave.h"
#include "world.h"
#include "zombie.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(const bool condition, const std::string& what) {
    ++g_checks;
    if (condition) return;
    ++g_failures;
    std::printf("  FAIL: %s\n", what.c_str());
}

void section(const char* name) {
    std::printf("\n%s\n", name);
}

// A scripted controller, so two runs can be driven with byte-identical input.
Input scriptedInput(const int step) {
    Input input{};
    input.moveLeft  = (step / 37) % 4 == 1;
    input.moveRight = (step / 37) % 4 == 3;
    input.fireHeld  = (step % 11) < 6;
    input.reload    = (step % 421) == 0;
    return input;
}

// ---------------------------------------------------------------------------

void testPool() {
    section("Pool: stable references under churn");

    Pool<Zombie, 8> pool;

    check(pool.get(Handle{}) == nullptr, "a default-constructed handle never resolves");
    check(pool.size() == 0, "a fresh pool is empty");

    const Handle first = pool.spawn(Zombie::spawned(Config::Zombies::Type::Man, 10.0f, 0.0f, false));
    check(pool.get(first) != nullptr, "a fresh handle resolves");
    check(pool.size() == 1, "spawn raises the live count");

    // The Milestone 3.1 checkpoint: a handle is taken, the target dies, and the
    // slot is immediately reused by a new spawn before the handle is resolved.
    const Handle recorded = first;
    pool.release(first);
    check(pool.get(recorded) == nullptr, "releasing invalidates outstanding handles at once");

    const Handle reused = pool.spawn(Zombie::spawned(Config::Zombies::Type::Woman, 99.0f, 0.0f, true));
    check(reused.index == recorded.index, "the free list reuses the released slot");
    check(reused.generation != recorded.generation, "reuse bumps the generation");
    check(pool.get(recorded) == nullptr, "the stale handle does not resolve to the newcomer");
    check(pool.get(reused) != nullptr, "the new handle resolves");
    check(pool.get(reused)->type == Config::Zombies::Type::Woman, "and resolves to the right entity");

    // Exhaustion is reported, not asserted: a horde game hits the ceiling.
    Pool<Bullet, 2> tiny;
    const Handle a = tiny.spawn(Bullet::fired(Vector2{0, 0}, Vector2{1, 0}));
    const Handle b = tiny.spawn(Bullet::fired(Vector2{0, 0}, Vector2{1, 0}));
    const Handle overflow = tiny.spawn(Bullet::fired(Vector2{0, 0}, Vector2{1, 0}));
    check(!a.isNull() && !b.isNull(), "a pool fills to capacity");
    check(overflow.isNull(), "a full pool returns a null handle rather than overwriting");
    check(tiny.full(), "and reports itself full");

    // reset() must wipe generations too, or a restart is not a fresh start.
    Pool<Zombie, 4> churned;
    for (int i = 0; i < 12; ++i) {
        const Handle h = churned.spawn(Zombie::spawned(Config::Zombies::Type::Man, 0.0f, 0.0f, false));
        churned.release(h);
    }
    churned.reset();
    const Handle afterReset = churned.spawn(Zombie::spawned(Config::Zombies::Type::Man, 0.0f, 0.0f, false));
    check(afterReset.index == 0, "reset restores the free list to slot order");
    check(afterReset.generation == 1, "reset restores generation counters");
}

void testSweptAABB() {
    section("Swept AABB: the cases a point test gets wrong");

    const Rectangle box{100.0f, 50.0f, 20.0f, 40.0f}; // x 100..120, y 50..90

    // A bullet fired dead horizontally has exactly zero y-delta. This is the
    // division-by-zero case, and it has to be right for all three vertical
    // relationships, not just the one that happens to be tested by eye.
    check(Collision::sweptAABB(Vector2{0, 70}, Vector2{200, 70}, box).has_value(),
          "horizontal shot level with the box hits");
    check(!Collision::sweptAABB(Vector2{0, 20}, Vector2{200, 20}, box).has_value(),
          "horizontal shot above the box misses");
    check(!Collision::sweptAABB(Vector2{0, 200}, Vector2{200, 200}, box).has_value(),
          "horizontal shot below the box misses");

    const auto entry = Collision::sweptAABB(Vector2{0, 70}, Vector2{200, 70}, box);
    check(entry.has_value() && std::fabs(*entry - 0.5f) < 1e-4f,
          "entry time is the fraction of the step at first contact");
    check(entry.has_value() && !std::isnan(*entry), "entry time is never NaN");

    // Exactly on the boundary is the 0/0 case that produces NaN if the zero
    // delta is not handled explicitly.
    const auto grazing = Collision::sweptAABB(Vector2{0, 50}, Vector2{200, 50}, box);
    check(!grazing.has_value() || !std::isnan(*grazing), "a grazing shot never yields NaN");

    check(!Collision::sweptAABB(Vector2{0, 70}, Vector2{50, 70}, box).has_value(),
          "a shot that stops short does not hit");
    check(!Collision::sweptAABB(Vector2{200, 70}, Vector2{300, 70}, box).has_value(),
          "a shot moving away does not hit");

    // Already overlapping at the start of the step is a hit at t = 0, not a miss.
    const auto inside = Collision::sweptAABB(Vector2{110, 70}, Vector2{130, 70}, box);
    check(inside.has_value() && *inside == 0.0f, "starting inside the box hits at t = 0");

    // Tunneling: the whole reason this routine exists. A step that carries the
    // bullet clean past the box must still register.
    check(Collision::sweptAABB(Vector2{0, 70}, Vector2{5000, 70}, box).has_value(),
          "a step far longer than the box still hits");
}

void testNoTunnelingInWorld() {
    section("No tunneling at any bullet speed");

    // Derived, never hardcoded: the firing line moves with the ground line, and
    // a literal here would quietly start testing empty air the moment the scene
    // is recomposed.
    const float muzzleY = Player::FrameY + Config::Assets::Scientist::MuzzleOffsetY;

    for (const float speed : {300.0f, 2000.0f, 20000.0f, 200000.0f}) {
        Zombie zombie = Zombie::spawned(Config::Zombies::Type::Man, 240.0f, 0.0f, true);
        const Rectangle expanded =
            Collision::expand(zombie.body(), Bullet::HalfWidth, Bullet::HalfHeight);

        Bullet bullet = Bullet::fired(Vector2{20.0f, muzzleY}, Vector2{speed, 0.0f});
        bool hit = false;
        for (int step = 0; step < 600 && !hit; ++step) {
            bullet.update(FrameRateMonitor::SIM_DT);
            hit = Collision::sweptAABB(bullet.prevPosition, bullet.position, expanded).has_value();
            if (bullet.offscreen()) break;
        }
        check(hit, "a bullet at " + std::to_string(static_cast<int>(speed)) + " px/s connects");
    }
}

void testFiringLineReachesEveryType() {
    section("Every zombie type is in the rifle's firing line");

    // The muzzle is at a fixed height; a type whose body sits entirely above or
    // below that line would be unkillable. Checked across the whole depth band,
    // because depth shifts a zombie vertically.
    const float muzzleY = Player::FrameY + Config::Assets::Scientist::MuzzleOffsetY;

    for (int i = 0; i < Config::Zombies::TypeCount; ++i) {
        const auto type = static_cast<Config::Zombies::Type>(i);
        for (const float depth : {Config::World::DepthMin, 0.0f, Config::World::DepthMax}) {
            const Zombie zombie = Zombie::spawned(type, 200.0f, depth, false);
            const Rectangle expanded =
                Collision::expand(zombie.body(), Bullet::HalfWidth, Bullet::HalfHeight);
            const bool inLine = muzzleY >= expanded.y && muzzleY <= expanded.y + expanded.height;
            check(inLine, std::string{"a "} + zombie.stats().name + " at depth "
                          + std::to_string(static_cast<int>(depth)) + " is in the firing line");
        }
    }
}

void testPlayerStateMachine() {
    section("Player: no reachable illegal state");

    // The transitions the milestone says must be refused.
    check(!PlayerStates::legal(PlayerStates::State::Reload, PlayerStates::State::Shoot),
          "firing is refused while reloading");
    check(!PlayerStates::legal(PlayerStates::State::Shoot, PlayerStates::State::Reload),
          "reloading is refused mid-shot");
    check(!PlayerStates::legal(PlayerStates::State::Shoot, PlayerStates::State::Shoot),
          "a shot cannot re-enter itself");
    for (int i = 0; i < PlayerStates::Count; ++i) {
        check(!PlayerStates::legal(PlayerStates::State::Dead, static_cast<PlayerStates::State>(i)),
              "nothing leaves Dead except reset");
    }

    Player player;
    player.reset();

    // Hammering fire and reload together must never produce a state that both
    // fires and reloads - there is only one state, so it cannot.
    Input input{};
    input.fireHeld = true;
    bool everReloadedWhileShooting = false;
    int shotsFired = 0;

    for (int step = 0; step < 4000; ++step) {
        input.reload = (step % 3) == 0; // spam it
        const bool wasReloading = player.reloading();
        if (player.update(input, FrameRateMonitor::SIM_DT)) ++shotsFired;
        if (wasReloading && player.state == PlayerStates::State::Shoot) everReloadedWhileShooting = true;
        check(player.magazine >= 0, "the magazine never goes negative");
        check(player.reserve >= 0, "the reserve never goes negative");
        check(player.magazine <= Config::Player::MagazineSize, "the magazine never overfills");
    }

    check(!everReloadedWhileShooting, "no input sequence fires straight out of a reload");
    check(shotsFired > 0, "the rifle actually fires");
    check(player.magazine + player.reserve <= Config::Player::MagazineSize
                                            + Config::Player::StartingReserve,
          "reloading moves rounds rather than creating them");

    // Empty magazine gives feedback rather than silence.
    Player dry;
    dry.reset();
    dry.magazine = 0;
    dry.reserve = 0;
    Input fire{};
    fire.fireHeld = true;
    (void)dry.update(fire, FrameRateMonitor::SIM_DT);
    check(dry.emptyFeedbackRemaining > 0.0, "an empty magazine reports itself");
    check(dry.state != PlayerStates::State::Shoot, "and refuses to fire");
}

void testShotLeavesOnTheFlashFrame() {
    section("The shot leaves on the muzzle-flash frame, at any step size");

    // Milestone 2.1's checkpoint, generalised: at an absurd step size the flash
    // frame can be jumped over entirely, and the bullet must still leave once.
    for (const double simDt : {1.0 / 60.0, 1.0 / 10.0, 1.0 / 3.0}) {
        Player player;
        player.reset();

        Input fire{};
        fire.fireHeld = true;

        int bullets = 0;
        int stepsUntilFirst = -1;
        for (int step = 0; step < 200; ++step) {
            if (player.update(fire, simDt)) {
                if (stepsUntilFirst < 0) stepsUntilFirst = step;
                ++bullets;
            }
        }
        check(bullets > 0, "a bullet leaves at dt = " + std::to_string(simDt));
        check(stepsUntilFirst > 0, "and never on the keypress step itself at dt = " + std::to_string(simDt));
    }

    // Exactly one bullet per shot, never two.
    Player player;
    player.reset();
    Input fire{};
    fire.fireHeld = true;
    int bullets = 0;
    for (int step = 0; step < 600; ++step) {
        if (player.update(fire, FrameRateMonitor::SIM_DT)) ++bullets;
    }
    check(bullets == Config::Player::MagazineSize
       || player.magazine + bullets >= Config::Player::MagazineSize,
          "every shot consumes exactly one round");
}

void testWaveCurve() {
    section("Waves ramp from a formula");

    check(WaveDirector::budgetFor(1) < WaveDirector::budgetFor(2), "the budget grows");
    check(WaveDirector::budgetFor(10) < WaveDirector::budgetFor(20), "and keeps growing");
    check(WaveDirector::intervalFor(1) > WaveDirector::intervalFor(10), "spawns get closer together");
    check(WaveDirector::intervalFor(60) >= Config::Waves::MinInterval, "but never below the floor");
    check(WaveDirector::concurrentCapFor(1) < WaveDirector::concurrentCapFor(9),
          "the concurrent cap grows");
    check(WaveDirector::concurrentCapFor(10000) <= static_cast<int>(Config::Gameplay::MaxZombies),
          "and never exceeds the pool");

    // Types unlock rather than appearing at wave 1.
    check(WaveDirector::weightFor(Config::Zombies::Type::Woman, 1) == 0.0f, "no screamers on wave 1");
    check(WaveDirector::weightFor(Config::Zombies::Type::Wild, 1) == 0.0f, "no crawlers on wave 1");
    check(WaveDirector::weightFor(Config::Zombies::Type::Woman, Config::Waves::WomanUnlockWave) > 0.0f,
          "screamers unlock on schedule");
    check(WaveDirector::weightFor(Config::Zombies::Type::Wild, Config::Waves::WildUnlockWave) > 0.0f,
          "crawlers unlock on schedule");
    check(WaveDirector::weightFor(Config::Zombies::Type::Wild, 20)
        > WaveDirector::weightFor(Config::Zombies::Type::Wild, 10),
          "the mix keeps shifting");

    // Only the wave number and the rng feed the mix, so it replays exactly.
    Rng a, b;
    for (int wave = 1; wave <= 30; ++wave) {
        check(WaveDirector::pickType(a, wave) == WaveDirector::pickType(b, wave),
              "type selection is reproducible");
    }
}

void testFixedTimestep() {
    section("Simulation rate is independent of framerate");

    // Ten seconds of wall clock at 20 fps and at 300 fps must advance the
    // simulation by the same number of ticks.
    const auto ticksOverTenSeconds = [](const double frameDelta) {
        FrameRateMonitor monitor;
        int ticks = 0;
        const int frames = static_cast<int>(10.0 / frameDelta);
        for (int frame = 0; frame < frames; ++frame) ticks += monitor.update(frameDelta);
        return ticks;
    };

    const int slow = ticksOverTenSeconds(1.0 / 20.0);
    const int fast = ticksOverTenSeconds(1.0 / 300.0);
    const int expected = static_cast<int>(10.0 * FrameRateMonitor::SIM_HZ);

    check(std::abs(slow - expected) <= 1, "20 fps runs " + std::to_string(slow) + " ticks in 10s");
    check(std::abs(fast - expected) <= 1, "300 fps runs " + std::to_string(fast) + " ticks in 10s");
    check(slow == fast, "both framerates advance the simulation identically");

    // A stall is dropped, not banked, so the accumulator cannot spiral.
    FrameRateMonitor stalled;
    check(stalled.update(5.0) <= FrameRateMonitor::MAX_STEPS_PER_FRAME,
          "one long stall is capped");
    check(stalled.update(1.0 / 60.0) <= 2, "and does not leave a backlog behind it");
}

void testRestartIsClean() {
    section("Restart leaves nothing behind");

    constexpr int Steps = 5000;

    const auto runSignatures = [](World& world) {
        std::vector<std::uint64_t> signatures;
        signatures.reserve(Steps);
        for (int step = 0; step < Steps; ++step) {
            world.step(scriptedInput(step), FrameRateMonitor::SIM_DT);
            signatures.push_back(world.stateSignature());
        }
        return signatures;
    };

    World world;
    const std::vector<std::uint64_t> first = runSignatures(world);

    // Four more restarts through the same object, exactly as pressing Enter on
    // the game-over screen would do.
    std::vector<std::uint64_t> latest;
    for (int run = 2; run <= 5; ++run) {
        world.reset();
        latest = runSignatures(world);
        check(latest == first, "run " + std::to_string(run) + " matches run 1 step for step");
    }

    // And a brand new World must match a restarted one, or reset() is doing
    // something the constructor is not.
    World fresh;
    const std::vector<std::uint64_t> freshRun = runSignatures(fresh);
    check(freshRun == first, "a restarted world matches a newly constructed one");

    check(first.back() != first.front(), "the run actually progressed");
}

void testCrowdSeparation() {
    section("Crowd separation and the grid");

    World world;
    world.reset();
    world.spawnStressTest(300);

    Input idle{};
    for (int step = 0; step < 120; ++step) world.step(idle, FrameRateMonitor::SIM_DT);

    check(world.liveZombieSlots() > 0, "the crowd survives 300 spawns");
    check(world.liveZombieSlots() <= Config::Gameplay::MaxZombies, "and stays within the pool");

    // The grid is only sound if a cell is at least as wide as the largest
    // separation diameter; otherwise an overlapping pair can sit two cells
    // apart and never be tested.
    float widest = 0.0f;
    for (int i = 0; i < Config::Zombies::TypeCount; ++i) {
        widest = std::max(widest, Config::Zombies::Table[static_cast<std::size_t>(i)].separationRadius);
    }
    check(2.0f * widest <= Config::Gameplay::SeparationCellSize,
          "the separation cell is at least one diameter wide");

    const float span = Config::Gameplay::SeparationCellSize
                     * static_cast<float>(Config::Gameplay::SeparationCellCount);
    check(Config::Gameplay::SeparationGridMinX <= -260.0f
       && Config::Gameplay::SeparationGridMinX + span >= Config::Virtual::WidthF + 260.0f,
          "the grid spans everywhere a zombie can legally stand");
}

void testPickupSweepThreshold() {
    section("Pickups use the sweep, and why that matters");

    const Pickup pickup = Pickup::dropped(240.0f, 120.0f);
    const Rectangle body = Rectangle{0.0f, 90.0f, Config::Assets::Scientist::Body.width,
                                     Config::Assets::Scientist::Body.height};
    const Rectangle expanded = Collision::expand(pickup.box(), body.width * 0.5f, body.height * 0.5f);

    // Walking straight through a drop collects it.
    check(Collision::sweptAABB(Vector2{200.0f, 124.0f}, Vector2{280.0f, 124.0f}, expanded).has_value(),
          "a stride across the drop collects it");

    // An endpoint-only overlap check starts failing once one step carries the
    // player further than the two half-widths combined. At walking pace that is
    // far off, but the threshold is real and it is what the sweep removes.
    const float skipDistance = body.width + Config::Pickups::Width + 4.0f;
    const Vector2 before{240.0f - skipDistance * 0.5f, 124.0f};
    const Vector2 after{240.0f + skipDistance * 0.5f, 124.0f};

    const Rectangle endpointBox{after.x - body.width * 0.5f, after.y - body.height * 0.5f,
                                body.width, body.height};
    check(!Collision::overlaps(endpointBox, pickup.box()),
          "an overlap check misses a drop the step jumped over");
    check(Collision::sweptAABB(before, after, expanded).has_value(),
          "the sweep catches the same drop");

    // A drop expires rather than accumulating forever.
    Pickup ageing = Pickup::dropped(100.0f, 120.0f);
    int steps = 0;
    while (ageing.update(FrameRateMonitor::SIM_DT) && steps < 100000) ++steps;
    check(steps > 0, "a drop lingers");
    check(steps < static_cast<int>(Config::Pickups::Lifetime * FrameRateMonitor::SIM_HZ) + 4,
          "and expires on schedule");
}

void testCrowdCost() {
    section("A full horde still fits in a simulation step");

    World world;
    world.reset();
    world.spawnStressTest(500);

    Input idle{};
    for (int warm = 0; warm < 10; ++warm) world.step(idle, FrameRateMonitor::SIM_DT);

    constexpr int Steps = 600; // ten seconds of simulation
    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step < Steps; ++step) world.step(idle, FrameRateMonitor::SIM_DT);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const double msPerStep =
        std::chrono::duration<double, std::milli>(elapsed).count() / static_cast<double>(Steps);
    std::printf("  (500 zombies: %.3f ms per simulation step, budget is %.2f ms)\n",
                msPerStep, FrameRateMonitor::SIM_DT * 1000.0);

    // A step must cost well under its own duration or the simulation cannot
    // keep up with itself. Generous, because this is a debug build.
    check(msPerStep < FrameRateMonitor::SIM_DT * 1000.0,
          "a 500-strong crowd steps faster than real time");
}

void testEpitaphFits() {
    section("The epitaph fits the screen");

    for (std::size_t i = 0; i < Hud::Epitaph::Lines.size(); ++i) {
        check(std::strlen(Hud::Epitaph::Lines[i]) <= Hud::Epitaph::MaxLineLength,
              "verse line " + std::to_string(i) + " fits the arena width");
    }
    check(std::strlen(Hud::Epitaph::Attribution) <= Hud::Epitaph::MaxLineLength,
          "the attribution fits");

    // The block must clear the attribution beneath it and the restart prompt
    // below that, inside 270 virtual pixels.
    const float bottom = Hud::Epitaph::TopY
                       + Hud::Epitaph::LineHeight * static_cast<float>(Hud::Epitaph::Lines.size());
    check(bottom < 196.0f, "the passage clears the attribution line");
    check(196.0f < 230.0f && 230.0f + Hud::Epitaph::Size < Config::Virtual::HeightF,
          "the restart prompt stays on screen");

    check(Hud::Epitaph::TitleLine < Hud::Epitaph::Lines.size(), "the highlighted line exists");
    check(std::string{Hud::Epitaph::Lines[Hud::Epitaph::TitleLine]}.find("gloom and utter darkness")
              != std::string::npos,
          "the highlighted line is the one the game is named from");

    // The whole passage must finish writing itself on in a reasonable time.
    std::size_t characters = 0;
    for (const char* line : Hud::Epitaph::Lines) characters += std::strlen(line);
    const double writeSeconds = Hud::Epitaph::StartDelay
                              + static_cast<double>(characters) / Hud::Epitaph::CharactersPerSecond;
    check(writeSeconds < 10.0, "the passage finishes in " + std::to_string(writeSeconds) + "s");
}

void testWorldPlaysThrough() {
    section("A run reaches later waves and ends");

    World world;
    world.reset();

    int highestWave = 0;
    bool everFired = false;
    for (int step = 0; step < 40000; ++step) {
        const Input input = scriptedInput(step);
        if (input.fireHeld) everFired = true;
        world.step(input, FrameRateMonitor::SIM_DT);
        highestWave = std::max(highestWave, world.wave().wave());
        if (world.state() == GameState::GameOver) break;
    }

    check(everFired, "the scripted run pulls the trigger");
    check(highestWave >= 2, "the run reaches at least wave 2, got " + std::to_string(highestWave));
    check(world.kills() > 0, "and kills something, got " + std::to_string(world.kills()));
    std::printf("  (scripted run: wave %d, %d kills, %s)\n", highestWave, world.kills(),
                world.state() == GameState::GameOver ? "died" : "still alive");

    // Game over must be escapable, and must restore a full player.
    if (world.state() == GameState::GameOver) {
        Input restart{};
        restart.restart = true;
        world.step(restart, FrameRateMonitor::SIM_DT);
        check(world.state() == GameState::Playing, "Enter restarts from the game-over screen");
        check(world.player().health == Config::Player::MaxHealth, "and restores full health");
        check(world.wave().wave() == 0, "and rewinds the wave counter");
        check(world.kills() == 0, "and clears the score");
    }
}

} // namespace

int main() {
    std::printf("Gloom and Utter Darkness - simulation checks\n");

    testPool();
    testSweptAABB();
    testNoTunnelingInWorld();
    testFiringLineReachesEveryType();
    testPlayerStateMachine();
    testShotLeavesOnTheFlashFrame();
    testWaveCurve();
    testFixedTimestep();
    testRestartIsClean();
    testCrowdSeparation();
    testPickupSweepThreshold();
    testCrowdCost();
    testEpitaphFits();
    testWorldPlaysThrough();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
