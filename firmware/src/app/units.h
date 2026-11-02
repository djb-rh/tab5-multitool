// The Units app's engine: unit tables, conversion, typed-input parsing and display
// formatting. Plain C++17 and the standard library only (no Arduino, M5 or display
// code, no exceptions, no RTTI) so the same file is tested on the Mac:
//   test/units_test.sh
//
// Everything is index based. A category ("Length") holds a fixed table of units
// ("Inches"); the UI keeps a category index and two unit indices and calls convert().
// The tables are constexpr arrays in flash; nothing here allocates except the
// std::string results of the format helpers.
//
// Symbols and names are plain ASCII ("m^2", "um", "N-m", "F") because the device's
// DejaVu GFX fonts only carry printable ASCII. The typed-name aliases also accept the
// UTF-8 forms people paste or type on a Mac ("m²", "µm", "°F", "N·m", "″").
#pragma once

#include <cstdint>
#include <string>

namespace mt {
namespace units {

// How a unit relates to its category's base unit (b = value in base units).
enum class Kind : uint8_t {
  Linear,      // b = v * factor
  Affine,      // b = (v + offset) * factor   (Celsius, Fahrenheit)
  Reciprocal,  // b = factor / v              (L/100 km against km/L: bigger is thirstier)
};

// Unit::flags bits.
enum : uint8_t {
  kCommon = 1,  // show in the short list
  kApprox = 2,  // defined by a convention or an average, not exactly (Mach, month, year)
};

struct Unit {
  const char *name;     // "Inches"
  const char *symbol;   // "in" (ASCII)
  // Extra names for typed input, '|' separated. A space matches any run of spaces or
  // none ("fl oz" also matches "floz"). A leading '~' marks a weak alias that only wins
  // when nothing better fits (so "8 oz" is mass, but "8 oz to mL" is fluid ounces).
  // The name and symbol are always accepted too.
  const char *aliases;
  double factor;
  double offset;        // Affine only
  Kind kind;
  uint8_t flags;
};

struct Category {
  const char *name;     // "Length"
  const Unit *units;
  uint8_t count;
  uint8_t defaultFrom;  // a sensible starting pair for the pickers (Length: in -> mm)
  uint8_t defaultTo;
  const char *note;     // one line for the UI, or nullptr
};

// Category indices, in table order. Earlier categories win ties when a typed unit
// could be either ("m" is metres before minutes, "'" is feet before arcminutes).
enum Cat : uint8_t {
  kLength, kArea, kVolume, kMass, kTemperature, kTime, kSpeed, kAcceleration,
  kPressure, kEnergy, kPower, kForce, kTorque, kDataSize, kDataRate, kAngle,
  kFrequency, kFuelEconomy, kDensity, kFlow, kCurrent, kVoltage, kResistance,
  kCapacitance, kCatCount
};

int categoryCount();
// nullptr when out of range.
const Category *category(int cat);
int unitCount(int cat);  // 0 when out of range
const Unit *unit(int cat, int index);
// Index of the category named name (case-insensitive), or -1.
int findCategory(const char *name);

// ---- Conversion ----

// Converts value from one unit to another in the same category. On a bad index or a
// result that is not finite (0 mpg is infinite L/100 km) it returns 0 and sets
// *ok = false; it never returns NaN or infinity. from == to returns value unchanged.
double convert(double value, int cat, int from, int to, bool *ok = nullptr);
// To and from the category's base unit (metres, litres, kg, kelvin...). Same rules.
double toBase(double value, int cat, int unit, bool *ok = nullptr);
double fromBase(double base, int cat, int unit, bool *ok = nullptr);

// ---- Cooking: volume <-> weight for common ingredients (approximate) ----

// Densities are typical US kitchen values (spooned-and-levelled flour, packed brown
// sugar), good to a few percent; the UI should say "approx.".
struct Ingredient {
  const char *name;
  double gramsPerCup;  // per US cup (236.5882365 mL)
};
int ingredientCount();
const Ingredient *ingredient(int index);
// Converts between any Volume unit and any Mass unit, either way, through the
// ingredient's density. Two units of the same category convert normally. Other
// categories fail (*ok = false, returns 0).
double convertIngredient(double value, int ingredient, int fromCat, int fromUnit, int toCat,
                         int toUnit, bool *ok = nullptr);

// ---- Typed input ----

struct UnitRef {
  int cat = -1;
  int unit = -1;
  bool valid() const { return cat >= 0 && unit >= 0; }
};

// Finds a unit by what the user typed: name, symbol or alias, case-insensitive, with
// exact case preferred ("MB" megabytes, "Mb" megabits, "mb" millibars; "C" Celsius,
// "c" cups). cat >= 0 restricts the search to that category; otherwise the best match
// wins, ties going to the earlier category. Surrounding spaces are ignored.
UnitRef findUnit(const char *text, int cat = -1);

// Evaluates arithmetic: + - * / (and the UTF-8 × ÷ −; "x" is not multiply), ^ (right
// associative, binds tighter than unary minus: -2^2 = -4), parentheses, decimals,
// exponents (1e3, 2.5E-4), and thousands commas ("1,000"). Also mixed numbers as a
// tape measure reads: "2 3/8" and "2-3/8" are 2.375. False on a syntax error,
// division by zero or a non-finite result.
bool evalExpression(const char *text, double *out);

// Parses the value field for a converter whose from-unit is (cat, unit). Accepts a bare
// expression ("3/8", "12*2.54"), taken to be in that unit, or one or more number+unit
// pairs from the same category, which are added up: "5 ft 3 in", "5'3\"", "6' 2.5\"",
// "1h 20m 30s", "2 lb 4 oz", "40° 26' 46\"". The result is in (cat, unit). Pairs only
// add for linear units (no "5 C 3 F"). False if anything is not understood.
bool parseQuantity(const char *text, int cat, int unit, double *out);

// Whole-line conversion typed on the keyboard: "5 ft 3 in to cm", "100 F in C",
// "3/8 in mm", "3 in in mm", "5in mm", "1h 20m 30s -> min", "8 oz = mL".
// Separators: "to", "into", "as", "->", "=", or "in" when that reading works (the
// words left of it parse as a quantity and the one unit right of it fits); with no
// separator the last unit is the target. With no target at all ("5 ft 3 in") the
// target is the from-unit. The category is the one every typed unit fits, preferring
// exact-case matches, then the earlier category.
struct LineResult {
  bool ok = false;         // understood: cat/from/to/value are set
  bool hasTarget = false;  // a target unit was typed (otherwise to == from)
  bool resultOk = false;   // result is finite (0 mpg -> L/100 km is not)
  int cat = -1;
  int from = -1;           // the first unit typed: "5 ft 3 in" is from feet
  int to = -1;
  double value = 0;        // the typed quantity in from-units (5.25 for "5 ft 3 in")
  double result = 0;       // value in to-units
  const char *error = "";  // short reason when !ok or !resultOk
};
LineResult parseLine(const char *text);

// ---- Display ----

struct NumberFormat {
  int digits = 9;          // significant digits (1..17)
  bool grouping = false;   // "1,234,567.5"
  int sciHigh = 12;        // scientific when |v| >= 10^sciHigh ...
  int sciLow = -6;         // ... or |v| < 10^sciLow (zero never is)
  bool timesTen = false;   // "1.5 x 10^-12" instead of "1.5e-12"
};
// Rounds to `digits` significant digits, trims trailing zeros, never "-0".
// Infinity and NaN give "inf", "-inf", "NaN" (convert() never returns them).
std::string formatNumber(double v, const NumberFormat &f = NumberFormat());
// formatNumber plus " " plus the unit symbol.
std::string formatWithUnit(double v, int cat, int unit, const NumberFormat &f = NumberFormat());

// Nearest fraction with denominator maxDen (reduced), as a tape measure reads.
struct Fraction {
  bool negative = false;
  long long whole = 0;
  int num = 0;             // 0 when the value rounds to a whole number
  int den = 1;
  double error = 0;        // value - fraction (positive: the real value is larger)
  bool exact = true;       // |error| is negligible
};
Fraction nearestFraction(double v, int maxDen = 64);
// "11/32", "2 3/8", "-1 1/2", "3"; "~" in front when not exact. showError appends the
// signed difference: "~11/32 (+0.0012)". maxDen is clamped to 1..1024.
std::string formatFraction(double v, int maxDen = 64, bool showError = false);
// A length in inches as feet and inches: "6' 2.5\"", "-1' 0.25\"", "3.25\"" (no feet
// under a foot). fracDen > 0 shows the inches as a fraction to that denominator
// ("6' 2 1/2\"", "~" in front when rounded); otherwise up to `decimals` places.
std::string formatFeetInches(double inches, int fracDen = 0, int decimals = 2);
// A time in seconds as h:mm:ss with up to 3 trimmed decimals: "1:20:30",
// "0:00:59.25", "2d 03:04:05" from a day up, "-0:00:30".
std::string formatDuration(double seconds);

}  // namespace units
}  // namespace mt
