#include "Creature.h"

#include <iostream>
#include <stdlib.h>

#include <boost/shared_ptr.hpp>

#include <libconfig.h++>

/**
 * Load creature definition from config file.  
 *
 * Uses libconfig for parsing config file.
 *
 * return 1 on succes, 0 on error
 */
int Creature::initFromFile (const char* configFile, World *w) {
    using namespace libconfig;
    
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
    if (!config.exists("limbs")) {
	cerr<<"Creature::initFromFile limbs member does not exist"<<endl;
	return 0;
    }
    Setting &limbConfig = config.lookup("limbs");
    int nLimbs = limbConfig.getLength();
    for (int i = 0; i<nLimbs; ++i) {
	try {
	    Setting &curLimb = limbConfig[i];
	    string name = curLimb["name"];
	    float x = curLimb["position"]["x"], 
		  y = curLimb["position"]["y"];
	    
	    b2BodyDef bone;
	    bone.position.Set(x,y);
	    curLimb.lookupValue("angle", bone.angle);
	    curLimb.lookupValue("angularDamping", bone.angularDamping);
	    
	    BodyP limb(w->createBody(&bone));
	    
	    Setting &shapes = curLimb["shapes"];
	    int nShapes = shapes.getLength();
	    for (int j = 0; j<nShapes; ++j) {
		Setting &curShape = shapes[j];
		string type = curShape["type"];
		auto_ptr<b2ShapeDef> shapeDefp;
		if (type == "box") {
		    float w = curShape["w"],
			  h = curShape["h"];
		    shapeDefp = auto_ptr<b2ShapeDef>(new b2PolygonDef);
		    ((b2PolygonDef*)shapeDefp.get())->SetAsBox(w,h);
		} else if (type == "ball") {
		    shapeDefp = auto_ptr<b2ShapeDef>(new b2CircleDef);
		    ((b2CircleDef*)shapeDefp.get())->radius = 
			curShape["radius"];
		    float x = curShape["position"]["x"],
			  y = curShape["position"]["y"];
		    ((b2CircleDef*)shapeDefp.get())->localPosition.Set(x,y); 
		} else {
		    cerr<<"Creature::initFromFile limb "<<name
			<<", shape "<<j<<" type "<<type<<" unknown."<<endl;
		    return 0;
		}
		shapeDefp->density = curShape["density"];
		
		curShape.lookupValue("friction", shapeDefp->friction);
		curShape.lookupValue("groupIndex",
				     (int&)(shapeDefp->filter.groupIndex)); 

		limb->CreateShape(shapeDefp.get());
	    }

	    limb->SetMassFromShapes();

	    parts.push_back(limb);
	    limbs.insert(make_pair(name, limb));
	} catch (SettingTypeException te) {
	    cerr<<"Creature::initFromFile problem processing limb "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::initFromFile problem processing limb "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::initFromFile problem processing limb "<<i<<endl;
	    return 0;
	}
    }

    //Joints: 
    // name = "knee";
    // type,  
    // XXX so far only revolute
    //    obj1
    //    obj2
    //    position.{x,y}
    //    lowerAngle
    //    upperAngle
    if (!config.exists("joints")) {
	cerr<<"Creature::initFromFile joints member does not exist"<<endl;
	return 0;
    }
    Setting &jointConfig = config.lookup("joints");
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
		cerr<<"Creature::initFromFile joint "<<i<<":"<<name
		    <<", unknown type "<<type<<endl;
		return 0;
	    }
	
	} catch (SettingTypeException te) {
	    cerr<<"Creature::initFromFile problem processing joint "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::initFromFile problem processing joint "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::initFromFile problem processing joint "<<i<<endl;
	    return 0;
	}
    }
    
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
    if (!config.exists("muscles")) {
	cerr<<"Creature::initFromFile muscles member does not exist"<<endl;
	return 0;
    }
    Setting &muscleConfig = config.lookup("muscles");
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
				      curMuscle["maxEq"]));
	    muscles.push_back(muscle);
	} catch (SettingTypeException te) {
	    cerr<<"Creature::initFromFile problem processing muscle "<<i<<endl;
	    cerr<<"    SettingTypeException: "<<((SettingException)te).what()
		<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (SettingNotFoundException te) {
	    cerr<<"Creature::initFromFile problem processing muscle "<<i<<endl;
	    cerr<<"    SettingNotFoundException: "
		<<((SettingException)te).what()<<", "<<te.getPath()<<endl;
	    return 0;
	} catch (...) {
	    cerr<<"Creature::initFromFile problem processing muscle "<<i<<endl;
	    return 0;
	}
    }
   

    //Shapes
    // name = "foot";
    //  body = "shin";
    //  position = { x = 0.0; y = -3.0; };
    if (config.exists("shapes")) {
	Setting &shapeConfig = config.lookup("shapes");
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
		cerr<<"Creature::initFromFile problem processing shape "
		    <<i<<endl;
		return 0;
	    }
	}
    }

