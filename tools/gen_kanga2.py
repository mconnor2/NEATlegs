#!/usr/bin/env python3
"""Generate kanga2.cfg: a one-legged kangaroo/raptor with a horizontal
torso balanced over the hip by a tail.  All geometry is derived from a few
parameters so the design can be iterated; prints a design summary.

Usage: tools/gen_kanga2.py [out.cfg] ['{"param": value, ...}']

Conventions: x forward (toes, head), y up, body angle CCW positive.
Joint angles start at 0 in the initial pose (Creature sets the joint
reference angle from the start pose), so limits are relative to it.
"""
import math, sys, json

P = dict(
    # leg
    ankleY=0.06, heel=0.12, toe=0.40, footT=0.06, footBall=0.03,
    shinL=0.50, shinTilt=30.0,       # degrees forward from vertical
    thighL=0.45, thighTilt=25.0,     # degrees backward from vertical
    legW=0.08,                       # limb thickness
    # torso (one box: tail behind the hip, chest ahead) + head ball
    torsoL=1.30, torsoT=0.08, torsoTilt=10.0,  # degrees, front up
    headR=0.09, headDensity=2.0,
    # joint ranges relative to start pose (degrees)
    ankle=(-60, 30), knee=(-95, 63), hip=(-40, 40),
    # muscles: stiffness range as fraction of maxForce per metre of stretch
    stretchAtMaxK=0.2, kSpan=4.0, kd=1.0,
    minArm=0.02,  # reject muscles whose moment arm drops below this (m)
    forcePerWeight=8.0, wattsPerKg=50.0,
    # One push-pull spring per joint, each on the inside of its joint's bend
    # so its line of action never crosses the joint (checked by the sweep
    # below).  Attachment distances along the segments and offsets (m):
    calfUp=0.30, calfOff=0.05,        # heel to back of shin
    kneeUp=0.30, kneeOff=0.05,        # back of thigh to back of shin
    hipTorso=0.25, hipThigh=0.20, hipOff=0.05,  # tail underside to back of thigh
    # Optional antagonist: top of torso ahead of the hip to front of thigh
    # (the underside route crosses the hip, the top one doesn't)
    hipFront=False, hipFrontTorso=0.35, hipFrontThigh=0.25, hipFrontOff=0.06,
    headFloor=0.35, popSize=200,
    balanceShift=0.0,   # move torso box forward (m): >0 nose-heavy
    groundLimbs=['foot'],       # any other limb touching the ground = fall
    rateSensors=True,           # torso spin, torso velocity, foot contact
    addLink=0.05, addNode=0.05, # NEAT structural mutation rates
)
if len(sys.argv) > 2:
    P.update(json.loads(sys.argv[2]))
out = sys.argv[1] if len(sys.argv) > 1 else 'kanga2.cfg'

rad = math.radians
def add(a, b): return (a[0]+b[0], a[1]+b[1])
def sub(a, b): return (a[0]-b[0], a[1]-b[1])
def mul(a, s): return (a[0]*s, a[1]*s)
def unit(a): l = math.hypot(*a); return (a[0]/l, a[1]/l)
def perp_ccw(a): return (-a[1], a[0])
def length(a): return math.hypot(*a)

# Joints
A = (0.0, P['ankleY'])
shinDir = (math.sin(rad(P['shinTilt'])), math.cos(rad(P['shinTilt'])))
K = add(A, mul(shinDir, P['shinL']))
thighDir = (-math.sin(rad(P['thighTilt'])), math.cos(rad(P['thighTilt'])))
H = add(K, mul(thighDir, P['thighL']))

