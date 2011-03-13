#include "Creature.h"
#include "World.h"
#include "BoxScreen.h"

#include <iostream>
#include <stdlib.h>

#include <boost/shared_ptr.hpp>

#include <libconfig.h++>
    
using namespace libconfig;

inline double limit_norm (double v, const double min, const double max) {
    if (v < min) v = min;
    if (v > max) v = max;
    return (v - min) / (max - min);
}

class Sensor {
    public:
	virtual double read() = 0;
};

class JointSensor : public Sensor {
    public:
	JointSensor (const RevoluteJointP &j) : joint(j) { }
	double read() {
	    return limit_norm(joint->GetJointAngle(),
			      joint->GetLowerLimit(),
			      joint->GetUpperLimit());
	}
    private:
	const RevoluteJointP joint;
};

class HeightSensor : public Sensor {
    public:
	HeightSensor(const shapePos &_t, 
		     const double _minH, const double _maxH) : 
	    t(_t), minH(_minH), maxH(_maxH) { }
	double read() {
	    Vec2 v = t.b->GetWorldPoint(t.localPos);
	    return limit_norm(v.y,minH,maxH);
	}
    private:
	const shapePos t;
	const double minH, maxH;
};
		
class BodyAngleSensor : public Sensor {
    public:
	BodyAngleSensor(const BodyP &_t, 
			const double _minA, const double _maxA) :
	    t(_t), minA(_minA), maxA(_maxA) { }
	
	double read() {
	    return limit_norm(t->GetAngle(),minA,maxA);
	}
    private:
	const BodyP t;
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
	    
	    b2BodyDef bone;
	    bone.type = b2_dynamicBody;
	    bone.position.Set(x,y);
	    curLimb.lookupValue("angle", bone.angle);
	    curLimb.lookupValue("angularDamping", bone.angularDamping);
	    
	    BodyP limb(w->createBody(&bone));
	    BodyPos bp;
	    bp.b = limb;
	    bp.defaultPos.Set(x,y);
	    bp.angle = bone.angle;
	    
	    Setting &shapes = curLimb["shapes"];
	    int nShapes = shapes.getLength();
	    for (int j = 0; j<nShapes; ++j) {
		Setting &curShape = shapes[j];
		string type = curShape["type"];
		b2FixtureDef fixtureDef;
		
		fixtureDef.density = curShape["density"];
		
		curShape.lookupValue("friction", fixtureDef.friction);
		curShape.lookupValue("groupIndex",
				     (int&)(fixtureDef.filter.groupIndex)); 
		if (type == "box") {
		    float w = curShape["w"],
			  h = curShape["h"];
		    b2PolygonShape box;
		    box.SetAsBox(w,h);
		    fixtureDef.shape = &box;
		    limb->CreateFixture(&fixtureDef);
		} else if (type == "ball") {
		    b2CircleShape ball;
		    ball.m_radius = curShape["radius"];
		    float x = curShape["position"]["x"],
			  y = curShape["position"]["y"];
		    ball.m_p.Set(x,y);
		    fixtureDef.shape = &ball;
		    limb->CreateFixture(&fixtureDef);
		} else {
		    cerr<<"Creature::initFromFile limb "<<name
			<<", shape "<<j<<" type "<<type<<" unknown."<<endl;
		    return 0;
		}
	    }

