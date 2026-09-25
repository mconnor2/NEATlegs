#include "CreatureSpec.h"

#include <iostream>
#include <stdexcept>

#include <libconfig.h++>

using namespace std;
using libconfig::Setting;

namespace {

runtime_error error (const string &where, const string &what) {
    return runtime_error(where + ": " + what);
}

const Setting &child (const Setting &s, const char *key, const string &where) {
    if (!s.exists(key)) throw error(where, string("missing '") + key + "'");
    return s[key];
}

//A number, accepting integers where decimals are expected (as libconfig's
// auto-conversion did)
double number (const Setting &s, const char *key, const string &where) {
    const Setting &v = child(s, key, where);
    switch (v.getType()) {
        case Setting::TypeInt: return (int)v;
        case Setting::TypeInt64: return (double)(long long)v;
        case Setting::TypeFloat: return (double)v;
        default: throw error(where, string("'") + key + "' must be a number");
    }
}

float optionalFloat (const Setting &s, const char *key, float fallback,
                     const string &where) {
    return s.exists(key) ? (float)number(s, key, where) : fallback;
}

string text (const Setting &s, const char *key, const string &where) {
    const Setting &v = child(s, key, where);
    if (v.getType() != Setting::TypeString)
        throw error(where, string("'") + key + "' must be a string");
    return v.c_str();
}

Vec2 point (const Setting &s, const char *key, const string &where) {
    const Setting &p = child(s, key, where);
    string at = where + "." + key;
    return Vec2{(float)number(p, "x", at), (float)number(p, "y", at)};
}

const Setting &list (const libconfig::Config &config, const char *key) {
    if (!config.exists(key))
        throw runtime_error(string("config has no '") + key + "' section");
    const Setting &s = config.lookup(key);
    if (!s.isList() && !s.isArray())
        throw runtime_error(string("'") + key + "' must be a list");
    return s;
}

string entry (const char *kind, int i, const string &name = "") {
    return string(kind) + " " + to_string(i) +
           (name.empty() ? "" : " '" + name + "'");
}

template<class T>
int indexOf (const vector<T> &items, const string &name) {
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].name == name) return (int)i;
    return -1;
}

template<class T>
void checkUnique (const vector<T> &items, const string &name,
                  const string &where) {
    if (indexOf(items, name) >= 0)
        throw error(where, "duplicate name '" + name + "'");
}

int resolve (int index, const char *kind, const string &name,
             const string &where) {
    if (index < 0)
        throw error(where, string("unknown ") + kind + " '" + name + "'");
    return index;
}

}

int CreatureSpec::limbIndex (const string &name) const {
    return indexOf(limbs, name);
}

int CreatureSpec::jointIndex (const string &name) const {
    return indexOf(joints, name);
}

int CreatureSpec::pointIndex (const string &name) const {
    return indexOf(points, name);
}