/*
    b2BodyDef LshinBone;
    //LshinBone.AddShape(&shinBoneDef);
    //LshinBone.AddShape(&footBallDef);
    LshinBone.position.Set(0.0f, height+10.00f);
    LshinBone.angle = 0.00;
    LshinBone.angularDamping = 0.01f;

    Body *Lshin = w->createBody(&LshinBone);
    Lshin->CreateShape(&shinBoneDef);
    Lshin->CreateShape(&footBallDef);
    Lshin->SetMassFromShapes();

    parts.push_back(Lshin);

    b2BodyDef LthighBone;
    //LthighBone.AddShape(&thighBoneDef);
    LthighBone.position.Set(0.0f,height+15.00f);
    LthighBone.angle = -0.00;
    LthighBone.angularDamping = 0.01f;

    Body *Lthigh = w->createBody(&LthighBone);
    Lthigh->CreateShape(&thighBoneDef);
    Lthigh->SetMassFromShapes();

    parts.push_back(Lthigh);

    b2RevoluteJointDef LkneeDef;
    LkneeDef.Initialize(Lthigh,Lshin, Vec2(0.0f, height+13.0f));
    //LkneeDef.body1 = Lthigh;
    //LkneeDef.body2 = Lshin;
    //LkneeDef.anchorPoint.Set(0.0f, height+13.0f);
    LkneeDef.lowerAngle = -0.001f;
    LkneeDef.upperAngle = b2_pi;
    LkneeDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&LkneeDef));

    //Create muscle between thigh and shin:
    muscles.push_back(new Muscle(Lshin,  Vec2(0.5f,1.5f),
				 Lthigh, Vec2(0.5f,1.0f),
				 6500.0f, 3.3f));
*/
/* XXX
    b2PolygonDef backBoneDef;
    backBoneDef.SetAsBox(0.5f,3.0f);
    backBoneDef.density = 1.0f;
    //backBoneDef.groupIndex = -1;

    b2CircleDef headBallDef;
    headBallDef.radius = 0.5f;
    headBallDef.density = 5.0f;
    headBallDef.localPosition.Set(0.0f, 3.0f);
    //headBallDef.groupIndex = -1;

    b2BodyDef backBone;
    //backBone.AddShape(&backBoneDef);
    //backBone.AddShape(&headBallDef);
    backBone.position.Set(0.0f, height+20.00f);
    backBone.angle = 0.00;
    backBone.angularDamping = 0.01f;

    BodyP back(w->createBody(&backBone));
    back->CreateShape(&backBoneDef);
    back->CreateShape(&headBallDef);
    back->SetMassFromShapes();

    parts.push_back(back);
    limbs.insert(make_pair("back", back));
    
    shapePos head;
    head.localPos.Set(0.0f, 3.0f);
    head.b = back;
    shapes.insert(make_pair("head",head));

    b2RevoluteJointDef hipDef;
    hipDef.Initialize(back.get(), thigh.get(), Vec2(0.0f, height+17.0f));
    //hipDef.body1 = back;
    //hipDef.body2 = thigh;
    //hipDef.anchorPoint.Set(0.0f, height+17.0f);

    //Right leg hip starts at 0, can rotate (CCW) b2_pi
    hipDef.lowerAngle = -0.01f; 
    hipDef.upperAngle = b2_pi * 3.0f/4.0f;
    hipDef.enableLimit = true;

    RevoluteJointP hip = boost::dynamic_pointer_cast<RevoluteJoint, Joint>
						    (w->createJoint(&hipDef));
    joints.insert(make_pair("hip",hip));
    
    //Create muscle between thigh and back:
    MuscleP quad(new Muscle(thigh, Vec2(0.5f,-1.0f),
			    back,  Vec2(0.5f,-1.5f),
			    //7000.0f, 3.4f));
			    3000.0f, 10000.0f,
			    1.5f, 4.5f));
    muscles.push_back(quad);
    
//.*

    b2RevoluteJointDef LhipDef;
    LhipDef.Initialize(back, Lthigh, Vec2(0.0f, height+17.0f));
    //LhipDef.body1 = back;
    //LhipDef.body2 = Lthigh;
    //LhipDef.anchorPoint.Set(0.0f, height+17.0f);
    
    //Left leg hip starts at 0, can rotate (CW) -b2_pi
    LhipDef.lowerAngle = -b2_pi;
    LhipDef.upperAngle = 0.001f;
    LhipDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&LhipDef));
    
    //Create muscle between thigh and back:
    muscles.push_back(new Muscle(Lthigh, Vec2(-0.5f,-1.0f),
				 back,  Vec2(-0.0f,-2.0f),
				 7000.0f, 3.4f));
*/

    w->addCreature(this);
    return 1;
}

