// The Formulas app's engine: a library of scientific formulas, and a solver that
// finds whichever variables are left blank. Plain C++17 and the standard library
// only (no Arduino, M5 or display code, no exceptions, no RTTI) so the same files
// are tested on the Mac: test/formulas_test.sh
//
// A formula is a few variables and one or more equations relating them, as text
// ("V = I*R", "P = V^2/R"). Everything else comes from the equations: an equation
// with one unknown is rearranged by inverting its expression tree (or solved
// numerically when the unknown appears more than once), solved values feed the
// next equation, and the same tree is what the app typesets. Redundant equations
// are normal: Ohm's law carries all of V=IR, P=VI, P=I^2R, P=V^2/R so that any two
// of the four determine the rest.
//
// All solver values are in SI units for the variable's dimension. Units come from
// units.h, plus the extra dimensions below that the Units app does not list.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "units.h"

namespace mt {
namespace formulas {

// ---- Dimensions ----

// 0 .. units::kCatCount-1 are the Units app's categories; these follow them.
enum Dim : uint8_t {
  kNone = units::kCatCount,  // a pure number: no unit button
  kRatio,          // ratio, %, ppm
  kDecibel,        // dB (a pure number with a name)
  kTempDiff,       // a temperature DIFFERENCE: K, C deg, F deg (no offset)
  kCharge,         // C, mC, uC, nC, Ah, mAh
  kInductance,     // H, mH, uH, nH
  kMagField,       // T, mT, uT, G
  kMagFlux,        // Wb, mWb
  kConductance,    // S, mS, uS
  kResistivity,    // ohm-m, ohm-cm, uohm-cm
  kAngularSpeed,   // rad/s, deg/s, rpm, rev/s
  kAngularAccel,   // rad/s^2, deg/s^2
  kStiffness,      // N/m, N/mm, lbf/in
  kMomentum,       // kg-m/s, N-s
  kInertia,        // kg-m^2, g-cm^2, lb-ft^2
  kAreaMoment,     // m^4, mm^4, cm^4, in^4 (second moment of area)
  kSpecificHeat,   // J/(kg-K), kJ/(kg-K), cal/(g-C), BTU/(lb-F)
  kThermalCond,    // W/(m-K), BTU/(hr-ft-F)
  kThermalRes,     // K/W
  kHeatTransfer,   // W/(m^2-K)
  kExpansion,      // 1/K, 1/F, ppm/K
  kViscosity,      // Pa-s, mPa-s, cP, P
  kKinViscosity,   // m^2/s, cSt, St
  kAmount,         // mol, mmol, umol
  kMolarity,       // mol/L, mmol/L, mol/m^3  (SI unit here: mol/m^3)
  kMolarMass,      // g/mol, kg/mol           (SI unit here: kg/mol)
  kElectricField,  // V/m, kV/m, V/mm
  kIntensity,      // W/m^2, mW/cm^2
  kIlluminance,    // lux, foot-candle
  kLuminousFlux,   // lumen
  kRadioactivity,  // Bq, kBq, Ci, mCi
  kSpecificEnergy, // J/kg, kJ/kg, MJ/kg, BTU/lb (latent heat, heating value)
  kMolarEnergy,    // J/mol, kJ/mol, kcal/mol (activation energy)
  kLinearDensity,  // kg/m, g/m (a string's mass per length)
  kDimCount
};

struct DimInfo {
  const char *name;           // "Inductance"
  const units::Unit *units;   // the unit table (units.h's tables for its categories)
  uint8_t count;              // 0 for kNone
  uint8_t si;                 // index of the SI unit in the table
};
// nullptr when out of range.
const DimInfo *dim(int d);
// Between a unit of the dimension and SI. On a bad index or a non-finite result
// these return 0 and set *ok = false.
double toSI(double v, int d, int unit, bool *ok = nullptr);
double fromSI(double si, int d, int unit, bool *ok = nullptr);

// Reads a typed field whose current unit is (d, unit), giving the value in SI.
// Accepts arithmetic ("3/8", "2*pi", "12*2.54"), an SI prefix letter straight after
// the number, as engineers write it ("4.7k", "100n", "2.2u", "10M", "33p", "1G",
// "5m" = 5 milli), and a number with any unit of the dimension ("3 ft", "5 ft 3 in",
// "2.2 uF", "100 F"), which is then read in that unit instead of (d, unit).
// *usedUnit, if given, gets the typed unit's index, or -1 if none was typed.
// False if anything is not understood.
bool parseValue(const char *text, int d, int unit, double *si, int *usedUnit = nullptr);

// The unit in which an SI value reads best: from the kCommon units, the one that
// puts the number nearest 1..1000 (0.0033 A -> mA). Falls back to `fallback` for
// zero, kNone, a dimension with an offset unit (temperature) or an empty table.
int bestUnit(double si, int d, int fallback);
// value in the unit, 6 significant digits, plain ASCII number only.
std::string formatValue(double si, int d, int unit);

// ---- The library ----

// Var::flags bits.
enum : uint8_t {
  kPositive = 1,   // only positive values make sense; a solve giving <= 0 is an error
  kInteger = 2,    // a count (turns, teeth, moles of charge transferred)
  kConstant = 4,   // starts filled in with Var::preset (g, a material property);
                   // it stays until changed, and does not count as an entered value
  kAcute = 8,      // an angle from 0 to 90 deg: a solve outside that is an error, and
                   // the sin^-1 branch beyond 90 deg is not offered as a second solution
};

struct Var {
  const char *sym;     // identifier in the equations: "V", "R1", "theta" (ASCII)
  const char *label;   // shown symbol, UTF-8, subscript as _{}: "R_{1}", "θ", "ΔT", "V_{out}"
  const char *name;    // "Voltage"
  const char *help;    // one short line on what it is and typical values:
                       // "Across the resistor. A AA cell is 1.5 V, mains 120 V."
  uint8_t dim;         // Dim or units::Cat
  const char *unit;    // the unit to start in, by symbol ("uF", "nm", "deg"); nullptr = SI
  uint8_t flags;
  double preset;       // kConstant: the starting value, in SI
  // Handy values, in SI, or nullptr: "Copper=1.68e-8|Aluminium=2.65e-8|Nichrome=1.1e-6".
  const char *presets;
};

struct Group {
  const char *name;    // "Electrical"
  const char *blurb;   // "Ohm's law, power, R C L, AC"
  uint32_t color;      // RGB888 for the group's tab and accents
};

struct Formula {
  const char *id;       // stable short id, kept in NVS ("ohm")
  const char *name;     // "Ohm's law and power"
  uint8_t group;        // index into groups
  const char *keywords; // more search words, space separated: "voltage current watts"
  const char *blurb;    // one or two sentences: what it is for, what it assumes
  const Var *vars;      // at most kMaxVars
  uint8_t varCount;
  const char *const *equations;  // "lhs = rhs"; the first is the one shown by default
  uint8_t equationCount;
  uint8_t needed;       // how many entered values determine the rest (kConstant vars
                        // do not count)
  const char *diagram;  // drawing script (below), or nullptr
};

constexpr int kMaxVars = 8;

int groupCount();
const Group *group(int i);          // nullptr when out of range
int formulaCount();
const Formula *formula(int i);      // nullptr when out of range; grouped, best known first
int findFormula(const char *id);    // index, or -1
// Formula indices of a group, in order; returns how many were written.
int inGroup(int g, int *out, int max);
// The formulas to show first with nothing searched: Ohm's law, power and friends.
int quickList(int *out, int max);
// Ranked search over name, keywords, group name and variable names and symbols:
// whole-word and prefix matches on the name first. Every word of the query must
// match somewhere. "ohm", "watts", "cap reactance", "kinetic", "f=ma" style queries
// work ('=', '*', '/' are treated as spaces). Returns how many were written.
int search(const char *query, int *out, int max);

// ---- Expressions ----

// A parsed expression, for evaluating and for typesetting.
struct Node {
  enum Op : uint8_t { Num, Var, Const, Add, Sub, Mul, Div, Pow, Neg, Func } op = Num;
  double value = 0;          // Num, Const
  int var = -1;              // Var: index into Formula::vars
  const char *name = "";     // Const: shown symbol ("π", "c", "k_{B}"); Func: "sqrt", "sin"...
  const Node *a = nullptr;   // Add..Pow: left; Neg, Func: the argument
  const Node *b = nullptr;   // Add..Pow: right; Func with two arguments (atan2, log base)
};

// Named constants in equations: pi, c (speed of light), h, hbar, kB, NA, Rgas, qe, eps0,
// mu0, sigma (Stefan-Boltzmann), Gnewton, g0 (9.80665), F (Faraday). Functions: sqrt,
// cbrt, abs, exp, ln, log10, sin, cos, tan, asin, acos, atan, atan2(y,x), sinh, cosh, tanh.
// Variables shadow constants of the same name within a formula.
struct Constant {
  const char *sym;
  const char *label;
  const char *name;
  double value;
};
int constantCount();
const Constant *constant(int i);

// ---- Solving ----

enum class Cell : uint8_t {
  Empty,     // no value
  Entered,   // typed by the user
  Preset,    // a kConstant's value, or one the user changed
  Solved,    // worked out
};

// One formula being worked on: which variables have values, and where they came from.
class Sheet {
 public:
  Sheet();
  ~Sheet();
  Sheet(const Sheet &) = delete;
  Sheet &operator=(const Sheet &) = delete;

