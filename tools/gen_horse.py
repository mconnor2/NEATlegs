#!/usr/bin/env python3
"""Generate horse.cfg: a quadruped with a horizontal barrel, a neck and
head held up in front, and two-segment legs ending in round hooves.  All
geometry is derived from a few parameters so the design can be iterated;
prints a design summary.

Usage: tools/gen_horse.py [out.cfg] ['{"param": value, ...}']

Conventions: x forward (head), y up, body angle CCW positive.  Joint
angles start at 0 in the initial pose (Creature sets the joint reference
angle from the start pose), so limits are relative to it.

Each leg is an upper and a lower segment with the hoof (a ball) at the
bottom of the lower one, standing with the hoof under its hip or
shoulder.  As in a horse the middle joints bend towards the middle of
the body: the hind leg's stifle points forward and the foreleg's elbow
points back.  The left and right leg of a pair start in the same place,
and every part shares groupIndex -1 so the legs pass each other (in 2D
they are side by side).
"""
import math, sys, json

P = dict(
    # barrel: a box, hips and shoulders near its ends, jointDrop below
    # its centre line
    bodyL=1.10, bodyT=0.30, bodyDensity=1.5,
    hipX=0.45, shoulderX=0.42, jointDrop=0.08,
    # neck: a chain of balls up and forward from the front of the barrel,
    # head ball at its end and a muzzle ball ahead of it
    neckAngle=50.0, neckL=0.55, neckR=0.07, neckBalls=4, neckDensity=1.0,
    headR=0.09, muzzleR=0.06, muzzle=(0.12, -0.06), headDensity=1.0,
    # legs (m): segment lengths, how far the upper segment leans from
    # vertical (towards the middle joint), thickness, hoof radius.  Legs
    # much lighter than this (0.07 wide, density 1) get yanked so hard by
    # their muscles that the joints come apart.
    hindUpper=0.42, hindLower=0.52, hindLean=20.0,
    foreUpper=0.38, foreLower=0.56, foreLean=20.0,
    legW=0.08, legDensity=2.0,
    hoofR=0.05, hoofDensity=2.0, hoofFriction=1.6,
    # joint ranges relative to the start pose (degrees).  Hips and
    # shoulders: + swings the leg forward.  Middle joints: flex (fold the
    # leg) up to flex degrees, and extend to straightStop degrees short of
    # a straight leg.
    hip=(-40, 35), shoulder=(-35, 40), flex=100, straightStop=5,
    # muscles: maxK gives maxForce at stretchAtMaxK of the start length;
    # minK = minKFrac * maxK (low lets a muscle relax, which only pays
    # when holding force costs energy)
    stretchAtMaxK=0.2, minKFrac=0.1, kd=1.0,
    minArm=0.02,  # reject muscles whose moment arm drops below this (m)
    # Each leg carries about a quarter of the weight, so muscles get half
    # the two-legged creatures' 8x body weight (with 8x the median
    # distance was a tenth, as random networks fling the legs about)
    forcePerWeight=4.0, wattsPerKg=50.0,
    # One push-pull spring per joint.  Middle joints: inside the bend,
    # from upper segment to lower, midUp/midDown along the segments from
    # the joint, midOff off the segment axes.  Hips and shoulders: from a
    # point on the barrel straight above the upper segment's attachment,
    # topUp above the joint, to the upper segment topDown along it and
    # topOff off its axis on the side the middle joint points to.
    midUp=0.25, midDown=0.15, midOff=0.06,
    topUp=0.12, topDown=0.25, topOff=0.06,
    # False for one leg at each end (a planar quadruped)
    pairs=True,
    # Start mid-stride: left hooves this far (m) ahead of their hip or
    # shoulder, right hooves as far behind.  Legs of a pair otherwise
    # start identical; staggered, the median distance went from 10 to
    # 12 m (8 seeds, 250 generations), though gaits weren't cleaner.
    stagger=0.15,
    headFloor=0.9, popSize=1000,
    # Extra sensors beyond joint angles, head height and barrel angle, any
    # of 'spin' (barrel angular velocity), 'velocity' (barrel x and y),
    # 'contact' (each hoof on the ground), 'hoofHeight' (each hoof)
    extraSensors=[],
    addLink=0.05, addNode=0.05, # NEAT structural mutation rates
    # Objective, as the walker's: survival shaping, and a metabolic
    # energy cost (positiveWork, negativeWork, forceTime weights; None
    # for positive work alone).
    fitnessBase=0.1, survivalExponent=0.5, energyCost=(4, 0.8333, 1),
    energyBudget=0.0,
    maxSteps=1000,      # episode length cap (steps at 60 Hz)
)
if len(sys.argv) > 2:
    P.update(json.loads(sys.argv[2]))