class Body:
    def __init__(s, name, pos, angle):
        s.name, s.pos, s.angle, s.shapes = name, pos, angle, []
        s.mass, s.mx = 0.0, (0.0, 0.0)
    def local(s, w):        # world point -> local coordinates
        d = sub(w, s.pos); c, sn = math.cos(-s.angle), math.sin(-s.angle)
        return (d[0]*c - d[1]*sn, d[0]*sn + d[1]*c)
    def world(s, l):
        c, sn = math.cos(s.angle), math.sin(s.angle)
        return add(s.pos, (l[0]*c - l[1]*sn, l[0]*sn + l[1]*c))
    def box(s, hw, hh, density, **kw):
        s.shapes.append(dict(type='box', w=hw, h=hh, density=density, **kw))
        m = 4*hw*hh*density; s.mass += m; s.mx = add(s.mx, mul(s.pos, m))
    def ball(s, r, lpos, density, **kw):
        s.shapes.append(dict(type='ball', radius=r, position=lpos, density=density, **kw))
        m = math.pi*r*r*density; s.mass += m; s.mx = add(s.mx, mul(s.world(lpos), m))

def segment(name, p, q, w):  # box between two points, local +y from p to q
    d = unit(sub(q, p)); angle = math.atan2(-d[0], d[1])
    b = Body(name, mul(add(p, q), 0.5), angle)
    b.box(w/2, length(sub(q, p))/2, 1.0, groupIndex=-1)
    return b, d

# Foot: flat, heel behind the ankle, toes ahead
footLen = P['heel'] + P['toe']
foot = Body('foot', ((P['toe']-P['heel'])/2, P['footT']/2), 0.0)
foot.box(footLen/2, P['footT']/2, 1.0, groupIndex=-1, friction=1.6)
foot.ball(P['footBall'], (-footLen/2, 0.0), 1.0, groupIndex=-1, friction=1.6)
foot.ball(P['footBall'], (footLen/2, 0.0), 1.0, groupIndex=-1, friction=1.6)

shin, sd = segment('shin', A, K, P['legW'])
thigh, td = segment('thigh', K, H, P['legW'])

# Torso: one box along the torso axis; place it so the torso (box + head)
# balances about the hip
tilt = rad(P['torsoTilt']); ax = (math.cos(tilt), math.sin(tilt))
Lb, rb = P['torsoL'], P['torsoT']
mBox = Lb*rb*1.0; mHead = math.pi*P['headR']**2*P['headDensity']
# head centre sits at the box front: offset from hip = c + Lb/2 ; box at c
# balance: mBox*c + mHead*(c + Lb/2) = 0
c = -mHead*(Lb/2)/(mBox + mHead) + P['balanceShift']
back = Body('back', add(H, mul(ax, c)), tilt)
back.box(Lb/2, rb/2, 1.0, groupIndex=-1)
back.ball(P['headR'], (Lb/2, 0.0), P['headDensity'], groupIndex=-1)
bodies = [foot, shin, thigh, back]

mass = sum(b.mass for b in bodies)
com = mul(add(add(add(foot.mx, shin.mx), thigh.mx), back.mx), 1/mass)
weight = 10*mass
headW = back.world((Lb/2, 0.0))
heelX, toeX = -P['heel'] - P['footBall'], P['toe'] + P['footBall']

# Muscles: attachment world points at the start pose
shinBack = (-sd[1], sd[0])  if (-sd[1]) < 0 else (sd[1], -sd[0])   # normal pointing -x
thighFront = perp_ccw(td) if perp_ccw(td)[0] > 0 else mul(perp_ccw(td), -1)
thighBack = mul(thighFront, -1)
under = mul(perp_ccw(ax), -1)                                    # torso underside
mus = []
def muscle(name, b1, w1, b2, w2):
    L0 = length(sub(w1, w2)); mus.append((name, b1, b1.local(w1), b2, b2.local(w2), L0))
muscle('calf', foot, (-P['heel'] + 0.01, P['footT']),
       shin, add(add(A, mul(sd, P['calfUp'])), mul(shinBack, P['calfOff'])))
muscle('knee', thigh, add(add(K, mul(td, P['kneeUp'])), mul(thighBack, P['kneeOff'])),
       shin, add(add(K, mul(sd, -P['kneeUp'])), mul(shinBack, P['kneeOff'])))
