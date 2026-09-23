#!/usr/bin/env python3
"""Generate walker.cfg: a two-legged walker with an upright torso and head.
All geometry is derived from a few parameters so the design can be
iterated; prints a design summary.

Usage: tools/gen_walker.py [out.cfg] ['{"param": value, ...}']

Conventions: x forward (toes), y up, body angle CCW positive.  Joint
angles start at 0 in the initial pose (Creature sets the joint reference
angle from the start pose), so limits are relative to it.  Both legs start
in the same place; legs and torso share groupIndex -1 so they pass each
other (in 2D the legs are side by side, not in front of each other).
"""
import math, sys, json

P = dict(
    # leg (m): ankle height, foot extent behind/ahead of the ankle, foot
    # thickness and end-ball radius, segment lengths and thickness
    ankleY=0.05, heel=0.038, toe=0.19, footT=0.10, footBall=0.05,
    shinL=0.53, thighL=0.48, legW=0.127,
    # torso (box above the hip) and head ball on top
    torsoL=0.76, torsoW=0.127, torsoDensity=1.0,
    headR=0.15, headDensity=2.0,
    # joint ranges relative to the standing start pose (degrees).  Hip
    # extension lets the stance leg get behind the body; knee flexion stops
    # short of where the hamstring would cross the knee.
    ankle=(-60, 30), knee=(-135, 0), hip=(-30, 120),
    # muscles: stiffness range as fraction of maxForce per metre of stretch
    stretchAtMaxK=0.2, kSpan=4.0, kd=1.0,
    minArm=0.02,  # reject muscles whose moment arm drops below this (m)
    forcePerWeight=8.0, wattsPerKg=50.0,
    # One push-pull spring per joint, each on the inside of its joint's bend
    # so its line of action never crosses the joint (checked by the sweep
    # below).  Distances along the segments from the joint, and offsets
    # from the segment axis (m):
    tibToe=0.19, tibUp=0.27, tibOff=0.0635,  # top of toes to front of shin
    hamUp=0.36, hamDown=0.13, hamOff=0.0635, # back of thigh to back of shin
    hipTorso=0.19, hipThigh=0.36, hipOff=0.10,  # front of pelvis to thigh
    # Population: at 200 and 500 most runs drag one foot (median share of
    # the worse foot's travel spent sliding 0.72, ~28 left/right support
    # switches); 1000 steps properly more often (0.49, 60 switches) and goes
    # furthest (median 26 m against 15 m at 200 and 25 m at 500), at about
    # 4x the cost of 200 per generation.
    headFloor=0.75, popSize=1000,
    groundLimbs=['footL', 'footR'],  # any other limb touching = fall
    # Extra sensors beyond joint angles, heights and torso angle, any of
    # 'spin' (torso angular velocity), 'velocity' (torso x and y) and
    # 'contact' (each foot on the ground).  Unlike kanga2, the walker does
    # better without them (8 seeds each): all three halved the median
    # distance, foot contact alone hurt nearly as much, and spin alone made
    # no difference.
    extraSensors=[],
    addLink=0.05, addNode=0.05, # NEAT structural mutation rates
    # Objective as kanga2's, with the same budget per kg of body.  At
    # population 1000 the budget binds once gaits work (5 of 8 runs spent
    # it) without making them drag, and gives more distance per joule than
    # 256 J (26 m against 19 m within 128 J; 8 seeds each).
    fitnessBase=0.1, survivalExponent=0.5, energyBudget=128.0,
)
if len(sys.argv) > 2:
    P.update(json.loads(sys.argv[2]))
out = sys.argv[1] if len(sys.argv) > 1 else 'walker.cfg'

rad = math.radians
def add(a, b): return (a[0]+b[0], a[1]+b[1])
def sub(a, b): return (a[0]-b[0], a[1]-b[1])
def mul(a, s): return (a[0]*s, a[1]*s)
def length(a): return math.hypot(*a)

class Body:
    def __init__(s, name, pos):
        s.name, s.pos, s.shapes = name, pos, []
        s.mass, s.mx = 0.0, (0.0, 0.0)
    def local(s, w): return sub(w, s.pos)    # all bodies start unrotated
    def world(s, l): return add(s.pos, l)
    def box(s, hw, hh, density, **kw):
        s.shapes.append(dict(type='box', w=hw, h=hh, density=density, **kw))
        m = 4*hw*hh*density; s.mass += m; s.mx = add(s.mx, mul(s.pos, m))
    def ball(s, r, lpos, density, **kw):
        s.shapes.append(dict(type='ball', radius=r, position=lpos, density=density, **kw))
        m = math.pi*r*r*density; s.mass += m; s.mx = add(s.mx, mul(s.world(lpos), m))

