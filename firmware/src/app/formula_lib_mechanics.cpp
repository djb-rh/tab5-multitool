// The library: Motion, Forces & energy, Rotation & machines.
#include "formula_diagrams.h"
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {
namespace {

using namespace units;
constexpr uint8_t G1 = lib::kMotion;
constexpr uint8_t G2 = lib::kForces;
constexpr uint8_t G3 = lib::kRotation;

// ---- Motion ----

constexpr Var speedV[] = {
  V("v", "v", "Average speed", "Walking is 1.4 m/s, highway driving 30 m/s.", kSpeed, "km/h"),
  V("d", "d", "Distance", "Covered.", kLength, "km"),
  V("t", "t", "Time", "Taken.", kTime, "min"),
};
constexpr const char *speedE[] = {"v = d/t"};

constexpr Var suvatV[] = {
  S("s", "s", "Displacement", "How far it ends up from the start.", kLength),
  S("u", "u", "Starting speed", "At the start of the time. 0 from rest.", kSpeed),
  S("v", "v", "Final speed", "At the end of the time.", kSpeed),
  S("a", "a", "Acceleration", "Steady throughout. A brisk car is 3 m/s^2.", kAcceleration),
  V("t", "t", "Time", "From start to end.", kTime),
};
constexpr const char *suvatE[] = {
  "v = u + a*t", "s = u*t + a*t^2/2", "v^2 = u^2 + 2*a*s", "s = (u + v)*t/2", "s = v*t - a*t^2/2",
};

constexpr Var fallV[] = {
  V("h", "h", "Height", "How far it falls.", kLength),
  V("t", "t", "Fall time", "From rest.", kTime),
  V("v", "v", "Impact speed", "At the bottom.", kSpeed),
  kVarG,
};
constexpr const char *fallE[] = {"h = g*t^2/2", "v = g*t", "v^2 = 2*g*h"};

constexpr Var projV[] = {
  V("R", "R", "Range", "Back at launch height.", kLength),
  V("H", "H", "Top height", "Above the launch point.", kLength),
  V("T", "T", "Flight time", "Up and back down.", kTime),
  V("v", "v", "Launch speed", "A hard-hit ball leaves at 40 m/s.", kSpeed),
  V("theta", "θ", "Launch angle", "Above level. 45 deg goes furthest.", kAngle, "deg", kPositive | kAcute),
  kVarG,
};
constexpr const char *projE[] = {
  "R = v^2*sin(2*theta)/g", "H = v^2*sin(theta)^2/(2*g)", "T = 2*v*sin(theta)/g",
  "tan(theta) = 4*H/R", "tan(theta) = g*T^2/(2*R)",
};

constexpr Var vecV[] = {
  S("vx", "v_{x}", "Across part", "Along the level.", kSpeed),
  S("vy", "v_{y}", "Upward part", "Square to the level.", kSpeed),
  V("v", "v", "Speed", "The whole vector's length.", kSpeed),
  S("theta", "θ", "Angle", "From the level.", kAngle, "deg"),
};
constexpr const char *vecE[] = {
  "v = sqrt(vx^2 + vy^2)", "vx = v*cos(theta)", "vy = v*sin(theta)", "theta = atan2(vy, vx)",
};

constexpr Var stopV[] = {
  V("d", "d", "Stopping distance", "Thinking plus braking. At 100 km/h about 80 m.", kLength),
  V("v", "v", "Speed", "When the driver sees the hazard.", kSpeed, "km/h"),
  V("tr", "t_{r}", "Reaction time", "1 to 1.5 s for most drivers.", kTime),
  V("mu", "μ", "Grip", "Tyre on road: 0.8 dry, 0.5 wet, 0.15 on ice.", kNone, nullptr, kPositive, kPresetFriction),
  kVarG,
};
constexpr const char *stopE[] = {"d = v*tr + v^2/(2*mu*g)"};

constexpr Formula kMotionTable[] = {
  formula("suvat", "Constant acceleration (SUVAT)", G1, "kinematics equations of motion speed distance time braking",
          "Straight-line motion with steady acceleration. Any three give the other two.",
          suvatV, suvatE, 3, dSuvat),
  formula("speed", "Speed, distance, time", G1, "velocity average rate travel pace",
          "Average speed over a journey.", speedV, speedE, 2, dCar),
  formula("free-fall", "Free fall", G1, "drop gravity falling height impact",
          "A drop from rest, ignoring air.", fallV, fallE, 1, dDrop),
  formula("projectile", "Projectile", G1, "ballistics range launch angle thrown trajectory cannon",
          "Range, top height and flight time, over level ground and ignoring air.",
          projV, projE, 2, dProjectile),
  formula("vector", "Vector components", G1, "resolve polar rectangular x y",
          "A speed split into parts along and across. Works for any vector.", vecV, vecE, 2, dVector),
  formula("stopping", "Stopping distance", G1, "braking car reaction thinking road safety",
          "How far a car travels before it stops: reaction time, then braking on the grip available.",
          stopV, stopE, 3, dCar),
};

// ---- Forces & energy ----

constexpr Var fmaV[] = {
  S("F", "F", "Net force", "1 N lifts a small apple.", kForce),
  V("m", "m", "Mass", "Of the object.", kMass),
  S("a", "a", "Acceleration", "Gravity's is 9.81 m/s^2.", kAcceleration),
};
constexpr const char *fmaE[] = {"F = m*a"};

constexpr Var weightV[] = {
  V("W", "W", "Weight", "The force of gravity on it. 1 kg weighs 9.81 N.", kForce),
  V("m", "m", "Mass", "The same anywhere.", kMass),
  kVarG,
};
constexpr const char *weightE[] = {"W = m*g"};

constexpr Var fricV[] = {
  V("F", "F", "Friction force", "Largest it can be before sliding.", kForce),
  V("mu", "μ", "Friction coefficient", "Rubber on road 0.8, steel on steel 0.6, Teflon 0.04.", kNone, nullptr, kPositive, kPresetFriction),
  V("N", "N", "Normal force", "Pressing the surfaces together. On a level floor, the weight.", kForce),
};
constexpr const char *fricE[] = {"F = mu*N"};

constexpr Var inclV[] = {
  V("m", "m", "Mass", "On the slope.", kMass),
  V("theta", "θ", "Slope angle", "From level.", kAngle, "deg", kPositive | kAcute),
  V("Fp", "F_{p}", "Force down the slope", "What you hold back to keep it still.", kForce),
  V("N", "N", "Normal force", "Square to the slope.", kForce),
  kVarG,
};
constexpr const char *inclE[] = {"Fp = m*g*sin(theta)", "N = m*g*cos(theta)", "tan(theta) = Fp/N"};

constexpr Var inclaV[] = {
  V("a", "a", "Acceleration", "Down the slope.", kAcceleration),
  V("theta", "θ", "Slope angle", "Steep enough to slide: above atan(μ).", kAngle, "deg", kPositive | kAcute),
  V("mu", "μ", "Friction coefficient", "Between block and slope.", kNone, nullptr, kPositive, kPresetFriction),
  kVarG,
};
constexpr const char *inclaE[] = {"a = g*(sin(theta) - mu*cos(theta))"};

constexpr Var workV[] = {
  S("W", "W", "Work", "Energy moved by the force.", kEnergy),
  V("F", "F", "Force", "Along the rope or handle.", kForce),
  V("d", "d", "Distance", "Moved.", kLength),
  V("theta", "θ", "Angle", "Between force and motion. 0 if straight along.", kAngle, "deg", kAcute),
};
constexpr const char *workE[] = {"W = F*d*cos(theta)"};

constexpr Var keV[] = {
  V("E", "E", "Kinetic energy", "A 1500 kg car at 30 m/s has 675 kJ.", kEnergy),
  V("m", "m", "Mass", "Of the moving thing.", kMass),
  V("v", "v", "Speed", "How fast it moves.", kSpeed),
};
constexpr const char *keE[] = {"E = m*v^2/2"};

constexpr Var peV[] = {
  V("E", "E", "Potential energy", "Gained by lifting.", kEnergy),
  V("m", "m", "Mass", "What is lifted.", kMass),
  V("h", "h", "Height", "Raised.", kLength),
  kVarG,
};
constexpr const char *peE[] = {"E = m*g*h"};

constexpr Var powV[] = {
  V("P", "P", "Power", "Work per second. A person sustains about 100 W.", kPower),
  V("W", "W", "Work", "Energy delivered.", kEnergy),
  V("t", "t", "Time", "Taken.", kTime),
};
constexpr const char *powE[] = {"P = W/t"};

constexpr Var powfV[] = {
  V("P", "P", "Power", "Pushing at that speed. 1 hp is 746 W.", kPower, "hp"),
  V("F", "F", "Force", "Along the motion.", kForce),
  V("v", "v", "Speed", "Steady speed.", kSpeed),
};
constexpr const char *powfE[] = {"P = F*v"};

constexpr Var momV[] = {
  S("p", "p", "Momentum", "Mass times speed.", kMomentum),
  V("m", "m", "Mass", "Of the moving thing.", kMass),
  S("v", "v", "Velocity", "With its direction as a sign.", kSpeed),
};
constexpr const char *momE[] = {"p = m*v"};

constexpr Var impV[] = {
  V("F", "F", "Average force", "During the push.", kForce),
  V("t", "t", "Time", "That the force acts. A bat touches a ball for 1 ms.", kTime, "ms"),
  V("m", "m", "Mass", "Of the thing pushed.", kMass),
  V("dv", "Δv", "Change of speed", "From the push.", kSpeed),
};
constexpr const char *impE[] = {"F*t = m*dv"};

constexpr Var inelV[] = {
  V("m1", "m_{1}", "Mass 1", "First body.", kMass),
  S("u1", "u_{1}", "Speed 1", "Before. Opposite directions have opposite signs.", kSpeed),
  V("m2", "m_{2}", "Mass 2", "Second body.", kMass),
  S("u2", "u_{2}", "Speed 2", "Before.", kSpeed),
  S("v", "v", "Speed after", "Of the two stuck together.", kSpeed),
};
constexpr const char *inelE[] = {"v = (m1*u1 + m2*u2)/(m1 + m2)", "m1*u1 + m2*u2 = (m1 + m2)*v"};

constexpr Var hookeV[] = {
  S("F", "F", "Force", "Stretching the spring.", kForce),
  V("k", "k", "Spring rate", "A pen spring about 500 N/m, a car spring 30 N/mm.", kStiffness),
  S("x", "x", "Stretch", "From the spring's free length.", kLength, "mm"),
};
constexpr const char *hookeE[] = {"F = k*x"};

constexpr Var sprEV[] = {
  V("E", "E", "Stored energy", "In the stretched spring.", kEnergy),
  V("k", "k", "Spring rate", "Force per length.", kStiffness),
  V("x", "x", "Stretch", "From the free length.", kLength, "mm"),
};
constexpr const char *sprEE[] = {"E = k*x^2/2"};

constexpr Var sprTV[] = {
  V("T", "T", "Period", "One full bounce.", kTime),
  V("m", "m", "Mass", "On the spring.", kMass),
  V("k", "k", "Spring rate", "Force per length.", kStiffness),
};
constexpr const char *sprTE[] = {"T = 2*pi*sqrt(m/k)"};

constexpr Var pendV[] = {
  V("T", "T", "Period", "One full swing, there and back. A 1 m pendulum takes 2 s.", kTime),
  V("L", "L", "Length", "Pivot to the bob's centre.", kLength),
  kVarG,
};
constexpr const char *pendE[] = {"T = 2*pi*sqrt(L/g)"};

constexpr Var centV[] = {
  V("F", "F", "Centripetal force", "Pulling towards the centre.", kForce),
  V("m", "m", "Mass", "Going round.", kMass),
  V("v", "v", "Speed", "Along the circle.", kSpeed),
  V("r", "r", "Radius", "Of the circle.", kLength),
};
constexpr const char *centE[] = {"F = m*v^2/r"};

constexpr Var centaV[] = {
  V("a", "a", "Acceleration", "Towards the centre.", kAcceleration),
  V("v", "v", "Speed", "Along the circle.", kSpeed),
  V("r", "r", "Radius", "Of the circle.", kLength),
  V("w", "ω", "Angular speed", "How fast it goes round.", kAngularSpeed, "rpm"),
};
constexpr const char *centaE[] = {"a = v^2/r", "v = w*r", "a = w^2*r"};

constexpr Formula kForcesTable[] = {
  formula("fma", "Newton's second law", G2, "f=ma f ma m a force mass acceleration",
          "Force is mass times acceleration.", fmaV, fmaE, 2, dBlock),
  formula("weight", "Weight", G2, "gravity mass newtons planet",
          "The force of gravity on a mass.", weightV, weightE, 1, dWeight),
  formula("friction", "Friction", G2, "coulomb sliding grip static kinetic mu",
          "The most friction a contact can give, from how hard it is pressed.", fricV, fricE, 2, dFriction),
  formula("incline", "Forces on a slope", G2, "inclined plane ramp hill normal",
          "Gravity split into down the slope and into the slope.", inclV, inclE, 2, dIncline),
  formula("incline-slide", "Sliding down a slope", G2, "inclined plane ramp friction acceleration",
          "How fast something slides down, against friction.", inclaV, inclaE, 2, dIncline2),
  formula("work", "Work", G2, "force distance energy joules",
          "Energy moved by a force over a distance.", workV, workE, 3, dWork),
  formula("kinetic", "Kinetic energy", G2, "ke motion moving speed",
          "Energy of motion, at everyday speeds.", keV, keE, 2, dMoving),
  formula("potential", "Potential energy", G2, "pe gravitational height mgh lifted",
          "Energy gained by raising a mass, near a planet's surface.", peV, peE, 2, dDrop),
  formula("power", "Power from work", G2, "mechanical watts rate energy time",
          "How fast work is done.", powV, powE, 2, dWork),
  formula("power-force", "Power from force and speed", G2, "mechanical watts drive horsepower",
          "The power to keep a force moving at a speed.", powfV, powfE, 2, dMoving),
  formula("momentum", "Momentum", G2, "mass velocity p mv",
          "Mass times velocity; it survives collisions.", momV, momE, 2, dMoving),
  formula("impulse", "Impulse", G2, "force time change momentum crash",
          "A force over a time changes momentum.", impV, impE, 3, dBlock),
  formula("collision", "Collision, stuck together", G2, "inelastic momentum conservation crash",
          "Two bodies that collide and stay together.", inelV, inelE, 4, dCollision),
  formula("hooke", "Hooke's law", G2, "spring rate stiffness constant stretch",
          "A spring pushes back in proportion to its stretch.", hookeV, hookeE, 2, dSpring),
  formula("spring-energy", "Spring energy", G2, "elastic potential stored",
          "Energy held in a stretched or squeezed spring.", sprEV, sprEE, 2, dSpring),
  formula("spring-period", "Mass on a spring", G2, "oscillation bounce frequency shm",
          "How long a mass takes to bounce once on a spring.", sprTV, sprTE, 2, dSpring),
  formula("pendulum", "Pendulum", G2, "period swing clock bob length",
          "Time for one swing, for small swings.", pendV, pendE, 1, dPendulum),
  formula("centripetal", "Centripetal force", G2, "circular motion mass rope turn",
          "The inward pull that keeps a mass on a circle.", centV, centE, 3, dCircular),
  formula("centripetal-a", "Centripetal acceleration", G2, "circular motion spin g force centrifuge",
          "Acceleration of something going round at steady speed.", centaV, centaE, 2, dCircular),
};

// ---- Rotation & machines ----

constexpr Var angV[] = {
  V("w", "ω", "Angular speed", "A 7200 rpm disk spins at 754 rad/s.", kAngularSpeed, "rpm"),
  V("f", "f", "Frequency", "Turns per second.", kFrequency),
  V("T", "T", "Period", "Time for a turn.", kTime),
};
constexpr const char *angE[] = {"w = 2*pi*f", "T = 1/f", "w = 2*pi/T"};

constexpr Var rimV[] = {
  V("v", "v", "Rim speed", "Of a point on the wheel.", kSpeed),
  V("w", "ω", "Angular speed", "Of the wheel.", kAngularSpeed, "rpm"),
  V("r", "r", "Radius", "Of the wheel.", kLength),
};
constexpr const char *rimE[] = {"v = w*r"};

constexpr Var torqV[] = {
  S("T", "T", "Torque", "Twisting force. A hand on a wrench gives about 50 N-m.", kTorque),
  S("F", "F", "Force", "Square to the arm.", kForce),
  V("r", "r", "Arm length", "From the pivot to the force.", kLength),
};
constexpr const char *torqE[] = {"T = F*r"};

constexpr Var rkeV[] = {
  V("E", "E", "Rotational energy", "Stored in the spin. How a flywheel stores energy.", kEnergy),
  V("I", "I", "Moment of inertia", "Of the spinning body.", kInertia),
  V("w", "ω", "Angular speed", "Of the spin.", kAngularSpeed, "rpm"),
};
constexpr const char *rkeE[] = {"E = I*w^2/2"};

constexpr Var idiscV[] = {
  V("I", "I", "Moment of inertia", "About the axle through the centre.", kInertia),
  V("m", "m", "Mass", "Of the disc.", kMass),
  V("r", "r", "Radius", "Of the disc.", kLength),
};
constexpr const char *idiscE[] = {"I = m*r^2/2"};
constexpr const char *isphereE[] = {"I = 2*m*r^2/5"};

constexpr Var irodV[] = {
  V("I", "I", "Moment of inertia", "About the middle, square to the rod.", kInertia),
  V("m", "m", "Mass", "Of the rod.", kMass),
  V("L", "L", "Length", "Of the rod.", kLength),
};
constexpr const char *irodE[] = {"I = m*L^2/12"};

constexpr Var shaftV[] = {
  V("P", "P", "Power", "Through the shaft. 1 hp is 746 W.", kPower, "kW"),
  V("T", "T", "Torque", "In the shaft.", kTorque),
  V("w", "ω", "Shaft speed", "An engine idles near 800 rpm.", kAngularSpeed, "rpm"),
};
constexpr const char *shaftE[] = {"P = T*w"};

constexpr Var gearV[] = {
  V("N1", "N_{1}", "Teeth, driver", "On the input gear.", kNone, nullptr, kPositive | kInteger),
  V("N2", "N_{2}", "Teeth, driven", "On the output gear.", kNone, nullptr, kPositive | kInteger),
  V("w1", "ω_{1}", "Speed, driver", "Of the input gear.", kAngularSpeed, "rpm"),
  V("w2", "ω_{2}", "Speed, driven", "Of the output gear.", kAngularSpeed, "rpm"),
};
constexpr const char *gearE[] = {"w1*N1 = w2*N2"};

constexpr Var geartV[] = {
  V("N1", "N_{1}", "Teeth, driver", "On the input gear.", kNone, nullptr, kPositive | kInteger),
  V("N2", "N_{2}", "Teeth, driven", "On the output gear.", kNone, nullptr, kPositive | kInteger),
  V("T1", "T_{1}", "Torque in", "On the input shaft.", kTorque),
  V("T2", "T_{2}", "Torque out", "On the output shaft, ignoring losses.", kTorque),
};
constexpr const char *geartE[] = {"T2/T1 = N2/N1"};

constexpr Var pulV[] = {
  V("d1", "d_{1}", "Driver diameter", "Of the pulley on the motor.", kLength, "mm"),
  V("d2", "d_{2}", "Driven diameter", "Of the other pulley.", kLength, "mm"),
  V("w1", "ω_{1}", "Driver speed", "A 4-pole motor runs near 1750 rpm.", kAngularSpeed, "rpm"),
  V("w2", "ω_{2}", "Driven speed", "Of the other pulley.", kAngularSpeed, "rpm"),
};
constexpr const char *pulE[] = {"w1*d1 = w2*d2"};

constexpr Var beltV[] = {
  V("v", "v", "Belt speed", "Or surface speed. V-belts stay below 30 m/s.", kSpeed),
  V("d", "d", "Pulley diameter", "Or the work's diameter on a lathe.", kLength, "mm"),
  V("w", "ω", "Speed", "Of the pulley or spindle.", kAngularSpeed, "rpm"),
};
constexpr const char *beltE[] = {"v = w*d/2"};

constexpr Var beltLV[] = {
  V("L", "L", "Belt length", "Round the two pulleys.", kLength, "mm"),
  V("C", "C", "Centre distance", "Between the shafts.", kLength, "mm"),
  V("D", "D", "Large pulley", "Diameter.", kLength, "mm"),
  V("d", "d", "Small pulley", "Diameter.", kLength, "mm"),
};
constexpr const char *beltLE[] = {"L = 2*C + pi*(D + d)/2 + (D - d)^2/(4*C)"};

constexpr Var leverV[] = {
  V("F1", "F_{1}", "Effort", "What you push with.", kForce),
  V("d1", "d_{1}", "Effort arm", "Pivot to your hand.", kLength),
  V("F2", "F_{2}", "Load", "What gets moved.", kForce),
  V("d2", "d_{2}", "Load arm", "Pivot to the load.", kLength),
};
constexpr const char *leverE[] = {"F1*d1 = F2*d2"};

constexpr Var blockV[] = {
  V("F", "F", "Pull", "On the rope, ignoring friction.", kForce),
  V("W", "W", "Load", "What is lifted.", kForce),
  V("n", "n", "Supporting ropes", "Rope strands holding the moving block.", kNone, nullptr, kPositive | kInteger),
};
constexpr const char *blockE[] = {"W = n*F"};

constexpr Var boltV[] = {
  V("T", "T", "Tightening torque", "On the wrench.", kTorque),
  K("K", "K", "Nut factor", "0.2 for dry steel, 0.15 for oiled.", kNone, 0.2, nullptr,
    "Dry steel=0.2|Zinc plated=0.2|Oiled=0.15|Anti-seize=0.12"),
  V("d", "d", "Bolt diameter", "Nominal: M10 is 10 mm.", kLength, "mm"),
  V("F", "F", "Clamp force", "Tension in the bolt.", kForce, "kN"),
};
constexpr const char *boltE[] = {"T = K*d*F"};

constexpr Formula kRotationTable[] = {
  formula("rpm", "Angular speed", G3, "rad/s frequency rpm hertz period omega convert",
          "Three ways of saying how fast something turns.", angV, angE, 1, dDisc),
  formula("rim-speed", "Rim speed", G3, "tangential peripheral wheel circumference",
          "How fast the edge of a wheel travels.", rimV, rimE, 2, dDisc),
  formula("torque", "Torque", G3, "moment wrench twist lever arm",
          "A force at a distance from a pivot.", torqV, torqE, 2, dWrench),
  formula("rot-energy", "Rotational energy", G3, "kinetic flywheel spin",
          "Energy stored in something spinning.", rkeV, rkeE, 2, dDisc),
  formula("inertia-disc", "Inertia of a disc", G3, "moment cylinder flywheel solid",
          "A solid disc or cylinder about its axle.", idiscV, idiscE, 2, dDisc),
  formula("inertia-sphere", "Inertia of a ball", G3, "moment solid sphere",
          "A solid ball about a line through its centre.", idiscV, isphereE, 2, dDisc),
  formula("inertia-rod", "Inertia of a rod", G3, "moment bar stick",
          "A thin rod about its middle.", irodV, irodE, 2, dRodSpin),
  formula("shaft-power", "Shaft power", G3, "torque rpm horsepower engine motor kw",
          "Power through a turning shaft, from torque and speed.", shaftV, shaftE, 2, dShaft),
  formula("gears", "Gear speeds", G3, "ratio teeth reduction gearbox",
          "How teeth set the speed of meshing gears.", gearV, gearE, 3, dGears),
  formula("gear-torque", "Gear torque", G3, "ratio teeth reduction gearbox",
          "What gears lose in speed they gain in torque, less friction.", geartV, geartE, 3, dGears),
  formula("pulleys", "Pulley speeds", G3, "belt drive ratio sheave",
          "How two pulleys on a belt set each other's speed.", pulV, pulE, 3, dBeltDrive),
  formula("belt-speed", "Belt or cutting speed", G3, "surface sfm lathe drill spindle linear",
          "Surface speed of a pulley or a part on a lathe.", beltV, beltE, 2, dBeltSpeed),
  formula("belt-length", "Belt length", G3, "pulley open drive centre distance",
          "Length of a belt round two pulleys.", beltLV, beltLE, 3, dBeltLength),
  formula("lever", "Lever", G3, "fulcrum see-saw moment mechanical advantage balance",
          "Force times distance balances on each side of a pivot.", leverV, leverE, 3, dLever),
  formula("block-tackle", "Block and tackle", G3, "pulley rope mechanical advantage hoist",
          "Each supporting rope shares the load.", blockV, blockE, 2, dBlockTackle),
  formula("bolt-torque", "Bolt torque", G3, "preload clamp tension tighten",
          "How much clamp a tightening torque gives: T = KdF. An estimate, good to about 25%.",
          boltV, boltE, 2, dBolt),
};

}  // namespace

Part motionPart() { return partOf(kMotionTable); }
Part forcesPart() { return partOf(kForcesTable); }
Part rotationPart() { return partOf(kRotationTable); }

}  // namespace lib
}  // namespace formulas
}  // namespace mt
