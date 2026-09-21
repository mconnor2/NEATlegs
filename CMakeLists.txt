cmake_minimum_required(VERSION 3.20)
project(NEATlegs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
find_package(box2d 3 CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIGPP REQUIRED IMPORTED_TARGET libconfig++)

# NEAT genetic algorithm library (no physics or graphics dependencies)
add_library(neat STATIC
    NEAT/random.cpp
    NEAT/Network.cpp
    NEAT/InnovationStore.cpp
    NEAT/Genome.cpp
    NEAT/Specie.cpp
    NEAT/GeneticAlgorithm.cpp
    NEAT/RunLog.cpp
)
target_include_directories(neat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/NEAT)
target_link_libraries(neat PUBLIC PkgConfig::LIBCONFIGPP Threads::Threads)

# SDL3 window/renderer/font helper shared by all visual programs
add_library(display STATIC Display.cpp)
target_include_directories(display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(display PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(display PRIVATE
    NEATLEGS_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ProggyClean.ttf")

# Run statistics drawn over a Display (only needs NEAT/Stats.h)
add_library(statsview STATIC StatsOverlay.cpp)
target_include_directories(statsview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/NEAT)
target_link_libraries(statsview PUBLIC display)

# Box2D creature simulation
add_library(physics STATIC World.cpp Creature.cpp BoxScreen.cpp)
target_include_directories(physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(physics PUBLIC box2d::box2d display PkgConfig::LIBCONFIGPP)

add_executable(legs legs.cpp)
target_link_libraries(legs PRIVATE physics)

add_executable(hopper hopper.cpp)
target_link_libraries(hopper PRIVATE physics neat statsview)

add_executable(testMult testMult.cpp)
target_link_libraries(testMult PRIVATE physics neat)

# NEAT-only demos
add_executable(xorTest NEAT/xorTest.cpp)
target_link_libraries(xorTest PRIVATE neat)

add_executable(maxTest NEAT/maxTest.cpp)
target_link_libraries(maxTest PRIVATE neat)

add_executable(poleBalance NEAT/poleBalance.cpp)
target_link_libraries(poleBalance PRIVATE neat statsview)

# Physics checks, run with ctest
enable_testing()
add_executable(muscleEnergy tests/muscleEnergy.cpp)
target_link_libraries(muscleEnergy PRIVATE physics)
foreach(creature hopper walker kanga)
    add_test(NAME muscleEnergy_${creature}
             COMMAND muscleEnergy ${CMAKE_CURRENT_SOURCE_DIR}/${creature}.cfg)
endforeach()
