#include "Creature.h"
#include "World.h"
#include "BoxScreen.h"

#include <iostream>
#include <stdexcept>
#include <stdlib.h>

using namespace libconfig;

//Look up a named part, failing loudly instead of silently handing Box2D
// a null id when the config references a name that was never defined.
template<class Map>
typename Map::mapped_type findPart(const Map &m, const string &name,
				   const char *what) {
    typename Map::const_iterator i = m.find(name);
    if (i == m.end())
	throw runtime_error(string("unknown ") + what + " '" + name + "'");
    return i->second;
}

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

int readLimbs(Setting &limbConfig, bodyMap &limbs, bodyPosList &parts,
	      World *w) 
{ 
    //Limbs specify:
    // name
    // position.{x,y} 
    // angle
    // angularDamping
    // list of shapes:
    //    each has type box or ball
    //    for box: w, h
    //    for ball: radius
    //	  density
    //    optional position
    //	  optional friction
    int nLimbs = limbConfig.getLength();
    for (int i = 0; i<nLimbs; ++i) {
	try {
	    Setting &curLimb = limbConfig[i];
	    string name = curLimb["name"];
	    float x = curLimb["position"]["x"], 
		  y = curLimb["position"]["y"];
	    
	    float angle = 0.0f;
	    b2BodyDef bone = b2DefaultBodyDef();
	    bone.type = b2_dynamicBody;
	    bone.position = {x,y};
	    curLimb.lookupValue("angle", angle);
	    bone.rotation = b2MakeRot(angle);
	    curLimb.lookupValue("angularDamping", bone.angularDamping);
	    
	    BodyId limb = b2CreateBody(w->id(), &bone);
	    BodyPos bp;
	    bp.b = limb;
	    bp.defaultPos = {x,y};
	    bp.angle = angle;
	    
	    Setting &shapes = curLimb["shapes"];
	    int nShapes = shapes.getLength();
	    for (int j = 0; j<nShapes; ++j) {
		Setting &curShape = shapes[j];
		string type = curShape["type"];
		b2ShapeDef shapeDef = b2DefaultShapeDef();
		
		shapeDef.density = curShape["density"];
		
		//Keep Box2D 2.x default friction, which the configs assume
		shapeDef.material.friction = 0.2f;
		curShape.lookupValue("friction", shapeDef.material.friction);
		int groupIndex = 0;
		curShape.lookupValue("groupIndex", groupIndex);
		shapeDef.filter.groupIndex = groupIndex;
		if (type == "box") {
		    float w = curShape["w"],
			  h = curShape["h"];
		    b2Polygon box = b2MakeBox(w,h);
		    b2CreatePolygonShape(limb, &shapeDef, &box);
		} else if (type == "ball") {
		    b2Circle ball;
		    ball.radius = curShape["radius"];
		    float x = curShape["position"]["x"],
			  y = curShape["position"]["y"];
		    ball.center = {x,y};
		    b2CreateCircleShape(limb, &shapeDef, &ball);
		} else {
		    cerr<<"Creature::initFromFile limb "<<name
			<<", shape "<<j<<" type "<<type<<" unknown."<<endl;
		    return 0;
		}
	    }

	    parts.push_back(bp);
	    limbs.insert(make_pair(name, limb));
	} catch (SettingTypeException &te) {
	    cerr<<"Creature::readLimbs problem processing limb "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<te.what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException &te) {
	    cerr<<"Creature::readLimbs problem processing limb "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<te.what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (exception &e) {
	    cerr<<"Creature::readLimbs problem processing limb "
		<<i<<": "<<e.what()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readLimbs problem processing limb "<<i<<endl;
	    return 0;
	}
    }
    return 1;
}

int readJoints(Setting &jointConfig, jointMap &joints, bodyMap &limbs,
	       World *w) 
{
    //Joints: 
    // name = "knee";
    // type,  
    // XXX so far only revolute
    //    obj1
    //    obj2
    //    position.{x,y}
    //    lowerAngle
    //    upperAngle
    int nJoints = jointConfig.getLength();
    for (int i = 0; i<nJoints; ++i) {
	try {
	    Setting &curJoint = jointConfig[i];
	    string name = curJoint["name"];
	    string type = curJoint["type"];
	    if (type == "revolute") {
		string obj1 = curJoint["obj1"];
		string obj2 = curJoint["obj2"];
		float x = curJoint["position"]["x"],
		      y = curJoint["position"]["y"];
		BodyId bodyA = findPart(limbs, obj1, "limb"),
		       bodyB = findPart(limbs, obj2, "limb");
		Vec2 anchor = {x, y};

		//Equivalent of Box2D 2.x b2RevoluteJointDef::Initialize
		b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
		jointDef.bodyIdA = bodyA;
		jointDef.bodyIdB = bodyB;
		jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA, anchor);
		jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB, anchor);
		jointDef.referenceAngle =
		    b2Rot_GetAngle(b2Body_GetRotation(bodyB)) -
		    b2Rot_GetAngle(b2Body_GetRotation(bodyA));
		jointDef.lowerAngle = curJoint["lowerAngle"];
		jointDef.upperAngle = curJoint["upperAngle"];
		//XXX default to enableLimit true
		jointDef.enableLimit = true;

		JointId joint = b2CreateRevoluteJoint(w->id(), &jointDef);
		joints.insert(make_pair(name, joint));

	    } else {
		cerr<<"Creature::readJoints joint "<<i<<":"<<name
		    <<", unknown type "<<type<<endl;
		return 0;
	    }
	
	} catch (SettingTypeException &te) {
	    cerr<<"Creature::readJoints problem processing joint "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<te.what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException &te) {
	    cerr<<"Creature::readJoints problem processing joint "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<te.what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (exception &e) {
	    cerr<<"Creature::readJoints problem processing joint "
		<<i<<": "<<e.what()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readJoints problem processing joint "<<i<<endl;
	    return 0;
	}
    }
       
    return 1;
}

