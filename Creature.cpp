#include "Creature.h"

#include <iostream>
#include <stdlib.h>


Creature::Creature (World *w) {
    //XXX Eventually will read geometry description from file
    //    but for now lets just hard code some shape.

    float height = 50.0f + (10.0-20.0*(float)rand()/(RAND_MAX+1.0f));

    b2BoxDef shinBoneDef;
    shinBoneDef.extents.Set(0.5f,3.0f);
    shinBoneDef.density = 1.0f;
    shinBoneDef.groupIndex = -1;

    b2CircleDef footBallDef;
    footBallDef.radius = 0.5f;
    footBallDef.density = 1.0f;
    footBallDef.localPosition.Set(0.0f, -3.0f);
    footBallDef.groupIndex = -1;
    footBallDef.friction = 0.6f;

    b2BodyDef shinBone;
    shinBone.AddShape(&shinBoneDef);
    shinBone.AddShape(&footBallDef);
    shinBone.position.Set(0.0f, height+10.00f);
    shinBone.rotation = 0.00;
    shinBone.angularDamping = 0.01f;

    Body *shin = w->createBody(&shinBone);
    parts.push_back(shin);

    b2BoxDef thighBoneDef;
    thighBoneDef.extents.Set(0.5f,2.0f);
    thighBoneDef.density = 1.0f;
    thighBoneDef.groupIndex = -1;

    b2BodyDef thighBone;
    thighBone.AddShape(&thighBoneDef);
    thighBone.position.Set(0.0f,height+15.00f);
    thighBone.rotation = -0.00;
    thighBone.angularDamping = 0.01f;

    Body *thigh = w->createBody(&thighBone);
    parts.push_back(thigh);

    b2RevoluteJointDef kneeDef;
    kneeDef.body1 = thigh;
    kneeDef.body2 = shin;
    kneeDef.anchorPoint.Set(0.0f, height+13.0f);
    kneeDef.lowerAngle = -b2_pi;
    kneeDef.upperAngle = 0.001f;
    kneeDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&kneeDef));

    //Create muscle between thigh and shin:
    muscles.push_back(new Muscle(shin,  Vec2(-0.5f,1.5f),
				 thigh, Vec2(-0.5f,1.0f),
				 4000.0f, 3.5f));
    
    b2BodyDef LshinBone;
    LshinBone.AddShape(&shinBoneDef);
    LshinBone.AddShape(&footBallDef);
    LshinBone.position.Set(0.0f, height+10.00f);
    LshinBone.rotation = 0.00;
    LshinBone.angularDamping = 0.01f;

    Body *Lshin = w->createBody(&LshinBone);
    parts.push_back(Lshin);

    b2BodyDef LthighBone;
    LthighBone.AddShape(&thighBoneDef);
    LthighBone.position.Set(0.0f,height+15.00f);
    LthighBone.rotation = -0.00;
    LthighBone.angularDamping = 0.01f;

    Body *Lthigh = w->createBody(&LthighBone);
    parts.push_back(Lthigh);

    b2RevoluteJointDef LkneeDef;
    LkneeDef.body1 = Lthigh;
    LkneeDef.body2 = Lshin;
    LkneeDef.anchorPoint.Set(0.0f, height+13.0f);
    LkneeDef.lowerAngle = -0.001f;
    LkneeDef.upperAngle = b2_pi;
    LkneeDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&LkneeDef));

    //Create muscle between thigh and shin:
    muscles.push_back(new Muscle(Lshin,  Vec2(0.5f,1.5f),
				 Lthigh, Vec2(0.5f,1.0f),
				 4000.0f, 3.5f));

    b2BoxDef backBoneDef;
    backBoneDef.extents.Set(0.5f,3.0f);
    backBoneDef.density = 1.0f;
    //backBoneDef.groupIndex = -1;

    b2CircleDef headBallDef;
    headBallDef.radius = 0.5f;
    headBallDef.density = 50.0f;
    headBallDef.localPosition.Set(0.0f, 3.0f);
    //headBallDef.groupIndex = -1;

    b2BodyDef backBone;
    backBone.AddShape(&backBoneDef);
    backBone.AddShape(&headBallDef);
    backBone.position.Set(0.0f, height+20.00f);
    backBone.rotation = 0.00;
    backBone.angularDamping = 0.01f;

    Body *back = w->createBody(&backBone);
    parts.push_back(back);

    b2RevoluteJointDef hipDef;
    hipDef.body1 = back;
    hipDef.body2 = thigh;
    hipDef.anchorPoint.Set(0.0f, height+17.0f);

    //Right leg hip starts at 0, can rotate (CCW) b2_pi
    hipDef.lowerAngle = -0.001f;
    hipDef.upperAngle = b2_pi;
    hipDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&hipDef));
    
    //Create muscle between thigh and back:
    muscles.push_back(new Muscle(thigh, Vec2(0.5f,-1.0f),
				 back,  Vec2(0.5f,-4.0f),
				 6000.0f, 3.7f));
    
    b2RevoluteJointDef LhipDef;
    LhipDef.body1 = back;
    LhipDef.body2 = Lthigh;
    LhipDef.anchorPoint.Set(0.0f, height+17.0f);
    
    //Left leg hip starts at 0, can rotate (CW) -b2_pi
    LhipDef.lowerAngle = -b2_pi;
    LhipDef.upperAngle = 0.001f;
    LhipDef.enableLimit = true;

    joints.push_back((RevoluteJoint*) w->createJoint(&LhipDef));
    
    //Create muscle between thigh and back:
    muscles.push_back(new Muscle(Lthigh, Vec2(-0.5f,-1.0f),
				 back,  Vec2(-0.5f,-4.0f),
				 6000.0f, 3.7f));

    w->addCreature(this);
}
    
void Creature::wake () {
    for (bodyList::iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	(*i)->WakeUp();
    }
}


void Creature::draw (BoxScreen *screen) {
    /* Really should change this to a for_all */
    //Draw body shapes
    for (bodyList::iterator i = parts.begin();
	 i != parts.end(); ++i)
    {
	screen->drawBody(*i);
    }

    //Draw musculature
    for (muscleList::iterator i = muscles.begin();
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

void Muscle::draw (BoxScreen *screen) {
    //Just draw line representing spring
    Vec2 a1W = body1->GetWorldPoint(end1L);
    Vec2 a2W = body2->GetWorldPoint(end2L);

    screen->worldLine(a1W,a2W, 0xFF0000FF);
}