  // Starts on a formula: kConstant variables at their presets, the rest empty.
  void open(const Formula *f);
  const Formula *current() const;

  // A value typed for variable i, in SI, then a solve. If that makes more entered
  // values than the formula needs, the one entered longest ago stops being an input
  // and is solved for instead: with V, I and R all filled, retyping I recomputes
  // whichever of V and R was entered first. That is how a filled-in sheet is reused.
  void set(int i, double si);
  // Makes variable i the one to solve for: it stops being an input (a kConstant
  // returns to Solved too), then a solve.
  void solveFor(int i);
  // Empties variable i (a kConstant returns to its preset), then a solve.
  void clear(int i);
  // Back to the state open() left.
  void clearAll();

  Cell cell(int i) const;
  bool hasValue(int i) const;   // Entered, Preset or Solved
  double value(int i) const;    // SI; 0 without a value

  // How many more values are needed to solve everything; 0 when there is nothing
  // left to do or the sheet is stuck for another reason (see error()).
  int missing() const;
  // "" or a short reason to show: "Needs v above 0", "No real solution: b^2 < 4ac",
  // "These values don't fit together", "Two solutions: showing the positive one".
  const char *error() const;
  // Second solution of the same variable when an equation has two (t on the way up
  // and down), and which variable it belongs to; -1 if none.
  int altVar() const;
  double altValue() const;

