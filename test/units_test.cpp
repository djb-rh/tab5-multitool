// Host test for the Units engine: builds with the Mac's clang, no device.
//   test/units_test.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../firmware/src/app/units.h"

using namespace mt::units;

static int failures = 0;
static int checks = 0;

#define CHECK(c) \
  do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bool near(double a, double b, double rel = 1e-12) {
  return std::fabs(a - b) <= rel * std::fmax(std::fabs(a), std::fabs(b)) + 1e-300;
}

// Unit index in cat by exact symbol; fails the test if missing.
static int U(int cat, const char *sym) {
  for (int i = 0; i < unitCount(cat); i++)
    if (!strcmp(unit(cat, i)->symbol, sym)) return i;
  printf("FAIL: no unit '%s' in %s\n", sym, category(cat)->name);
  failures++;
  return -1;
}

static void conv(int line, int cat, double v, const char *from, const char *to, double want, double rel = 1e-12) {
  checks++;
  bool ok = false;
  const double got = convert(v, cat, U(cat, from), U(cat, to), &ok);
  if (!ok || !near(got, want, rel)) {
    printf("FAIL line %d: %g %s -> %s = %.17g (ok=%d), want %.17g\n", line, v, from, to, got, ok, want);
    failures++;
  }
}
#define CONV(...) conv(__LINE__, __VA_ARGS__)

static void expr(int line, const char *text, bool wantOk, double want = 0) {
  checks++;
  double got = 0;
  const bool ok = evalExpression(text, &got);
  if (ok != wantOk || (ok && !near(got, want, 1e-12))) {
    printf("FAIL line %d: eval(\"%s\") ok=%d %.17g, want ok=%d %.17g\n", line, text, ok, got, wantOk, want);
    failures++;
  }
}
#define EXPR(...) expr(__LINE__, __VA_ARGS__)

static void qty(int line, const char *text, int cat, const char *unitSym, bool wantOk, double want = 0) {
  checks++;
  double got = 0;
  const bool ok = parseQuantity(text, cat, U(cat, unitSym), &got);
  if (ok != wantOk || (ok && !near(got, want, 1e-12))) {
    printf("FAIL line %d: quantity(\"%s\" in %s) ok=%d %.17g, want ok=%d %.17g\n", line, text, unitSym, ok, got,
           wantOk, want);
    failures++;
  }
}
#define QTY(...) qty(__LINE__, __VA_ARGS__)

static void find(int line, const char *text, int cat, const char *sym, int restrict = -1) {
  checks++;
  const UnitRef r = findUnit(text, restrict);
  const bool good = sym ? (r.cat == cat && r.unit >= 0 && !strcmp(unit(r.cat, r.unit)->symbol, sym)) : !r.valid();
  if (!good) {
    printf("FAIL line %d: findUnit(\"%s\") = %s/%s, want %s/%s\n", line, text,
           r.valid() ? category(r.cat)->name : "-", r.valid() ? unit(r.cat, r.unit)->symbol : "-",
           sym ? category(cat)->name : "-", sym ? sym : "-");
    failures++;
  }
}
#define FIND(...) find(__LINE__, __VA_ARGS__)

// parseLine: want category, from and to symbols, typed value and result.
static void line(int ln, const char *text, int cat, const char *from, const char *to, double value, double result,
                 bool resultOk = true) {
  checks++;
  const LineResult r = parseLine(text);
  bool good = r.ok && r.cat == cat && !strcmp(unit(cat, r.from)->symbol, from) &&
              !strcmp(unit(cat, r.to)->symbol, to) && near(r.value, value, 1e-12) && r.resultOk == resultOk &&
              (!resultOk || near(r.result, result, 1e-12));
  if (!good) {
    printf("FAIL line %d: parseLine(\"%s\") ok=%d cat=%s %s->%s value=%.17g result=%.17g resultOk=%d err=%s\n", ln,
           text, r.ok, r.ok ? category(r.cat)->name : "-", r.ok ? unit(r.cat, r.from)->symbol : "-",
           r.ok ? unit(r.cat, r.to)->symbol : "-", r.value, r.result, r.resultOk, r.error);
    failures++;
  }
}
#define LINE(...) line(__LINE__, __VA_ARGS__)

static void lineBad(int ln, const char *text) {
  checks++;
  const LineResult r = parseLine(text);
  if (r.ok) {
    printf("FAIL line %d: parseLine(\"%s\") should fail, got %s %s->%s\n", ln, text, category(r.cat)->name,
           unit(r.cat, r.from)->symbol, unit(r.cat, r.to)->symbol);
    failures++;
  }
}
#define LINEBAD(...) lineBad(__LINE__, __VA_ARGS__)

