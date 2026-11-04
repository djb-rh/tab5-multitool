// The library: Chemistry, Geometry.
#include "formula_diagrams.h"
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {
namespace {

using namespace units;
constexpr uint8_t G1 = lib::kChemistry;
constexpr uint8_t G2 = lib::kGeometry;

// ---- Chemistry ----

constexpr Var moleV[] = {
  V("n", "n", "Amount", "In moles. 18 g of water is a mole.", kAmount),
  V("m", "m", "Mass", "Of the sample.", kMass, "g"),
  V("M", "M", "Molar mass", "From the formula. Water 18.015, NaCl 58.44 g/mol.", kMolarMass, "g/mol", kPositive,
    "Water=0.018015|Salt NaCl=0.05844|CO2=0.04401|Glucose=0.18016|Ethanol=0.04607|Oxygen O2=0.032|Iron=0.055845"),
};
constexpr const char *moleE[] = {"n = m/M"};

constexpr Var molarV[] = {
  V("c", "c", "Concentration", "Seawater salt is about 0.6 mol/L.", kMolarity),
  V("n", "n", "Amount", "Of what is dissolved.", kAmount, "mmol"),
  V("V", "V", "Volume", "Of the solution.", kVolume, "L"),
};
constexpr const char *molarE[] = {"c = n/V"};

constexpr Var dilV[] = {
  V("c1", "c_{1}", "Stock strength", "Before diluting.", kMolarity),
  V("V1", "V_{1}", "Stock volume", "Taken from the bottle.", kVolume, "mL"),
  V("c2", "c_{2}", "Final strength", "After diluting.", kMolarity),
  V("V2", "V_{2}", "Final volume", "Including the water added.", kVolume, "mL"),
};
constexpr const char *dilE[] = {"c1*V1 = c2*V2"};

constexpr Var phV[] = {
  S("pH", "pH", "pH", "7 is neutral, lemon juice about 2, bleach 12.", kNone),
  V("H", "[H+]", "Hydrogen ions", "Concentration in the solution.", kMolarity),
};
constexpr const char *phE[] = {"pH = -log10(H/c0)"};

constexpr Var arrV[] = {
  V("k", "k", "Rate constant", "First-order, per second.", kFrequency),
  V("A", "A", "Pre-exponential factor", "Same units as k. Often about 1e13.", kFrequency),
  V("Ea", "E_{a}", "Activation energy", "The barrier. Typically 50 kJ/mol.", kMolarEnergy, "kJ/mol"),
  V("T", "T", "Temperature", "Absolute.", kTemperature, "C"),
};
constexpr const char *arrE[] = {"k = A*exp(-Ea/(Rgas*T))"};

constexpr Var hlV[] = {
  V("N", "N", "Left", "After time t, in any units of amount.", kNone),
  V("N0", "N_{0}", "At the start", "Same units as N.", kNone),
  V("t", "t", "Time", "Elapsed.", kTime, "d"),
  V("th", "t_{1/2}", "Half-life", "Carbon-14 5730 years, iodine-131 8 days.", kTime, "d"),
};
constexpr const char *hlE[] = {"N = N0*0.5^(t/th)"};

constexpr Var actV[] = {
  V("A", "A", "Activity", "Decays per second. A banana: about 15 Bq.", kRadioactivity),
  V("lam", "λ", "Decay constant", "Chance of decay per second.", kFrequency),
  V("N", "N", "Atoms", "How many are left.", kNone),
};
constexpr const char *actE[] = {"A = lam*N"};

constexpr Var nernstV[] = {
  S("E", "E", "Cell voltage", "Under these conditions.", kVoltage),
  S("E0", "E°", "Standard voltage", "Daniell cell 1.10 V.", kVoltage),
  V("T", "T", "Temperature", "Absolute.", kTemperature, "C"),
  V("z", "z", "Electrons", "Moved per reaction.", kNone, nullptr, kPositive | kInteger),
  V("Q", "Q", "Reaction quotient", "1 at standard conditions.", kNone),
};
constexpr const char *nernstE[] = {"E = E0 - Rgas*T/(z*F)*ln(Q)"};

constexpr Var elecV[] = {
  V("m", "m", "Mass plated", "At the electrode.", kMass, "g"),
  V("Q", "Q", "Charge passed", "Current times time.", kCharge),
  V("M", "M", "Molar mass", "Copper 63.55 g/mol.", kMolarMass, "g/mol"),
  V("z", "z", "Charge of the ion", "Cu2+ is 2.", kNone, nullptr, kPositive | kInteger),
};
constexpr const char *elecE[] = {"m = Q*M/(z*F)"};

constexpr Formula kChemistryTable[] = {
  formula("moles", "Moles", G1, "mass molar amount substance grams",
          "Mass to moles and back.", moleV, moleE, 2, dScale),
  formula("molarity", "Molarity", G1, "concentration solution mol/l",
          "Moles dissolved per volume.", molarV, molarE, 2, dBeaker),
  formula("dilution", "Dilution", G1, "c1v1 c2v2 concentration stock water",
          "Water changes the strength, not the amount.", dilV, dilE, 3, dDilution),
  formula("ph", "pH", G1, "acid base hydrogen ion acidity",
          "Acidity on a log scale.", phV, phE, 1, dPh),
  formula("arrhenius", "Arrhenius equation", G1, "reaction rate activation energy temperature kinetics",
          "How reactions speed up with temperature.", arrV, arrE, 3, dBarrier),
  formula("half-life", "Half-life", G1, "radioactive decay carbon dating exponential",
          "What is left after a time.", hlV, hlE, 3, dDecay),
  formula("activity", "Radioactivity", G1, "becquerel curie decay",
          "How many atoms decay each second.", actV, actE, 2, dDecay),
  formula("nernst", "Nernst equation", G1, "electrochemistry cell voltage battery",
          "Cell voltage away from standard conditions.", nernstV, nernstE, 4, dCell),
  formula("electrolysis", "Electrolysis", G1, "faraday plating mass deposit",
          "How much metal a charge plates out.", elecV, elecE, 3, dCell),
};

// ---- Geometry ----

constexpr Var circV[] = {
  V("r", "r", "Radius", "Centre to edge.", kLength, "cm"),
  V("d", "d", "Diameter", "Edge to edge through the centre.", kLength, "cm"),
  V("C", "C", "Circumference", "Distance round.", kLength, "cm"),
  V("A", "A", "Area", "Inside.", kArea, "cm^2"),
};
constexpr const char *circE[] = {"d = 2*r", "C = 2*pi*r", "A = pi*r^2"};

constexpr Var arcV[] = {
  V("r", "r", "Radius", "Of the circle.", kLength, "cm"),
  V("theta", "θ", "Angle", "At the centre.", kAngle, "deg"),
  V("s", "s", "Arc length", "Along the curve.", kLength, "cm"),
  V("c", "c", "Chord", "Straight across the ends.", kLength, "cm"),
  V("A", "A", "Sector area", "The pie slice.", kArea, "cm^2"),
};
constexpr const char *arcE[] = {
  "s = r*theta", "A = r^2*theta/2", "c = 2*r*sin(theta/2)", "c = 2*s/theta*sin(theta/2)",
  "c = 2*sqrt(2*A/theta)*sin(theta/2)", "A = s*r/2",
};

constexpr Var sphV[] = {
  V("r", "r", "Radius", "Centre to surface.", kLength, "cm"),
  V("V", "V", "Volume", "Inside.", kVolume, "cm^3"),
  V("A", "A", "Surface area", "Outside.", kArea, "cm^2"),
};
constexpr const char *sphE[] = {"V = 4*pi*r^3/3", "A = 4*pi*r^2"};

constexpr Var cylV[] = {
  V("r", "r", "Radius", "Of the end.", kLength, "cm"),
  V("h", "h", "Height", "Between the ends.", kLength, "cm"),
  V("V", "V", "Volume", "Inside.", kVolume, "cm^3"),
  V("A", "A", "Surface area", "Including both ends.", kArea, "cm^2"),
};
constexpr const char *cylE[] = {"V = pi*r^2*h", "A = 2*pi*r*(r + h)", "A = 2*pi*r^2 + 2*V/r"};

constexpr Var coneV[] = {
  V("r", "r", "Base radius", "Of the circle.", kLength, "cm"),
  V("h", "h", "Height", "Tip to base, square on.", kLength, "cm"),
  V("s", "s", "Slant height", "Tip to the rim.", kLength, "cm"),
  V("V", "V", "Volume", "Inside.", kVolume, "cm^3"),
  V("A", "A", "Surface area", "Including the base.", kArea, "cm^2"),
};
constexpr const char *coneE[] = {
  "s = sqrt(r^2 + h^2)", "V = pi*r^2*h/3", "A = pi*r*(r + s)", "V = pi*(s^2 - h^2)*h/3",
  "A = pi*r*(r + sqrt(r^2 + h^2))", "A = pi*r^2 + pi*r*sqrt(r^2 + (3*V/(pi*r^2))^2)",
  "V = pi*r^2*sqrt(s^2 - r^2)/3",
};

constexpr Var rectV[] = {
  V("w", "w", "Width", "One side.", kLength, "cm"),
  V("h", "h", "Height", "The other side.", kLength, "cm"),
  V("A", "A", "Area", "Inside.", kArea, "cm^2"),
  V("P", "P", "Perimeter", "Distance round.", kLength, "cm"),
  V("d", "d", "Diagonal", "Corner to corner.", kLength, "cm"),
};
constexpr const char *rectE[] = {
  "A = w*h", "P = 2*(w + h)", "d = sqrt(w^2 + h^2)", "P = 2*(w + A/w)", "d^2 = w^2 + (A/w)^2",
  "d^2 = w^2 + (P/2 - w)^2",
};

constexpr Var boxV[] = {
  V("l", "l", "Length", "One edge.", kLength, "cm"),
  V("w", "w", "Width", "Another edge.", kLength, "cm"),
  V("h", "h", "Height", "The third edge.", kLength, "cm"),
  V("V", "V", "Volume", "Inside.", kVolume, "cm^3"),
  V("A", "A", "Surface area", "All six faces.", kArea, "cm^2"),
};
constexpr const char *boxE[] = {
  "V = l*w*h", "A = 2*(l*w + l*h + w*h)", "A = 2*(l*w + V/w + V/l)", "A = 2*(l*h + V/h + V/l)",
  "A = 2*(w*h + V/h + V/w)",
};

constexpr Var triV[] = {
  V("A", "A", "Area", "Inside.", kArea, "cm^2"),
  V("b", "b", "Base", "Any side.", kLength, "cm"),
  V("h", "h", "Height", "From that side to the far corner, square on.", kLength, "cm"),
};
constexpr const char *triE[] = {"A = b*h/2"};

constexpr Var pythV[] = {
  V("a", "a", "Side a", "One short side.", kLength, "cm"),
  V("b", "b", "Side b", "The other short side.", kLength, "cm"),
  V("c", "c", "Hypotenuse", "The long side, opposite the right angle.", kLength, "cm"),
};
constexpr const char *pythE[] = {"c = sqrt(a^2 + b^2)"};

constexpr Var rtV[] = {
  V("a", "a", "Opposite", "Across from angle A.", kLength, "cm"),
  V("b", "b", "Adjacent", "Next to angle A.", kLength, "cm"),
  V("c", "c", "Hypotenuse", "The long side.", kLength, "cm"),
  V("A", "A", "Angle A", "Between b and c.", kAngle, "deg", kPositive | kAcute),
  V("B", "B", "Angle B", "Between a and c.", kAngle, "deg", kPositive | kAcute),
};
constexpr const char *rtE[] = {
  "c = sqrt(a^2 + b^2)", "a = c*sin(A)", "b = c*cos(A)", "tan(A) = a/b", "A + B = pi/2",
  "b = c*sin(B)", "a = c*cos(B)", "tan(B) = b/a",
};

constexpr Var lcV[] = {
  V("c", "c", "Side c", "Opposite the angle C.", kLength, "cm"),
  V("a", "a", "Side a", "One side next to C.", kLength, "cm"),
  V("b", "b", "Side b", "The other side next to C.", kLength, "cm"),
  V("C", "C", "Angle C", "Between a and b.", kAngle, "deg"),
};
constexpr const char *lcE[] = {"c^2 = a^2 + b^2 - 2*a*b*cos(C)"};

constexpr Var tsV[] = {
  V("a", "a", "Side a", "Opposite A.", kLength, "cm"),
  V("b", "b", "Side b", "Opposite B.", kLength, "cm"),
  V("c", "c", "Side c", "Opposite C.", kLength, "cm"),
  V("A", "A", "Angle A", "Opposite a.", kAngle, "deg"),
  V("B", "B", "Angle B", "Opposite b.", kAngle, "deg"),
  V("C", "C", "Angle C", "Opposite c.", kAngle, "deg"),
};
constexpr const char *tsE[] = {
  "A + B + C = pi", "a^2 = b^2 + c^2 - 2*b*c*cos(A)", "b^2 = a^2 + c^2 - 2*a*c*cos(B)",
  "c^2 = a^2 + b^2 - 2*a*b*cos(C)", "a/sin(A) = b/sin(B)", "a/sin(A) = c/sin(C)", "b/sin(B) = c/sin(C)",
};

constexpr Var slopeV[] = {
  V("rise", "rise", "Rise", "Up.", kLength),
  V("run", "run", "Run", "Along the level.", kLength),
  V("G", "G", "Grade", "Rise over run. A 1 in 12 ramp is 8.3%.", kRatio, "%"),
  V("theta", "θ", "Angle", "From level.", kAngle, "deg", kPositive | kAcute),
};
constexpr const char *slopeE[] = {"G = rise/run", "tan(theta) = rise/run", "tan(theta) = G"};

constexpr Var quadV[] = {
  S("x", "x", "Root", "Where the curve crosses zero.", kNone),
  S("a", "a", "x^2 coefficient", "Not zero.", kNone),
  S("b", "b", "x coefficient", "The middle term.", kNone),
  S("c", "c", "Constant", "The last term.", kNone),
};
constexpr const char *quadE[] = {"a*x^2 + b*x + c = 0"};

constexpr Formula kGeometryTable[] = {
  formula("circle", "Circle", G2, "radius diameter circumference area pi round",
          "Any one measurement gives the rest.", circV, circE, 1, dCircle),
  formula("arc", "Arc and sector", G2, "chord radians pie slice bend",
          "A slice of a circle.", arcV, arcE, 2, dArc),
  formula("sphere", "Sphere", G2, "ball volume surface area globe",
          "Any one measurement gives the rest.", sphV, sphE, 1, dSphere),
  formula("cylinder", "Cylinder", G2, "volume tank can tube surface",
          "A round can, ends included.", cylV, cylE, 2, dCylinder),
  formula("cone", "Cone", G2, "volume slant funnel surface",
          "A round cone, base included.", coneV, coneE, 2, dCone),
  formula("rectangle", "Rectangle", G2, "area perimeter diagonal square",
          "Any two measurements give the rest.", rectV, rectE, 2, dRectangle),
  formula("box", "Box", G2, "cuboid volume surface area rectangular prism",
          "A rectangular box.", boxV, boxE, 3, dBox),
  formula("triangle-area", "Triangle area", G2, "base height half",
          "Half the base times the height.", triV, triE, 2, dTriangleH),
  formula("pythagoras", "Pythagoras", G2, "right triangle hypotenuse a2 b2 c2",
          "The sides of a right triangle.", pythV, pythE, 2, dRightTri),
  formula("right-triangle", "Right-triangle trig", G2, "sin cos tan soh cah toa angle",
          "Sides and angles of a right triangle. Any two, as long as one is a side.",
          rtV, rtE, 2, dRightTri),
  formula("law-cosines", "Law of cosines", G2, "triangle side angle sas",
          "A third side from two sides and the angle between.", lcV, lcE, 3, dTriangleAng),
  formula("triangle", "Any triangle", G2, "solve sides angles sss sas asa law of sines cosines",
          "Any three, as long as one is a side.", tsV, tsE, 3, dTriangleAng),
  formula("slope", "Slope and grade", G2, "gradient ramp pitch rise run percent angle",
          "A slope as a ratio, a percentage or an angle.", slopeV, slopeE, 2, dSlope),
  formula("quadratic", "Quadratic equation", G2, "roots polynomial parabola",
          "Solves a x^2 + b x + c = 0.", quadV, quadE, 3, dParabola),
};

}  // namespace

Part chemistryPart() { return partOf(kChemistryTable); }
Part geometryPart() { return partOf(kGeometryTable); }

}  // namespace lib
}  // namespace formulas
}  // namespace mt
