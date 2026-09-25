#include "Creature.h"
#include "World.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

using namespace std;

inline double limit_norm (double v, const double min, const double max) {
    if (v < min) v = min;
    if (v > max) v = max;
    return (v - min) / (max - min);
}

class Sensor {
    public:
        virtual ~Sensor() { }
        virtual double read() = 0;
};

class JointSensor : public Sensor {
    public:
        JointSensor (JointId j) : joint(j) { }
        double read() {
            return limit_norm(b2RevoluteJoint_GetAngle(joint),
                              b2RevoluteJoint_GetLowerLimit(joint),
                              b2RevoluteJoint_GetUpperLimit(joint));
        }
    private:
        const JointId joint;
};

class HeightSensor : public Sensor {
    public:
        HeightSensor(const shapePos &_t,
                     const double _minH, const double _maxH) :
            t(_t), minH(_minH), maxH(_maxH) { }
        double read() {
            Vec2 v = b2Body_GetWorldPoint(t.b, t.localPos);
            return limit_norm(v.y,minH,maxH);
        }
    private:
        const shapePos t;
        const double minH, maxH;
};

class BodyAngleSensor : public Sensor {
    public:
        BodyAngleSensor(BodyId _t,
                        const double _minA, const double _maxA) :
            t(_t), minA(_minA), maxA(_maxA) { }

        //Note Box2D 3 reports body angle in [-pi, pi]
        double read() {
            return limit_norm(b2Rot_GetAngle(b2Body_GetRotation(t)),minA,maxA);
        }
    private:
        const BodyId t;
        const double minA, maxA;
};

//Rotation rate of a limb (rad/s, CCW positive)
class AngularVelocitySensor : public Sensor {
    public:
        AngularVelocitySensor(BodyId _t, double _minW, double _maxW) :
            t(_t), minW(_minW), maxW(_maxW) { }
        double read() {
            return limit_norm(b2Body_GetAngularVelocity(t), minW, maxW);
        }
    private:
        const BodyId t;
        const double minW, maxW;
};

//Velocity of a limb's centre of mass along x or y (m/s)
class VelocitySensor : public Sensor {
    public:
        VelocitySensor(BodyId _t, bool _vertical, double _minV, double _maxV) :
            t(_t), vertical(_vertical), minV(_minV), maxV(_maxV) { }
        double read() {
            Vec2 v = b2Body_GetLinearVelocity(t);
            return limit_norm(vertical ? v.y : v.x, minV, maxV);
        }
    private:
        const BodyId t;
        const bool vertical;
        const double minV, maxV;
};

//1 while a limb touches anything outside the creature (the ground), else 0
class ContactSensor : public Sensor {
    public:
        ContactSensor(const Creature *_c, BodyId _t) : c(_c), t(_t) { }
        double read() { return c->touchesOutside(t) ? 1.0 : 0.0; }
    private:
        const Creature *c;
        const BodyId t;
};

/**
 * Build the creature from a parsed spec: limbs (bodies with their shapes),
 * then joints, muscles, named points and sensors, in spec order.
 */