# Joints: a straight, vertical leg
A = (0.0, P['ankleY'])
K = (0.0, A[1] + P['shinL'])
H = (0.0, K[1] + P['thighL'])
headC = (0.0, H[1] + P['torsoL'])

def leg(side):
    # Foot: flat box from heel to toe, balls at both ends
    footLen = P['heel'] + P['toe']
    foot = Body('foot' + side, ((P['toe'] - P['heel'])/2, A[1]))
    foot.box(footLen/2, P['footT']/2, 1.0, friction=1.6, groupIndex=-1)
    foot.ball(P['footBall'], (footLen/2, 0.0), 1.0, friction=1.6, groupIndex=-1)
    foot.ball(P['footBall'], (-footLen/2, 0.0), 1.0, friction=1.6, groupIndex=-1)
    shin = Body('shin' + side, mul(add(A, K), 0.5))
    shin.box(P['legW']/2, P['shinL']/2, 1.0, groupIndex=-1)
    thigh = Body('thigh' + side, mul(add(K, H), 0.5))
    thigh.box(P['legW']/2, P['thighL']/2, 1.0, groupIndex=-1)
    return foot, shin, thigh

footR, shinR, thighR = leg('R')
footL, shinL, thighL = leg('L')
back = Body('back', (0.0, H[1] + P['torsoL']/2))
back.box(P['torsoW']/2, P['torsoL']/2, P['torsoDensity'], groupIndex=-1)
back.ball(P['headR'], (0.0, P['torsoL']/2), P['headDensity'], groupIndex=-1)
bodies = [footR, shinR, thighR, footL, shinL, thighL, back]

mass = sum(b.mass for b in bodies)
com = mul((sum(b.mx[0] for b in bodies), sum(b.mx[1] for b in bodies)), 1/mass)
weight = 10*mass
heelX, toeX = -P['heel'] - P['footBall'], P['toe'] + P['footBall']

# Muscles, as world points in the start pose: (name, joint, body1, point1,
# body2, point2), body2 being the distal one the joint rotates
def muscles(side, foot, shin, thigh):
    return [
        ('tibialis' + side, 'ankle', shin, (P['tibOff'], A[1] + P['tibUp']),
         foot, (P['tibToe'], A[1] + P['footT']/2)),
        ('hamstring' + side, 'knee', thigh, (-P['hamOff'], K[1] + P['hamUp']),
         shin, (-P['hamOff'], K[1] - P['hamDown'])),
        ('hipFlexor' + side, 'hip', back, (P['hipOff'], H[1] + P['hipTorso']),
         thigh, (P['hipOff'], H[1] - P['hipThigh'])),
    ]
mus = muscles('R', footR, shinR, thighR) + muscles('L', footL, shinL, thighL)
jointPos = dict(ankle=A, knee=K, hip=H)

# Sweep each muscle over the range of the joint it crosses (distal body
# rotated about the joint).  A straight-line spring whose line of action
# crosses the joint flips between extending and flexing it, and near the
# crossing has no leverage at all, so reject those designs.  Rest-length
# ranges come from the lengths the muscle actually passes through.
def rotAbout(p, c, a):
    d = sub(p, c); cs, sn = math.cos(a), math.sin(a)
    return add(c, (d[0]*cs - d[1]*sn, d[0]*sn + d[1]*cs))
sweep, bad = {}, []
for name, jn, b1, w1, b2, w2 in mus:
    J, lim = jointPos[jn], P[jn]
    arms, lens = [], []
    for deg in range(lim[0], lim[1] + 1):
        p2 = rotAbout(w2, J, rad(deg))
        d = sub(p2, w1); Ln = length(d)
        arms.append((d[0]*(J[1]-w1[1]) - d[1]*(J[0]-w1[0])) / Ln); lens.append(Ln)
    flips = sum(1 for k in range(1, len(arms)) if arms[k-1]*arms[k] <= 0)
    minArm = min(abs(x) for x in arms)
    sweep[name] = (min(arms), max(arms), minArm, min(lens), max(lens))
    if flips or minArm < P['minArm']:
        bad.append('%s: moment arm about the %s %+.3f..%+.3f m (min |arm| %.3f)'
                   % (name, jn, min(arms), max(arms), minArm))