void Creature::reset () {
    Vec2 zero(0, 0);
    BodyP back = limbs["back"];
    back->SetXForm(Vec2(0.0f, 14.0f), 0);
    back->SetLinearVelocity(zero);
    back->SetAngularVelocity(0);

    BodyP thigh = limbs["thigh"];
    thigh->SetXForm(Vec2(0.0f, 9.0f), 0);
    thigh->SetLinearVelocity(zero);
    thigh->SetAngularVelocity(0);

    BodyP shin = limbs["shin"];
    shin->SetXForm(Vec2(0.0f, 4.0f), 0);
    shin->SetLinearVelocity(zero);
    shin->SetAngularVelocity(0);
}

/*
bool Creature::getShapePosition(const string &name, Vec2 *p) const {
    if (shapes.count(name) == 0) {
	return false;
    }

    Shape *s = shapes[name];

    Body *b = s->GetBody();

    if (b == NULL)
	return false;

    switch (s->GetType()) {
	case e_circleShape:
	{
	    const b2CircleShape* circle = dynamic_cast<const b2CircleShape*>(s);
	    *p = b->GetWorldPoint(circle->GetLocalPosition());
	    return true;
	}
	break;
	case e_polygonShape:
	{
	    const b2PolygonShape* poly = dynamic_cast<const b2PolygonShape*>(s);
	    *p = b->GetWorldPoint(poly->GetCentroid());
	    return true;
	}
	break;
	case e_unknownShape:
	default:
	    cerr<<"getShapePosition: unknown shape."<<endl;
	    return false;
    }
}
*/

void Creature::wake () {
    for (bodyList::iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	(*i)->WakeUp();
    }
}


void Creature::draw (BoxScreen *screen) const {
    /* Really should change this to a for_all */
    //Draw body shapes
    for (bodyList::const_iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	screen->drawBody(*i);
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

/**
 * Find and apply the force between the two bodies muscle is attached to.
 *
 * returns the magnitude of this force.
 */
float Muscle::update () {
    Vec2 a1W = body1->GetWorldPoint(end1L);
    Vec2 a2W = body2->GetWorldPoint(end2L);

    Vec2 diff = a1W - a2W;
    float length = diff.Length();
    //XXX check if length == 0?
    //diff now normalized, points from a2->a1
    diff *= 1.0f/length;
    
    //Now find how far we are from equilibrium to determine spring force
    float force = k*(eq-length);

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