int readMuscles (Setting &muscleConfig, muscleList &muscles, bodyMap &limbs)
{
    //Muscles:
    // name = "hamstring";
    //  obj1 = "shin";
    //  pos1 = { x = -0.5; y = 1.5; };
    //  obj2 = "thigh";
    //  pos2 = { x = -0.5; y = 1.0; };
    //  minK = 3000.0;
    //  maxK = 10000.0;
    //  minEq = 1.5;
    //  maxEq = 4.5;
    int nMuscles = muscleConfig.getLength();
    for (int i = 0; i<nMuscles; ++i) {
	try {
	    Setting &curMuscle = muscleConfig[i];
	    string name = curMuscle["name"];
	    string obj1 = curMuscle["obj1"],
		   obj2 = curMuscle["obj2"];
	    float x1 = curMuscle["pos1"]["x"],
		  y1 = curMuscle["pos1"]["y"],
		  x2 = curMuscle["pos2"]["x"],
		  y2 = curMuscle["pos2"]["y"];
	    MuscleP muscle(new Muscle(findPart(limbs, obj1, "limb"),
				      Vec2{x1, y1},
				      findPart(limbs, obj2, "limb"),
				      Vec2{x2, y2},
				      curMuscle["minK"],
				      curMuscle["maxK"],
				      curMuscle["minEq"],
				      curMuscle["maxEq"],
				      curMuscle["kd"]));
	    muscles.push_back(muscle);
	} catch (SettingTypeException &te) {
	    cerr<<"Creature::readMuscles problem processing muscle "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<te.what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException &te) {
	    cerr<<"Creature::readMuscles problem processing muscle "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<te.what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (exception &e) {
	    cerr<<"Creature::readMuscles problem processing muscle "
		<<i<<": "<<e.what()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readMuscles problem processing muscle "<<i<<endl;
	    return 0;
	}
    }
    return 1;
}

int readShapes(Setting &shapeConfig, shapeMap &shapes, bodyMap &limbs) 
{
    //Shapes
    // name = "foot";
    //  body = "shin";
    //  position = { x = 0.0; y = -3.0; };
    int nShapes = shapeConfig.getLength();
    for (int i = 0; i<nShapes; ++i) {
	try {
	    Setting &curShape = shapeConfig[i];
	    string name = curShape["name"],
		   body = curShape["body"];
	    float x = curShape["position"]["x"],
		  y = curShape["position"]["y"];
	    shapePos s;
	    s.localPos = {x,y};
	    s.b = findPart(limbs, body, "limb");
	    shapes.insert(make_pair(name, s));
	} catch (exception &e) {
	    cerr<<"Creature::readShapes problem processing shape "
		<<i<<": "<<e.what()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readShapes problem processing shape "
		<<i<<endl;
	    return 0;
	}
    }
    return 1;
}


int readSensors (Setting &sensorConfig, sensorList &sensors, 
		 bodyMap &limbs, jointMap &joints, shapeMap &shapes)
{ 
    //Sensors:
    // type = {JointSensor, HeightSensor, BodyAngleSensor
    // target
    //   for HeightSensor: minH, maxH
    //   for BodyAngleSensor: minA, maxA
    int nSensors = sensorConfig.getLength();
    for (int i = 0; i<nSensors; ++i) {
	try {
	    Setting &curSensor = sensorConfig[i];
	    string type = curSensor["type"];
	    string target = curSensor["target"];
	    
	    if (type == "JointSensor") {
		SensorP s(new JointSensor(findPart(joints, target,
								  "joint")));
		sensors.push_back(s);
	    } else if (type == "HeightSensor") {
		double minH = curSensor["minH"],
		       maxH = curSensor["maxH"];
		SensorP s(new HeightSensor(findPart(shapes, target,
								   "shape"),
					   minH, maxH));
		sensors.push_back(s);
	    } else if (type == "BodyAngleSensor") {
		double minA = curSensor["minA"],
		       maxA = curSensor["maxA"];
		SensorP s(new BodyAngleSensor(findPart(limbs, target,
								      "limb"),
					      minA, maxA));
		sensors.push_back(s);
	    } else {
		cerr<<"Creature::readSensors sensor "<<i
		    <<", unknown type: "<<type<<endl;
		return 0;
	    }
	} catch (SettingTypeException &te) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<endl;
	    cerr<<"    SettingTypeException: "<<te.what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException &te) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<te.what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (exception &e) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<": "<<e.what()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<endl;
	    return 0;
	}
    }
    return 1;
}

