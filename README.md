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

Running
-------

Programs are built into `build/`.  legs simply tests the physical
configuration and simulation, dropping whatever figure is given to it via -C
configuration file, and watching it fall (space resets, any other key quits).

hopper runs the actual genetic algorithm, optionally displaying fittest member every 10 generations.  Command line parameters are:

* -C configuration file
* -N number of generations (default 1000)
* -V if included, display simulation of fittest member every 10 generations

For example:

    ./build/hopper -C walker.cfg -N 200 -V

The rest of the many (many) algorithm parameters are specified in the configuration file, along with the specification of the walker.

The NEAT-only demos (xorTest, poleBalance [-V], maxTest) are built as well, and
testMult checks that repeated simulations of one genome are deterministic.

Configuration File
------------------

The configuration file is formatted to be read by [libconfig][2], which has a
simple and hopefully fairly obvious structure and formatting.  The config file
is broken up into sections:

[2]: http://www.hyperrealm.com/libconfig/

* global for NEAT genetic algorithm parameters
* limbs for specifying a list of physical limbs of the creature
* joints join limbs by name (currently just revolute type joints)
* muscles specify a list of joints that attach two limbs
* shapes name points on the limbs to be referenced in sensors
* sensors specify input that translate body position to input to the network.
  There are currently 3 types of sensors: Joint angle, Height of shape, Limb
  angle.

Three example configuration files are included:

* hopper.cfg: one legged hopper
* walker.cfg: two legged walker, now with feet
* kanga.cfg: fanciful one legged, 2-D kangaroo with big foot and tail

ToDo
----

* Better statistics gathering during simulation
* Remove stagnating species
* More Sensors (force, ground location)
* Energy consumption based objective function
* Ramped objective for more complicated tasks