out = sys.argv[1] if len(sys.argv) > 1 else 'horse.cfg'

rad = math.radians
def add(a, b): return (a[0]+b[0], a[1]+b[1])
def sub(a, b): return (a[0]-b[0], a[1]-b[1])
def mul(a, s): return (a[0]*s, a[1]*s)
def dot(a, b): return a[0]*b[0] + a[1]*b[1]
def unit(a): l = math.hypot(*a); return (a[0]/l, a[1]/l)
def perp(a): return (-a[1], a[0])
def length(a): return math.hypot(*a)

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
        s.shapes.append(dict(type='ball', radius=r, position=lpos,
                             density=density, **kw))
        m = math.pi*r*r*density; s.mass += m
        s.mx = add(s.mx, mul(s.world(lpos), m))

def segment(name, p, q):  # box between two points, local +y from p to q
    d = unit(sub(q, p)); angle = math.atan2(-d[0], d[1])
    b = Body(name, mul(add(p, q), 0.5), angle)
    b.box(P['legW']/2, length(sub(q, p))/2, P['legDensity'], groupIndex=-1)
    return b

# Leg geometry: upper segment from the top joint T leaning `lean` degrees
# towards the middle joint's side s (+1 forward, -1 back), lower segment
# from the middle joint M to the hoof centre F straight below T
def legHeight(u, l, lean):
    a = rad(lean)
    return P['hoofR'] + u*math.cos(a) + math.sqrt(l*l - (u*math.sin(a))**2)
jointY = max(legHeight(P['hindUpper'], P['hindLower'], P['hindLean']),
             legHeight(P['foreUpper'], P['foreLower'], P['foreLean']))
bodyY = jointY + P['jointDrop']

back = Body('back', (0.0, bodyY), 0.0)
back.box(P['bodyL']/2, P['bodyT']/2, P['bodyDensity'], groupIndex=-1)
na = rad(P['neckAngle']); nd = (math.cos(na), math.sin(na))
neckBase = (P['bodyL']/2 - P['neckR'], P['bodyT']/2 - P['neckR'])
for k in range(1, P['neckBalls'] + 1):
    c = add(neckBase, mul(nd, P['neckL']*(k - 0.5)/P['neckBalls']))
    back.ball(P['neckR'], c, P['neckDensity'], groupIndex=-1)
headL = add(neckBase, mul(nd, P['neckL']))
back.ball(P['headR'], headL, P['headDensity'], groupIndex=-1)
back.ball(P['muzzleR'], add(headL, P['muzzle']), P['headDensity'],
          groupIndex=-1)
headW = back.world(headL)

def rot(v, a):
    c, sn = math.cos(a), math.sin(a)
    return (v[0]*c - v[1]*sn, v[0]*sn + v[1]*c)

class Leg:
    def __init__(s, end, side, top, u, l, lean, sgn, topJoint, midJoint,
                 topRange, dx):
        s.side, s.T, s.sgn = side, top, sgn
        a = rad(lean)
        M0 = add(top, (sgn*u*math.sin(a), -u*math.cos(a)))
        F0 = (top[0], P['hoofR'])
        bend0 = math.degrees(math.acos(dot(unit(sub(M0, top)),
                                           unit(sub(F0, M0)))))
        # Hoof dx ahead of the top joint: same segments, bent at the
        # middle joint to reach it (law of cosines), still bending to sgn
        s.F = (top[0] + dx, P['hoofR'])
        tf = sub(s.F, top); d = length(tf)
        al = math.acos((u*u + d*d - l*l)/(2*u*d))
        al = al if rot(tf, al)[0]*sgn > rot(tf, -al)[0]*sgn else -al
        s.M = add(top, mul(rot(unit(tf), al), u))
        swing = math.degrees(math.atan2(*sub(s.M, top)) -
                             math.atan2(*sub(M0, top)))
        topRange = (int(round(topRange[0] + swing)),
                    int(round(topRange[1] + swing)))
        s.upper = segment(end + 'Upper' + side, s.T, s.M)
        s.lower = segment(end + 'Lower' + side, s.M, s.F)
        # The hoof is its own body, locked to the lower segment, so the
        # rest of the leg can't count as ground contact
        s.hoof = Body(end + 'Hoof' + side, s.F, 0.0)
        s.hoof.ball(P['hoofR'], (0.0, 0.0), P['hoofDensity'],
                    friction=P['hoofFriction'], groupIndex=-1)
        s.hoofJoint = 'pastern' + ('H' if sgn > 0 else 'F') + side
        s.topJoint, s.midJoint = topJoint + side, midJoint + side
        # angle between the segments, 180 when straight
        du, dl = unit(sub(s.M, s.T)), unit(sub(s.F, s.M))
        s.bend = math.degrees(math.acos(max(-1, min(1, dot(du, dl)))))
        # Stops are where they would be from the square stance: flexion
        # up to flex beyond that bend, extension to straightStop short of
        # straight.  Hind (stifle forward) flexes clockwise.
        ext = int(math.floor(s.bend - P['straightStop']))
        flex = int(round(bend0 + P['flex'] - s.bend))
        s.topRange = topRange
        s.midRange = (-flex, ext) if sgn > 0 else (-ext, flex)

