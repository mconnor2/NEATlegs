#include "Creature.h"

#include <iostream>
#include <stdlib.h>

#include <boost/shared_ptr.hpp>


Creature::Creature (World *w) {
    //XXX Eventually will read geometry description from file
    //    but for now lets just hard code some shape.

    float height = 50.0f + (10.0-20.0*(float)rand()/(RAND_MAX+1.0f));

    b2PolygonDef shinBoneDef;
    shinBoneDef.SetAsBox(0.5f,3.0f);
    shinBoneDef.density = 1.0f;
    shinBoneDef.filter.groupIndex = -1;

    b2CircleDef footBallDef;
    footBallDef.radius = 0.5f;
    footBallDef.density = 1.0f;
    footBallDef.localPosition.Set(0.0f, -3.0f);
    footBallDef.filter.groupIndex = -1;
    footBallDef.friction = 1.6f;
    
    b2BodyDef shinBone;
    //shinBone.AddShape(&shinBoneDef);
    //shinBone.AddShape(&footBallDef);
    shinBone.position.Set(0.0f, height+10.00f);
    shinBone.angle = 0.00;
    shinBone.angularDamping = 0.01f;

    BodyP shin(w->createBody(&shinBone));
    shin->CreateShape(&shinBoneDef);
    shin->CreateShape(&footBallDef);
    shin->SetMassFromShapes();
    
    parts.push_back(shin);
    limbs.insert(make_pair("shin",shin));
    
    shapePos foot;
    foot.localPos.Set(0.0f, -3.0f);
    foot.b = shin;
    shapes.insert(make_pair("foot",foot));

    b2PolygonDef thighBoneDef;
    thighBoneDef.SetAsBox(0.5f,2.0f);
    thighBoneDef.density = 1.0f;
    thighBoneDef.filter.groupIndex = -1;

    b2BodyDef thighBone;
    //thighBone.AddShape(&thighBoneDef);
    thighBone.position.Set(0.0f,height+15.00f);
    thighBone.angle = -0.00;
    thighBone.angularDamping = 0.01f;

    BodyP thigh(w->createBody(&thighBone));
    thigh->CreateShape(&thighBoneDef);
    thigh->SetMassFromShapes();

    parts.push_back(thigh);
    limbs.insert(make_pair("thigh",thigh));

    b2RevoluteJointDef kneeDef;
    kneeDef.Initialize(thigh.get(), shin.get(), Vec2(0.0f, height+13.0f));
    //kneeDef.body1 = thigh;
    //kneeDef.body2 = shin;
    //kneeDef.anchorPoint.Set(0.0f, height+13.0f);
    kneeDef.lowerAngle = -b2_pi+0.02f;
    kneeDef.upperAngle = -0.02f;
    kneeDef.enableLimit = true;

    RevoluteJointP knee = boost::dynamic_pointer_cast<RevoluteJoint,Joint>
						(w->createJoint(&kneeDef));
    joints.insert(make_pair("knee",knee));

    //Create muscle between thigh and shin:
    MuscleP hamstring(new Muscle(shin,  Vec2(-0.5f,1.5f),
				 thigh, Vec2(-0.5f,1.0f),
				 //7000.0f, 3.3f
				 3000.0f, 10000.0f, 
				 1.5f, 4.5f));
    muscles.push_back(hamstring);

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
/*

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
