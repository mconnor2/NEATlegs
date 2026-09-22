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

Targets: libraries `neat` (NEAT/, no graphics/physics deps), `display` (SDL3 wrapper), `statsview` (stats overlay drawn on a `Display`), `physics` (World/Creature/BoxScreen); executables `legs`, `hopper`, `xorTest`, `maxTest`, `poleBalance`; tests `neatTests`, `creatureConfig`, `muscleEnergy`. Everything builds with `-Wall -Wextra` and should stay warning-free.

`ld: warning: building for macOS-X but linking with dylib ... built for newer version` comes from an outdated Command Line Tools SDK, not the project.

`(cd build && ctest)` runs everything in `tests/` (about half a second; `ctest -R <name>` for one, `--output-on-failure` for details). Tests are plain executables using the `TEST`/`CHECK` macros in `tests/check.h`; no framework:
- `neat` (`neatTests.cpp`): genome save/load and clone round trips, `compat` properties, mating invariants (innovation numbers sorted and unique, dominant parent's genes kept), networks against hand calculations, innovation sharing, seeded RNG, and a seeded GA giving the same history twice in one process.
- `creatureConfig`: a minimal config builds; unknown names, bad types and missing required fields make `createCreature` return null; the shipped configs build.
- `muscleEnergy_<creature>`: each creature is dropped into free fall with its muscles driven adversarially. The centre of mass must stay in free fall, internal kinetic energy must never exceed the measured muscle work, and positive work must stay within the `maxPower` budget.
- `determinism` (`determinism.cmake`): two `hopper -S` runs of kanga2 must write identical genomes, snapshots and `species.csv`. That covers the GA, worker threads and physics together.

Randomness: `seed_rand(seed)` / `dev_seed_rand()` (which returns the seed it chose) in `NEAT/random.h`. `hopper -S <seed>` repeats a run exactly; the seed is printed and saved as `seed.txt` in the run directory. Only draws on the seeding thread are reproducible, so fitness functions that use random numbers on worker threads (`poleBalance`'s random start) aren't.

Other sanity checks:
- `./build/xorTest`, `./build/poleBalance` — GA works (poleBalance reaches fitness ~1.0)
- `./build/hopper -C walker.cfg -N 40 -o /tmp/run` — headless GA run on a creature (writes stats CSVs and genome snapshots to the run dir; defaults to `runs/<config>-<time>/`, which is gitignored)
- `./build/hopper -C <run>/config.cfg -r <run>/best.genome` — replay a saved genome (should reproduce its recorded fitness exactly)

Visual programs (`legs`, `hopper -V`, `poleBalance -V`) open an SDL window. To render headlessly use `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`.

## Architecture

**NEAT library (`NEAT/`)** — `GeneticAlgorithm` owns the population and `Specie`s (with ids stable across generations); `Genome` holds genes and builds a `Network` (`createNewNetwork()`, `run(in, out)`); `InnovationStore` tracks innovation numbers. GA hyperparameters live in `ExpParameters`, loaded from the config's `global` section. The fitness function is a `std::function<double(const GenomeP)>` and **is called concurrently from `std::thread` workers** in `GeneticAlgorithm::runFitness()`, so it must be thread safe. Related constraints:
- `Network` borrows the genome's `links` array and writes neuron pointers into it, so one genome must never be evaluated twice concurrently (holds today since each genome appears once per population).
- The KISS RNG in `random.cpp` is `thread_local`; `dev_seed_rand()` seeds the calling thread and new threads derive from it.

**Statistics** — the GA doesn't print. `nextGeneration()` appends a `GenerationStats` (`NEAT/Stats.h`) to `history()` and keeps the top genomes in `topGenomes()`. Both are captured *before* fitness sharing, which divides `genome->fitness` by species size in place, so after `nextGeneration()` returns `genome->fitness` is the shared value; use `topGenomes()`' paired raw fitness. Diversity is mean pairwise `Genome::compat` (O(n²), no RNG draws so it doesn't change the run). `RunLog` (`NEAT/RunLog.*`) turns this into the console table, `stats.csv`/`species.csv`, and genome snapshots via `Genome::save`/`Genome::load`. `drawStatsOverlay` (`StatsOverlay.*`) draws it over a replay; fitness functors take an optional per-frame overlay callback.

**Watch mode (`hopper -V` / `-d N`)** — SDL must stay on the main thread, so `watchEvolution()` runs the GA loop on a background `std::thread` and the main thread replays the latest published best. They share only a `Monitor`: stats copies each generation and a `Genome::clone()` of the best every N generations. It has to be a clone, because building a `Network` writes into the genome's link array and the GA may be evaluating the same champion. In the fitness functor, any key breaks out of the replay; `Display::quitRequested()` tells callers whether it was a quit. Closing the window sets `Monitor::stop`, and the GA finishes its generation and runs `RunLog::finish`. Headless, SDL turns SIGTERM into a quit (SIGINT too, unless the process inherited it as ignored, e.g. a background `&` job).

**Physics (top level)** — Box2D 3 uses value ids (`b2BodyId`, `b2JointId`) instead of pointers; `boxTypes.h` holds the typedefs.
- `World` owns a `b2WorldId` (ground, stepping with substeps). Box2D's global world table isn't thread safe, so world create/destroy is mutex-guarded; libconfig lazily mutates `Config` on reads, so `createCreature` is mutex-guarded too.
- `Creature` builds bodies/joints/muscles/sensors from config (`initFromFile`), exposes named `limbs`, `joints`, `shapes` maps, and fills network input via `setInput()` (bias first). Unknown names in the config raise an error instead of creating null ids.
- `Muscle` is a damped spring between two body points, force applied once per `World::step()`; the network scales stiffness and rest length within configured min/max. `reset()` sets the rest length to the current length, so creatures start relaxed.
- Muscle energy: changing stiffness or rest length changes stored spring energy for free, so the limits are what bound energy input. `maxForce` clamps the force. `maxPower` limits force × lengthening rate at the start of the step, and also drains a per-muscle energy reserve (0.25 s × `maxPower`) by the *measured* positive work; at zero the muscle goes slack. The reserve is what guarantees the average-power bound, because a stiff muscle starting from rest reads zero power yet does a lot of work within one step. A `maxForce` far above body weight lets one step deliver far more than the budget, so configs use about 8× body weight. Work is measured in `World::step`'s after-step hook as F·Δx_com + τ·Δθ per body. That's exact, because Box2D applies the force as a constant force and torque for the whole step. F × ΔL is only first-order accurate and under-counted fast-rotating limbs. `Creature::positiveWork()` is stored in `Genome::energy` and reported as the `energy` stat.
- `BoxScreen` maps meters→pixels and draws via `Display` (SDL3 renderer; lines/circles/points/text, frame limiter). Colors are `0xRRGGBBAA`.

**`hopper.cpp`** is the GA driver and fitness functor. Couplings with the config file:
- `nInput` = sensors + 1 (bias) and `nOutput` = 2 × muscles are **computed from the config**, overriding `global`.
- Outputs map to muscles as `out[2m]` → `scaleStrength`, `out[2m+1]` → `scaleLength`.
- The creature must define a shape named `"head"`; the run ends if it drops below `HEAD_FLOOR`; fitness = head's max x.

**Config sections**: `global` (NEAT params, plus optional `headFloor` and `groundLimbs`, both read by `hopper.cpp`), `limbs`, `joints` (revolute only), `muscles`, `shapes` (named points on limbs), `sensors` (JointSensor, HeightSensor, BodyAngleSensor, AngularVelocitySensor, VelocitySensor, ContactSensor). `groundLimbs` lists the limbs allowed to touch the ground; any other limb touching ends the run, via `Creature::touchesOutside`. `kanga2.cfg` is generated by `tools/gen_kanga2.py`: edit the parameters there and regenerate, rather than editing the cfg. Joint angles start at 0 in the start pose (the reference angle is taken at creation), so limits are relative to the start pose. Shape friction defaults to 0.2 (Box2D 2.x default the configs were tuned for). `BodyAngleSensor` sees angles in [-π, π] under Box2D 3 (was unbounded in 2.x). `hopper.cfg` and `walker.cfg` still use minA = 0, maxA = 2π, so any forward lean (a negative angle) reads as 0.

**Creature scale**: all configs are roughly human-sized (head at about 2 m, hopper and walker masses 0.3–0.6 kg), so muscle, force and power numbers compare across models, and everything fits the 640×480 view at 100 px/m. `hopper.cfg` was Froude-scaled down from an original 17 m design (see its header comment). `HEAD_FLOOR` (0.75 m, in `hopper.cpp`) is shared by all configs.

**Known objective issue**: fitness is the head's max x, so a creature can gain distance by diving forward and falling at the end of a run, or by scooting along on a tail or torso. `groundLimbs` closes the scooting route (kanga2 uses it). Energy isn't in the fitness yet; it's only tracked.

**Evolving kanga2** (5–10 runs per variant, 1000 generations; the spread between runs is large):
- without `groundLimbs`, most runs scoot on the tail
- with it, adding the rate/contact sensors doubled median fitness and let most runs last all 1000 steps
- an add-link mutation rate of 0.05 (instead of 0.2) helped further and cut network bloat
- population 400 helped about as much, at twice the cost per generation
- Muscles must keep their line of action clear of the joint over its whole range. A straight spring on the outside of a bend (like a quad without a kneecap) crosses the joint, and flips between extending and flexing it. The generator sweeps each muscle and rejects that, and takes rest-length ranges from the swept lengths.
- With the knee muscle behind the knee, nearly every run hops instead of shuffling. Early falls (head below `headFloor` at about 200–450 steps) still dominate the results, and a second hip muscle didn't help.

`legs.notes` holds the original design notes.