/**
 * Load creature definition from config file.  
 *
 * Uses libconfig for parsing config file.
 *
 * return 1 on succes, 0 on error
 */
//int Creature::initFromFile (const char* configFile, World *w) {
int Creature::initFromFile (const Config &config, World *w) {
    //Config specifies limbs, joints, muscles and shapes
    if (!config.exists("limbs")) {
	cerr<<"Creature::initFromFile limbs member does not exist"<<endl;
	return 0;
    }
    if (!readLimbs(config.lookup("limbs"), limbs, parts, w)) {
	cerr<<"Creature::initFromFile problem reading limbs"<<endl;
	return 0;
    }

    if (!config.exists("joints")) {
	cerr<<"Creature::initFromFile joints member does not exist"<<endl;
	return 0;
    }
    if (!readJoints(config.lookup("joints"), joints, limbs, w)) {
	cerr<<"Creature::initFromFile problem reading joints"<<endl;
	return 0;
    }
  
    if (!config.exists("muscles")) {
	cerr<<"Creature::initFromFile muscles member does not exist"<<endl;
	return 0;
    }
    if (!readMuscles(config.lookup("muscles"), muscles, limbs)) {
	cerr<<"Creature::initFromFile problem reading muscles"<<endl;
	return 0;
    }

    if (config.exists("shapes") && 
	!readShapes(config.lookup("shapes"),shapes, limbs)) 
    {
	cerr<<"Creature::initFromFile problem reading shapes"<<endl;
	return 0;
    }
    
    if (config.exists("sensors") &&
	!readSensors(config.lookup("sensors"), sensors, 
		     limbs, joints, shapes)) 
    {
	cerr<<"Creature::initFromFile problem reading sensors"<<endl;
	return 0;
    }

    return 1;
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

void Creature::draw (BoxScreen *screen) const {
    /* Really should change this to a for_all */
    //Draw body shapes
    for (bodyPosList::const_iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	screen->drawBody(i->b);
    }

    //Draw musculature
    for (muscleList::const_iterator i = muscles.begin();
	 i != muscles.end(); ++i)
    {
	(*i)->draw(screen);
    }

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

void Creature::setInput(double *input) const {
    if (useBias) {
	*input++ = 1.0;
    }
    for (sensorList::const_iterator i = sensors.begin(); 
	 i != sensors.end(); ++i) 
    {
	*input++ = (*i)->read();
    }
}


/**
 * Find and apply the force between the two bodies muscle is attached to.
 *
 * returns the magnitude of this force.
 */
float Muscle::update () const {
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
    force -= kd * b2Dot(vel, diff);

    diff *= force;

    //Now apply force to body1
    b2Body_ApplyForce(body1, diff, a1W, true);

    //Reverse force, and apply to body2
    diff *= -1;
    b2Body_ApplyForce(body2, diff, a2W, true);

    return force;
}

void Muscle::draw (BoxScreen *screen) const {
    //Just draw line representing spring
    Vec2 a1W = b2Body_GetWorldPoint(body1, end1L);
    Vec2 a2W = b2Body_GetWorldPoint(body2, end2L);

    screen->worldLine(a1W,a2W, 0xFF0000FF);
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