if bad:
    sys.exit('Muscle placement rejected, line of action gets too close to '
             'or crosses its joint:\n  ' + '\n  '.join(bad))

extraDefs = dict(
    spin=['{ type = "AngularVelocitySensor"; target = "back"; minW = -10; maxW = 10; }'],
    velocity=['{ type = "VelocitySensor"; target = "back"; axis = "x"; minV = -3; maxV = 3; }',
              '{ type = "VelocitySensor"; target = "back"; axis = "y"; minV = -3; maxV = 3; }'],
    contact=['{ type = "ContactSensor"; target = "footL"; }',
             '{ type = "ContactSensor"; target = "footR"; }'])
extra = [d for k in P['extraSensors'] for d in extraDefs[k]]

maxForce = P['forcePerWeight']*weight
maxPower = P['wattsPerKg']*mass/len(mus)

def f(x): return ('%.4f' % x).rstrip('0').rstrip('.') if abs(x) > 1e-9 else '0.0'
def pt(p): return '{ x = %s; y = %s; }' % (f(p[0]), f(p[1]))

L = []
L.append('''
# walker: two legs, upright torso and head.  Generated by
# tools/gen_walker.py; edit the parameters there rather than this file.
#
# Mass %.3f kg, weight %.2f N, head at %.2f m.  Centre of mass x = %.3f over
# feet spanning x = %.3f..%.3f.  Joint limits are relative to the standing
# start pose.
''' % (mass, weight, headC[1], com[0], heelX, toeX))
L.append('''global: {
    popSize = %d;
    startPopulationPercent = 0.5;

    //Run ends when the head drops below this height (m)
    headFloor = %s;

    //Only these limbs may touch the ground; any other touching ends the run
    groundLimbs = (%s);

    //Fitness = (max head x + fitnessBase) * (fraction of the run survived)
    // ^ survivalExponent.  The run also ends once positive muscle work
    // reaches energyBudget (J), which counts as surviving.
    fitnessBase = %s;
    survivalExponent = %s;
    energyBudget = %s;

    //Mating probabilities:
    inheritAllLinks = false;
    inheritDominant = 0.9;
    linkEnabledRate = 0.1;

    weightMutationRate   = 0.2;
    weightPerturbScale   = 0.1;
    weightPerturbNormal  = 0.6;
    weightPerturbUniform = 0.39;

    addLinkMutationRate = %s;
    addNodeMutationRate = %s;

    compatGDiff = 1.0;
    compatWDiff = 0.4;

    compatThresh = 7.0;

    singleMate = 0.2;
    specieMate = 0.99;

    oldAge = 5;

    targetSpecies = 10;
    threshAdapt = 0.2;

    //nInput/nOutput are computed from sensors and muscles
    nInput = %d;
    nOutput = %d;
};
''' % (P['popSize'], f(P['headFloor']),
       ', '.join('"%s"' % g for g in P['groundLimbs']),
       f(P['fitnessBase']), f(P['survivalExponent']), f(P['energyBudget']),
       f(P['addLink']), f(P['addNode']),
       10 + len(extra) + 1, 2*len(mus)))

L.append('limbs = (')
blocks = []
for b in bodies:
    s = '    { name = "%s";\n      position = %s;\n      angle = 0.0;\n      angularDamping = 0.01;\n      shapes = (\n' % (b.name, pt(b.pos))
    sh = []
    for x in b.shapes:
        t = '\t{ type = "%s";\n' % x['type']
        if x['type'] == 'box': t += '\t  w = %s; h = %s;\n' % (f(x['w']), f(x['h']))
        else: t += '\t  radius = %s;\n\t  position = %s;\n' % (f(x['radius']), pt(x['position']))
        t += '\t  density = %s;\n' % f(x['density'])
        if 'friction' in x: t += '\t  friction = %s;\n' % f(x['friction'])
        t += '\t  groupIndex = %d;\n\t}' % x['groupIndex']
        sh.append(t)
    s += ',\n'.join(sh) + ');\n    }'
    blocks.append(s)
L.append(',\n'.join(blocks) + ');\n')

def joint(name, a, b, p, lim):
    return '''    { name = "%s";
      type = "revolute";
      obj1 = "%s";
      obj2 = "%s";
      position = %s;
      lowerAngle = %s; # %d deg from start pose
      upperAngle = %s; # %d deg
    }''' % (name, a, b, pt(p), f(rad(lim[0])), lim[0], f(rad(lim[1])), lim[1])