static void str(int line, const std::string &got, const char *want) {
  checks++;
  if (got != want) {
    printf("FAIL line %d: \"%s\", want \"%s\"\n", line, got.c_str(), want);
    failures++;
  }
}
#define STR(...) str(__LINE__, __VA_ARGS__)

int main() {
  // ---- Table sanity: every unit has a name, an ASCII symbol, a finite positive
  // factor; defaults are in range; no symbol repeats within a category.
  int totalUnits = 0;
  CHECK(categoryCount() == kCatCount);
  for (int c = 0; c < categoryCount(); c++) {
    const Category *cat = category(c);
    CHECK(cat && cat->name && cat->count > 1);
    CHECK(cat->defaultFrom < cat->count && cat->defaultTo < cat->count && cat->defaultFrom != cat->defaultTo);
    CHECK(findCategory(cat->name) == c);
    int common = 0;
    for (int i = 0; i < cat->count; i++) {
      const Unit *u = unit(c, i);
      totalUnits++;
      CHECK(u->name && *u->name && u->symbol && *u->symbol && u->aliases);
      for (const char *s = u->symbol; *s; s++) CHECK(*s >= 0x20 && *s < 0x7f);
      for (const char *s = u->name; *s; s++) CHECK(*s >= 0x20 && *s < 0x7f);
      CHECK(std::isfinite(u->factor) && u->factor > 0);
      common += (u->flags & kCommon) != 0;
      for (int j = 0; j < i; j++) CHECK(strcmp(unit(c, j)->symbol, u->symbol) != 0);
      // Every unit's own symbol and name find it within its category.
      UnitRef r = findUnit(u->symbol, c);
      CHECK(r.cat == c && r.unit == i);
      r = findUnit(u->name, c);
      CHECK(r.cat == c && r.unit == i);
      // Round trip through every other unit.
      for (int j = 0; j < cat->count; j++) {
        bool ok1, ok2;
        const double there = convert(1.2345, c, i, j, &ok1);
        const double back = convert(there, c, j, i, &ok2);
        CHECK(ok1 && ok2 && near(back, 1.2345, 1e-11));
      }
    }
    CHECK(common >= 2);
  }
  printf("%d categories, %d units, %d ingredients\n", categoryCount(), totalUnits, ingredientCount());
  for (int c = 0; c < categoryCount(); c++) {
    const Category *cat = category(c);
    printf("  %-13s %2d  default %s -> %s\n", cat->name, cat->count, unit(c, cat->defaultFrom)->symbol,
           unit(c, cat->defaultTo)->symbol);
  }

  // ---- Exact (by definition) conversions.
  CONV(kLength, 1, "in", "mm", 25.4);
  CONV(kLength, 1, "ft", "in", 12);
  CONV(kLength, 1, "yd", "m", 0.9144);
  CONV(kLength, 1, "mi", "km", 1.609344);
  CONV(kLength, 1, "mi", "ft", 5280);
  CONV(kLength, 1, "nmi", "m", 1852);
  CONV(kLength, 1, "mil", "mm", 0.0254);
  CONV(kLength, 1000, "um", "mm", 1);
  CONV(kLength, 3.0 / 8, "in", "mm", 9.525);
  CONV(kArea, 1, "ac", "ft^2", 43560);
  CONV(kArea, 1, "mi^2", "ac", 640);
  CONV(kArea, 1, "ha", "m^2", 10000);
  CONV(kArea, 1, "ft^2", "in^2", 144);
  CONV(kArea, 1, "courts", "ft^2", 880);
  CONV(kVolume, 1, "gal", "in^3", 231);
  CONV(kVolume, 1, "gal", "L", 3.785411784);
  CONV(kVolume, 1, "gal", "fl oz", 128);
  CONV(kVolume, 1, "gal", "cup", 16);
  CONV(kVolume, 1, "qt", "pt", 2);
  CONV(kVolume, 1, "cup", "tbsp", 16);
  CONV(kVolume, 1, "tbsp", "tsp", 3);
  CONV(kVolume, 1, "fl oz", "mL", 29.5735295625);
  CONV(kVolume, 1, "UK gal", "L", 4.54609);
  CONV(kVolume, 1, "UK pt", "UK fl oz", 20);
  CONV(kVolume, 1, "UK gal", "UK pt", 8);
  CONV(kVolume, 1, "ft^3", "in^3", 1728);
  CONV(kVolume, 1, "yd^3", "ft^3", 27);
  CONV(kVolume, 1, "bbl", "gal", 42);
  CONV(kVolume, 1, "m^3", "L", 1000);
  CONV(kVolume, 1, "cm^3", "mL", 1);
  CONV(kMass, 1, "lb", "g", 453.59237);
  CONV(kMass, 1, "lb", "oz", 16);
  CONV(kMass, 1, "st", "lb", 14);
  CONV(kMass, 1, "ton", "lb", 2000);
  CONV(kMass, 1, "long ton", "lb", 2240);
  CONV(kMass, 1, "lb", "gr", 7000);
  CONV(kMass, 1, "ozt", "gr", 480);
  CONV(kMass, 1, "ozt", "g", 31.1034768);
  CONV(kMass, 1, "t", "kg", 1000);
  CONV(kMass, 5, "ct", "g", 1);
  CONV(kTemperature, 100, "C", "F", 212);
  CONV(kTemperature, -40, "C", "F", -40);
  CONV(kTemperature, -40, "F", "C", -40);
  CONV(kTemperature, 0, "K", "F", -459.67);
  CONV(kTemperature, 0, "C", "K", 273.15);
  CONV(kTemperature, 0, "C", "R", 491.67);
  CONV(kTemperature, 98.6, "F", "C", 37);
  CONV(kTemperature, 100, "F", "C", 37.777777777777778);
  CONV(kTemperature, 300, "K", "C", 26.85);
  CONV(kTemperature, 0, "R", "K", 0);
  {
    bool ok;
    CHECK(convert(32, kTemperature, U(kTemperature, "F"), U(kTemperature, "C"), &ok) == 0 && ok);  // snapped
    CHECK(convert(273.15, kTemperature, U(kTemperature, "K"), U(kTemperature, "C"), &ok) == 0 && ok);
  }
  CONV(kTime, 1, "h", "s", 3600);
  CONV(kTime, 1, "d", "min", 1440);
  CONV(kTime, 1, "wk", "d", 7);
  CONV(kTime, 1, "yr", "d", 365.2425);
  CONV(kTime, 1, "yr", "mo", 12);
  CONV(kTime, 1500, "ms", "s", 1.5);
  CONV(kSpeed, 1, "mph", "km/h", 1.609344);
  CONV(kSpeed, 1, "kn", "km/h", 1.852);
  CONV(kSpeed, 1, "m/s", "km/h", 3.6);
  CONV(kSpeed, 60, "mph", "ft/s", 88);
  CONV(kSpeed, 1, "Mach", "m/s", 340.294);
  CONV(kSpeed, 60, "mm/s", "mm/min", 3600);
  CONV(kSpeed, 1, "in/s", "in/min", 60);
  CONV(kAcceleration, 1, "g", "m/s^2", 9.80665);
  CONV(kAcceleration, 1, "g", "ft/s^2", 9.80665 / 0.3048);
  CONV(kPressure, 1, "psi", "Pa", 6894.757293168361);
  CONV(kPressure, 1, "atm", "kPa", 101.325);
  CONV(kPressure, 1, "atm", "psi", 14.695948775513449);
  CONV(kPressure, 760, "Torr", "atm", 1);
  CONV(kPressure, 1, "bar", "kPa", 100);
  CONV(kPressure, 1, "mbar", "hPa", 1);
  CONV(kPressure, 1, "inHg", "mmHg", 25.4);
  CONV(kPressure, 1, "inH2O", "Pa", 249.08891);
  CONV(kPressure, 1, "kgf/cm^2", "Pa", 98066.5);
  CONV(kPressure, 1, "MPa", "psi", 145.03773773020923);
  CONV(kEnergy, 1, "kWh", "MJ", 3.6);
  CONV(kEnergy, 1, "kcal", "kJ", 4.184);
  CONV(kEnergy, 1, "cal", "J", 4.184);
  CONV(kEnergy, 1, "Wh", "J", 3600);
  CONV(kEnergy, 1, "BTU", "J", 1055.05585262);
  CONV(kEnergy, 1, "thm", "BTU", 100000);
  CONV(kEnergy, 1, "ft-lbf", "J", 1.3558179483314004);
  CONV(kEnergy, 1, "eV", "J", 1.602176634e-19);
  CONV(kEnergy, 1, "erg", "J", 1e-7);
  CONV(kPower, 1, "hp", "W", 745.69987158227022);
  CONV(kPower, 1, "hp", "ft-lbf/s", 550);
  CONV(kPower, 1, "PS", "W", 735.49875);
  CONV(kPower, 1, "TR", "BTU/h", 12000);
  CONV(kPower, 1, "TR", "W", 3516.8528420666667);
  CONV(kPower, 1, "kW", "W", 1000);
  CONV(kForce, 1, "lbf", "N", 4.4482216152605);
  CONV(kForce, 1, "kgf", "N", 9.80665);
  CONV(kForce, 1, "dyn", "N", 1e-5);
  CONV(kForce, 1, "lbf", "ozf", 16);
  CONV(kForce, 1, "kip", "lbf", 1000);
  CONV(kForce, 1000, "gf", "kgf", 1);
  CONV(kTorque, 1, "lbf-ft", "lbf-in", 12);
  CONV(kTorque, 1, "lbf-in", "ozf-in", 16);
  CONV(kTorque, 1, "kgf-m", "kgf-cm", 100);
  CONV(kTorque, 1, "lbf-ft", "N-m", 1.3558179483314004);
  CONV(kTorque, 1, "N-m", "N-cm", 100);
  CONV(kDataSize, 1, "GiB", "B", 1073741824);
  CONV(kDataSize, 1, "GB", "B", 1e9);
  CONV(kDataSize, 1, "MiB", "MB", 1.048576);
  CONV(kDataSize, 1, "B", "bit", 8);
  CONV(kDataSize, 1, "TB", "TiB", 1e12 / 1099511627776.0);
  CONV(kDataSize, 1, "KiB", "bit", 8192);
  CONV(kDataSize, 1, "Mbit", "kB", 125);
  CONV(kDataRate, 100, "Mbit/s", "MB/s", 12.5);
  CONV(kDataRate, 1, "MiB/s", "bit/s", 8388608);
  CONV(kDataRate, 1, "Gbit/s", "Mbit/s", 1000);
  CONV(kAngle, 180, "deg", "rad", M_PI);
  CONV(kAngle, 1, "turn", "grad", 400);
  CONV(kAngle, 1, "deg", "arcmin", 60);
  CONV(kAngle, 1, "deg", "arcsec", 3600);
  CONV(kAngle, 1000, "mrad", "rad", 1);
  CONV(kFrequency, 60, "rpm", "Hz", 1);
  CONV(kFrequency, 1, "GHz", "MHz", 1000);
  CONV(kFrequency, 2 * M_PI, "rad/s", "Hz", 1);
  CONV(kFuelEconomy, 1, "mpg", "km/L", 1.609344 / 3.785411784);
  CONV(kFuelEconomy, 1, "mpg", "mpg UK", 4.54609 / 3.785411784);
  CONV(kFuelEconomy, 10, "L/100 km", "km/L", 10);
  CONV(kFuelEconomy, 4, "L/100 km", "km/L", 25);
  CONV(kFuelEconomy, 30, "mpg", "L/100 km", 100 * 3.785411784 / (30 * 1.609344));
  CONV(kFuelEconomy, 10, "L/100 km", "mpg", 100 * 3.785411784 / (10 * 1.609344));
  {
    bool ok = true;
    const int mpg = U(kFuelEconomy, "mpg"), l100 = U(kFuelEconomy, "L/100 km");
    CHECK(convert(0, kFuelEconomy, mpg, l100, &ok) == 0 && !ok);   // 0 mpg is infinitely thirsty
    CHECK(convert(0, kFuelEconomy, l100, mpg, &ok) == 0 && !ok);
    CHECK(convert(0, kFuelEconomy, l100, l100, &ok) == 0 && ok);   // same unit: unchanged
    CHECK(convert(NAN, kLength, 0, 1, &ok) == 0 && !ok);
    CHECK(convert(INFINITY, kLength, 0, 1, &ok) == 0 && !ok);
    CHECK(convert(1, kLength, 0, 99, &ok) == 0 && !ok);
    CHECK(convert(1, 99, 0, 1, &ok) == 0 && !ok);
    CHECK(convert(1, -1, 0, 1, &ok) == 0 && !ok);
  }
  CONV(kDensity, 1, "g/cm^3", "kg/m^3", 1000);
  CONV(kDensity, 1, "lb/ft^3", "kg/m^3", 16.018463373960138);
  CONV(kDensity, 1, "lb/in^3", "lb/ft^3", 1728);
  CONV(kDensity, 1, "g/cm^3", "lb/gal", 3.785411784 / 0.45359237);
  CONV(kDensity, 1, "oz/in^3", "lb/in^3", 1.0 / 16);
  CONV(kFlow, 1, "gpm", "L/min", 3.785411784);
  CONV(kFlow, 1, "CFM", "L/min", 28.316846592);
  CONV(kFlow, 1, "m^3/h", "L/min", 1000.0 / 60);
  CONV(kFlow, 1, "L/s", "L/h", 3600);
  CONV(kFlow, 1, "cfs", "CFM", 60);
  CONV(kCurrent, 1, "A", "mA", 1000);
  CONV(kVoltage, 1, "kV", "V", 1000);
  CONV(kResistance, 4.7, "kohm", "ohm", 4700);
  CONV(kCapacitance, 1, "uF", "nF", 1000);
  CONV(kCapacitance, 100, "nF", "pF", 100000);

  // ---- Ingredients.
  {
    auto ing = [](const char *name) {
      for (int i = 0; i < ingredientCount(); i++)
        if (!strcmp(ingredient(i)->name, name)) return i;
      return -1;
    };
    bool ok;
    const int cup = U(kVolume, "cup"), g = U(kMass, "g"), tbsp = U(kVolume, "tbsp"), mL = U(kVolume, "mL");
    CHECK(near(convertIngredient(1, ing("All-purpose flour"), kVolume, cup, kMass, g, &ok), 120) && ok);
    CHECK(near(convertIngredient(1, ing("Butter"), kVolume, cup, kMass, g, &ok), 227) && ok);
    CHECK(near(convertIngredient(100, ing("Granulated sugar"), kMass, g, kVolume, cup, &ok), 0.5) && ok);
    CHECK(near(convertIngredient(1, ing("Water"), kVolume, mL, kMass, g, &ok), 1) && ok);
    CHECK(near(convertIngredient(2, ing("Butter"), kVolume, tbsp, kMass, U(kMass, "oz"), &ok),
               227.0 / 8 / 28.349523125));
    CHECK(near(convertIngredient(1, ing("Honey"), kVolume, cup, kVolume, tbsp, &ok), 16) && ok);
    CHECK(convertIngredient(1, ing("Honey"), kLength, 0, kMass, g, &ok) == 0 && !ok);
    CHECK(convertIngredient(1, -1, kVolume, cup, kMass, g, &ok) == 0 && !ok);
  }

  // ---- Expressions.
  EXPR("3/8", true, 0.375);
  EXPR("12*2.54", true, 30.48);
  EXPR("12 * 2.54", true, 30.48);
  EXPR("(1+2)^2", true, 9);
  EXPR("-2^2", true, -4);
  EXPR("2^3^2", true, 512);
  EXPR("2^-1", true, 0.5);
  EXPR("1e3", true, 1000);
  EXPR("2.5E-3", true, 0.0025);
  EXPR(".5", true, 0.5);
  EXPR("-.5", true, -0.5);
  EXPR("1,000", true, 1000);
  EXPR("1,234,567.5", true, 1234567.5);
  EXPR("10 - 2 - 3", true, 5);
  EXPR("2 + 3 * 4", true, 14);
  EXPR("100/4/5", true, 5);
  EXPR("-(3 - 5)", true, 2);
  EXPR("--3", true, 3);
  EXPR("6 × 7", true, 42);
  EXPR("84 ÷ 2", true, 42);
  EXPR("5 − 3", true, 2);
  EXPR("2 3/8", true, 2.375);
  EXPR("2-3/8", true, 2.375);
  EXPR("-2 1/2", true, -2.5);
  EXPR("5-3/8*2", true, 4.25);
  EXPR("0.1+0.2-0.3", true, 0);
  EXPR("  42  ", true, 42);
  EXPR("10/0", false);
  EXPR("(1+2", false);
  EXPR("1+", false);
  EXPR("", false);
  EXPR("abc", false);
  EXPR("5 ft", false);
  EXPR("3 x 4", false);
  EXPR("1e999", false);
  EXPR("2 3", false);
  EXPR("((((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))))", false);  // nesting limit

  // ---- Value field (parseQuantity).
  QTY("3/8", kLength, "in", true, 0.375);
  QTY("12*2.54", kLength, "cm", true, 30.48);
  QTY("5 ft 3 in", kLength, "in", true, 63);
  QTY("5ft3in", kLength, "in", true, 63);
  QTY("5ft3 in", kLength, "in", true, 63);
  QTY("2 ft3", kVolume, "in^3", true, 3456);
  QTY("5'3\"", kLength, "in", true, 63);
  QTY("5' 3\"", kLength, "ft", true, 5.25);
  QTY("6' 2.5\"", kLength, "in", true, 74.5);
  QTY("6’ 2½", kLength, "in", false);
  QTY("5 ft 2 1/2 in", kLength, "in", true, 62.5);
  QTY("2 3/8 in", kLength, "mm", true, 60.325);
  QTY("2-3/8\"", kLength, "in", true, 2.375);
  QTY("1 mi 100 yd", kLength, "yd", true, 1860);
  QTY("12 in", kLength, "ft", true, 1);
  QTY("5 ft 3", kLength, "in", false);
  QTY("5 kg", kLength, "in", false);
  QTY("1h 20m 30s", kTime, "s", true, 4830);
  QTY("1 hour 20 minutes", kTime, "min", true, 80);
  QTY("1h20m30s", kTime, "min", true, 80.5);
  QTY("2 lb 4 oz", kMass, "oz", true, 36);
  QTY("2 lbs. 4 oz.", kMass, "lb", true, 2.25);
  QTY("40° 26' 46\"", kAngle, "deg", true, 40 + 26 / 60.0 + 46 / 3600.0);
  QTY("1 gal 2 qt", kVolume, "qt", true, 6);
  QTY("1 cup 2 tbsp", kVolume, "tbsp", true, 18);
  QTY("100 F", kTemperature, "C", true, 37.777777777777778);
  QTY("5 C 3 F", kTemperature, "C", false);  // affine units don't add
  QTY("", kLength, "in", false);
  QTY("ft", kLength, "in", false);
  QTY("10 L/100km", kFuelEconomy, "km/L", true, 10);

  // ---- findUnit.
  FIND("in", kLength, "in");
  FIND("inch", kLength, "in");
  FIND("Inches", kLength, "in");
  FIND("\"", kLength, "in");
  FIND("ft", kLength, "ft");
  FIND("'", kLength, "ft");
  FIND("'", kAngle, "arcmin", kAngle);
  FIND("FEET", kLength, "ft");
  FIND("lbs", kMass, "lb");
  FIND("LBS", kMass, "lb");
  FIND("lbs", kForce, "lbf", kForce);
  FIND("°F", kTemperature, "F");
  FIND("F", kTemperature, "F");
  FIND("degF", kTemperature, "F");
  FIND("deg F", kTemperature, "F");
  FIND("degf", kTemperature, "F");
  FIND("C", kTemperature, "C");
  FIND("c", kVolume, "cup");
  FIND("F", kCapacitance, "F", kCapacitance);
  FIND("MB", kDataSize, "MB");
  FIND("Mb", kDataSize, "Mbit");
  FIND("mb", kPressure, "mbar");
  FIND("Mbps", kDataRate, "Mbit/s");
  FIND("MBps", kDataRate, "MB/s");
  FIND("kph", kSpeed, "km/h");
  FIND("m", kLength, "m");
  FIND("m", kTime, "min", kTime);
  FIND("g", kMass, "g");
  FIND("oz", kMass, "oz");
  FIND("fl oz", kVolume, "fl oz");
  FIND("floz", kVolume, "fl oz");
  FIND("FL. OZ", kVolume, "fl oz");
  FIND("T", kMass, "t");
  FIND("T", kVolume, "tbsp", kVolume);
  FIND("Cal", kEnergy, "kcal");
  FIND("cal", kEnergy, "cal");
  FIND("µm", kLength, "um");
  FIND("m²", kArea, "m^2");
  FIND("sq ft", kArea, "ft^2");
  FIND("N·m", kTorque, "N-m");
  FIND("Nm", kTorque, "N-m");
  FIND("nm", kLength, "nm");
  FIND("ft-lb", kEnergy, "ft-lbf");
  FIND("ft-lb", kTorque, "lbf-ft", kTorque);
  FIND("in-lb", kTorque, "lbf-in");
  FIND("l/100km", kFuelEconomy, "L/100 km");
  FIND("L/100 km", kFuelEconomy, "L/100 km");
  FIND("mohm", kResistance, "mohm");
  FIND("Mohm", kResistance, "Mohm");
  FIND("uF", kCapacitance, "uF");
  FIND("  psi  ", kPressure, "psi");
  FIND("cfm", kFlow, "CFM");
  FIND("rpm", kFrequency, "rpm");
  FIND("ton", kMass, "ton");
  FIND("Ton", kPower, "TR", kPower);
  FIND("bogus", 0, nullptr);
  FIND("", 0, nullptr);
  FIND("inches", 0, nullptr, kMass);

  // ---- Whole lines.
  LINE("5 ft 3 in to cm", kLength, "ft", "cm", 5.25, 160.02);
  LINE("100 F in C", kTemperature, "F", "C", 100, 37.777777777777778);
  LINE("100F in C", kTemperature, "F", "C", 100, 37.777777777777778);
  LINE("100 f in c", kTemperature, "F", "C", 100, 37.777777777777778);
  LINE("-40 c to f", kTemperature, "C", "F", -40, -40);
  LINE("3/8 in mm", kLength, "in", "mm", 0.375, 9.525);
  LINE("3 in in mm", kLength, "in", "mm", 3, 76.2);
  LINE("5 in to mm", kLength, "in", "mm", 5, 127);
  LINE("5in mm", kLength, "in", "mm", 5, 127);
  LINE("5in->mm", kLength, "in", "mm", 5, 127);
  LINE("5 in = mm", kLength, "in", "mm", 5, 127);
  LINE("5 in → mm", kLength, "in", "mm", 5, 127);
  LINE("1 ft in in", kLength, "ft", "in", 1, 12);
  LINE("1 ft to in", kLength, "ft", "in", 1, 12);
  LINE("5ft3 in to cm", kLength, "ft", "cm", 5.25, 160.02);
  LINE("5 ft3 in l", kVolume, "ft^3", "L", 5, 5 * 28.316846592);
  LINE("1 ft 1/2 in", kLength, "ft", "ft", 1 + 0.5 / 12, 1 + 0.5 / 12);
  LINE("5'3\" to cm", kLength, "ft", "cm", 5.25, 160.02);
  LINE("6' 2.5\" in cm", kLength, "ft", "cm", 74.5 / 12, 189.23);
  LINE("2 3/8 in mm", kLength, "in", "mm", 2.375, 60.325);
  LINE("2-3/8\" to mm", kLength, "in", "mm", 2.375, 60.325);
  LINE("12*2.54 cm in in", kLength, "cm", "in", 30.48, 12);
  LINE("1h 20m 30s to min", kTime, "h", "min", 4830.0 / 3600, 80.5);
  LINE("20m to s", kTime, "min", "s", 20, 1200);
  LINE("2 lb 4 oz in kg", kMass, "lb", "kg", 2.25, 1.0205828325);
  LINE("8 oz to ml", kVolume, "fl oz", "mL", 8, 236.5882365);
  LINE("8 oz to g", kMass, "oz", "g", 8, 226.796185);
  LINE("10 lb to N", kForce, "lbf", "N", 10, 44.482216152605);
  LINE("30 mpg to l/100km", kFuelEconomy, "mpg", "L/100 km", 30, 100 * 3.785411784 / (30 * 1.609344));
  LINE("10 L/100km in mpg", kFuelEconomy, "L/100 km", "mpg", 10, 100 * 3.785411784 / (10 * 1.609344));
  LINE("0 mpg to l/100km", kFuelEconomy, "mpg", "L/100 km", 0, 0, false);
  LINE("1 GiB to MB", kDataSize, "GiB", "MB", 1, 1073.741824);
  LINE("100 Mbps in MB/s", kDataRate, "Mbit/s", "MB/s", 100, 12.5);
  LINE("60 mph to km/h", kSpeed, "mph", "km/h", 60, 96.56064);
  LINE("60mph kph", kSpeed, "mph", "km/h", 60, 96.56064);
  LINE("72 degF to degC", kTemperature, "F", "C", 72, 22.222222222222221);
  LINE("72 °F as °C", kTemperature, "F", "C", 72, 22.222222222222221);
  LINE("1 cup in tbsp", kVolume, "cup", "tbsp", 1, 16);
  LINE("2 T to tsp", kVolume, "tbsp", "tsp", 2, 6);
  LINE("100 ft-lb to Nm", kTorque, "lbf-ft", "N-m", 100, 135.58179483314004);
  LINE("25 in-lb to N-m", kTorque, "lbf-in", "N-m", 25, 25 * 0.1129848290276167);
  LINE("2 kg cm in oz-in", kTorque, "kgf-cm", "ozf-in", 2, 2 * 0.0980665 / (4.4482216152605 / 16 * 0.0254));
  LINE("30 psi in bar", kPressure, "psi", "bar", 30, 30 * 6894.757293168361 / 1e5);
  LINE("29.92 inHg to mbar", kPressure, "inHg", "mbar", 29.92, 29.92 * 3386.38864034 / 100);
  LINE("200 hp to kW", kPower, "hp", "kW", 200, 149.13997431645404);
  LINE("5 ft 3 in", kLength, "ft", "ft", 5.25, 5.25);
  LINE("12 in", kLength, "in", "in", 12, 12);
  LINE("5 m", kLength, "m", "m", 5, 5);
  LINE("5 ft =", kLength, "ft", "ft", 5, 5);
  LINE("90 deg to rad", kAngle, "deg", "rad", 90, M_PI / 2);
  LINE("5 nm to mm", kLength, "nm", "mm", 5, 5e-6);
  LINE("10 nm to ft-lb", kTorque, "N-m", "lbf-ft", 10, 10 / 1.3558179483314004);
  LINE("3 in. to mm", kLength, "in", "mm", 3, 76.2);
  LINE("1,000 ft in m", kLength, "ft", "m", 1000, 304.8);
  LINEBAD("how many mm in 1 in?");
  LINEBAD("hello");
  LINEBAD("");
  LINEBAD("5 ft to kg");
  LINEBAD("to mm");
  LINEBAD("5 to mm");
  LINEBAD("5 ft to in to mm");
  LINEBAD("5 ft 3");
  LINEBAD("5 C 3 F to K");

  // ---- Number formatting.
  STR(formatNumber(1234567.891), "1234567.89");
  {
    NumberFormat g;
    g.grouping = true;
    STR(formatNumber(1234567.891, g), "1,234,567.89");
    STR(formatNumber(-1234.5, g), "-1,234.5");
    STR(formatNumber(999, g), "999");
    STR(formatNumber(100000, g), "100,000");
    STR(formatNumber(0.000123, g), "0.000123");
    NumberFormat t;
    t.timesTen = true;
    STR(formatNumber(2.5e12, t), "2.5 x 10^12");
    STR(formatNumber(-1.5e-9, t), "-1.5 x 10^-9");
    NumberFormat d4;
    d4.digits = 4;
    STR(formatNumber(M_PI, d4), "3.142");
    STR(formatNumber(99999, d4), "100000");
  }
  STR(formatNumber(0.1 + 0.2), "0.3");
  STR(formatNumber(1.2345678e-12), "1.2345678e-12");
  STR(formatNumber(1.23456789123e-12), "1.23456789e-12");
  STR(formatNumber(-0.0), "0");
  STR(formatNumber(0), "0");
  STR(formatNumber(1e15), "1e15");
  STR(formatNumber(-1e-7), "-1e-7");
  STR(formatNumber(100.0), "100");
  STR(formatNumber(25.400000000000002), "25.4");
  STR(formatNumber(12345678901.0), "12345678900");
  STR(formatNumber(999999999999.0), "1e12");
  STR(formatNumber(0.000001), "0.000001");
  STR(formatNumber(0.00000099), "9.9e-7");
  STR(formatNumber(2.0 / 3), "0.666666667");
  STR(formatNumber(-2.0 / 3), "-0.666666667");
  STR(formatNumber(NAN), "NaN");
  STR(formatNumber(INFINITY), "inf");
  STR(formatNumber(-INFINITY), "-inf");
  STR(formatWithUnit(25.4, kLength, U(kLength, "mm")), "25.4 mm");

  // ---- Fractions, feet-inches, durations.
  STR(formatFraction(0.34375), "11/32");
  STR(formatFraction(0.34), "~11/32");
  STR(formatFraction(0.34, 64, true), "~11/32 (-0.0037)");
  STR(formatFraction(0.4, 8, true), "~3/8 (+0.025)");
  STR(formatFraction(0.34, 8, true), "~3/8 (-0.035)");
  STR(formatFraction(2.375), "2 3/8");
  STR(formatFraction(-1.5), "-1 1/2");
  STR(formatFraction(0.999), "~1");
  STR(formatFraction(0), "0");
  STR(formatFraction(-0.001), "~0");
  STR(formatFraction(3), "3");
  STR(formatFraction(0.5, 2), "1/2");
  STR(formatFraction(0.3, 16), "~5/16");
  STR(formatFraction(0.1, 100), "1/10");
  {
    const Fraction f = nearestFraction(0.34);
    CHECK(f.num == 11 && f.den == 32 && f.whole == 0 && !f.exact && near(f.error, 0.34 - 0.34375));
    const Fraction g = nearestFraction(-2.75, 16);
    CHECK(g.negative && g.whole == 2 && g.num == 3 && g.den == 4 && g.exact);
  }
  STR(formatFeetInches(74.5), "6' 2.5\"");
  STR(formatFeetInches(74.5, 16), "6' 2 1/2\"");
  STR(formatFeetInches(74.51, 16), "~6' 2 1/2\"");
  STR(formatFeetInches(11.999), "1' 0\"");
  STR(formatFeetInches(11.999, 64), "~1' 0\"");
  STR(formatFeetInches(3.25), "3.25\"");
  STR(formatFeetInches(-74.5), "-6' 2.5\"");
  STR(formatFeetInches(72), "6' 0\"");
  STR(formatFeetInches(0), "0\"");
  STR(formatFeetInches(-0.001), "0\"");
  STR(formatFeetInches(63.123456, 0, 3), "5' 3.123\"");
  STR(formatDuration(4830), "1:20:30");
  STR(formatDuration(90061.5), "1d 01:01:01.5");
  STR(formatDuration(59.25), "0:00:59.25");
  STR(formatDuration(-30), "-0:00:30");
  STR(formatDuration(0), "0:00:00");
  STR(formatDuration(0.0004), "0:00:00");
  STR(formatDuration(3599.9996), "1:00:00");

  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