muscle('hip', back, add(add(H, mul(ax, -P['hipTorso'])), mul(under, P['hipOff'])),
       thigh, add(add(H, mul(td, -P['hipThigh'])), mul(thighBack, P['hipOff'])))
if P['hipFront']:
    muscle('hipFront', back, add(add(H, mul(ax, P['hipFrontTorso'])), mul(under, -P['hipFrontOff'])),
           thigh, add(add(H, mul(td, -P['hipFrontThigh'])), mul(thighFront, P['hipFrontOff'])))

# Sweep each muscle over the range of the joint it crosses (distal body
# rotated about the joint).  A straight-line spring whose line of action
# crosses the joint flips between extending and flexing it, and near the
# crossing has no leverage at all, so reject those designs.  Rest-length
# ranges come from the lengths the muscle actually passes through.
jointFor = {frozenset(('shin', 'foot')): ('ankle', A, 'foot', P['ankle']),
            frozenset(('thigh', 'shin')): ('knee', K, 'shin', P['knee']),
            frozenset(('back', 'thigh')): ('hip', H, 'thigh', P['hip'])}
def rotAbout(p, c, a):
    d = sub(p, c); cs, sn = math.cos(a), math.sin(a)
    return add(c, (d[0]*cs - d[1]*sn, d[0]*sn + d[1]*cs))
sweep = {}
bad = []
for name, b1, l1, b2, l2, L0 in mus:
    jn, J, distal, lim = jointFor[frozenset((b1.name, b2.name))]
    w1, w2 = b1.world(l1), b2.world(l2)
    arms, lens = [], []
    for deg in range(lim[0], lim[1] + 1):
        a = math.radians(deg)
        p1 = rotAbout(w1, J, a) if b1.name == distal else w1
        p2 = rotAbout(w2, J, a) if b2.name == distal else w2
        d = sub(p2, p1); Ln = length(d)
        arms.append((d[0]*(J[1]-p1[1]) - d[1]*(J[0]-p1[0])) / Ln); lens.append(Ln)
    flips = sum(1 for k in range(1, len(arms)) if arms[k-1]*arms[k] <= 0)
    minArm = min(abs(x) for x in arms)
    sweep[name] = (jn, min(arms), max(arms), minArm, min(lens), max(lens))
    if flips or minArm < P['minArm']:
        bad.append('%s: moment arm about the %s %+.3f..%+.3f m (min |arm| %.3f)'
                   % (name, jn, min(arms), max(arms), minArm))
if bad:
    sys.exit('Muscle placement rejected, line of action gets too close to '
             'or crosses its joint:\n  ' + '\n  '.join(bad))

maxForce = P['forcePerWeight']*weight
maxPower = P['wattsPerKg']*mass/len(mus)

def f(x): return ('%.4f' % x).rstrip('0').rstrip('.') if abs(x) > 1e-9 else '0.0'
def pt(p): return '{ x = %s; y = %s; }' % (f(p[0]), f(p[1]))