Creature::Creature (const CreatureSpec &spec, World &w) {
    for (const LimbSpec &l : spec.limbs) {
        b2BodyDef bone = b2DefaultBodyDef();
        bone.type = b2_dynamicBody;
        bone.position = l.position;
        bone.rotation = b2MakeRot(l.angle);
        if (l.angularDamping) bone.angularDamping = *l.angularDamping;

        BodyId limb = b2CreateBody(w.id(), &bone);
        for (const ShapeSpec &sh : l.shapes) {
            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = sh.density;
            shapeDef.material.friction = sh.friction;
            shapeDef.filter.groupIndex = sh.groupIndex;
            if (sh.type == ShapeSpec::Box) {
                b2Polygon box = b2MakeBox(sh.w, sh.h);
                b2CreatePolygonShape(limb, &shapeDef, &box);
            } else {
                b2Circle ball;
                ball.radius = sh.radius;
                ball.center = sh.position;
                b2CreateCircleShape(limb, &shapeDef, &ball);
            }
        }

        BodyPos bp;
        bp.b = limb;
        bp.defaultPos = l.position;
        bp.angle = l.angle;
        parts.push_back(bp);
        limbBodies.push_back(limb);
        limbs.insert(make_pair(l.name, limb));
    }

    for (const JointSpec &j : spec.joints) {
        BodyId bodyA = limbBodies[j.limb1], bodyB = limbBodies[j.limb2];

        //Equivalent of Box2D 2.x b2RevoluteJointDef::Initialize
        b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
        jointDef.bodyIdA = bodyA;
        jointDef.bodyIdB = bodyB;
        jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA, j.anchor);
        jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB, j.anchor);
        jointDef.referenceAngle =
            b2Rot_GetAngle(b2Body_GetRotation(bodyB)) -
            b2Rot_GetAngle(b2Body_GetRotation(bodyA));
        jointDef.lowerAngle = j.lowerAngle;
        jointDef.upperAngle = j.upperAngle;
        jointDef.enableLimit = true;

        JointId joint = b2CreateRevoluteJoint(w.id(), &jointDef);
        jointIds.push_back(joint);
        joints.insert(make_pair(j.name, joint));
    }

    for (const MuscleSpec &m : spec.muscles) {
        muscles.push_back(MuscleP(new Muscle(
            limbBodies[m.limb1], m.pos1, limbBodies[m.limb2], m.pos2,
            m.minK, m.maxK, m.minEq, m.maxEq, m.kd, m.maxForce, m.maxPower)));
    }

    vector<shapePos> points;
    for (const PointSpec &p : spec.points) {
        shapePos sp;
        sp.localPos = p.localPos;
        sp.b = limbBodies[p.limb];
        points.push_back(sp);
        shapes.insert(make_pair(p.name, sp));
    }

    for (const SensorSpec &s : spec.sensors) {
        Sensor *sensor = nullptr;
        switch (s.type) {
            case SensorSpec::Joint:
                sensor = new JointSensor(jointIds[s.target]);
                break;
            case SensorSpec::Height:
                sensor = new HeightSensor(points[s.target], s.min, s.max);
                break;
            case SensorSpec::BodyAngle:
                sensor = new BodyAngleSensor(limbBodies[s.target],
                                             s.min, s.max);
                break;
            case SensorSpec::AngularVelocity:
                sensor = new AngularVelocitySensor(limbBodies[s.target],
                                                   s.min, s.max);
                break;
            case SensorSpec::Velocity:
                sensor = new VelocitySensor(limbBodies[s.target], s.vertical,
                                            s.min, s.max);
                break;
            case SensorSpec::Contact:
                sensor = new ContactSensor(this, limbBodies[s.target]);
                break;
        }
        sensors.push_back(SensorP(sensor));
    }
}

void Creature::reset () {
    Vec2 zero = {0, 0};
    for (bodyPosList::iterator i = parts.begin();
         i != parts.end(); ++i)
    {
        b2Body_SetTransform(i->b, i->defaultPos, b2MakeRot(i->angle));
        b2Body_SetLinearVelocity(i->b, zero);
        b2Body_SetAngularVelocity(i->b, 0);
        b2Body_SetAwake(i->b, true);
    }
    for (muscleList::iterator i = muscles.begin();
         i != muscles.end(); ++i)
    {
        (*i)->reset();
    }
}

void Creature::activate () {
    for (bodyPosList::iterator i = parts.begin();
         i != parts.end(); ++i)
    {
        b2Body_Enable(i->b);
    }
}

void Creature::draw (Renderer &r) const {
    for (const BodyPos &p : parts) drawBody(p.b, r);
    for (const MuscleP &m : muscles) m->draw(r);
}

void Creature::update () {
    //For now just update forces of the muscles
    //  XXX in future will also handle passing info to brain, updating
    //  muscles and such.
    for (muscleList::iterator i = muscles.begin();
         i != muscles.end(); ++i)
    {
        (*i)->update();
    }
}

void Creature::afterStep (float dt) {
    for (auto &m : muscles) m->afterStep(dt);
}

bool Creature::touchesOutside (BodyId b) const {
    int cap = b2Body_GetContactCapacity(b);
    if (cap == 0) return false;
    std::vector<b2ContactData> contacts(cap);
    int n = b2Body_GetContactData(b, contacts.data(), cap);
    for (int i = 0; i < n; ++i) {
        if (contacts[i].manifold.pointCount == 0) continue;
        BodyId other = b2Shape_GetBody(contacts[i].shapeIdA);
        if (B2_ID_EQUALS(other, b))
            other = b2Shape_GetBody(contacts[i].shapeIdB);
        bool own = false;
        for (const BodyPos &p : parts)
            if (B2_ID_EQUALS(p.b, other)) { own = true; break; }
        if (!own) return true;
    }
    return false;
}

double Creature::positiveWork () const {
    double w = 0;
    for (auto &m : muscles) w += m->positiveWork();
    return w;
}

double Creature::negativeWork () const {
    double w = 0;
    for (auto &m : muscles) w += m->negativeWork();
    return w;
}