hindT = (-P['hipX'], jointY); foreT = (P['shoulderX'], jointY)
legs = []
for side in ('RL' if P['pairs'] else ['']):
    dx = {'L': P['stagger'], 'R': -P['stagger'], '': 0.0}[side]
    legs.append(Leg('hind', side, hindT, P['hindUpper'], P['hindLower'],
                    P['hindLean'], +1, 'hip', 'stifle', P['hip'], dx))
    legs.append(Leg('fore', side, foreT, P['foreUpper'], P['foreLower'],
                    P['foreLean'], -1, 'shoulder', 'elbow', P['shoulder'],
                    dx))
legs.sort(key=lambda g: (g.sgn < 0, g.side))   # hindL hindR foreL foreR
bodies = [back] + [b for g in legs for b in (g.upper, g.lower, g.hoof)]

mass = sum(b.mass for b in bodies)
com = mul((sum(b.mx[0] for b in bodies), sum(b.mx[1] for b in bodies)),
          1/mass)
weight = 10*mass

# Muscles, as world points in the start pose: (name, joint, joint point,
# distal body, range, body1, point1, body2, point2)
mus = []
for g in legs:
    du, dl = unit(sub(g.M, g.T)), unit(sub(g.F, g.M))
    chord = sub(mul(add(g.T, g.F), 0.5), g.M)       # into the bend
    nu = perp(du) if dot(perp(du), chord) > 0 else mul(perp(du), -1)
    nl = perp(dl) if dot(perp(dl), chord) > 0 else mul(perp(dl), -1)
    mus.append((g.midJoint + 'Muscle', g.midJoint, g.M, g.lower, g.midRange,
                g.upper, add(add(g.M, mul(du, -P['midUp'])),
                             mul(nu, P['midOff'])),
                g.lower, add(add(g.M, mul(dl, P['midDown'])),
                             mul(nl, P['midOff']))))
    # outside of the bend on the upper segment, barrel point above it
    p2 = add(add(g.T, mul(du, P['topDown'])), mul(nu, -P['topOff']))
    p1 = (p2[0], g.T[1] + P['topUp'])
    mus.append((g.topJoint + 'Muscle', g.topJoint, g.T, g.upper, g.topRange,
                back, p1, g.upper, p2))

# Sweep each muscle over the range of the joint it crosses (distal body
# rotated about the joint).  A straight-line spring whose line of action
# crosses the joint flips between extending and flexing it, and near the
# crossing has no leverage at all, so reject those designs.  Rest-length
# ranges come from the lengths the muscle actually passes through.
def rotAbout(p, c, a):
    d = sub(p, c); cs, sn = math.cos(a), math.sin(a)
    return add(c, (d[0]*cs - d[1]*sn, d[0]*sn + d[1]*cs))
sweep, bad = {}, []
for name, jn, J, distal, lim, b1, w1, b2, w2 in mus:
    arms, lens = [], []
    for deg in range(lim[0], lim[1] + 1):
        p2 = rotAbout(w2, J, rad(deg))
        d = sub(p2, w1); Ln = length(d)
        arms.append((d[0]*(J[1]-w1[1]) - d[1]*(J[0]-w1[0])) / Ln)
        lens.append(Ln)
    flips = sum(1 for k in range(1, len(arms)) if arms[k-1]*arms[k] <= 0)
    minArm = min(abs(x) for x in arms)
    sweep[name] = (min(arms), max(arms), minArm, min(lens), max(lens))
    if flips or minArm < P['minArm']:
        bad.append('%s: moment arm about the %s %+.3f..%+.3f m '
                   '(min |arm| %.3f)' % (name, jn, min(arms), max(arms),
                                         minArm))