CreatureSpec parseCreatureSpec (const libconfig::Config &config) {
    CreatureSpec spec;

    const Setting &limbs = list(config, "limbs");
    for (int i = 0; i < limbs.getLength(); ++i) {
        const Setting &l = limbs[i];
        string where = entry("limb", i);
        LimbSpec limb;
        limb.name = text(l, "name", where);
        where = entry("limb", i, limb.name);
        checkUnique(spec.limbs, limb.name, where);
        limb.position = point(l, "position", where);
        limb.angle = optionalFloat(l, "angle", 0.0f, where);
        if (l.exists("angularDamping"))
            limb.angularDamping = (float)number(l, "angularDamping", where);

        const Setting &shapes = child(l, "shapes", where);
        for (int j = 0; j < shapes.getLength(); ++j) {
            const Setting &s = shapes[j];
            string at = where + ": " + entry("shape", j);
            ShapeSpec shape;
            string type = text(s, "type", at);
            shape.density = (float)number(s, "density", at);
            shape.friction = optionalFloat(s, "friction", shape.friction, at);
            if (s.exists("groupIndex"))
                shape.groupIndex = (int)number(s, "groupIndex", at);
            if (type == "box") {
                shape.type = ShapeSpec::Box;
                shape.w = (float)number(s, "w", at);
                shape.h = (float)number(s, "h", at);
            } else if (type == "ball") {
                shape.type = ShapeSpec::Ball;
                shape.radius = (float)number(s, "radius", at);
                shape.position = point(s, "position", at);
            } else {
                throw error(at, "unknown shape type '" + type + "'");
            }
            limb.shapes.push_back(shape);
        }
        spec.limbs.push_back(limb);
    }

    const Setting &joints = list(config, "joints");
    for (int i = 0; i < joints.getLength(); ++i) {
        const Setting &j = joints[i];
        string where = entry("joint", i);
        JointSpec joint;
        joint.name = text(j, "name", where);
        where = entry("joint", i, joint.name);
        checkUnique(spec.joints, joint.name, where);
        string type = text(j, "type", where);
        if (type != "revolute")
            throw error(where, "unknown joint type '" + type + "'");
        string obj1 = text(j, "obj1", where), obj2 = text(j, "obj2", where);
        joint.limb1 = resolve(spec.limbIndex(obj1), "limb", obj1, where);
        joint.limb2 = resolve(spec.limbIndex(obj2), "limb", obj2, where);
        joint.anchor = point(j, "position", where);
        joint.lowerAngle = (float)number(j, "lowerAngle", where);
        joint.upperAngle = (float)number(j, "upperAngle", where);
        spec.joints.push_back(joint);
    }

    const Setting &muscles = list(config, "muscles");
    bool warned = false;
    for (int i = 0; i < muscles.getLength(); ++i) {
        const Setting &m = muscles[i];
        string where = entry("muscle", i);
        MuscleSpec muscle;
        muscle.name = text(m, "name", where);
        where = entry("muscle", i, muscle.name);
        string obj1 = text(m, "obj1", where), obj2 = text(m, "obj2", where);
        muscle.limb1 = resolve(spec.limbIndex(obj1), "limb", obj1, where);
        muscle.limb2 = resolve(spec.limbIndex(obj2), "limb", obj2, where);
        muscle.pos1 = point(m, "pos1", where);
        muscle.pos2 = point(m, "pos2", where);
        muscle.minK = (float)number(m, "minK", where);
        muscle.maxK = (float)number(m, "maxK", where);
        muscle.minEq = (float)number(m, "minEq", where);
        muscle.maxEq = (float)number(m, "maxEq", where);
        muscle.kd = (float)number(m, "kd", where);
        muscle.maxForce = optionalFloat(m, "maxForce", MuscleSpec::Unlimited,
                                        where);
        muscle.maxPower = optionalFloat(m, "maxPower", MuscleSpec::Unlimited,
                                        where);
        if (!m.exists("maxPower") && !warned) {
            cerr<<"Warning: muscle '"<<muscle.name<<"' has no maxPower, so a "
                  "controller can pump unbounded energy into the creature"
                <<endl;
            warned = true;
        }
        spec.muscles.push_back(muscle);
    }

    if (config.exists("shapes")) {
        const Setting &points = list(config, "shapes");
        for (int i = 0; i < points.getLength(); ++i) {
            const Setting &p = points[i];
            string where = entry("shape", i);
            PointSpec pt;
            pt.name = text(p, "name", where);
            where = entry("shape", i, pt.name);
            checkUnique(spec.points, pt.name, where);
            string body = text(p, "body", where);
            pt.limb = resolve(spec.limbIndex(body), "limb", body, where);
            pt.localPos = point(p, "position", where);
            spec.points.push_back(pt);
        }
    }

    if (config.exists("sensors")) {
        const Setting &sensors = list(config, "sensors");
        for (int i = 0; i < sensors.getLength(); ++i) {
            const Setting &s = sensors[i];
            string where = entry("sensor", i);
            SensorSpec sensor;
            string type = text(s, "type", where),
                   target = text(s, "target", where);
            where += " (" + type + ")";
            if (type == "JointSensor") {
                sensor.type = SensorSpec::Joint;
                sensor.target = resolve(spec.jointIndex(target), "joint",
                                        target, where);
            } else if (type == "HeightSensor") {
                sensor.type = SensorSpec::Height;
                sensor.target = resolve(spec.pointIndex(target), "shape",
                                        target, where);
                sensor.min = number(s, "minH", where);
                sensor.max = number(s, "maxH", where);
            } else if (type == "BodyAngleSensor") {
                sensor.type = SensorSpec::BodyAngle;
                sensor.target = resolve(spec.limbIndex(target), "limb",
                                        target, where);
                sensor.min = number(s, "minA", where);
                sensor.max = number(s, "maxA", where);
            } else if (type == "AngularVelocitySensor") {
                sensor.type = SensorSpec::AngularVelocity;
                sensor.target = resolve(spec.limbIndex(target), "limb",
                                        target, where);
                sensor.min = number(s, "minW", where);
                sensor.max = number(s, "maxW", where);
            } else if (type == "VelocitySensor") {
                sensor.type = SensorSpec::Velocity;
                sensor.target = resolve(spec.limbIndex(target), "limb",
                                        target, where);
                string axis = text(s, "axis", where);
                if (axis != "x" && axis != "y")
                    throw error(where, "axis must be \"x\" or \"y\"");
                sensor.vertical = axis == "y";
                sensor.min = number(s, "minV", where);
                sensor.max = number(s, "maxV", where);
            } else if (type == "ContactSensor") {
                sensor.type = SensorSpec::Contact;
                sensor.target = resolve(spec.limbIndex(target), "limb",
                                        target, where);
            } else {
                throw error(where, "unknown sensor type");
            }
            spec.sensors.push_back(sensor);
        }
    }

    return spec;
}
