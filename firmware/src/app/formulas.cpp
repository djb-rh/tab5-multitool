#include "formulas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

#include "formula_lib.h"

namespace mt {
namespace formulas {

using units::Kind;
using units::Unit;

namespace {

constexpr uint8_t C = units::kCommon;

constexpr Unit L(const char *n, const char *s, const char *a, double f, uint8_t fl = 0) {
  return Unit{n, s, a, f, 0, Kind::Linear, fl};
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kIn = 0.0254;
constexpr double kFt = 0.3048;
constexpr double kLb = 0.45359237;
constexpr double kLbf = 4.4482216152605;  // kLb * standard gravity
constexpr double kBtu = 1055.05585262;    // joules, as units.cpp
constexpr double kDegF = 5.0 / 9;         // kelvin in a Fahrenheit degree

// ---- The extra unit tables ----

// Every table here has its SI unit at factor 1, and kDimExtra names it by index.

constexpr Unit kRatioU[] = {
  L("Ratio", "ratio", "x|times", 1, C),
  L("Percent", "%", "percent|pct", 0.01, C),
  L("Parts per million", "ppm", "parts per million", 1e-6, C),
};
constexpr Unit kDecibelU[] = {
  L("Decibels", "dB", "db|decibel|decibels", 1, C),
};
// A difference, so no offset: 10 C of difference is 10 K. The plain "C" and "F" are
// aliases so a typed "10 C" means what it says in a difference field.
constexpr Unit kTempDiffU[] = {
  L("Kelvin", "K", "kelvin|kelvins", 1, C),
  L("Celsius degrees", "C deg", "C|°C|degC|deg C|Cdeg|celsius|celsius degrees", 1, C),
  L("Fahrenheit degrees", "F deg", "F|°F|degF|deg F|Fdeg|fahrenheit|fahrenheit degrees", kDegF, C),
};
constexpr Unit kChargeU[] = {
  L("Coulombs", "C", "coulomb|coulombs", 1, C),
  L("Millicoulombs", "mC", "millicoulomb|millicoulombs", 1e-3),
  L("Microcoulombs", "uC", "µC|μC|microcoulomb|microcoulombs", 1e-6, C),
  L("Nanocoulombs", "nC", "nanocoulomb|nanocoulombs", 1e-9, C),
  L("Amp-hours", "Ah", "A-h|amp hour|amp hours|amp-hour|amp-hours", 3600, C),
  L("Milliamp-hours", "mAh", "mA-h|milliamp hour|milliamp hours|milliamp-hour|milliamp-hours", 3.6, C),
};
constexpr Unit kInductanceU[] = {
  L("Henries", "H", "henry|henrys|henries", 1, C),
  L("Millihenries", "mH", "millihenry|millihenrys|millihenries", 1e-3, C),
  L("Microhenries", "uH", "µH|μH|microhenry|microhenrys|microhenries", 1e-6, C),
  L("Nanohenries", "nH", "nanohenry|nanohenrys|nanohenries", 1e-9, C),
};
constexpr Unit kMagFieldU[] = {
  L("Teslas", "T", "tesla|teslas", 1, C),
  L("Milliteslas", "mT", "millitesla|milliteslas", 1e-3, C),
  L("Microteslas", "uT", "µT|μT|microtesla|microteslas", 1e-6, C),
  L("Gauss", "G", "gauss", 1e-4),
};
constexpr Unit kMagFluxU[] = {
  L("Webers", "Wb", "weber|webers", 1, C),
  L("Milliwebers", "mWb", "milliweber|milliwebers", 1e-3, C),
};
constexpr Unit kConductanceU[] = {
  L("Siemens", "S", "siemens|mho|mhos", 1, C),
  L("Millisiemens", "mS", "millisiemens", 1e-3, C),
  L("Microsiemens", "uS", "µS|μS|microsiemens", 1e-6, C),
};
constexpr Unit kResistivityU[] = {
  L("Ohm-meters", "ohm-m", "ohm m|ohm meter|ohm meters|ohm metre|ohm metres|Ω-m|Ωm|Ω m", 1, C),
  L("Ohm-centimeters", "ohm-cm", "ohm cm|Ω-cm|Ωcm|Ω cm", 0.01, C),
  L("Microhm-centimeters", "uohm-cm", "µohm-cm|μohm-cm|µΩ-cm|μΩ-cm|microhm cm|microhm-cm", 1e-8, C),
};
constexpr Unit kAngularSpeedU[] = {
  L("Radians per second", "rad/s", "rad/sec|radians per second", 1, C),
  L("Degrees per second", "deg/s", "°/s|deg/sec|degrees per second", kPi / 180, C),
  L("Revolutions per minute", "rpm", "rev/min|r/min|revs per minute|revolutions per minute", 2 * kPi / 60, C),
  L("Revolutions per second", "rev/s", "rps|revs per second|revolutions per second", 2 * kPi, C),
};
constexpr Unit kAngularAccelU[] = {
  L("Radians per second squared", "rad/s^2", "rad/s2|rad/s²", 1, C),
  L("Degrees per second squared", "deg/s^2", "deg/s2|deg/s²|°/s^2|°/s2|°/s²", kPi / 180, C),
};
constexpr Unit kStiffnessU[] = {
  L("Newtons per meter", "N/m", "newtons per meter|newtons per metre", 1, C),
  L("Newtons per millimeter", "N/mm", "newtons per millimeter|newtons per millimetre", 1e3, C),
  L("Pounds-force per inch", "lbf/in", "lb/in|pounds per inch", kLbf / kIn, C),
};
constexpr Unit kMomentumU[] = {
  L("Kilogram meters per second", "kg-m/s", "kg m/s|kgm/s|kg·m/s", 1, C),
  L("Newton-seconds", "N-s", "N s|Ns|N·s|newton second|newton seconds|newton-second|newton-seconds", 1, C),
};
constexpr Unit kInertiaU[] = {
  L("Kilogram square meters", "kg-m^2", "kg m^2|kgm^2|kg-m2|kgm2|kg·m²|kg m2", 1, C),
  L("Gram square centimeters", "g-cm^2", "g cm^2|gcm^2|g-cm2|gcm2|g·cm²", 1e-7, C),
  L("Pound square feet", "lb-ft^2", "lb ft^2|lbft^2|lb-ft2|lbft2|lb·ft²", kLb * kFt * kFt, C),
};
constexpr Unit kAreaMomentU[] = {
  L("Meters to the fourth", "m^4", "m4|m⁴", 1, C),
  L("Millimeters to the fourth", "mm^4", "mm4|mm⁴", 1e-12, C),
  L("Centimeters to the fourth", "cm^4", "cm4|cm⁴", 1e-8, C),
  L("Inches to the fourth", "in^4", "in4|in⁴", kIn * kIn * kIn * kIn, C),
};
constexpr Unit kSpecificHeatU[] = {
  L("Joules per kg kelvin", "J/(kg-K)", "J/kg-K|J/kgK|J/kg/K|J/(kg K)|J/(kg·K)", 1, C),
  L("Kilojoules per kg kelvin", "kJ/(kg-K)", "kJ/kg-K|kJ/kgK|kJ/kg/K|kJ/(kg K)|kJ/(kg·K)", 1e3, C),
  L("Calories per gram C", "cal/(g-C)", "cal/g-C|cal/gC|cal/g/C|cal/(g C)", 4184, C),
  L("BTU per pound F", "BTU/(lb-F)", "BTU/lb-F|BTU/lbF|BTU/lb/F|BTU/(lb F)", kBtu / (kLb * kDegF), C),
};
constexpr Unit kThermalCondU[] = {
  L("Watts per meter kelvin", "W/(m-K)", "W/m-K|W/mK|W/m/K|W/(m K)|W/(m·K)", 1, C),
  L("BTU per hour foot F", "BTU/(hr-ft-F)", "BTU/hr-ft-F|BTU/(h-ft-F)", kBtu / 3600 / (kFt * kDegF), C),
};
constexpr Unit kThermalResU[] = {
  L("Kelvin per watt", "K/W", "C/W|°C/W|kelvin per watt", 1, C),
};
constexpr Unit kHeatTransferU[] = {
  L("Watts per square meter kelvin", "W/(m^2-K)", "W/m^2-K|W/m2-K|W/m2K|W/m^2/K|W/(m2 K)|W/(m²·K)", 1, C),
};
constexpr Unit kExpansionU[] = {
  L("Per kelvin", "1/K", "per K|/K|1/C|per C|/C", 1, C),
  L("Per Fahrenheit degree", "1/F", "per F|/F", 1 / kDegF, C),
  L("Parts per million per kelvin", "ppm/K", "ppm/C|ppm per K|ppm per C", 1e-6, C),
};
constexpr Unit kViscosityU[] = {
  L("Pascal-seconds", "Pa-s", "Pa s|Pas|Pa·s|pascal second|pascal seconds", 1, C),
  L("Millipascal-seconds", "mPa-s", "mPa s|mPas|mPa·s", 1e-3, C),
  L("Centipoise", "cP", "cp|centipoise", 1e-3, C),
  L("Poise", "P", "poise", 0.1),
};
constexpr Unit kKinViscosityU[] = {
  L("Square meters per second", "m^2/s", "m2/s|m²/s", 1, C),
  L("Centistokes", "cSt", "cst|centistokes", 1e-6, C),
  L("Stokes", "St", "stokes", 1e-4),
};
constexpr Unit kAmountU[] = {
  L("Moles", "mol", "mole|moles", 1, C),
  L("Millimoles", "mmol", "millimole|millimoles", 1e-3, C),
  L("Micromoles", "umol", "µmol|μmol|micromole|micromoles", 1e-6, C),
};
constexpr Unit kMolarityU[] = {
  L("Moles per liter", "mol/L", "M|mol/l|molar|moles per liter|moles per litre", 1000, C),
  L("Millimoles per liter", "mmol/L", "mM|mmol/l|millimolar", 1, C),
  L("Moles per cubic meter", "mol/m^3", "mol/m3|mol/m³", 1),
};
constexpr Unit kMolarMassU[] = {
  L("Grams per mole", "g/mol", "grams per mole", 1e-3, C),
  L("Kilograms per mole", "kg/mol", "kilograms per mole", 1),
};
constexpr Unit kElectricFieldU[] = {
  L("Volts per meter", "V/m", "volts per meter|volts per metre", 1, C),
  L("Kilovolts per meter", "kV/m", "kilovolts per meter|kilovolts per metre", 1e3, C),
  L("Volts per millimeter", "V/mm", "volts per millimeter|volts per millimetre", 1e3, C),
};
constexpr Unit kIntensityU[] = {
  L("Watts per square meter", "W/m^2", "W/m2|W/m²", 1, C),
  L("Milliwatts per square cm", "mW/cm^2", "mW/cm2|mW/cm²", 10, C),
};
constexpr Unit kIlluminanceU[] = {
  L("Lux", "lux", "lx", 1, C),
  L("Foot-candles", "foot-candle", "fc|ft-c|foot candle|foot candles|foot-candles", 1 / (kFt * kFt), C),
};
constexpr Unit kLuminousFluxU[] = {
  L("Lumens", "lm", "lumen|lumens", 1, C),
};
constexpr Unit kRadioactivityU[] = {
  L("Becquerels", "Bq", "becquerel|becquerels", 1, C),
  L("Kilobecquerels", "kBq", "kilobecquerel|kilobecquerels", 1e3, C),
  L("Curies", "Ci", "curie|curies", 3.7e10, C),
  L("Millicuries", "mCi", "millicurie|millicuries", 3.7e7, C),
};
constexpr Unit kSpecificEnergyU[] = {
  L("Joules per kilogram", "J/kg", "joules per kilogram", 1, C),
  L("Kilojoules per kilogram", "kJ/kg", "kilojoules per kilogram", 1e3, C),
  L("Megajoules per kilogram", "MJ/kg", "megajoules per kilogram", 1e6, C),
  L("BTU per pound", "BTU/lb", "btu per pound", kBtu / kLb, C),
};
constexpr Unit kMolarEnergyU[] = {
  L("Joules per mole", "J/mol", "joules per mole", 1, C),
  L("Kilojoules per mole", "kJ/mol", "kilojoules per mole", 1e3, C),
  L("Kilocalories per mole", "kcal/mol", "kilocalories per mole", 4184, C),
};
constexpr Unit kLinearDensityU[] = {
  L("Kilograms per meter", "kg/m", "kilograms per meter|kilograms per metre", 1, C),
  L("Grams per meter", "g/m", "grams per meter|grams per metre", 1e-3, C),
};

template <size_t N>
constexpr DimInfo extra(const char *name, const Unit (&u)[N], uint8_t si) {
  return DimInfo{name, u, (uint8_t)N, si};
}

// In enum order, from kNone.
constexpr DimInfo kDimExtra[] = {
  {"Number", nullptr, 0, 0},
  extra("Ratio", kRatioU, 0),
  extra("Decibels", kDecibelU, 0),
  extra("Temperature difference", kTempDiffU, 0),
  extra("Charge", kChargeU, 0),
  extra("Inductance", kInductanceU, 0),
  extra("Magnetic field", kMagFieldU, 0),
  extra("Magnetic flux", kMagFluxU, 0),
  extra("Conductance", kConductanceU, 0),
  extra("Resistivity", kResistivityU, 0),
  extra("Angular speed", kAngularSpeedU, 0),
  extra("Angular acceleration", kAngularAccelU, 0),
  extra("Stiffness", kStiffnessU, 0),
  extra("Momentum", kMomentumU, 0),
  extra("Moment of inertia", kInertiaU, 0),
  extra("Second moment of area", kAreaMomentU, 0),
  extra("Specific heat", kSpecificHeatU, 0),
  extra("Thermal conductivity", kThermalCondU, 0),
  extra("Thermal resistance", kThermalResU, 0),
  extra("Heat transfer coefficient", kHeatTransferU, 0),
  extra("Thermal expansion", kExpansionU, 0),
  extra("Viscosity", kViscosityU, 0),
  extra("Kinematic viscosity", kKinViscosityU, 0),
  extra("Amount of substance", kAmountU, 0),
  extra("Concentration", kMolarityU, 2),
  extra("Molar mass", kMolarMassU, 1),
  extra("Electric field", kElectricFieldU, 0),
  extra("Intensity", kIntensityU, 0),
  extra("Illuminance", kIlluminanceU, 0),
  extra("Luminous flux", kLuminousFluxU, 0),
  extra("Radioactivity", kRadioactivityU, 0),
  extra("Specific energy", kSpecificEnergyU, 0),
  extra("Molar energy", kMolarEnergyU, 0),
  extra("Linear density", kLinearDensityU, 0),
};
static_assert(sizeof kDimExtra / sizeof kDimExtra[0] == kDimCount - kNone,
              "kDimExtra must match enum Dim");

// The SI unit of each units.h category, by symbol: its base unit is not always SI
// (Volume is litres, Flow is L/min, Angle is degrees). units.h has no exact SI unit
// for Fuel economy, and none exists anyone would use, so that stays on km/L; data is
// counted in bits to agree with bit/s.
constexpr const char *kCatSI[] = {
  "m", "m^2", "m^3", "kg", "K", "s", "m/s", "m/s^2", "Pa", "J", "W", "N", "N-m", "bit", "bit/s",
  "rad", "Hz", "km/L", "kg/m^3", "m^3/s", "A", "V", "ohm", "F",
};
static_assert(sizeof kCatSI / sizeof kCatSI[0] == units::kCatCount, "kCatSI must match enum Cat");

const DimInfo *dimTable() {
  static DimInfo table[units::kCatCount];
  static bool filled = false;
  if (!filled) {
    for (int c = 0; c < units::kCatCount; c++) {
      const units::Category *cat = units::category(c);
      uint8_t si = 0;
      for (int u = 0; u < cat->count; u++)
        if (!strcmp(cat->units[u].symbol, kCatSI[c])) si = (uint8_t)u;
      table[c] = DimInfo{cat->name, cat->units, cat->count, si};
    }
    filled = true;
  }
  return table;
}

inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
inline bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

std::string trimmed(const char *s) {
  while (isSpace(*s)) s++;
  size_t n = strlen(s);
  while (n > 0 && isSpace(s[n - 1])) n--;
  return std::string(s, n);
}

// ---- Typed units for the extra dimensions ----

// A against B, a space in B matching any run of spaces or none, as units.h says.
bool sameName(const char *a, size_t an, const char *b, size_t bn, bool exact) {
  size_t i = 0, j = 0;
  while (j < bn) {
    if (b[j] == ' ') {
      while (i < an && a[i] == ' ') i++;
      j++;
      continue;
    }
    if (i >= an) return false;
    if (exact ? a[i] != b[j] : lower(a[i]) != lower(b[j])) return false;
    i++;
    j++;
  }
  return i == an;
}

// True if u goes by this text: its symbol, its name or an alias.
bool unitNamed(const Unit &u, const char *t, size_t n, bool exact) {
  if (sameName(t, n, u.symbol, strlen(u.symbol), exact)) return true;
  if (sameName(t, n, u.name, strlen(u.name), exact)) return true;
  const char *a = u.aliases;
  while (a && *a) {
    const char *end = strchr(a, '|');
    const size_t len = end ? (size_t)(end - a) : strlen(a);
    const char *name = *a == '~' ? a + 1 : a;
    if (sameName(t, n, name, len - (size_t)(name - a), exact)) return true;
    a = end ? end + 1 : nullptr;
  }
  return false;
}

// Index in the dimension's table of the unit with this exact-case name, or -1.
int unitExact(const DimInfo *info, const char *t, size_t n) {
  for (int u = 0; u < info->count; u++)
    if (unitNamed(info->units[u], t, n, true)) return u;
  return -1;
}

// Exact case first, then any case.
int unitByName(int d, const DimInfo *info, const std::string &t) {
  if (d < units::kCatCount) return units::findUnit(t.c_str(), d).unit;
  const int e = unitExact(info, t.data(), t.size());
  if (e >= 0) return e;
  for (int u = 0; u < info->count; u++)
    if (unitNamed(info->units[u], t.data(), t.size(), false)) return u;
  return -1;
}

// Swaps "pi" and "π", as whole words, for the number: evalExpression has no names.
std::string withPi(const std::string &t) {
  std::string out;
  for (size_t i = 0; i < t.size();) {
    const bool afterWord = i > 0 && (isAlpha(t[i - 1]) || isDigit(t[i - 1]));
    const bool afterNumber = i > 0 && (isDigit(t[i - 1]) || t[i - 1] == ')');
    if (!strncmp(t.c_str() + i, "π", 2) ||
        ((!afterWord || afterNumber) && i + 1 < t.size() && lower(t[i]) == 'p' && lower(t[i + 1]) == 'i' &&
         (i + 2 >= t.size() || !isAlpha(t[i + 2])))) {
      if (afterNumber) out += '*';   // "2pi"
      out += "(3.141592653589793)";
      i += 2;
    } else {
      out += t[i++];
    }
  }
  return out;
}

// The multiplier of an engineer's prefix letter, or 0.
double prefixValue(const std::string &s) {
  if (s == "µ" || s == "μ") return 1e-6;
  if (s.size() != 1) return 0;
  switch (s[0]) {
    case 'f': return 1e-15;
    case 'p': return 1e-12;
    case 'n': return 1e-9;
    case 'u': return 1e-6;
    case 'm': return 1e-3;
    case 'k': case 'K': return 1e3;
    case 'M': return 1e6;
    case 'G': return 1e9;
    case 'T': return 1e12;
    default: return 0;
  }
}

// True for a unit that is a power of ten of the SI unit: the metric family.
bool isDecimal(const DimInfo *info, int unit) {
  if (unit < 0 || unit >= info->count) return false;
  if (info->units[unit].kind != Kind::Linear) return false;
  const double r = std::log10(info->units[unit].factor / info->units[info->si].factor);
  return std::fabs(r - std::round(r)) < 1e-9;
}

// What "5k" is 5 thousand of: the unprefixed metric unit, in SI. A field showing a
// non-metric unit means that unit (3k in feet is 3000 ft).
double prefixRoot(int d, int unit, const DimInfo *info, bool *ok) {
  if (d == units::kMass && isDecimal(info, unit)) return 1e-3;     // grams
  if (d == units::kVolume && isDecimal(info, unit)) return 1e-3;   // litres
  if (d == kNone || isDecimal(info, unit)) return 1;
  return toSI(1, d, unit, ok);
}

// "12 mAh": the number and the unit, for the dimensions units::parseQuantity does not
// know, adding pairs up as it does. *first gets the first unit's index.
bool parsePairs(const std::string &text, int d, const DimInfo *info, double *si, int *first) {
  double total = 0;
  *first = -1;
  size_t at = 0;
  while (at < text.size()) {
    bool found = false;
    for (size_t p = at + 1; p < text.size() && !found; p++) {
      double v = 0;
      if (!units::evalExpression(text.substr(at, p - at).c_str(), &v)) continue;
      for (size_t end = text.size(); end > p; end--) {
        if (end < text.size() && text[end] != ' ') continue;
        const std::string name = trimmed(text.substr(p, end - p).c_str());
        if (name.empty()) continue;
        const int u = unitByName(d, info, name);
        if (u < 0) continue;
        bool ok = true;
        total += toSI(v, d, u, &ok);
        if (!ok) return false;
        if (*first < 0) *first = u;
        at = end;
        while (at < text.size() && text[at] == ' ') at++;
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  *si = total;
  return *first >= 0;
}

}  // namespace

// ---- Dimensions ----

const DimInfo *dim(int d) {
  if (d < 0 || d >= kDimCount) return nullptr;
  if (d >= kNone) return &kDimExtra[d - kNone];
  return &dimTable()[d];
}

double toSI(double v, int d, int unit, bool *ok) {
  if (ok) *ok = false;
  const DimInfo *info = dim(d);
  double r = 0;
  if (!info) return 0;
  if (d == kNone) {
    r = v;
  } else if (unit < 0 || unit >= info->count) {
    return 0;
  } else if (d < units::kCatCount) {
    bool good = false;
    r = units::convert(v, d, unit, info->si, &good);
    if (!good) return 0;
  } else {
    r = v * info->units[unit].factor;
  }
  if (!std::isfinite(r)) return 0;
  if (ok) *ok = true;
  return r;
}

double fromSI(double si, int d, int unit, bool *ok) {
  if (ok) *ok = false;
  const DimInfo *info = dim(d);
  double r = 0;
  if (!info) return 0;
  if (d == kNone) {
    r = si;
  } else if (unit < 0 || unit >= info->count) {
    return 0;
  } else if (d < units::kCatCount) {
    bool good = false;
    r = units::convert(si, d, info->si, unit, &good);
    if (!good) return 0;
  } else {
    r = si / info->units[unit].factor;
  }
  if (!std::isfinite(r)) return 0;
  if (ok) *ok = true;
  return r;
}

bool parseValue(const char *text, int d, int unit, double *si, int *usedUnit) {
  const DimInfo *info = dim(d);
  if (!info || !text || !si) return false;
  if (usedUnit) *usedUnit = -1;
  const std::string t = withPi(trimmed(text));
  if (t.empty()) return false;

  // A bare number or sum, in the field's unit.
  double v = 0;
  bool ok = false;
  if (units::evalExpression(t.c_str(), &v)) {
    const double r = toSI(v, d, unit, &ok);
    if (ok) *si = r;
    return ok;
  }

  // A prefix letter straight after the number, unless the letter is itself a unit
  // of this dimension in exactly that case ("5m" of length is metres).
  size_t cut = t.size() - 1;
  while (cut > 0 && ((unsigned char)t[cut] & 0xC0) == 0x80) cut--;
  const std::string tail = t.substr(cut);
  const double mult = prefixValue(tail);
  const bool prefixable = d != units::kTemperature && d != units::kAngle && d != kRatio &&
                          d != kDecibel && d != kTempDiff;
  if (cut > 0 && mult != 0 && prefixable && !isSpace(t[cut - 1]) &&
      unitExact(info, tail.data(), tail.size()) < 0 &&
      units::evalExpression(t.substr(0, cut).c_str(), &v)) {
    ok = true;
    const double base = prefixRoot(d, unit, info, &ok);
    if (!ok) return false;
    const double r = v * mult * base;
    if (!std::isfinite(r)) return false;
    *si = r;
    return true;
  }

  // Number and unit pairs.
  if (d == kNone) return false;
  int first = -1;
  double r = 0;
  if (parsePairs(t, d, info, &r, &first)) {
    *si = r;
    if (usedUnit) *usedUnit = first;
    return true;
  }
  if (d < units::kCatCount && units::parseQuantity(t.c_str(), d, info->si, &r) && std::isfinite(r)) {
    *si = r;
    if (usedUnit) {
      // Forms like 5'3": the first thing after the first number that is a unit.
      size_t p = 0;
      while (p < t.size() && (isDigit(t[p]) || t[p] == '.' || t[p] == '-' || t[p] == ' ')) p++;
      for (size_t n = t.size() - p; n > 0 && *usedUnit < 0; n--) {
        const int u = units::findUnit(t.substr(p, n).c_str(), d).unit;
        if (u >= 0) *usedUnit = u;
      }
    }
    return true;
  }
  return false;
}

int bestUnit(double si, int d, int fallback) {
  const DimInfo *info = dim(d);
  if (!info || info->count == 0 || si == 0 || !std::isfinite(si)) return fallback;
  // Scales that do not suit a jump to another unit: a small angle is not clearer in
  // arcminutes, nor a ratio in ppm.
  if (d == units::kAngle || d == units::kFuelEconomy || d == kRatio || d == kDecibel || d == kTempDiff)
    return fallback;
  for (int u = 0; u < info->count; u++)
    if (info->units[u].kind != Kind::Linear) return fallback;

  bool ok = false;
  const double here = std::fabs(fromSI(si, d, fallback, &ok));
  if (ok && here >= 1 && here < 1000) return fallback;

  // How far outside 1..1000 a value sits, in decades.
  auto miss = [](double v) {
    const double l = std::log10(v);
    return l < 0 ? -l : (l >= 3 ? l - 3 + 1e-9 : 0.0);
  };
  // Stay in the family the field is in if that will do (metric with metric); then any
  // common unit. Among those in range the smaller number reads best: 2 h, not 120 min.
  // Hz never leaves its family: 0.5 Hz is not better said in rad/s.
  const bool decimal = isDecimal(info, fallback);
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1 && d == units::kFrequency) break;
    int best = -1;
    double smallest = 1e300;
    for (int u = 0; u < info->count; u++) {
      if (!(info->units[u].flags & units::kCommon)) continue;
      if (pass == 0 && isDecimal(info, u) != decimal) continue;
      const double v = std::fabs(fromSI(si, d, u, &ok));
      if (ok && v > 0 && miss(v) == 0 && v < smallest) {
        smallest = v;
        best = u;
      }
    }
    if (best >= 0) return best;
  }
  // Nothing in range: the nearest, in the family.
  int best = fallback;
  double nearest = 1e300;
  for (int u = 0; u < info->count; u++) {
    if (!(info->units[u].flags & units::kCommon) || isDecimal(info, u) != decimal) continue;
    const double v = std::fabs(fromSI(si, d, u, &ok));
    if (ok && v > 0 && miss(v) < nearest) {
      nearest = miss(v);
      best = u;
    }
  }
  return best;
}

std::string formatValue(double si, int d, int unit) {
  bool ok = false;
  const double v = fromSI(si, d, unit, &ok);
  units::NumberFormat f;
  f.digits = 6;
  f.sciHigh = 9;
  f.sciLow = -4;
  return units::formatNumber(ok ? v : si, f);
}

// ---- The library's lookups ----

int groupCount() { return lib::groupTotal(); }

const Group *group(int i) {
  if (i < 0 || i >= lib::groupTotal()) return nullptr;
  return &lib::groups()[i];
}

int formulaCount() {
  int n = 0;
  for (int p = 0; p < lib::partCount(); p++) n += lib::part(p).count;
  return n;
}

const Formula *formula(int i) {
  if (i < 0) return nullptr;
  for (int p = 0; p < lib::partCount(); p++) {
    const lib::Part &part = lib::part(p);
    if (i < part.count) return &part.formulas[i];
    i -= part.count;
  }
  return nullptr;
}

int findFormula(const char *id) {
  if (!id) return -1;
  const int n = formulaCount();
  for (int i = 0; i < n; i++)
    if (!strcmp(formula(i)->id, id)) return i;
  return -1;
}

int inGroup(int g, int *out, int max) {
  int count = 0;
  const int n = formulaCount();
  for (int i = 0; i < n && count < max; i++)
    if (formula(i)->group == g) out[count++] = i;
  return count;
}

int quickList(int *out, int max) {
  int count = 0;
  for (const char *const *id = lib::quickIds(); *id && count < max; id++) {
    const int i = findFormula(*id);
    if (i >= 0) out[count++] = i;
  }
  return count;
}

// ---- Search ----

namespace {

// Splits lower-cased text into words: letters, digits and anything non-ASCII (so a
// typed θ is a word); everything else separates.
void splitWords(const char *text, std::vector<std::string> *out) {
  std::string w;
  for (const char *p = text; p && *p; p++) {
    const char c = *p;
    if (isAlpha(c) || isDigit(c) || ((unsigned char)c & 0x80)) {
      w += lower(c);
    } else if (!w.empty()) {
      out->push_back(w);
      w.clear();
    }
  }
  if (!w.empty()) out->push_back(w);
}

// How well one query word fits one field: 3 the same word, 2 the same but for a plural
// 's', 1 the start of a word, 0 not at all.
int fieldMatch(const std::string &q, const char *field) {
  std::vector<std::string> words;
  splitWords(field, &words);
  int best = 0;
  const bool plural = q.size() > 2 && q.back() == 's';
  for (const std::string &w : words) {
    if (w == q) return 3;
    if (plural && w.size() + 1 == q.size() && !q.compare(0, w.size(), w)) best = std::max(best, 2);
    // One letter only ever means itself: "f" of "f=ma" is not "field".
    if (q.size() > 1 && w.size() > q.size() && !w.compare(0, q.size(), q)) best = std::max(best, 1);
  }
  return best;
}

int wordScore(const std::string &q, const Formula *f) {
  static const int kName[] = {0, 90, 96, 100};
  static const int kKeyword[] = {0, 40, 56, 60};
  static const int kGroup[] = {0, 12, 18, 20};
  static const int kVar[] = {0, 10, 24, 30};
  int s = kName[fieldMatch(q, f->name)];
  s = std::max(s, kKeyword[fieldMatch(q, f->keywords)]);
  const Group *g = group(f->group);
  if (g) s = std::max(s, kGroup[fieldMatch(q, g->name)]);
  for (int v = 0; v < f->varCount; v++) {
    s = std::max(s, kVar[fieldMatch(q, f->vars[v].name)]);
    if (fieldMatch(q, f->vars[v].sym) == 3) s = std::max(s, kVar[3]);
  }
  return s;
}

}  // namespace

int search(const char *query, int *out, int max) {
  std::vector<std::string> words;
  splitWords(query, &words);
  if (words.empty()) return 0;
  std::vector<std::pair<int, int>> hits;  // score, index
  const int n = formulaCount();
  for (int i = 0; i < n; i++) {
    int total = 0;
    bool all = true;
    for (const std::string &w : words) {
      const int s = wordScore(w, formula(i));
      if (!s) {
        all = false;
        break;
      }
      total += s;
    }
    if (all) hits.push_back({total, i});
  }
  std::stable_sort(hits.begin(), hits.end(),
                   [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.first > b.first; });
  int count = 0;
  for (const auto &h : hits) {
    if (count >= max) break;
    out[count++] = h.second;
  }
  return count;
}

// ---- Constants ----

namespace {

// CODATA 2022 where measured; the rest exact in the 2019 SI.
constexpr Constant kConstants[] = {
  {"pi", "π", "Pi", kPi},
  {"c", "c", "Speed of light", 299792458.0},
  {"h", "h", "Planck constant", 6.62607015e-34},
  {"hbar", "h_{bar}", "Reduced Planck constant", 1.054571817e-34},
  {"kB", "k_{B}", "Boltzmann constant", 1.380649e-23},
  {"NA", "N_{A}", "Avogadro constant", 6.02214076e23},
  {"Rgas", "R", "Molar gas constant", 8.314462618},
  {"qe", "e", "Elementary charge", 1.602176634e-19},
  {"eps0", "ε_{0}", "Vacuum permittivity", 8.8541878188e-12},
  {"mu0", "μ_{0}", "Vacuum permeability", 1.25663706127e-6},
  {"sigma", "σ", "Stefan-Boltzmann constant", 5.670374419e-8},
  {"Gnewton", "G", "Gravitational constant", 6.67430e-11},
  {"g0", "g_{0}", "Standard gravity", 9.80665},
  {"F", "F", "Faraday constant", 96485.33212},
  // Beyond the header's list.
  {"bWien", "b", "Wien displacement constant", 2.897771955e-3},
  {"c0", "c°", "Standard concentration (1 mol/L)", 1000.0},
};
constexpr int kConstantCount = sizeof kConstants / sizeof kConstants[0];

}  // namespace

int constantCount() { return kConstantCount; }
const Constant *constant(int i) { return i >= 0 && i < kConstantCount ? &kConstants[i] : nullptr; }

// ---- Expressions ----

namespace {

using Fn1 = double (*)(double);
struct Function {
  const char *name;
  int args;
  Fn1 fn;  // nullptr for the two-argument ones
};
double fnSqrt(double x) { return std::sqrt(x); }
double fnCbrt(double x) { return std::cbrt(x); }
double fnAbs(double x) { return std::fabs(x); }
double fnExp(double x) { return std::exp(x); }
double fnLn(double x) { return std::log(x); }
double fnLog10(double x) { return std::log10(x); }
double fnSin(double x) { return std::sin(x); }
double fnCos(double x) { return std::cos(x); }
double fnTan(double x) { return std::tan(x); }
double fnAsin(double x) { return std::asin(x); }
double fnAcos(double x) { return std::acos(x); }
double fnAtan(double x) { return std::atan(x); }
double fnSinh(double x) { return std::sinh(x); }
double fnCosh(double x) { return std::cosh(x); }
double fnTanh(double x) { return std::tanh(x); }
double fnAsinh(double x) { return std::asinh(x); }
double fnAcosh(double x) { return std::acosh(x); }
double fnAtanh(double x) { return std::atanh(x); }

constexpr Function kFunctions[] = {
  {"sqrt", 1, fnSqrt}, {"cbrt", 1, fnCbrt}, {"abs", 1, fnAbs}, {"exp", 1, fnExp},
  {"ln", 1, fnLn}, {"log10", 1, fnLog10}, {"sin", 1, fnSin}, {"cos", 1, fnCos},
  {"tan", 1, fnTan}, {"asin", 1, fnAsin}, {"acos", 1, fnAcos}, {"atan", 1, fnAtan},
  {"sinh", 1, fnSinh}, {"cosh", 1, fnCosh}, {"tanh", 1, fnTanh}, {"asinh", 1, fnAsinh},
  {"acosh", 1, fnAcosh}, {"atanh", 1, fnAtanh},
  {"atan2", 2, nullptr},  // atan2(y, x)
  {"log", 2, nullptr},    // log(x, base)
};

const Function *findFunction(const char *name, size_t n) {
  for (const Function &f : kFunctions)
    if (strlen(f.name) == n && !strncmp(f.name, name, n)) return &f;
  return nullptr;
}

// Nodes live here; a deque never moves what it holds.
class Arena {
 public:
  Node *add(Node::Op op) {
    nodes_.emplace_back();
    nodes_.back().op = op;
    return &nodes_.back();
  }
  const Node *number(double v) {
    Node *n = add(Node::Num);
    n->value = v;
    return n;
  }
  const Node *constant(double v, const char *label) {
    Node *n = add(Node::Const);
    n->value = v;
    n->name = label;
    return n;
  }
  const Node *var(int i) {
    Node *n = add(Node::Var);
    n->var = i;
    return n;
  }
  const Node *binary(Node::Op op, const Node *a, const Node *b) {
    Node *n = add(op);
    n->a = a;
    n->b = b;
    return n;
  }
  const Node *neg(const Node *a) {
    Node *n = add(Node::Neg);
    n->a = a;
    return n;
  }
  // The name must be the function table's or a literal: nodes only point at it.
  const Node *func(const char *name, const Node *a, const Node *b = nullptr) {
    Node *n = add(Node::Func);
    n->name = name;
    n->a = a;
    n->b = b;
    return n;
  }
  void clear() { nodes_.clear(); }

 private:
  std::deque<Node> nodes_;
};

// Recursive descent over "expr = expr". Precedence as units::evalExpression: ^ is
// right associative and binds tighter than unary minus.
class Parser {
 public:
  Parser(const char *text, const Formula *f, Arena *arena) : p_(text), f_(f), arena_(arena) {}

  bool equation(const Node **lhs, const Node **rhs) {
    *lhs = sum();
    skip();
    if (!ok_ || *p_ != '=') return false;
    p_++;
    *rhs = sum();
    skip();
    return ok_ && *p_ == 0;
  }

 private:
  void skip() {
    while (isSpace(*p_)) p_++;
  }
  const Node *fail() {
    ok_ = false;
    return arena_->number(0);
  }
  const Node *sum() {
    const Node *n = product();
    while (ok_) {
      skip();
      if (*p_ == '+') {
        p_++;
        n = arena_->binary(Node::Add, n, product());
      } else if (*p_ == '-') {
        p_++;
        n = arena_->binary(Node::Sub, n, product());
      } else {
        break;
      }
    }
    return n;
  }
  const Node *product() {
    const Node *n = unary();
    while (ok_) {
      skip();
      if (*p_ == '*') {
        p_++;
        n = arena_->binary(Node::Mul, n, unary());
      } else if (*p_ == '/') {
        p_++;
        n = arena_->binary(Node::Div, n, unary());
      } else {
        break;
      }
    }
    return n;
  }
  const Node *unary() {
    skip();
    if (*p_ == '-') {
      p_++;
      return arena_->neg(unary());
    }
    return power();
  }
  const Node *power() {
    const Node *n = atom();
    skip();
    if (ok_ && *p_ == '^') {
      p_++;
      return arena_->binary(Node::Pow, n, unary());
    }
    return n;
  }
  const Node *atom() {
    skip();
    if (*p_ == '(') {
      p_++;
      const Node *n = sum();
      skip();
      if (*p_ != ')') return fail();
      p_++;
      return n;
    }
    if (isDigit(*p_) || *p_ == '.') {
      char *end = nullptr;
      const double v = strtod(p_, &end);
      if (end == p_) return fail();
      p_ = end;
      return arena_->number(v);
    }
    if (isAlpha(*p_) || *p_ == '_') {
      const char *start = p_;
      while (isAlpha(*p_) || isDigit(*p_) || *p_ == '_') p_++;
      const size_t n = (size_t)(p_ - start);
      for (int i = 0; i < f_->varCount; i++) {
        const char *sym = f_->vars[i].sym;
        if (strlen(sym) == n && !strncmp(sym, start, n)) return arena_->var(i);
      }
      skip();
      if (*p_ == '(') return call(start, n);
      for (const Constant &c : kConstants)
        if (strlen(c.sym) == n && !strncmp(c.sym, start, n)) return arena_->constant(c.value, c.label);
    }
    return fail();
  }
  const Node *call(const char *name, size_t n) {
    const Function *fn = findFunction(name, n);
    if (!fn) return fail();
    p_++;
    const Node *a = sum();
    const Node *b = nullptr;
    skip();
    if (fn->args == 2) {
      if (*p_ != ',') return fail();
      p_++;
      b = sum();
      skip();
    }
    if (*p_ != ')') return fail();
    p_++;
    return arena_->func(fn->name, a, b);
  }

  const char *p_;
  const Formula *f_;
  Arena *arena_;
  bool ok_ = true;
};

// Evaluates, and also says how big the numbers that went into the result were, so a
// sum that cancels to nothing can be judged against its terms.
bool evalScaled(const Node *n, const double *vars, double *out, double *scale) {
  double a = 0, b = 0, sa = 0, sb = 0;
  switch (n->op) {
    case Node::Num:
    case Node::Const:
      *out = n->value;
      *scale = std::fabs(n->value);
      return true;
    case Node::Var:
      *out = vars[n->var];
      *scale = std::fabs(*out);
      return std::isfinite(*out);
    case Node::Neg:
      if (!evalScaled(n->a, vars, &a, &sa)) return false;
      *out = -a;
      *scale = sa;
      return true;
    case Node::Add:
    case Node::Sub:
    case Node::Mul:
    case Node::Div:
    case Node::Pow:
      if (!evalScaled(n->a, vars, &a, &sa) || !evalScaled(n->b, vars, &b, &sb)) return false;
      switch (n->op) {
        case Node::Add: *out = a + b; *scale = sa + sb; break;
        case Node::Sub: *out = a - b; *scale = sa + sb; break;
        case Node::Mul: *out = a * b; *scale = sa * sb; break;
        case Node::Div:
          if (b == 0) return false;
          *out = a / b;
          *scale = sa / std::fabs(b);
          break;
        default:
          *out = std::pow(a, b);
          *scale = std::fabs(*out);
          break;
      }
      return std::isfinite(*out);
    case Node::Func:
      if (!evalScaled(n->a, vars, &a, &sa)) return false;
      if (n->b && !evalScaled(n->b, vars, &b, &sb)) return false;
      if (!strcmp(n->name, "atan2")) {
        *out = std::atan2(a, b);
      } else if (!strcmp(n->name, "log")) {
        *out = std::log(a) / std::log(b);
      } else {
        const Function *fn = findFunction(n->name, strlen(n->name));
        if (!fn || !fn->fn) return false;
        *out = fn->fn(a);
      }
      // A square root keeps the terms' scale so that sqrt(a^2 - b^2) near zero is
      // not judged on its tiny result.
      *scale = !strcmp(n->name, "sqrt") ? std::sqrt(sa) : std::fabs(*out);
      return std::isfinite(*out);
  }
  return false;
}

int occurrences(const Node *n, int var) {
  if (!n) return 0;
  if (n->op == Node::Var) return n->var == var ? 1 : 0;
  return occurrences(n->a, var) + occurrences(n->b, var);
}

uint32_t varsIn(const Node *n) {
  if (!n) return 0;
  if (n->op == Node::Var) return 1u << n->var;
  return varsIn(n->a) | varsIn(n->b);
}

// Rearranges side = other for the variable, which appears once in side. Each place an
// inverse has two branches (a square root's sign, sin's other angle) uses one bit of
// `branches`; *branchPoints says how many places there were.
const Node *invert(const Node *side, const Node *other, int var, unsigned branches, int *branchPoints,
                   Arena *ar) {
  auto branch = [&]() {
    const bool second = (branches >> *branchPoints) & 1;
    ++*branchPoints;
    return second;
  };
  while (side->op != Node::Var) {
    const bool inA = occurrences(side->a, var) > 0;
    const Node *a = side->a;
    const Node *b = side->b;
    switch (side->op) {
      case Node::Add:
        other = ar->binary(Node::Sub, other, inA ? b : a);
        side = inA ? a : b;
        break;
      case Node::Sub:
        other = inA ? ar->binary(Node::Add, other, b) : ar->binary(Node::Sub, a, other);
        side = inA ? a : b;
        break;
      case Node::Mul:
        other = ar->binary(Node::Div, other, inA ? b : a);
        side = inA ? a : b;
        break;
      case Node::Div:
        other = inA ? ar->binary(Node::Mul, other, b) : ar->binary(Node::Div, a, other);
        side = inA ? a : b;
        break;
      case Node::Neg:
        other = ar->neg(other);
        side = a;
        break;
      case Node::Pow:
        if (inA) {
          const bool num = b->op == Node::Num;
          const double e = num ? b->value : 0;
          const bool even = num && e != 0 && std::fmod(e, 2.0) == 0;
          if (num && e == 2) {
            other = ar->func("sqrt", other);
          } else if (num && e == 3) {
            other = ar->func("cbrt", other);
          } else if (num && e == 0.5) {
            other = ar->binary(Node::Pow, other, ar->number(2));
          } else if (num && e == -1) {
            other = ar->binary(Node::Div, ar->number(1), other);
          } else if (num) {
            other = ar->binary(Node::Pow, other, ar->number(1 / e));
          } else {
            other = ar->binary(Node::Pow, other, ar->binary(Node::Div, ar->number(1), b));
          }
          if (even && branch()) other = ar->neg(other);
          side = a;
        } else {
          if (a->op == Node::Num && a->value == 10) {
            other = ar->func("log10", other);
          } else {
            other = ar->binary(Node::Div, ar->func("ln", other), ar->func("ln", a));
          }
          side = b;
        }
        break;
      case Node::Func: {
        const char *f = side->name;
        if (!strcmp(f, "atan2")) {
          other = inA ? ar->binary(Node::Mul, b, ar->func("tan", other))
                      : ar->binary(Node::Div, a, ar->func("tan", other));
          side = inA ? a : b;
          break;
        }
        if (!strcmp(f, "log")) {
          other = inA ? ar->binary(Node::Pow, b, other)
                      : ar->binary(Node::Pow, a, ar->binary(Node::Div, ar->number(1), other));
          side = inA ? a : b;
          break;
        }
        side = a;
        if (!strcmp(f, "sqrt")) {
          other = ar->binary(Node::Pow, other, ar->number(2));
        } else if (!strcmp(f, "cbrt")) {
          other = ar->binary(Node::Pow, other, ar->number(3));
        } else if (!strcmp(f, "abs")) {
          if (branch()) other = ar->neg(other);
        } else if (!strcmp(f, "exp")) {
          other = ar->func("ln", other);
        } else if (!strcmp(f, "ln")) {
          other = ar->func("exp", other);
        } else if (!strcmp(f, "log10")) {
          other = ar->binary(Node::Pow, ar->number(10), other);
        } else if (!strcmp(f, "sin")) {
          other = ar->func("asin", other);
          if (branch()) other = ar->binary(Node::Sub, ar->constant(kPi, "π"), other);
        } else if (!strcmp(f, "cos")) {
          other = ar->func("acos", other);
          if (branch()) other = ar->neg(other);
        } else if (!strcmp(f, "tan")) {
          other = ar->func("atan", other);
        } else if (!strcmp(f, "asin")) {
          other = ar->func("sin", other);
        } else if (!strcmp(f, "acos")) {
          other = ar->func("cos", other);
        } else if (!strcmp(f, "atan")) {
          other = ar->func("tan", other);
        } else if (!strcmp(f, "sinh")) {
          other = ar->func("asinh", other);
        } else if (!strcmp(f, "asinh")) {
          other = ar->func("sinh", other);
        } else if (!strcmp(f, "cosh")) {
          other = ar->func("acosh", other);
          if (branch()) other = ar->neg(other);
        } else if (!strcmp(f, "acosh")) {
          other = ar->func("cosh", other);
        } else if (!strcmp(f, "tanh")) {
          other = ar->func("atanh", other);
        } else if (!strcmp(f, "atanh")) {
          other = ar->func("tanh", other);
        } else {
          return nullptr;
        }
        break;
      }
      default:
        return nullptr;
    }
  }
  return other;
}

// One equation of the sheet's formula.
struct Eq {
  const Node *lhs = nullptr;
  const Node *rhs = nullptr;
  uint32_t vars = 0;
  uint8_t count[kMaxVars] = {};
};

// Both sides agree, judged against the size of the numbers that went into them.
// Evaluation failing is `failed`, which is neither agreement nor disagreement.
constexpr double kAgree = 1e-7;
enum class Fit { Yes, No, Failed };

Fit fits(const Eq &e, const double *vals) {
  double l = 0, r = 0, sl = 0, sr = 0;
  if (!evalScaled(e.lhs, vals, &l, &sl) || !evalScaled(e.rhs, vals, &r, &sr)) return Fit::Failed;
  return std::fabs(l - r) <= kAgree * (sl + sr) + 1e-300 ? Fit::Yes : Fit::No;
}

constexpr int kMaxRoots = 4;

struct Root {
  double x = 0;
  const Node *rhs = nullptr;   // the inverted tree, or nullptr for a numeric root
};

void addRoot(Root *roots, int *n, double x, const Node *rhs) {
  for (int i = 0; i < *n; i++)
    if (std::fabs(roots[i].x - x) <= 1e-9 * std::fmax(std::fabs(roots[i].x), std::fabs(x))) return;
  if (*n < kMaxRoots) {
    roots[*n].x = x;
    roots[*n].rhs = rhs;
    ++*n;
  }
}

// Positive first, smallest of them first, then zero, then the negatives nearest zero.
int rootClass(double x) { return x > 0 ? 0 : (x == 0 ? 1 : 2); }
void sortRoots(Root *roots, int n) {
  std::stable_sort(roots, roots + n, [](const Root &a, const Root &b) {
    const int ca = rootClass(a.x), cb = rootClass(b.x);
    if (ca != cb) return ca < cb;
    return std::fabs(a.x) < std::fabs(b.x);
  });
}

// lhs - rhs with the variable set to x.
struct Residual {
  const Eq *eq;
  double *vals;
  int var;
  bool at(double x, double *f, double *scale) const {
    vals[var] = x;
    double l = 0, r = 0, sl = 0, sr = 0;
    if (!evalScaled(eq->lhs, vals, &l, &sl) || !evalScaled(eq->rhs, vals, &r, &sr)) return false;
    *f = l - r;
    if (scale) *scale = sl + sr;
    return std::isfinite(*f);
  }
  // A genuine root, not rounding: the residual is nothing beside the terms, and the
  // sides are not far below the size of what made them. V/R*(1 - exp(-R*t/L)) with R
  // near 1e-16 is a sawtooth of rounding that crosses anything. A side of plain 0 is
  // the exception: a x^2 + b x + c = 0 cancels to nothing at a root.
  bool rootAt(double x) const {
    vals[var] = x;
    double l = 0, r = 0, sl = 0, sr = 0;
    if (!evalScaled(eq->lhs, vals, &l, &sl) || !evalScaled(eq->rhs, vals, &r, &sr)) return false;
    const double diff = std::fabs(l - r), size = std::fabs(l) + std::fabs(r), scale = sl + sr;
    if (diff > 1e-9 * scale + 1e-300) return false;
    return l == 0 || r == 0 || size * 1e7 >= scale;
  }
};

// Closes in on a sign change in [a, b], and keeps it only if the residual really goes
// to nothing there (a pole changes sign too).
void bisect(const Residual &res, double a, double fa, double b, Root *roots, int *n) {
  double fb = 0;
  if (!res.at(b, &fb, nullptr)) return;
  const double start = std::fmin(std::fabs(fa), std::fabs(fb));
  for (int i = 0; i < 200; i++) {
    const double m = a + (b - a) / 2;
    if (m == a || m == b) break;
    double fm = 0;
    if (!res.at(m, &fm, nullptr)) return;
    if (fm == 0) {
      a = b = m;
      break;
    }
    if ((fa < 0) == (fm < 0)) {
      a = m;
      fa = fm;
    } else {
      b = m;
    }
  }
  double f = 0;
  const double x = a + (b - a) / 2;
  // The residual has to fall away from where it started, too: a pole changes sign.
  if (res.at(x, &f, nullptr) && std::fabs(f) <= 1e-4 * start && res.rootAt(x)) addRoot(roots, n, x, nullptr);
}

// A dip in |f| that did not cross zero between samples: two close roots or a double
// one. Golden section down to the bottom of the dip, then look there.
void dip(const Residual &res, double a, double b, Root *roots, int *n) {
  constexpr double kGold = 0.6180339887498949;
  double fa = 0, fb = 0, sign = 0, f = 0;
  if (!res.at(a, &fa, nullptr) || !res.at(b, &fb, nullptr)) return;
  sign = fa < 0 ? -1 : 1;
  const double lo0 = a, hi0 = b;
  double c = b - kGold * (b - a), d = a + kGold * (b - a), fc = 0, fd = 0;
  if (!res.at(c, &fc, nullptr) || !res.at(d, &fd, nullptr)) return;
  for (int i = 0; i < 120; i++) {
    if (fc * sign < 0 || fd * sign < 0) break;
    if (fc * sign < fd * sign) {
      b = d;
      d = c;
      fd = fc;
      c = b - kGold * (b - a);
      if (!res.at(c, &fc, nullptr)) return;
    } else {
      a = c;
      c = d;
      fc = fd;
      d = a + kGold * (b - a);
      if (!res.at(d, &fd, nullptr)) return;
    }
    if (std::fabs(b - a) <= 1e-13 * std::fabs(a + b)) break;
  }
  const double x = fc * sign < fd * sign ? c : d;
  if (!res.at(x, &f, nullptr)) return;
  if (f * sign < 0) {
    bisect(res, lo0, fa, x, roots, n);
    bisect(res, x, f, hi0, roots, n);
  } else if (res.rootAt(x)) {
    addRoot(roots, n, x, nullptr);
  }
}

// Scans for sign changes: linearly over a turn for an angle, otherwise log-spaced over
// everything from far below an electron's mass to far above the Sun's, both signs and
// zero unless the variable is positive.
void numericRoots(const Eq &eq, double *vals, int var, const Var &v, Root *roots, int *n) {
  Residual res{&eq, vals, var};
  constexpr int kPerDecade = 12;
  constexpr int kDecades = 80;   // 1e-40 .. 1e40
  constexpr int kSide = kPerDecade * kDecades;
  const bool angle = v.dim == units::kAngle;
  const bool positive = (v.flags & kPositive) || (v.flags & kAcute);
  int first, last;
  if (angle) {
    first = 0;
    last = 720;
  } else {
    first = positive ? kSide + 1 : 0;
    last = 2 * kSide + 1;
  }
  auto gridAt = [&](int i) -> double {
    if (angle) {
      const double lo = (v.flags & kAcute) ? 1e-9 : (positive ? 1e-9 : -kPi + 1e-9);
      const double hi = (v.flags & kAcute) ? kPi / 2 : (positive ? 2 * kPi - 1e-9 : kPi);
      return lo + (hi - lo) * i / 720;
    }
    if (i == kSide) return 0;
    const int k = i < kSide ? kSide - 1 - i : i - kSide - 1;  // 0 at the smallest magnitude
    const double m = std::pow(10.0, -40.0 + (double)k / kPerDecade);
    return i < kSide ? -m : m;
  };
  bool have1 = false, have2 = false;   // the two previous samples
  double x1 = 0, f1 = 0, x2 = 0, f2 = 0;
  for (int i = first; i <= last; i++) {
    const double x = gridAt(i);
    double f = 0, scale = 0;
    if (!res.at(x, &f, &scale)) {
      have1 = have2 = false;
      continue;
    }
    if (f == 0) {
      addRoot(roots, n, x, nullptr);
    } else if (have1 && f1 != 0 && (f < 0) != (f1 < 0)) {
      bisect(res, x1, f1, x, roots, n);
    } else if (have1 && have2 && f1 != 0 && (f < 0) == (f1 < 0) && (f1 < 0) == (f2 < 0) &&
               std::fabs(f1) < std::fabs(f) && std::fabs(f1) < std::fabs(f2)) {
      dip(res, x2, x, roots, n);
    }
    x2 = x1;
    f2 = f1;
    have2 = have1;
    x1 = x;
    f1 = f;
    have1 = true;
  }
}

// A variable's label without the {} of its subscript, for messages.
std::string plainLabel(const Var &v) {
  std::string s;
  for (const char *p = v.label; *p; p++)
    if (*p != '_' && *p != '{' && *p != '}') s += *p;
  return s;
}

// One attempt at the whole sheet, taking given branches at each choice between roots.
struct Run {
  double val[kMaxVars] = {};
  bool known[kMaxVars] = {};
  bool solved[kMaxVars] = {};
  const Node *howL[kMaxVars] = {};
  const Node *howR[kMaxVars] = {};
  std::string error;
  bool complete = false;  // nothing went wrong

  struct Choice {
    int var;
    int count;
    int taken;
  };
  Choice choices[kMaxVars] = {};
  int choiceCount = 0;
};

}  // namespace

bool evaluate(const Node *n, const double *vars, double *out) {
  double s = 0;
  return n && out && evalScaled(n, vars, out, &s);
}

bool equation(const Formula *f, int e, const Node **lhs, const Node **rhs) {
  static Arena arena;
  static const Formula *cached = nullptr;
  static std::vector<std::pair<const Node *, const Node *>> trees;
  if (!f || e < 0 || e >= f->equationCount) return false;
  if (cached != f) {
    arena.clear();
    trees.clear();
    for (int i = 0; i < f->equationCount; i++) {
      const Node *l = nullptr, *r = nullptr;
      Parser p(f->equations[i], f, &arena);
      if (!p.equation(&l, &r)) l = r = nullptr;
      trees.push_back({l, r});
    }
    cached = f;
  }
  if (!trees[e].first) return false;
  if (lhs) *lhs = trees[e].first;
  if (rhs) *rhs = trees[e].second;
  return true;
}

// ---- The sheet ----

struct Sheet::Impl {
  const Formula *f = nullptr;
  Arena parsed;     // the formula's equations
  Arena solving;    // inverted trees of the last solve
  std::vector<Eq> eqs;

  Cell cell[kMaxVars] = {};
  double val[kMaxVars] = {};
  uint32_t age[kMaxVars] = {};   // when each Entered cell was typed
  bool released[kMaxVars] = {};  // a kConstant being solved for
  uint32_t clock = 0;

  const Node *howL[kMaxVars] = {};
  const Node *howR[kMaxVars] = {};
  std::string error;
  int missing = 0;
  int altVar = -1;
  double altValue = 0;

  void reset();
  void solve();
  int capacity() const;
  int enteredCount() const;
  void propagate(Run *run, const int *path, int pathLen);
  int rootsFrom(const Eq &e, int var, Run *run, Root *roots, const Node **lhs, std::string *why);
  bool allowed(const Var &v, double x) const;
};

// What a variable's flags permit.
bool Sheet::Impl::allowed(const Var &v, double x) const {
  if ((v.flags & kPositive) && x <= 0) return false;
  if ((v.flags & kAcute) && (x < 0 || x > kPi / 2 * (1 + 1e-12))) return false;
  return true;
}

static std::string limitMessage(const Var &v) {
  if (v.flags & kAcute) return "Needs " + plainLabel(v) + " from 0 to 90 deg";
  return "Needs " + plainLabel(v) + " above 0";
}

void Sheet::Impl::reset() {
  for (int i = 0; i < kMaxVars; i++) {
    cell[i] = Cell::Empty;
    val[i] = 0;
    age[i] = 0;
    released[i] = false;
    howL[i] = howR[i] = nullptr;
  }
  if (f) {
    for (int i = 0; i < f->varCount; i++) {
      if (f->vars[i].flags & kConstant) {
        cell[i] = Cell::Preset;
        val[i] = f->vars[i].preset;
      }
    }
  }
  clock = 0;
  error.clear();
  altVar = -1;
  altValue = 0;
  missing = 0;
}

int Sheet::Impl::capacity() const {
  int n = f->needed;
  for (int i = 0; i < f->varCount; i++)
    if (released[i]) n++;
  return n;
}

int Sheet::Impl::enteredCount() const {
  int n = 0;
  for (int i = 0; i < f->varCount; i++)
    if (cell[i] == Cell::Entered) n++;
  return n;
}

// The solutions of one equation for its one unknown, each checked against the equation
// and the variable's limits. Returns how many; *why says what ruled the rest out.
int Sheet::Impl::rootsFrom(const Eq &e, int var, Run *run, Root *roots, const Node **lhs, std::string *why) {
  const Var &v = f->vars[var];
  Root found[kMaxRoots];
  int n = 0;
  double vals[kMaxVars];
  memcpy(vals, run->val, sizeof vals);
  *lhs = nullptr;
  const int occurs = e.count[var];
  if (e.lhs->op == Node::Var && e.lhs->var == var && occurs == 1) {
    double x = 0;
    if (evaluate(e.rhs, vals, &x)) addRoot(found, &n, x, e.rhs);
  } else if (occurs == 1) {
    const bool left = occurrences(e.lhs, var) > 0;
    unsigned tries = 1;
    for (unsigned b = 0; b < tries && b < 8; b++) {
      int points = 0;
      const Node *rhs = invert(left ? e.lhs : e.rhs, left ? e.rhs : e.lhs, var, b, &points, &solving);
      if (points > 0 && b == 0) tries = 1u << points;
      double x = 0;
      if (rhs && evaluate(rhs, vals, &x)) addRoot(found, &n, x, rhs);
    }
  } else {
    numericRoots(e, vals, var, v, found, &n);
    memcpy(vals, run->val, sizeof vals);
  }

  int kept = 0;
  bool limited = false;
  for (int i = 0; i < n; i++) {
    vals[var] = found[i].x;
    // An inverse can add what the equation does not allow: sqrt(x) = -3 gives x = 9.
    if (fits(e, vals) != Fit::Yes) continue;
    if (!allowed(v, found[i].x)) {
      limited = true;
      continue;
    }
    roots[kept++] = found[i];
  }
  if (kept == 0) *why = limited ? limitMessage(v) : "No real solution for " + plainLabel(v);
  sortRoots(roots, kept);
  if (kept > 0 && occurs > 1) *lhs = e.lhs;
  return kept;
}

// Takes equations with one unknown until there are none. `path` gives the root to take
// at each choice so far; past its end the first root is taken.
void Sheet::Impl::propagate(Run *run, const int *path, int pathLen) {
  const int nv = f->varCount;
  run->complete = false;
  for (int i = 0; i < nv; i++) {
    run->known[i] = cell[i] == Cell::Entered || cell[i] == Cell::Preset;
    run->val[i] = run->known[i] ? val[i] : 0;
    if (run->known[i] && !allowed(f->vars[i], val[i])) {
      run->error = limitMessage(f->vars[i]);
      return;
    }
  }

  for (;;) {
    uint32_t knownMask = 0;
    for (int i = 0; i < nv; i++)
      if (run->known[i]) knownMask |= 1u << i;

    // The best equation there is: unknown alone on the left, then appearing once, then
    // anything. A numeric solve waits while an exact one could still help.
    int bestVar = -1, bestRank = 99;
    for (const Eq &e : eqs) {
      const uint32_t unknown = e.vars & ~knownMask;
      if (!unknown || (unknown & (unknown - 1))) continue;
      int u = 0;
      while (!((unknown >> u) & 1)) u++;
      int rank = e.count[u] > 1 ? 2 : 1;
      if (e.lhs->op == Node::Var && e.lhs->var == u && e.count[u] == 1) rank = 0;
      if (rank < bestRank) {
        bestRank = rank;
        bestVar = u;
      }
    }
    if (bestVar < 0) break;

    // That variable's equations, best first, until one gives an answer. A later one
    // gets its turn when the first fails: t = (v-u)/a with a = 0.
    Root roots[kMaxRoots];
    int n = 0;
    const Node *numericLhs = nullptr;
    const Node *numericRhs = nullptr;
    const Eq *from = nullptr;
    std::string why;
    for (int rank = 0; rank <= 2 && n == 0; rank++) {
      for (const Eq &e : eqs) {
        if ((e.vars & ~knownMask) != (1u << bestVar)) continue;
        int r = e.count[bestVar] > 1 ? 2 : 1;
        if (e.lhs->op == Node::Var && e.lhs->var == bestVar && e.count[bestVar] == 1) r = 0;
        if (r != rank) continue;
        std::string reason;
        n = rootsFrom(e, bestVar, run, roots, &numericLhs, &reason);
        if (n > 0) {
          from = &e;
          numericRhs = e.rhs;
          break;
        }
        if (why.empty()) why = reason;
      }
    }
    if (n == 0) {
      run->error = why;
      return;
    }

    // The variable's other equations have their say: of sin's two angles usually only
    // one also fits the law of cosines.
    double vals[kMaxVars];
    memcpy(vals, run->val, sizeof vals);
    int kept = 0;
    for (int i = 0; i < n; i++) {
      vals[bestVar] = roots[i].x;
      bool good = true;
      for (const Eq &e : eqs) {
        if (&e == from || (e.vars & ~knownMask) != (1u << bestVar)) continue;
        if (fits(e, vals) == Fit::No) good = false;
      }
      if (good) roots[kept++] = roots[i];
    }
    if (kept == 0) {
      run->error = "These values don't fit together";
      return;
    }

    int take = 0;
    if (kept > 1) {
      const int c = run->choiceCount;
      if (c < pathLen) take = std::min(path[c], kept - 1);
      if (c < kMaxVars) {
        run->choices[c] = Run::Choice{bestVar, kept, take};
        run->choiceCount++;
      }
    }
    const Root &r = roots[take];
    run->val[bestVar] = r.x;
    run->known[bestVar] = true;
    run->solved[bestVar] = true;
    if (numericLhs) {
      run->howL[bestVar] = numericLhs;
      run->howR[bestVar] = numericRhs;
    } else {
      run->howL[bestVar] = solving.var(bestVar);
      run->howR[bestVar] = r.rhs;
    }
  }

  // Everything that has all its values must hold.
  for (const Eq &e : eqs) {
    bool all = true;
    for (int i = 0; i < nv; i++)
      if (((e.vars >> i) & 1) && !run->known[i]) all = false;
    if (all && fits(e, run->val) == Fit::No) {
      run->error = "These values don't fit together";
      return;
    }
  }
  run->complete = true;
}

void Sheet::Impl::solve() {
  error.clear();
  altVar = -1;
  altValue = 0;
  missing = 0;
  for (int i = 0; i < kMaxVars; i++) {
    howL[i] = howR[i] = nullptr;
    if (cell[i] == Cell::Solved) {
      cell[i] = Cell::Empty;
      val[i] = 0;
    }
  }
  if (!f) return;
  solving.clear();

  // First roots first; on a dead end, step to the next combination of roots.
  Run first, good;
  bool have = false;
  int path[kMaxVars] = {};
  int pathLen = 0;
  for (int attempt = 0; attempt < 12 && !have; attempt++) {
    Run run;
    propagate(&run, path, pathLen);
    if (attempt == 0) first = run;
    if (run.complete) {
      good = run;
      have = true;
      break;
    }
    int j = run.choiceCount - 1;
    while (j >= 0 && run.choices[j].taken + 1 >= run.choices[j].count) j--;
    if (j < 0) break;
    for (int i = 0; i < j; i++) path[i] = run.choices[i].taken;
    path[j] = run.choices[j].taken + 1;
    pathLen = j + 1;
  }
  const Run &use = have ? good : first;
  if (!have) error = first.error;

  for (int i = 0; i < f->varCount; i++) {
    if (use.solved[i]) {
      cell[i] = Cell::Solved;
      val[i] = use.val[i];
      howL[i] = use.howL[i];
      howR[i] = use.howR[i];
    }
  }

  // A second solution is the same sheet with one choice taken the other way, if that
  // works all the way through.
  if (have) {
    for (int j = 0; j < good.choiceCount && altVar < 0; j++) {
      for (int k = 0; k < good.choices[j].count && altVar < 0; k++) {
        if (k == good.choices[j].taken) continue;
        for (int i = 0; i < j; i++) path[i] = good.choices[i].taken;
        path[j] = k;
        Run other;
        propagate(&other, path, j + 1);
        const int v = good.choices[j].var;
        if (other.complete && other.solved[v] && other.val[v] != good.val[v]) {
          altVar = v;
          altValue = other.val[v];
        }
      }
    }
    if (altVar >= 0) {
      const double x = val[altVar];
      if (x > 0 && altValue < 0) error = "Two solutions: showing the positive one";
      else if (std::fabs(x) < std::fabs(altValue)) error = "Two solutions: showing the smaller one";
      else error = "Two solutions: showing the larger one";
    }
  }

  // What is still blank.
  int unknown = 0;
  for (int i = 0; i < f->varCount; i++)
    if (cell[i] == Cell::Empty) unknown++;
  if (unknown && have) {
    const int short_ = capacity() - enteredCount();
    if (short_ > 0) missing = short_;
    else error = "Those values don't settle the rest";
  }
}

Sheet::Sheet() : impl_(new Impl) {}
Sheet::~Sheet() { delete impl_; }

void Sheet::open(const Formula *f) {
  Impl &s = *impl_;
  s.f = f;
  s.parsed.clear();
  s.solving.clear();
  s.eqs.clear();
  if (f) {
    for (int i = 0; i < f->equationCount; i++) {
      Eq e;
      Parser p(f->equations[i], f, &s.parsed);
      if (!p.equation(&e.lhs, &e.rhs)) continue;
      e.vars = varsIn(e.lhs) | varsIn(e.rhs);
      for (int v = 0; v < f->varCount; v++)
        e.count[v] = (uint8_t)(occurrences(e.lhs, v) + occurrences(e.rhs, v));
      s.eqs.push_back(e);
    }
  }
  s.reset();
  if (f) s.solve();
}

const Formula *Sheet::current() const { return impl_->f; }

void Sheet::set(int i, double si) {
  Impl &s = *impl_;
  if (!s.f || i < 0 || i >= s.f->varCount) return;
  s.val[i] = si;
  if (s.f->vars[i].flags & kConstant) {
    s.cell[i] = Cell::Preset;
    s.released[i] = false;
  } else {
    s.cell[i] = Cell::Entered;
    s.age[i] = ++s.clock;
  }
  while (s.enteredCount() > s.capacity()) {
    int oldest = -1;
    for (int v = 0; v < s.f->varCount; v++)
      if (v != i && s.cell[v] == Cell::Entered && (oldest < 0 || s.age[v] < s.age[oldest])) oldest = v;
    if (oldest < 0) break;
    s.cell[oldest] = Cell::Empty;
    s.val[oldest] = 0;
  }
  s.solve();
}

void Sheet::solveFor(int i) {
  Impl &s = *impl_;
  if (!s.f || i < 0 || i >= s.f->varCount) return;
  if (s.f->vars[i].flags & kConstant) s.released[i] = true;
  if (s.cell[i] != Cell::Solved) {
    s.cell[i] = Cell::Empty;
    s.val[i] = 0;
  }
  s.solve();
}

void Sheet::clear(int i) {
  Impl &s = *impl_;
  if (!s.f || i < 0 || i >= s.f->varCount) return;
  if (s.f->vars[i].flags & kConstant) {
    s.cell[i] = Cell::Preset;
    s.val[i] = s.f->vars[i].preset;
    s.released[i] = false;
  } else {
    s.cell[i] = Cell::Empty;
    s.val[i] = 0;
  }
  s.solve();
}

void Sheet::clearAll() {
  Impl &s = *impl_;
  s.reset();
  if (s.f) s.solve();
}

Cell Sheet::cell(int i) const {
  return impl_->f && i >= 0 && i < impl_->f->varCount ? impl_->cell[i] : Cell::Empty;
}

bool Sheet::hasValue(int i) const { return cell(i) != Cell::Empty; }

double Sheet::value(int i) const { return hasValue(i) ? impl_->val[i] : 0; }

int Sheet::missing() const { return impl_->missing; }
const char *Sheet::error() const { return impl_->error.c_str(); }
int Sheet::altVar() const { return impl_->altVar; }
double Sheet::altValue() const { return impl_->altValue; }

const Node *Sheet::howLhs(int i) const {
  return cell(i) == Cell::Solved ? impl_->howL[i] : nullptr;
}
const Node *Sheet::howRhs(int i) const {
  return cell(i) == Cell::Solved ? impl_->howR[i] : nullptr;
}

// ---- Diagrams ----

namespace {

struct Command {
  char op;
  int args;   // numbers; -1 for P (3 or more points)
  // How the numbers read: x and y pairs from the start, unless noted.
  int points; // how many leading x y pairs must be on the canvas
};

constexpr Command kCommands[] = {
  {'k', 1, 0},
  {'L', 4, 2}, {'A', 4, 2}, {'D', 4, 2}, {'Q', 4, 2}, {'M', 4, 2},
  {'r', 4, 2}, {'c', 4, 2}, {'l', 4, 2}, {'b', 4, 2}, {'s', 4, 2}, {'d', 4, 2}, {'e', 4, 2},
  {'B', 4, 1}, {'F', 4, 1}, {'Z', 5, 1},
  {'C', 3, 1}, {'O', 3, 1},
  {'W', 5, 1},
  {'P', -1, 0},
  {'g', 2, 1},
};

constexpr int kCanvasW = 400;
constexpr int kCanvasH = 240;

bool onCanvas(double x, double y) { return x >= 0 && x <= kCanvasW && y >= 0 && y <= kCanvasH; }

bool fail(std::string *why, const std::string &what, const std::string &cmd) {
  if (why) *why = what + ": \"" + cmd + "\"";
  return false;
}

// Characters as drawn: UTF-8 sequences count once, the _{ } of a subscript not at all.
int drawnLength(const char *s) {
  int n = 0;
  for (; *s; s++) {
    if (((unsigned char)*s & 0xC0) == 0x80) continue;
    if (*s == '{' || *s == '}') continue;
    if (*s == '_' && s[1] == '{') continue;
    n++;
  }
  return n;
}

}  // namespace

bool checkDiagram(const char *script, std::string *why) {
  if (!script) return true;
  const char *p = script;
  while (*p) {
    const char *end = strchr(p, ';');
    const std::string cmd = trimmed(std::string(p, end ? (size_t)(end - p) : strlen(p)).c_str());
    p = end ? end + 1 : p + strlen(p);
    if (cmd.empty()) continue;
    if (cmd.size() > 1 && cmd[1] != ' ') return fail(why, "unknown command", cmd);
    const char op = cmd[0];

    if (op == 'T' || op == 't') {
      char *q = nullptr;
      const double x = strtod(cmd.c_str() + 1, &q);
      const char *start = q;
      const double y = strtod(start, &q);
      if (q == start) return fail(why, "label needs x y text", cmd);
      while (isSpace(*q)) q++;
      if (!*q) return fail(why, "label has no text", cmd);
      const double half = drawnLength(q) * (op == 'T' ? 5.0 : 4.0);
      const double tall = op == 'T' ? 8 : 6;
      if (x - half < 0 || x + half > kCanvasW || y - tall < 0 || y + tall > kCanvasH)
        return fail(why, "label runs off the canvas", cmd);
      continue;
    }

    const Command *c = nullptr;
    for (const Command &k : kCommands)
      if (k.op == op) c = &k;
    if (!c) return fail(why, "unknown command", cmd);

    double args[64];
    int n = 0;
    const char *s = cmd.c_str() + 1;
    for (;;) {
      while (isSpace(*s)) s++;
      if (!*s) break;
      char *q = nullptr;
      const double v = strtod(s, &q);
      if (q == s) return fail(why, "not a number", cmd);
      if (n >= 64) return fail(why, "too many arguments", cmd);
      args[n++] = v;
      s = q;
    }
    if (c->args >= 0 && n != c->args) return fail(why, "wrong number of arguments", cmd);
    if (op == 'k') {
      if (args[0] != 0 && args[0] != 1 && args[0] != 2 && args[0] != 3) return fail(why, "colour must be 0..3", cmd);
      continue;
    }
    if (op == 'P') {
      if (n < 6 || n % 2) return fail(why, "polygon needs 3 or more points", cmd);
      for (int i = 0; i < n; i += 2)
        if (!onCanvas(args[i], args[i + 1])) return fail(why, "off the canvas", cmd);
      continue;
    }
    for (int i = 0; i < c->points; i++)
      if (!onCanvas(args[2 * i], args[2 * i + 1])) return fail(why, "off the canvas", cmd);
    if ((op == 'B' || op == 'F' || op == 'Z') &&
        (args[2] <= 0 || args[3] <= 0 || !onCanvas(args[0] + args[2], args[1] + args[3])))
      return fail(why, "box off the canvas", cmd);
    if ((op == 'C' || op == 'O') && (args[2] <= 0 || !onCanvas(args[0] - args[2], args[1] - args[2]) ||
                                     !onCanvas(args[0] + args[2], args[1] + args[2])))
      return fail(why, "circle off the canvas", cmd);
    if (op == 'W') {
      if (args[2] <= 0 || args[4] <= args[3]) return fail(why, "arc needs a radius and a0 < a1", cmd);
      for (double a = args[3]; a < args[4] + 1; a += 3) {
        const double r = std::fmin(a, args[4]) * kPi / 180;
        if (!onCanvas(args[0] + args[2] * std::cos(r), args[1] - args[2] * std::sin(r)))
          return fail(why, "arc off the canvas", cmd);
      }
    }
  }
  return true;
}

}  // namespace formulas
}  // namespace mt