js = []
for side in 'RL':
    js += [joint('ankle' + side, 'shin' + side, 'foot' + side, A, P['ankle']),
           joint('knee' + side, 'thigh' + side, 'shin' + side, K, P['knee']),
           joint('hip' + side, 'back', 'thigh' + side, H, P['hip'])]
L.append('# ankle: + lifts the toes; knee: - bends it; hip: + swings the leg forward')
L.append('joints = (\n' + ',\n'.join(js) + ');\n')

L.append('''# Muscle limits use the same rules as the other creatures: maxForce about
# %gx body weight, maxPower about %g W per kg split across muscles.
# Stiffness: maxK gives maxForce at %d%% stretch of the start length.
# minEq..maxEq span the lengths each muscle reaches over its joint's range.''' % (P['forcePerWeight'], P['wattsPerKg'], int(P['stretchAtMaxK']*100)))
mb = []
for name, jn, b1, w1, b2, w2 in mus:
    L0 = length(sub(w1, w2))
    maxK = maxForce/(P['stretchAtMaxK']*L0)
    amin, amax, marm, lmin, lmax = sweep[name]
    mb.append('''    { name = "%s"; # %s: start length %s, moment arm %s..%s m
      obj1 = "%s";
      pos1 = %s;
      obj2 = "%s";
      pos2 = %s;
      minK = %s;
      maxK = %s;
      minEq = %s;
      maxEq = %s;
      kd = %s;
      maxForce = %s;
      maxPower = %s;
    }''' % (name, jn, f(L0), f(marm), f(max(abs(amin), abs(amax))),
            b1.name, pt(b1.local(w1)), b2.name, pt(b2.local(w2)),
            f(maxK/P['kSpan']), f(maxK), f(lmin), f(lmax), f(P['kd']),
            f(maxForce), f(maxPower)))
L.append('muscles = (\n' + ',\n'.join(mb) + ');\n')

L.append('''shapes = (
    { name = "head";
      body = "back";
      position = { x = 0.0; y = %s; };
    },
    { name = "ankleR";
      body = "shinR";
      position = { x = 0.0; y = %s; };
    },
    { name = "ankleL";
      body = "shinL";
      position = { x = 0.0; y = %s; };
    });
''' % (f(P['torsoL']/2), f(-P['shinL']/2), f(-P['shinL']/2)))

L.append('''sensors = (
    { type = "JointSensor"; target = "ankleL"; },
    { type = "JointSensor"; target = "kneeL"; },
    { type = "JointSensor"; target = "hipL"; },
    { type = "JointSensor"; target = "ankleR"; },
    { type = "JointSensor"; target = "kneeR"; },
    { type = "JointSensor"; target = "hipR"; },
    { type = "HeightSensor"; target = "head"; minH = 0; maxH = 2; },
    { type = "HeightSensor"; target = "ankleL"; minH = 0; maxH = 0.5; },
    { type = "HeightSensor"; target = "ankleR"; minH = 0; maxH = 0.5; },
    # Box2D angles are in [-pi, pi]; lean either way must be visible
    { type = "BodyAngleSensor"; target = "back"; minA = -1.5708; maxA = 1.5708; }%s);
''' % ''.join(',\n    ' + d for d in extra))
open(out, 'w').write('\n'.join(L))

print('mass %.3f kg  weight %.2f N  maxForce %.1f N  maxPower %.2f W/muscle  budget %.0f J'
      % (mass, weight, maxForce, maxPower, P['energyBudget']))
print('ankle %s knee %s hip %s head %s' % (pt(A), pt(K), pt(H), pt(headC)))
print('COM x %.3f (feet %.3f..%.3f), COM y %.3f' % (com[0], heelX, toeX, com[1]))
print('mass: legs %.0f%%, torso %.0f%%, head %.0f%%' % (
    100*sum(b.mass for b in bodies[:6])/mass,
    100*4*P['torsoW']/2*P['torsoL']/2*P['torsoDensity']/mass,
    100*math.pi*P['headR']**2*P['headDensity']/mass))
for name, jn, b1, w1, b2, w2 in mus[:3]:
    amin, amax, marm, lmin, lmax = sweep[name]
    print('  %-10s %-6s start length %.3f  arm %+.3f..%+.3f (min |arm| %.3f)  length %.3f..%.3f'
          % (name, jn, length(sub(w1, w2)), amin, amax, marm, lmin, lmax))
