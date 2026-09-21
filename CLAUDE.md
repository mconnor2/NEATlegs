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

Targets: libraries `neat` (NEAT/, no graphics/physics deps), `display` (SDL3 wrapper), `physics` (World/Creature/BoxScreen); executables `legs`, `hopper`, `testMult`, `xorTest`, `maxTest`, `poleBalance`. `NEAT_PROFILE` (default ON) defines `PROFILE`, which adds `Genome::steps` and prints steps/sec per generation.

`ld: warning: building for macOS-X but linking with dylib ... built for newer version` comes from an outdated Command Line Tools SDK, not the project.

There is no unit-test framework. Sanity checks:
- `./build/xorTest`, `./build/poleBalance` — GA works (poleBalance reaches fitness ~1.0)
- `./build/testMult -C hopper.cfg` — same genome simulated 10× must print identical fitness (determinism)
- `./build/hopper -C walker.cfg -N 40` — headless GA run on a creature

Visual programs (`legs`, `hopper -V`, `poleBalance -V`) open an SDL window. To render headlessly use `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`.

## Architecture

**NEAT library (`NEAT/`)** — `GeneticAlgorithm` owns the population and `Specie`s; `Genome` holds genes and builds a `Network` (`createNewNetwork()`, `run(in, out)`); `InnovationStore` tracks innovation numbers. GA hyperparameters live in `ExpParameters`, loaded from the config's `global` section. The fitness function is a `std::function<double(const GenomeP)>` and **is called concurrently from `std::thread` workers** in `GeneticAlgorithm::runFitness()`, so it must be thread safe. Related constraints:
- `Network` borrows the genome's `links` array and writes neuron pointers into it, so one genome must never be evaluated twice concurrently (holds today since each genome appears once per population).
- The KISS RNG in `random.cpp` is `thread_local`; `dev_seed_rand()` seeds the calling thread and new threads derive from it.

**Physics (top level)** — Box2D 3 uses value ids (`b2BodyId`, `b2JointId`) instead of pointers; `boxTypes.h` holds the typedefs.
- `World` owns a `b2WorldId` (ground, stepping with substeps). Box2D's global world table isn't thread safe, so world create/destroy is mutex-guarded; libconfig lazily mutates `Config` on reads, so `createCreature` is mutex-guarded too.
- `Creature` builds bodies/joints/muscles/sensors from config (`initFromFile`), exposes named `limbs`, `joints`, `shapes` maps, and fills network input via `setInput()` (bias first). Unknown names in the config raise an error instead of creating null ids.
- `Muscle` is a damped spring between two body points, force applied once per `World::step()`; the network scales stiffness and rest length within configured min/max.
- `BoxScreen` maps meters→pixels and draws via `Display` (SDL3 renderer; lines/circles/points/text, frame limiter). Colors are `0xRRGGBBAA`.

**`hopper.cpp`** is the GA driver and fitness functor. Couplings with the config file:
- `nInput` = sensors + 1 (bias) and `nOutput` = 2 × muscles are **computed from the config**, overriding `global`.
- Outputs map to muscles as `out[2m]` → `scaleStrength`, `out[2m+1]` → `scaleLength`.
- The creature must define a shape named `"head"`; the run ends if it drops below `HEAD_FLOOR`; fitness = head's max x.

**Config sections**: `global` (NEAT params), `limbs`, `joints` (revolute only), `muscles`, `shapes` (named points on limbs), `sensors` (JointSensor, HeightSensor, BodyAngleSensor). Shape friction defaults to 0.2 (Box2D 2.x default the configs were tuned for). `BodyAngleSensor` sees angles in [-π, π] under Box2D 3 (was unbounded in 2.x).

**Known modeling issue**: muscle rest length/stiffness change instantly, which lets evolved controllers pump energy into the body; with `hopper.cfg` the GA finds "launch into the air" solutions (fitness in the hundreds of meters). Physics itself conserves momentum/energy correctly.

`legs.notes` holds the original design notes.