if bad:
    sys.exit('Muscle placement rejected, line of action gets too close to '
             'or crosses its joint:\n  ' + '\n  '.join(bad))

hooves = [g.hoof.name for g in legs]
def hoofName(g): return 'hoof' + ('H' if g.sgn > 0 else 'F') + g.side
extraDefs = dict(
    spin=['{ type = "AngularVelocitySensor"; target = "back"; '
          'minW = -10; maxW = 10; }'],
    velocity=['{ type = "VelocitySensor"; target = "back"; axis = "x"; '
              'minV = -3; maxV = 3; }',
              '{ type = "VelocitySensor"; target = "back"; axis = "y"; '
              'minV = -3; maxV = 3; }'],
    contact=['{ type = "ContactSensor"; target = "%s"; }' % n
             for n in hooves],
    hoofHeight=['{ type = "HeightSensor"; target = "%s"; '
                'minH = 0; maxH = 0.5; }' % hoofName(g) for g in legs])
extra = [d for k in P['extraSensors'] for d in extraDefs[k]]
nSensors = 2*len(legs) + 2 + len(extra)

maxForce = P['forcePerWeight']*weight
maxPower = P['wattsPerKg']*mass/len(mus)

def f(x):
    return ('%.4f' % x).rstrip('0').rstrip('.') if abs(x) > 1e-9 else '0.0'
def pt(p): return '{ x = %s; y = %s; }' % (f(p[0]), f(p[1]))

hoofXs = sorted(set(g.F[0] for g in legs))
L = []
L.append('''
# horse: four two-segment legs under a horizontal barrel, neck and head
# up in front.  Generated by tools/gen_horse.py; edit the parameters there
# rather than this file.
#
# Mass %.3f kg, weight %.2f N, head at %.2f m, barrel at %.2f m.  Centre
# of mass x = %.3f between hooves at x = %.3f and %.3f.  Joint limits are
# relative to the standing start pose.
''' % (mass, weight, headW[1], bodyY, com[0], hoofXs[0], hoofXs[-1]))
L.append('''global: {
    popSize = %d;
    startPopulationPercent = 0.5;

%s    //Run ends when the head drops below this height (m)
    headFloor = %s;

    //Only the hooves may touch the ground; any other limb touching ends
    // the run
    groundLimbs = (%s);

    //Fitness = (max head x + fitnessBase) * (fraction of the run survived)
    // ^ survivalExponent.  The run also ends once the energy cost
    // reaches energyBudget (J, 0 for none), which counts as surviving.
    fitnessBase = %s;
    survivalExponent = %s;
    energyBudget = %s;
%s
    //Mating probabilities:
    inheritAllLinks = false;
    inheritDominant = 0.9;
    linkEnabledRate = 0.1;

    weightMutationRate   = 0.8;
    weightPerturbScale   = 0.1;
    weightPerturbNormal  = 0.6;
    weightPerturbUniform = 0.39;

    addLinkMutationRate = %s;
    addNodeMutationRate = %s;

    compatGDiff = 1.0;
    compatWDiff = 0.4;

    compatThresh = 3.0;

    singleMate = 0.2;
    specieMate = 0.99;

    oldAge = 5;

    //A species stops reproducing after this many generations without
    // improving; only its fittest survivalThreshold breed
    dropoffAge = 15;
    survivalThreshold = 0.2;

    targetSpecies = 10;
    threshAdapt = 0.2;

    //nInput/nOutput are computed from sensors and muscles
    nInput = %d;
    nOutput = %d;
};
''' % (P['popSize'],
       ('' if P['maxSteps'] == 1000 else
        '    //Longest run (steps at 60 Hz)\n    maxSteps = %d;\n\n'
        % P['maxSteps']),
       f(P['headFloor']),
       ', '.join('"%s"' % n for n in hooves),
       f(P['fitnessBase']), f(P['survivalExponent']), f(P['energyBudget']),
       ('' if not P['energyCost'] else '''
    //Energy cost = positiveWork * W+ + negativeWork * |W-| + forceTime *
    // integral of |muscle force| dt (J per N s)
    energyCost = { positiveWork = %s; negativeWork = %s; forceTime = %s; };
''' % tuple(f(x) for x in P['energyCost'])),
       f(P['addLink']), f(P['addNode']),
       nSensors + 1, 2*len(mus)))

