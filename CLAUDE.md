# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

NEATlegs evolves neural-network controllers for 2D legged creatures. A creature (limbs, joints, muscles, sensors) is described in a libconfig `.cfg` file, simulated with Box2D, and controlled by networks evolved with a from-scratch implementation of Ken Stanley's NEAT algorithm (in `NEAT/`, usable independently). Fitness is currently how far the creature moves in a fixed number of steps.

## Build

CMake, C++17. Dependencies (Homebrew): `cmake pkgconf box2d sdl3 sdl3_ttf libconfig`. Box2D must be **3.x** (C API).

```sh
cmake -S . -B build
cmake --build build -j            # all targets
cmake --build build --target hopper
```

Libraries, and only `display` uses SDL:
- `neat` (NEAT/): GA, genomes, networks, stats, RunLog
- `physics`: CreatureSpec, World, Creature, drawing through `Renderer`
- `sim`: Simulation / runEpisode, on top of physics
- `view`: BoxScreen, mapping a `Renderer` onto a `Canvas`
- `statsview`: stats overlay on a `Canvas`
- `display`: SDL3 window implementing `Canvas`

Executables `legs`, `hopper`, `xorTest`, `maxTest`, `poleBalance`; tests `neatTests`, `creatureConfig`, `simTests`, `drawTests`, `muscleEnergy`. Everything builds with `-Wall -Wextra` and should stay warning-free. Headers don't use `using namespace std`. C++ is indented 4 spaces, no tabs, within 80 columns (`.editorconfig`).

`ld: warning: building for macOS-X but linking with dylib ... built for newer version` comes from an outdated Command Line Tools SDK, not the project.