	    parts.push_back(bp);
	    limbs.insert(make_pair(name, limb));
	} catch (SettingTypeException te) {
	    cerr<<"Creature::readLimbs problem processing limb "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::readLimbs problem processing limb "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
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
		b2RevoluteJointDef jointDef;
		jointDef.Initialize(limbs[obj1].get(), limbs[obj2].get(),
				    Vec2(x, y));
		jointDef.lowerAngle = curJoint["lowerAngle"];
		jointDef.upperAngle = curJoint["upperAngle"];
		//XXX default to enableLimit true
		jointDef.enableLimit = true;

		RevoluteJointP joint = 
		    boost::dynamic_pointer_cast<RevoluteJoint,Joint>
			(w->createJoint(&jointDef));
		joints.insert(make_pair(name, joint));

	    } else {
		cerr<<"Creature::readJoints joint "<<i<<":"<<name
		    <<", unknown type "<<type<<endl;
		return 0;
	    }
	
	} catch (SettingTypeException te) {
	    cerr<<"Creature::readJoints problem processing joint "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::readJoints problem processing joint "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
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
	    MuscleP muscle(new Muscle(limbs[obj1], Vec2(x1, y1),
				      limbs[obj2], Vec2(x2, y2),
				      curMuscle["minK"],
				      curMuscle["maxK"],
				      curMuscle["minEq"],
				      curMuscle["maxEq"],
				      curMuscle["kd"]));
	    muscles.push_back(muscle);
	} catch (SettingTypeException te) {
	    cerr<<"Creature::readMuscles problem processing muscle "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::readMuscles problem processing muscle "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
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
	    s.localPos.Set(x,y);
	    s.b = limbs[body];
	    shapes.insert(make_pair(name, s));
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
		SensorP s(new JointSensor(joints[target]));
		sensors.push_back(s);
	    } else if (type == "HeightSensor") {
		double minH = curSensor["minH"],
		       maxH = curSensor["maxH"];
		SensorP s(new HeightSensor(shapes[target], minH, maxH));
		sensors.push_back(s);
	    } else if (type == "BodyAngleSensor") {
		double minA = curSensor["minA"],
		       maxA = curSensor["maxA"];
		SensorP s(new BodyAngleSensor(limbs[target], minA, maxA));
		sensors.push_back(s);
	    } else {
		cerr<<"Creature::readSensors sensor "<<i
		    <<", unknown type: "<<type<<endl;
		return 0;
	    }
	} catch (SettingTypeException te) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::readSensors problem processing sensor "
		<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::readSensors problem processing limb "
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
int Creature::initFromFile (const char* configFile, World *w) {
    
    Config config;
    try {
	config.readFile(configFile);
    } catch (ParseException pe) {
	cerr<<"Creature::initFromFile parse error"<<endl;
	cerr<<"   config file "<<configFile<<endl;
	cerr<<"   line number "<<pe.getLine()<<endl;
	cerr<<"   error: "<<pe.getError()<<endl;

	return 0;
    } catch (...) {
	cerr<<"Creature::initFromFile error reading from file."<<endl;

	return 0;
    }
    config.setAutoConvert(true);

    //Config specifies limbs, joints, muscles and shapes
    
    if (!config.exists("limbs")) {
	cerr<<"Creature::initFromFile limbs member does not exist"<<endl;
	return 0;
    }
    Setting &limbConfig = config.lookup("limbs");
    if (!readLimbs(config.lookup("limbs"), limbs, parts, w)) {
	cerr<<"Creature::initFromFile problem reading limbs"<<endl;
	return 0;
    }

    if (!config.exists("joints")) {
	cerr<<"Creature::initFromFile joints member does not exist"<<endl;
	return 0;
    }
    Setting &jointConfig = config.lookup("joints");
    if (!readJoints(config.lookup("joints"), joints, limbs, w)) {
	cerr<<"Creature::initFromFile problem reading joints"<<endl;
	return 0;
    }
  
    if (!config.exists("muscles")) {
	cerr<<"Creature::initFromFile muscles member does not exist"<<endl;
	return 0;
    }
    Setting &muscleConfig = config.lookup("muscles");
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
    Vec2 zero(0, 0);
    for (bodyPosList::iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	i->b->SetTransform(i->defaultPos, i->angle);
	i->b->SetLinearVelocity(zero);
	i->b->SetAngularVelocity(0);
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
float Muscle::update () {
    Vec2 a1W = body1->GetWorldPoint(end1L);
    Vec2 a2W = body2->GetWorldPoint(end2L);

    Vec2 diff = a1W - a2W;
    float length = diff.Normalize();
    //XXX check if length == 0?
    //diff now normalized, points from a2->a1
    //diff *= 1.0f/length;
    
    //Now find how far we are from equilibrium to determine spring force
    float force = k*(eq-length);

    //Also apply dampening
    // find relative velocity of two points
    Vec2 vel = body1->GetLinearVelocityFromLocalPoint(end1L) - 
	       body2->GetLinearVelocityFromLocalPoint(end2L);
    force -= kd * b2Dot(vel, diff);

    diff *= force;

    //Now apply force to body1
    body1->ApplyForce(diff, a1W);

    //Reverse force, and apply to body2
    diff *= -1;
    body2->ApplyForce(diff, a2W);

    return force;
}

void Muscle::draw (BoxScreen *screen) const {
    //Just draw line representing spring
    Vec2 a1W = body1->GetWorldPoint(end1L);
    Vec2 a2W = body2->GetWorldPoint(end2L);

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
