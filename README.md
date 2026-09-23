NEAT Legs
=========

Evolves a neural controller for walking locomotion.  A physical model is
defined via configuration file, and neural networks compete and evolve to see
who can control it (with specified muscles and sensors) and best satisfy the
objective function, which is currently just how far can it move in a fixed
number of time steps.

Includes my own (almost complete) implementation of Ken Stanley's [NEAT][1]
genetic algorithm for neural networks, which can be used separately from the
walking code.  The NEAT sub-directory includes test code for learning XOR and
pole balancing (standard tasks mentioned in Stanley's thesis).

[1]: http://www.cs.ucf.edu/~kstanley/neat.html

The physics simulation uses Box2D, so figure just a 2D rigid body, which makes
two legged locomotion far easier (don't need to balance along z-axis).  Limbs
are specified in terms of boxes and circles, with simple revolute joints.  Some
form of walking motion is usually achieved, most likely due to the
passive-dynamic nature of the muscle controls.  Muscles are modeled as damped
springs, and the neural controller can modify both the desired length of the
muscle, along with the spring constant to control whether the muscle should be
tense or loose.

Requirements
------------

The code depends on:

* [Box2D][3] v3.x
* SDL3 and SDL3_ttf
* libconfig (C++ bindings)
* CMake 3.20+ and a C++17 compiler

[3]: https://box2d.org

On macOS with Homebrew:

    brew install cmake pkgconf box2d sdl3 sdl3_ttf libconfig

Fitness evaluation is spread across all hardware threads using the standard
library, so no threading library is needed.

Building
--------

    cmake -S . -B build
    cmake --build build -j
    (cd build && ctest)      # tests

Running
-------

Programs are built into `build/`.  legs simply tests the physical
configuration and simulation, dropping whatever figure is given to it via -C
configuration file, and watching it fall (space resets, any other key quits).

hopper runs the actual genetic algorithm, optionally displaying fittest member every 10 generations.  Command line parameters are:

* -C configuration file
* -N number of generations (default 1000)
* -V watch evolution: a window continuously replays the most recently
  published fittest member, with run statistics and a fitness history chart
  overlaid, while the GA keeps running in the background.  Space jumps to the
  newest published member; closing the window (or any other key) stops the
  run after the current generation.
* -d publish a new fittest member to the window every this many generations
  (default 10; implies -V)
* -o output directory for the run (default runs/<config>-<timestamp>)
* -s snapshot the top genomes every this many generations (default 10, 0 off)
* -k number of genomes per snapshot (default 3)
* -S random seed; the same seed repeats a run exactly.  Without it a seed is
  chosen, printed and saved to the run directory as seed.txt
* -r replay a saved genome in a window instead of running the GA

For example:

    ./build/hopper -C walker.cfg -N 200 -V
    ./build/hopper -C walker.cfg -N 500 -d 5
    ./build/hopper -C walker.cfg -r runs/walker-20260921-153000/best.genome

Each generation prints a summary line: population, species count, max/mean
fitness and its standard deviation, diversity (mean pairwise compatibility
distance between genomes), and mean hidden nodes / enabled links.  The run
directory holds:

* stats.csv: one row per generation with the numbers above and more
* species.csv: size and fitness of each species per generation (species ids
  are stable across generations)
* best.genome: the best genome seen so far
* snapshots/genNNNNN_rankK.genome: the top genomes at each snapshot
* config.cfg: copy of the configuration, so a saved genome can be replayed
  with `-C runs/.../config.cfg -r ...`

Genome files are plain text: a header line followed by one line per link.

The rest of the many (many) algorithm parameters are specified in the configuration file, along with the specification of the walker.

The NEAT-only demos (xorTest, poleBalance [-V], maxTest) are built as well.
The tests (ctest) check the NEAT library, creature config parsing, episodes
and their end rules, drawing, muscle energy limits, and that a seeded run is
fully repeatable.

Configuration File
------------------

The configuration file is formatted to be read by [libconfig][2], which has a
simple and hopefully fairly obvious structure and formatting.  The config file
is broken up into sections:

[2]: http://www.hyperrealm.com/libconfig/

* global for NEAT genetic algorithm parameters
* limbs for specifying a list of physical limbs of the creature
* joints join limbs by name (currently just revolute type joints)
* muscles specify a list of joints that attach two limbs.  Each muscle is a
  damped spring whose stiffness (minK..maxK) and rest length (minEq..maxEq)
  the network controls.  Optional maxForce and maxPower (watts) limit what it
  can deliver: without maxPower a controller can pump unbounded energy into
  the creature by changing stiffness and rest length.  Each muscle has an
  energy reserve worth 0.25 s of maxPower for bursts; average power over time
  can't exceed maxPower.  The included configs use roughly 8x body weight for
  maxForce and 50 W per kg of body mass for maxPower, split across muscles.
* shapes name points on the limbs to be referenced in sensors
* sensors specify input that translate body state to input to the network:
  JointSensor (joint angle), HeightSensor (height of a named shape),
  BodyAngleSensor (limb angle; Box2D angles are in -pi..pi, so use a range
  covering negative angles), AngularVelocitySensor (limb spin rate),
  VelocitySensor (limb velocity along x or y), ContactSensor (1 while a limb
  touches the ground).

Optional settings in the global section:

* headFloor: the run ends when the head drops below this height (default
  0.75 m)
* groundLimbs: list of limbs allowed to touch the ground, e.g. ("foot");
  any other limb touching ends the run as a fall

Three example configuration files are included:

* hopper.cfg: one legged hopper
* walker.cfg: two legged walker, now with feet
* kanga.cfg: fanciful one legged, 2-D kangaroo with big foot and tail (the
  original; its torso jams against the hip limit and it doesn't hop)
* kanga2.cfg: redesigned one-legged kangaroo/raptor with a forward-leaning
  torso balanced by a tail.  Generated by tools/gen_kanga2.py, which derives
  the geometry, balance and muscle limits from a few parameters

ToDo
----

* Better statistics gathering during simulation
* Remove stagnating species
* More Sensors (force, ground location)
* Energy consumption based objective function
* Ramped objective for more complicated tasks