L.append('limbs = (')
blocks = []
for b in bodies:
    s = ('    { name = "%s";\n      position = %s;\n      angle = %s;\n'
         '      angularDamping = 0.01;\n      shapes = (\n'
         % (b.name, pt(b.pos), f(b.angle)))
    sh = []
    for x in b.shapes:
        t = '\t{ type = "%s";\n' % x['type']
        if x['type'] == 'box':
            t += '\t  w = %s; h = %s;\n' % (f(x['w']), f(x['h']))
        else:
            t += '\t  radius = %s;\n\t  position = %s;\n' % (
                f(x['radius']), pt(x['position']))
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
    }''' % (name, a, b, pt(p), f(rad(lim[0])), lim[0], f(rad(lim[1])),
            lim[1])
js = []
for g in legs:
    js += [joint(g.topJoint, 'back', g.upper.name, g.T, g.topRange),
           joint(g.midJoint, g.upper.name, g.lower.name, g.M, g.midRange),
           joint(g.hoofJoint, g.lower.name, g.hoof.name, g.F, (0, 0))]
L.append('# hip, shoulder: + swings the leg forward; stifle: - folds the hind '
         'leg;\n# elbow: + folds the foreleg; pasterns lock the hooves on')
L.append('joints = (\n' + ',\n'.join(js) + ');\n')

L.append('''# Muscle limits use the same rules as the other creatures: maxForce about
# %gx body weight, maxPower about %g W per kg split across muscles.
# Stiffness: maxK gives maxForce at %d%% stretch of the start length.
# minEq..maxEq span the lengths each muscle reaches over its joint's range.'''
         % (P['forcePerWeight'], P['wattsPerKg'],
            int(P['stretchAtMaxK']*100)))
mb = []
for name, jn, J, distal, lim, b1, w1, b2, w2 in mus:
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
            f(maxK*P['minKFrac']), f(maxK), f(lmin), f(lmax), f(P['kd']),
            f(maxForce), f(maxPower)))
L.append('muscles = (\n' + ',\n'.join(mb) + ');\n')

pts = ['''    { name = "head";
      body = "back";
      position = %s;
    }''' % pt(headL)]
for g in legs:
    pts.append('''    { name = "%s";
      body = "%s";
      position = %s;
    }''' % (hoofName(g), g.hoof.name, pt((0.0, 0.0))))
L.append('shapes = (\n' + ',\n'.join(pts) + ');\n')

sens = ['{ type = "JointSensor"; target = "%s"; }' % j
        for g in legs for j in (g.topJoint, g.midJoint)]
sens += ['{ type = "HeightSensor"; target = "head"; minH = 0; maxH = 2; }',
         '# Box2D angles are in [-pi, pi]; pitch either way must be visible',
         '{ type = "BodyAngleSensor"; target = "back"; '
         'minA = -1.5708; maxA = 1.5708; }']
sens += extra
L.append('sensors = (\n' + ''.join(
    '    ' + s + ('\n' if s.startswith('#') else
                  (',\n' if k < len(sens) - 1 else ');\n'))
    for k, s in enumerate(sens)))
open(out, 'w').write('\n'.join(L))

print('mass %.3f kg  weight %.2f N  maxForce %.1f N  maxPower %.2f W/muscle'
      % (mass, weight, maxForce, maxPower))
print('barrel %.2f m  joints %.2f m  head %s  inputs %d outputs %d'
      % (bodyY, jointY, pt(headW), nSensors + 1, 2*len(mus)))
print('COM %s, hooves at x = %.3f and %.3f' % (pt(com), hoofXs[0],
                                                hoofXs[-1]))
print('mass: barrel+neck+head %.0f%%, legs %.0f%%'
      % (100*back.mass/mass, 100*(mass - back.mass)/mass))
for g in legs[::2]:
    print('  %s leg: bend %.0f deg, %s %s, %s %s'
          % (g.upper.name[:4], g.bend, g.topJoint, g.topRange, g.midJoint,
             g.midRange))
for name, jn, J, distal, lim, b1, w1, b2, w2 in mus[:2] + mus[4:6]:
    amin, amax, marm, lmin, lmax = sweep[name]
    print('  %-15s start length %.3f  arm %+.3f..%+.3f (min |arm| %.3f)  '
          'length %.3f..%.3f' % (name, length(sub(w1, w2)), amin, amax, marm,
                                 lmin, lmax))