  // How variable i got its value, for showing: the equation rearranged with i alone
  // on the left. Null if i is not Solved. lhs is always a Var node; rhs may be an
  // inverted copy of the stored equation's other side. For a variable solved
  // numerically this is the original equation (lhs may then be a whole expression).
  const Node *howLhs(int i) const;
  const Node *howRhs(int i) const;

 private:
  struct Impl;
  Impl *impl_;
};

// Equation e of the formula as trees, for showing it as stored. The trees live until
// the next call for a different formula. False on a bad index.
bool equation(const Formula *f, int e, const Node **lhs, const Node **rhs);

// Evaluates a tree with the given variable values (SI). False on domain errors.
bool evaluate(const Node *n, const double *vars, double *out);

// ---- Diagrams ----

// Formula::diagram is a small drawing script on a 400 x 240 canvas (y down), with
// commands separated by ';' and each command's arguments by spaces. Drawn by
// formulas_app.cpp, checked by the test (known commands, right argument counts,
// coordinates on the canvas).
//   k N            colour for what follows: 0 line, 1 group accent, 2 warm, 3 dim
//   L x1 y1 x2 y2  line
//   A x1 y1 x2 y2  arrow, head at the second point
//   D x1 y1 x2 y2  dimension line, heads at both ends
//   B x y w h      box outline          F x y w h      filled box
//   C x y r        circle outline       O x y r        filled circle
//   W x y r a0 a1  arc, degrees, 0 = east, counter-clockwise on screen
//   P x y x y ...  filled polygon (3 or more points)
//   Q x1 y1 x2 y2  ground hatching along a line (hatch below/right of it)
//   M x1 y1 x2 y2  spring (coil) between two points
//   Z x y w h n    sine wave, n cycles, in the box
//   Parts between two points, wire-sized, with the leads included:
//   r x1 y1 x2 y2  resistor            c x1 y1 x2 y2  capacitor
//   l x1 y1 x2 y2  inductor            b x1 y1 x2 y2  battery / DC source (+ at the 2nd point)
//   s x1 y1 x2 y2  AC source           d x1 y1 x2 y2  diode (points to the 2nd point)
//   e x1 y1 x2 y2  LED
//   g x y          ground symbol hanging from (x, y)
//   T x y text     label centred on (x, y); the rest of the command is the text, UTF-8,
//                  with _{ } subscripts, so a variable's label can be used as it is
//   t x y text     the same, smaller

// Checks a script; false with a reason if it is not valid.
bool checkDiagram(const char *script, std::string *why);

}  // namespace formulas
}  // namespace mt