L = []
L.append('''
# kanga2: one-legged hopper balanced by a long horizontal torso and tail
# (kangaroo / velociraptor).  Generated by tools/gen_kanga2.py; edit the
# parameters there rather than this file.
#
# Mass %.3f kg, weight %.2f N.  Centre of mass x = %.3f over a foot
# spanning x = %.3f..%.3f.  Torso balances about the hip (tail behind,
# head ahead).  All creature shapes share groupIndex -1 so the tail can't
# snag the leg.  Joint limits are relative to this start pose.
''' % (mass, weight, com[0], heelX, toeX))
L.append('''global: {
    popSize = %d;
    startPopulationPercent = 0.5;

    //Run ends when the head drops below this height (m)
    headFloor = %s;
%s
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
       ('' if not P['groundLimbs'] else '\n    //Only these limbs may touch the ground; any other touching ends the run\n    groundLimbs = (%s);\n' % ', '.join('"%s"' % g for g in P['groundLimbs'])),
       f(P['addLink']), f(P['addNode']),
       (10 if P['rateSensors'] else 6)+1, 2*len(mus)))

L.append('limbs = (')
blocks = []
for b in bodies:
    s = '    { name = "%s";\n      position = %s;\n      angle = %s;\n      angularDamping = 0.01;\n      shapes = (\n' % (b.name, pt(b.pos), f(b.angle))
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
L.append('joints = (\n' + ',\n'.join([
    joint('ankle', 'shin', 'foot', A, P['ankle']),
    joint('knee', 'thigh', 'shin', K, P['knee']),
    joint('hip', 'back', 'thigh', H, P['hip'])]) + ');\n')

L.append('''# Muscle limits use the same rules as the other creatures: maxForce about
# %gx body weight, maxPower about %g W per kg split across muscles.
# Stiffness: maxK gives maxForce at %d%% stretch of the start length.
# minEq..maxEq span the lengths each muscle reaches over its joint's range.''' % (P['forcePerWeight'], P['wattsPerKg'], int(P['stretchAtMaxK']*100)))
mb = []
for name, b1, l1, b2, l2, L0 in mus:
    maxK = maxForce/(P['stretchAtMaxK']*L0)
    jn, amin, amax, _, lmin, lmax = sweep[name]
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
    }''' % (name, jn, f(L0), f(abs(amin) if amin > 0 else abs(amax)), f(max(abs(amin), abs(amax))), b1.name, pt(l1), b2.name, pt(l2), f(maxK/P['kSpan']), f(maxK),
            f(lmin), f(lmax), f(P['kd']), f(maxForce), f(maxPower)))
L.append('muscles = (\n' + ',\n'.join(mb) + ');\n')

L.append('''shapes = (
    { name = "head";
      body = "back";
      position = { x = %s; y = 0.0; };
    },
    { name = "heel";
      body = "foot";
      position = { x = %s; y = 0.0; };
    });
''' % (f(Lb/2), f(-footLen/2)))

L.append('''sensors = (
    { type = "JointSensor"; target = "ankle"; },
    { type = "JointSensor"; target = "knee"; },
    { type = "JointSensor"; target = "hip"; },
    { type = "HeightSensor"; target = "head"; minH = 0; maxH = 2; },
    { type = "HeightSensor"; target = "heel"; minH = 0; maxH = 1; },
    # Box2D angles are in [-pi, pi]; lean either way must be visible
    { type = "BodyAngleSensor"; target = "back"; minA = -1.5708; maxA = 1.5708; }%s);
''' % (''',
    # Rates and contact: balance needs to know how fast it is tipping,
    # and hopping needs to know whether the foot is down
    { type = "AngularVelocitySensor"; target = "back"; minW = -10; maxW = 10; },
    { type = "VelocitySensor"; target = "back"; axis = "x"; minV = -3; maxV = 3; },
    { type = "VelocitySensor"; target = "back"; axis = "y"; minV = -3; maxV = 3; },
    { type = "ContactSensor"; target = "foot"; }''' if P['rateSensors'] else ''))
open(out, 'w').write('\n'.join(L))

print('mass %.3f kg  weight %.2f N  maxForce %.1f N  maxPower %.2f W/muscle' % (mass, weight, maxForce, maxPower))
print('ankle %s knee %s hip %s head %s' % (pt(A), pt(K), pt(H), pt(headW)))
print('COM x %.3f (foot %.3f..%.3f), COM y %.3f' % (com[0], heelX, toeX, com[1]))
print('torso: box centre %.3f from hip, head arm %.3f' % (c, c + Lb/2))
for name, b1, l1, b2, l2, L0 in mus:
    jn, amin, amax, marm, lmin, lmax = sweep[name]
    print('  %-8s %-6s start length %.3f  arm %+.3f..%+.3f (min |arm| %.3f)  length %.3f..%.3f'
          % (name, jn, L0, amin, amax, marm, lmin, lmax))