double Creature::forceTime () const {
    double ft = 0;
    for (auto &m : muscles) ft += m->forceTime();
    return ft;
}

void Creature::setInput(double *input) const {
    *input++ = 1.0;     //Bias
    for (sensorList::const_iterator i = sensors.begin();
         i != sensors.end(); ++i)
    {
        *input++ = (*i)->read();
    }
}


float Muscle::currentLength () const {
    return b2Length(b2Body_GetWorldPoint(body1, end1L) -
                    b2Body_GetWorldPoint(body2, end2L));
}

void Muscle::reset () {
    k = (minK + maxK) / 2.;
    eq = currentLength();
    if (eq < minEq) eq = minEq;
    if (eq > maxEq) eq = maxEq;
    appliedForce = {0, 0};
    torque1 = torque2 = 0;
    posWork = negWork = 0;
    forceSeconds = 0;
    reserve = maxPower * ReserveSeconds;
}

/**
 * Find and apply the force between the two bodies muscle is attached to.
 *
 * returns the force (positive pushes apart, negative pulls together).
 */
float Muscle::update () {
    Vec2 a1W = b2Body_GetWorldPoint(body1, end1L);
    Vec2 a2W = b2Body_GetWorldPoint(body2, end2L);

    float length;
    Vec2 diff = b2GetLengthAndNormalize(&length, a1W - a2W);
    //XXX check if length == 0?
    //diff now normalized, points from a2->a1
    //diff *= 1.0f/length;

    //Now find how far we are from equilibrium to determine spring force
    float force = k*(eq-length);

    //Also apply dampening
    // find relative velocity of two points
    Vec2 vel = b2Body_GetLocalPointVelocity(body1, end1L) -
               b2Body_GetLocalPointVelocity(body2, end2L);
    float lengthening = b2Dot(vel, diff);
    force -= kd * lengthening;

    //Out of energy: slack, only the (dissipative) damping remains
    if (reserve <= 0) force = -kd * lengthening;

    //Muscle limits.  Power delivered to the bodies is force times the
    // rate the attachment points separate; only positive power (energy
    // going in) is limited.
    if (force > maxForce) force = maxForce;
    if (force < -maxForce) force = -maxForce;
    if (force * lengthening > maxPower) force = maxPower / lengthening;

    diff *= force;

    com1Start = b2Body_GetWorldCenterOfMass(body1);
    com2Start = b2Body_GetWorldCenterOfMass(body2);
    rot1Start = b2Body_GetRotation(body1);
    rot2Start = b2Body_GetRotation(body2);
    appliedForce = diff;
    torque1 = b2Cross(a1W - com1Start, diff);
    torque2 = b2Cross(a2W - com2Start, -diff);

    //Now apply force to body1
    b2Body_ApplyForce(body1, diff, a1W, true);

    //Reverse force, and apply to body2
    diff *= -1;
    b2Body_ApplyForce(body2, diff, a2W, true);

    return force;
}

/**
 * Work done over the step by the constant force and torque applied to
 * each body (positive = energy into the bodies).
 */
void Muscle::afterStep (float dt) {
    Vec2 d1 = b2Body_GetWorldCenterOfMass(body1) - com1Start,
         d2 = b2Body_GetWorldCenterOfMass(body2) - com2Start;
    float th1 = b2RelativeAngle(b2Body_GetRotation(body1), rot1Start),
          th2 = b2RelativeAngle(b2Body_GetRotation(body2), rot2Start);
    double w = b2Dot(appliedForce, d1) + torque1 * th1
             - b2Dot(appliedForce, d2) + torque2 * th2;
    if (w > 0) posWork += w;
    else negWork += w;
    forceSeconds += b2Length(appliedForce) * dt;

    if (!std::isinf(maxPower)) {
        reserve += maxPower * dt - (w > 0 ? w : 0);
        reserve = std::min(reserve, maxPower * ReserveSeconds);
    }
}

void Muscle::draw (Renderer &r) const {
    //Just draw line representing spring
    r.segment(b2Body_GetWorldPoint(body1, end1L),
              b2Body_GetWorldPoint(body2, end2L), MuscleColor);
}

//Set length or strength of muscle to between min and max setting,
// linearly according to sc (between 0 and 1)
void Muscle::scaleLength (double sc) {
    if (sc < 0) sc = 0;
    if (sc > 1) sc = 1;
    eq = minEq + sc * (maxEq - minEq);
}

void Muscle::scaleStrength (double sc) {
    if (sc < 0) sc = 0;
    if (sc > 1) sc = 1;
    k = minK + sc * (maxK - minK);
}