`(cd build && ctest)` runs everything in `tests/` (about half a second; `ctest -R <name>` for one, `--output-on-failure` for details). Tests are plain executables using the `TEST`/`CHECK` macros in `tests/check.h`; no framework:
- `neat` (`neatTests.cpp`): genome save/load and clone round trips, `compat` properties, mating invariants (innovation numbers sorted and unique, dominant parent's genes kept), networks against hand calculations, innovation sharing, seeded RNG, and a seeded GA giving the same history twice in one process.
- `creatureConfig`: `parseCreatureSpec` resolves names to indices and applies defaults; unknown names, bad types, duplicate names and missing fields throw a message naming the entry; built creatures match their spec; the shipped configs parse and build. `tests/testCreature.h` holds the small two-limb creature the physics tests share.
- `sim` (`simTests.cpp`): each episode end rule (max steps, head below floor, forbidden limb contact, energy budget spent, stopped by the frame callback); force-time accounting (a saturated muscle gives maxForce × time) and the energy cost weights; survival and fitness shaping (defaults give plain `maxHeadX` bit for bit); outputs reaching the right muscles; the frame callback not changing results; option parsing and validation; determinism; 8 threads sharing one spec getting the single-threaded result.
- `draw` (`drawTests.cpp`): recording `Renderer`/`Canvas` check what the world draws, `BoxScreen`'s world-to-pixel mapping, camera and grid, and the stats overlay. No window or SDL needed.
- `muscleEnergy_<creature>`: each creature is dropped into free fall with its muscles driven adversarially. The centre of mass must stay in free fall, internal kinetic energy must never exceed the measured muscle work, and positive work must stay within the `maxPower` budget.
- `determinism` (`determinism.cmake`): two `hopper -S` runs of kanga2 must write identical genomes, snapshots and `species.csv`, and `hopper -e` on the saved best genome must give its recorded fitness exactly. That covers the GA, worker threads and physics together.

Randomness: `seed_rand(seed)` / `dev_seed_rand()` (which returns the seed it chose) in `NEAT/random.h`. `hopper -S <seed>` repeats a run exactly; the seed is printed and saved as `seed.txt` in the run directory. Only draws on the seeding thread are reproducible, so fitness functions that use random numbers on worker threads (`poleBalance`'s random start) aren't.

Other sanity checks:
- `./build/xorTest`, `./build/poleBalance` — GA works (poleBalance reaches fitness ~1.0)
- `./build/hopper -C walker.cfg -N 40 -o /tmp/run` — headless GA run on a creature (writes stats CSVs and genome snapshots to the run dir; defaults to `runs/<config>-<time>/`, which is gitignored)
- `./build/hopper -C <run>/config.cfg -r <run>/best.genome` — replay a saved genome (should reproduce its recorded fitness exactly); `-e` instead of `-r` evaluates it once headless and prints fitness, distance, steps, energy and end reason

Visual programs (`legs`, `hopper -V`, `poleBalance -V`) open an SDL window. To render headlessly use `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`.

## Architecture

**NEAT library (`NEAT/`)** — `GeneticAlgorithm` owns the population and `Specie`s (with ids stable across generations); `Genome` holds genes and builds a `Network` (`createNewNetwork()`, `run(in, out)`); `InnovationStore` tracks innovation numbers. GA hyperparameters live in `ExpParameters`, loaded from the config's `global` section. The fitness function is a `std::function<double(const GenomeP)>` and **is called concurrently from `std::thread` workers** in `GeneticAlgorithm::runFitness()`, so it must be thread safe. Related constraints:
- `Network` compiles the genome's enabled links into its own index-based storage, so `createNewNetwork()` is truly const and one genome can be evaluated on several threads at once. The first `run()` repeats propagation until every neuron has had input, capped at the neuron count.
- The KISS RNG in `random.cpp` is `thread_local`; `dev_seed_rand()` seeds the calling thread and new threads derive from it.

**Statistics** — the GA doesn't print. `nextGeneration()` appends a `GenerationStats` (`NEAT/Stats.h`) to `history()` and keeps the top genomes in `topGenomes()`. Both are captured *before* fitness sharing, which divides `genome->fitness` by species size in place, so after `nextGeneration()` returns `genome->fitness` is the shared value; use `topGenomes()`' paired raw fitness. Diversity is mean pairwise `Genome::compat` (O(n²), no RNG draws so it doesn't change the run). `RunLog` (`NEAT/RunLog.*`) turns this into the console table, `stats.csv`/`species.csv`, and genome snapshots via `Genome::save`/`Genome::load`. `drawStatsOverlay` (`StatsOverlay.*`) draws it over a replay; fitness functors take an optional per-frame overlay callback.

**Watch mode (`hopper -V` / `-d N`)** — SDL must stay on the main thread, so `watchEvolution()` runs the GA loop on a background `std::thread` and the main thread replays the latest published best. They share only a `Monitor`: stats copies each generation and a `Genome::clone()` of the best every N generations. It has to be a clone, because a replay writes `fitness`/`steps`/`energy` onto the genome, and the GA thread reads those. In the fitness functor, any key breaks out of the replay; `Display::quitRequested()` tells callers whether it was a quit. Closing the window sets `Monitor::stop`, and the GA finishes its generation and runs `RunLog::finish`. Headless, SDL turns SIGTERM into a quit (SIGINT too, unless the process inherited it as ignored, e.g. a background `&` job).

**Physics (top level)** — Box2D 3 uses value ids (`b2BodyId`, `b2JointId`) instead of pointers; `boxTypes.h` holds the typedefs.
- `CreatureSpec` (`parseCreatureSpec`) is the parsed, validated config: plain data with names resolved to indices. It's parsed once, on the main thread; libconfig can't be read from several threads at once, since it lazily builds C++ wrappers even on reads. Worker threads build creatures from the shared, read-only spec with no lock.
- `World` owns a `b2WorldId` (ground, stepping with substeps). Box2D's global world table isn't thread safe, so world create/destroy is mutex-guarded. `createCreature(spec)` can't fail.
- `Creature(spec, world)` builds bodies, joints, muscles, points and sensors in spec order; the Box2D creation order affects results, so keep it. It exposes `bodies()` (spec order) and named `limbs`, `joints`, `shapes` maps, and fills network input via `setInput()`, bias first.
- `Muscle` is a damped spring between two body points, force applied once per `World::step()`; the network scales stiffness and rest length within configured min/max. `reset()` sets the rest length to the current length, so creatures start relaxed.
- Muscle energy: changing stiffness or rest length changes stored spring energy for free, so the limits are what bound energy input. `maxForce` clamps the force. `maxPower` limits force × lengthening rate at the start of the step, and also drains a per-muscle energy reserve (0.25 s × `maxPower`) by the *measured* positive work; at zero the muscle goes slack. The reserve is what guarantees the average-power bound, because a stiff muscle starting from rest reads zero power yet does a lot of work within one step. A `maxForce` far above body weight lets one step deliver far more than the budget, so configs use about 8× body weight. Work is measured in `World::step`'s after-step hook as F·Δx_com + τ·Δθ per body. That's exact, because Box2D applies the force as a constant force and torque for the whole step. F × ΔL is only first-order accurate and under-counted fast-rotating limbs. `Muscle` also integrates |force| over time (`Creature::forceTime()`, N·s), which is what holding a force costs a real muscle even when nothing moves. The episode's energy cost (below) is stored in `Genome::energy` and reported as the `energy` stat.
- Drawing: `World::draw`/`Creature::draw` emit world-space primitives to a `Renderer` (`Renderer.h`). `BoxScreen` is a `Renderer` that maps metres to pixels (with camera follow and a grid) onto any `Canvas` (`Canvas.h`). `Display` is the SDL3 `Canvas`: window, text, frame limiter. Colors are `0xRRGGBBAA` (`Color.h`).

**Simulation (`Simulation.*`)** — `Simulation(spec, EpisodeOptions)` is one creature in its own world:
- `readSensors`/`applyOutputs` (two outputs per muscle: strength then length) and `step(afterPhysics)`
- end rules: max steps, head below `headFloor`, a limb outside `groundLimbs` touching the ground, the energy cost reaching `energyBudget` (0 = none; the step that reaches it counts, like max steps), or `stop()`
- `runEpisode(spec, options, controller, frame)` runs one from the start pose. The frame callback runs after physics and before the end rules, and can stop the episode.
- `EpisodeResult` holds steps, max head x/y (the ending step doesn't count), the energy cost and its parts (positive work, negative work, force-time), and the end reason. `episodeOptionsFromConfig` reads `global.maxSteps`/`headFloor`/`groundLimbs`/`energyBudget`/`energyCost`.
- Energy cost (`EnergyCost`) = `positiveWork` × W⁺ + `negativeWork` × |W⁻| + `forceTime` × ∫|F| dt, set by `global.energyCost = { positiveWork; negativeWork; forceTime; }`. The default (1, 0, 0) is positive work alone, bit for bit. A metabolic model uses the inverse muscle efficiencies, 4 and 0.83, plus a force-time weight in J per N·s (holding F costs like moving it at that speed). `maxPower` and the energy reserve stay mechanical.
- `episodeFitness` = (`maxHeadX` + `fitnessBase`) × survival^`survivalExponent`, from `FitnessOptions` (`fitnessOptionsFromConfig`). Survival is steps / maxSteps, but 1 when the episode ended by max steps or energy spent, so only falls and forbidden contacts are penalised. Unset options give plain `maxHeadX`, so existing configs reproduce their old runs exactly.

**`hopper.cpp`** is the GA driver:
- It parses the spec and episode options once and validates them by constructing a `Simulation`.
- Its fitness functor wraps a `Network` as the `Controller`, calls `runEpisode`, and sets fitness with `episodeFitness`. It also stores `maxHeadX` in `Genome::distance`, which goes into the `mean_distance`/`best_distance` stats next to `mean_steps`/`best_steps`, because fitness is no longer plain distance.
- With a display, the frame callback draws and polls the window.
- `nInput`/`nOutput` come from the spec (sensors + bias, 2 × muscles), overriding `global`.
- The creature must define a shape named `"head"`.

**Config sections**: `global` (NEAT params, plus optional `maxSteps` (default 1000), `headFloor`, `groundLimbs`, `energyBudget`, `energyCost`, `fitnessBase` and `survivalExponent`, read by `hopper.cpp`), `limbs`, `joints` (revolute only), `muscles`, `shapes` (named points on limbs), `sensors` (JointSensor, HeightSensor, BodyAngleSensor, AngularVelocitySensor, VelocitySensor, ContactSensor). `groundLimbs` lists the limbs allowed to touch the ground; any other limb touching ends the run, via `Creature::touchesOutside`. `kanga2.cfg` and `walker.cfg` are generated by `tools/gen_kanga2.py` and `tools/gen_walker.py`: edit the parameters there and regenerate, rather than editing the cfg. Both sweep each muscle over its joint's range and reject placements whose line of action crosses the joint. Joint angles start at 0 in the start pose (the reference angle is taken at creation), so limits are relative to the start pose. Shape friction defaults to 0.2 (Box2D 2.x default the configs were tuned for). `BodyAngleSensor` sees angles in [-π, π] under Box2D 3 (was unbounded in 2.x). `hopper.cfg` still uses minA = 0, maxA = 2π, so any forward lean (a negative angle) reads as 0.

**Creature scale**: all configs are roughly human-sized (head at about 2 m, hopper and walker masses 0.3–0.6 kg), so muscle, force and power numbers compare across models, and everything fits the 640×480 view at 100 px/m. `hopper.cfg` was Froude-scaled down from an original 17 m design (see its header comment). The default `headFloor` (0.75 m) suits all of them.

**Objective**: plain fitness is the head's max x, so a creature can gain distance by diving forward and falling at the end of a run, or by scooting along on a tail or torso. `groundLimbs` closes the scooting route (kanga2 uses it). Early in a run diving dominates: in kanga2's first generations the mean creature falls within about 50 steps, and the best covers ~2 m by falling forward. `survivalExponent` (with a small `fitnessBase`, so balancing alone earns something) makes an early fall worth little. `energyBudget` ends a run once its muscle work is spent, so fitness becomes distance per budget. kanga2 uses base 0.1, exponent 0.5 and a 60 J budget of positive work. The walker uses the same shaping with a metabolic cost (see below). A lunge at the very end of a full-length run still pays about 2 m.

**Evolving kanga2** (5–10 runs per variant, 1000 generations; the spread between runs is large):
- without `groundLimbs`, most runs scoot on the tail
- with it, adding the rate/contact sensors doubled median fitness and let most runs last all 1000 steps
- an add-link mutation rate of 0.05 (instead of 0.2) helped further and cut network bloat
- population 400 helped about as much, at twice the cost per generation
- Muscles must keep their line of action clear of the joint over its whole range. A straight spring on the outside of a bend (like a quad without a kneecap) crosses the joint, and flips between extending and flexing it. The generator sweeps each muscle and rejects that, and takes rest-length ranges from the swept lengths.
- With the knee muscle behind the knee, nearly every run hops instead of shuffling. Early falls (head below `headFloor` at about 200–450 steps) still dominate the results, and a second hip muscle didn't help.
- Objective (8 seeds per setting, 60 J budget): plain fitness gives a median of 7.4 m, and 6 of 8 best genomes fall. Survival shaping keeps the population up (mean survival ~550 steps instead of ~90), but with base 0.5 and exponent 1, half the runs settle on a 3–6 m shuffle (median 9.1 m within 60 J). Exponent 0.5 with base 0.1–0.25 gives a median of about 24 m, with 1–2 of 8 runs stuck. Base 0 weakens survival (the population falls more, median ~15 m).
- The budget binds only once a gait covers ~15 m or more; for seeds where it did, it gave clearly more distance per joule than survival shaping alone (e.g. 29 m instead of 20 m on 60 J).
- Max fitness sometimes drops between generations (the champion is lost, by up to 5 m), although fitness is deterministic.

**Evolving the walker** (8 runs per variant; 1000 generations unless noted):
- The original walker had no `groundLimbs` and a knee that folded 179°. Every run evolved kneeling on its shins (median 20 m), and none survived a feet-only rule.
- `gen_walker.py` fixes the joint ranges: hip −30..120° at first (60° now, see below), so the stance leg can get behind the body; knee −135..0°, short of where the hamstring crossed the knee; ankle −60..30°. It moves the hip muscle to a pelvis point 0.10 m in front of the hip, which keeps leverage over hip extension, and sets stiffness by kanga2's rule (maxForce at 20% stretch; the old springs were about 4× softer). With feet-only contact and kanga2's objective, the median went from 6.6 m (same rules on the old geometry) to 14.9 m.
- Rate and contact sensors, which helped kanga2, hurt the walker: all three halved the median and the population's survival, foot contact alone did nearly as badly, and torso spin alone made no difference. The walker ships without them.
- A more human mass split (lighter head, torso density 3) raised the centre of mass and made things worse (median 5 m).
- Population matters most for the gait. Some winners drag one foot along the ground while the other shuffles on its toes. At population 200 and 500, the median share of the worse foot's travel spent sliding is 0.72, with ~28 switches between left and right single support. At 1000 it is 0.49 with ~60 switches, and the median distance is 26 m (15 m at 200, 25 m at 500). The walker uses 1000.
- With positive work alone as the budget: kanga2's budget per kg (128 J) binds once gaits work at population 1000 (5 of 8 runs spend it) without making them drag, and gives more distance within 128 J than genomes evolved with 256 J (26 m against 19 m). 256 J gives more total distance (37 m) by spending ~250 J. At population 200 neither budget binds much. A gait-analysis script (per-foot contact, slide and air travel) was used for these numbers; it isn't in the repo.
- Those winners weren't walking naturally. Every one was a lopsided lunge: one hip jammed at its flexion stop, the other at the extension stop (the dragged leg), knees bent 40–120° in stance and almost never locked, head at 1.3–1.5 m of 1.82. Their muscles held 4–7× body weight of force all run, which cost nothing.
- The rest are 250-generation results with population 1000. Lowering `minK` alone made things worse (median 17 m at maxK/4, 6 m at 0): relaxing only pays when holding costs.
- The metabolic cost (4, 0.83, c) changed nothing while the 1000-step cap ended runs before the budget did. With `maxSteps` 3000 and budgets set to what the gaits spend per 1000 steps, every run ends by the budget: at `minK` maxK/4, c = 1 went furthest (median 21 m on 950 J), but the lunge stayed at every c.
- With `minK` at 0.05–0.1 of maxK and c = 1, knees lock straight (56–73% of stance) and the lunge goes, but at hip flexion 120° most runs jackknifed, torso folded onto the flexion stop (head ~1.1 m). Joint stops are free braces, and evolution moves to whichever one is left.
- Hip flexion to 60° with `minK` 0.1 gave the most natural walkers so far, and is what the walker uses: head 1.73 m (median), better knee locked 70% of stance, hips 63° apart instead of 140°, the least dragging (0.66) and the most left/right switching. Median distance 12 m on 950 J, best runs 40–45 m. What remains is lopsided control: the best run walks fully upright (head 1.89 m) but skates one foot; another alternates properly with one knee bent.

`legs.notes` holds the original design notes.
