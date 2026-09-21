#ifndef __CREATURE_H
#define __CREATURE_H

#include <box2d/box2d.h>
#include <vector>
#include <limits>
#include <map>
#include <libconfig.h++>

#include "boxTypes.h"
//#include "BoxScreen.h"
//#include "World.h"

class World;
class Creature;
class Muscle;
class BoxScreen;

using namespace std;

class Sensor;
typedef std::shared_ptr<Sensor> SensorP;
typedef vector<SensorP> sensorList;

class Creature {
    public:
    Creature (bool _useBias = true) : useBias(_useBias) { }

    /* Create Creature's body and add it to the world */
    int initFromFile(const libconfig::Config &config, World *w);

    // Apply muscle forces for the coming step
    void update ();
    // Account the work muscles did over the step just taken (dt seconds)
    void afterStep (float dt);

    void draw (BoxScreen *screen) const;

    void reset ();
    void activate ();

    inline int numSensors() const {
	return (sensors.size() + (useBias ? 1 : 0));
    }

    void setInput(double *input) const;

    // Mechanical work done by all muscles since the last reset().
    // positiveWork is energy put into the body, negativeWork (<= 0) is
    // energy absorbed (damping, braking).
    double positiveWork() const;
    double negativeWork() const;

    // If we want to access some body parts by name
    bodyMap limbs;
    jointMap joints;
    shapeMap shapes;

    /* Muscles necessary for controlling creature */
    muscleList muscles;
    
    private:

    /* Bodies and joints specifying the creature */
    bodyPosList parts;

    /* Sensors that translate input to the brain */
    sensorList sensors;
    const bool useBias;
};

class Muscle {
    public:
	static constexpr float Unlimited =
	    std::numeric_limits<float>::infinity();

	Muscle (BodyId b1, const Vec2 &l1, 
		BodyId b2, const Vec2 &l2,
		float _minK, float _maxK, 
		float _minEq, float _maxEq,
		float _kd,
		float _maxForce = Unlimited, float _maxPower = Unlimited) :
		k(0), minK(_minK), maxK(_maxK), kd(_kd),
		eq(0), minEq(_minEq), maxEq(_maxEq),
		maxForce(_maxForce), maxPower(_maxPower),
		body1(b1), body2(b2), end1L(l1), end2L(l2)
	{
	    reset();
	}

	// Mid stiffness, with the rest length at the muscle's current
	// length (clamped to its range) so it starts out relaxed.  Also
	// zeroes the work counters.
	void reset ();

	// Compute and apply this step's force.  Returns the force (positive
	// pushes the attachment points apart, negative pulls them together).
	float update ();

	// Account work done over the step just taken (dt seconds)
	void afterStep (float dt);

	// Seconds of full power a muscle can store for a burst
	static constexpr float ReserveSeconds = 0.25f;

	float maxPowerLimit () const { return maxPower; }

	double positiveWork () const { return posWork; }
	double negativeWork () const { return negWork; }

	void draw (BoxScreen *screen) const;

	void scaleLength (double sc);

	void scaleStrength (double sc);

    private:
	float k, minK, maxK;	//Hook's constant
	float kd;		//Spring dampening
	float eq, minEq, maxEq;	//Spring's equilibrium distance

	//Limits on what the muscle can deliver.  Without a power limit a
	// controller can pump unbounded energy in by changing k and eq, since
	// those change the spring's stored energy for free.
	//  - |force| <= maxForce
	//  - instantaneous power (force * lengthening rate) <= maxPower,
	//    which acts like muscle's force-velocity curve
	//  - measured positive work draws on an energy reserve that refills
	//    at maxPower and holds ReserveSeconds of it.  With the reserve
	//    empty the muscle goes slack (damping only).  This is what
	//    guarantees average power over any window stays <= maxPower; the
	//    instantaneous limit alone can't, because a stiff muscle starting
	//    from rest reads zero power yet does lots of work within a step.
	float maxForce, maxPower;
	float reserve = 0;

	//Work accounting.  Box2D applies a force at a point as a constant
	// force on the centre of mass plus a constant torque for the whole
	// step, so the exact work is F.dx_com + torque*dtheta on each body.
	// (F * change in muscle length is only a first-order approximation
	// and drifts when limbs rotate quickly.)
	Vec2 appliedForce = {0, 0};	//On body1; body2 gets the opposite
	float torque1 = 0, torque2 = 0;
	Vec2 com1Start = {0, 0}, com2Start = {0, 0};
	b2Rot rot1Start = b2Rot_identity, rot2Start = b2Rot_identity;
	double posWork = 0, negWork = 0;

	float currentLength () const;

	BodyId body1, body2;	//Bodies muscle connects
	Vec2 end1L, end2L;	//Local point of contact (fixed)
};

#endif
