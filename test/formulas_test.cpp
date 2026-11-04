// Host test for the Formulas engine and library: builds with the Mac's clang, no device.
//   test/formulas_test.sh
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "../firmware/src/app/formulas.h"

using namespace mt::formulas;
namespace units = mt::units;

static int failures = 0;
static int checks = 0;

#define CHECK(c) \
  do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bool near(double a, double b, double rel = 1e-9) {
  return std::fabs(a - b) <= rel * std::fmax(std::fabs(a), std::fabs(b)) + 1e-300;
}

static int unitOf(int d, const char *sym) {
  const DimInfo *info = dim(d);
  for (int i = 0; i < info->count; i++)
    if (!strcmp(info->units[i].symbol, sym)) return i;
  return -1;
}

static int varOf(const Formula *f, const char *sym) {
  for (int i = 0; i < f->varCount; i++)
    if (!strcmp(f->vars[i].sym, sym)) return i;
  return -1;
}

// ---- Units ----

static void value(int line, const char *text, int d, const char *u, double want, const char *typed = nullptr) {
  checks++;
  double si = 0;
  int used = -2;
  const int ui = u ? unitOf(d, u) : 0;
  const bool ok = parseValue(text, d, ui, &si, &used);
  const int wantUsed = typed ? unitOf(d, typed) : -1;
  if (!ok || !near(si, want, 1e-9) || used != wantUsed) {
    printf("FAIL line %d: parseValue(\"%s\", %s) ok=%d %.12g used=%d, want %.12g used=%d\n", line, text,
           dim(d)->name, ok, si, used, want, wantUsed);
    failures++;
  }
}
#define VALUE(...) value(__LINE__, __VA_ARGS__)

static void testUnits() {
  // Every dimension has its SI unit at the right index.
  const char *siName[kDimCount] = {};
  siName[units::kVolume] = "m^3"; siName[units::kArea] = "m^2"; siName[units::kTemperature] = "K";
  siName[units::kPressure] = "Pa"; siName[units::kAngle] = "rad"; siName[units::kFlow] = "m^3/s";
  siName[units::kMass] = "kg"; siName[units::kTime] = "s"; siName[kMolarity] = "mol/m^3"; siName[kMolarMass] = "kg/mol";
  siName[kAngularSpeed] = "rad/s"; siName[kCharge] = "C"; siName[kInductance] = "H";
  for (int d = 0; d < kDimCount; d++) {
    const DimInfo *info = dim(d);
    CHECK(info != nullptr);
    if (siName[d]) CHECK(!strcmp(info->units[info->si].symbol, siName[d]));
    if (d == kNone) continue;
    CHECK(info->count > 0);
    CHECK(info->si < info->count);
    for (int u = 0; u < info->count; u++) {
      bool ok = false;
      const double s = toSI(7.5, d, u, &ok);
      CHECK(ok);
      CHECK(near(fromSI(s, d, u, &ok), 7.5, 1e-12));
      CHECK(ok);
    }
    bool ok = false;
    CHECK(near(toSI(3.5, d, info->si, &ok), 3.5, 1e-12));
  }
  CHECK(dim(-1) == nullptr && dim(kDimCount) == nullptr);
  bool ok = true;
  CHECK(toSI(1, units::kLength, 99, &ok) == 0 && !ok);
  CHECK(fromSI(1, kInductance, -1, &ok) == 0 && !ok);
  CHECK(near(toSI(5, kNone, 0), 5));

  CHECK(near(toSI(1, units::kVolume, unitOf(units::kVolume, "L")), 1e-3));
  CHECK(near(toSI(60, units::kFlow, unitOf(units::kFlow, "L/min")), 1e-3));
  CHECK(near(toSI(180, units::kAngle, unitOf(units::kAngle, "deg")), M_PI));
  CHECK(near(toSI(0, units::kTemperature, unitOf(units::kTemperature, "C")), 273.15));
  CHECK(near(toSI(10, kTempDiff, unitOf(kTempDiff, "C deg")), 10));
  CHECK(near(toSI(9, kTempDiff, unitOf(kTempDiff, "F deg")), 5));
  CHECK(near(toSI(1, kCharge, unitOf(kCharge, "mAh")), 3.6));
  CHECK(near(toSI(3000, kAngularSpeed, unitOf(kAngularSpeed, "rpm")), 100 * M_PI));
  CHECK(near(toSI(1, kMolarity, unitOf(kMolarity, "mol/L")), 1000));
  CHECK(near(toSI(18, kMolarMass, unitOf(kMolarMass, "g/mol")), 0.018));
  CHECK(near(toSI(1, kSpecificHeat, unitOf(kSpecificHeat, "BTU/(lb-F)")), 4186.8, 1e-6));
  CHECK(near(toSI(1, kThermalCond, unitOf(kThermalCond, "BTU/(hr-ft-F)")), 1.730735, 1e-6));
  CHECK(near(toSI(1, kIlluminance, unitOf(kIlluminance, "foot-candle")), 10.7639, 1e-5));

  // parseValue: the cases from the brief.
  VALUE("4.7k", units::kResistance, "ohm", 4700);
  VALUE("4.7k", units::kResistance, "kohm", 4700);      // the letter is the prefix, not x kohm
  VALUE("100n", units::kCapacitance, "uF", 1e-7);
  VALUE("2.2u", units::kCapacitance, "F", 2.2e-6);
  VALUE("33p", units::kCapacitance, "uF", 33e-12);
  VALUE("10M", units::kResistance, "ohm", 1e7);
  VALUE("1G", units::kFrequency, "Hz", 1e9);
  VALUE("5m", units::kVoltage, "V", 0.005);             // no unit "m" here: milli
  VALUE("5m", units::kLength, "ft", 5, "m");            // metres
  VALUE("5 ft 3 in", units::kLength, "m", 1.6002, "ft");
  VALUE("5'3\"", units::kLength, "m", 1.6002, "ft");
  VALUE("2.2 uF", units::kCapacitance, "F", 2.2e-6, "uF");
  VALUE("3/8", units::kLength, "in", 0.009525);
  VALUE("2*pi", kNone, nullptr, 2 * M_PI);
  VALUE("2π", units::kAngle, "rad", 2 * M_PI);
  VALUE("12*2.54", units::kLength, "cm", 0.3048);
  VALUE("100 F", units::kTemperature, "C", 310.9277777778, "F");
  VALUE("100", units::kTemperature, "C", 373.15);
  VALUE("100 F", units::kCapacitance, "uF", 100, "F");
  VALUE("500 mAh", kCharge, "C", 1800, "mAh");
  VALUE("1 Ah 500 mAh", kCharge, "C", 5400, "Ah");
  VALUE("10 C", kTempDiff, "K", 10, "C deg");
  VALUE("100 mH", kInductance, "uH", 0.1, "mH");
  VALUE("1500 rpm", kAngularSpeed, "rad/s", 50 * M_PI, "rpm");
  VALUE("50%", kRatio, "ratio", 0.5, "%");
  VALUE("30 deg", units::kAngle, "rad", M_PI / 6, "deg");
  VALUE("2k", units::kMass, "kg", 2);                   // grams are mass's root
  VALUE("1,000", units::kLength, "m", 1000);
  double si = 0;
  CHECK(!parseValue("", units::kLength, 0, &si));
  CHECK(!parseValue("cheese", units::kLength, 0, &si));
  CHECK(!parseValue("5 V", units::kLength, 0, &si));
  CHECK(!parseValue("5 q", kInductance, 0, &si));
  CHECK(!parseValue("5 ft", kNone, 0, &si));

  // bestUnit and formatValue.
  const int A = unitOf(units::kCurrent, "A");
  CHECK(bestUnit(0.0033, units::kCurrent, A) == unitOf(units::kCurrent, "mA"));
  CHECK(bestUnit(3.3, units::kCurrent, A) == A);
  CHECK(bestUnit(4700, units::kResistance, unitOf(units::kResistance, "ohm")) == unitOf(units::kResistance, "kohm"));
  CHECK(bestUnit(1.5e-6, units::kCapacitance, unitOf(units::kCapacitance, "F")) == unitOf(units::kCapacitance, "uF"));
  CHECK(bestUnit(1500, units::kLength, unitOf(units::kLength, "m")) == unitOf(units::kLength, "km"));
  CHECK(bestUnit(0.002, kInductance, unitOf(kInductance, "H")) == unitOf(kInductance, "mH"));
  CHECK(bestUnit(0, units::kCurrent, A) == A);
  CHECK(bestUnit(5, kNone, 0) == 0);
  CHECK(bestUnit(1e6, units::kTemperature, 0) == 0);
  CHECK(bestUnit(-0.0033, units::kCurrent, A) == unitOf(units::kCurrent, "mA"));
  CHECK(bestUnit(0.5, units::kFrequency, unitOf(units::kFrequency, "Hz")) == unitOf(units::kFrequency, "Hz"));
  CHECK(formatValue(0.0033, units::kCurrent, unitOf(units::kCurrent, "mA")) == "3.3");
  CHECK(formatValue(1.0 / 3, kNone, 0) == "0.333333");
  CHECK(formatValue(273.15, units::kTemperature, unitOf(units::kTemperature, "C")) == "0");
}

// ---- The library's structure ----

// UTF-8 the device's fonts can draw: ASCII, Greek, and a few signs.
static bool drawable(const char *s, std::string *bad) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned cp = 0;
    int n = 0;
    if (*p < 0x80) { cp = *p; n = 1; }
    else if ((*p & 0xE0) == 0xC0) { cp = (unsigned)(*p & 0x1F) << 6 | (p[1] & 0x3F); n = 2; }
    else if ((*p & 0xF0) == 0xE0) { cp = (unsigned)(*p & 0x0F) << 12 | (unsigned)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); n = 3; }
    else { cp = 0xFFFF; n = 1; }
    const bool ok = cp < 0x80 || (cp >= 0x391 && cp <= 0x3C9) || cp == 0xD7 || cp == 0xF7 || cp == 0x221A ||
                    cp == 0xB0 || cp == 0xB1;
    if (!ok) {
      *bad = std::string((const char *)p, n);
      return false;
    }
    p += n;
  }
  return true;
}

static void text(const Formula *f, const char *what, const char *s, bool required = true) {
  checks++;
  if (!s) {
    if (required) { printf("FAIL %s: no %s\n", f->id, what); failures++; }
    return;
  }
  std::string bad;
  if ((required && !*s) || !drawable(s, &bad)) {
    printf("FAIL %s: %s \"%s\" (character \"%s\")\n", f->id, what, s, bad.c_str());
    failures++;
  }
}

static void testLibrary() {
  std::set<std::string> ids;
  std::set<std::string> scripts;
  int lastGroup = 0;
  for (int i = 0; i < formulaCount(); i++) {
    const Formula *f = formula(i);
    CHECK(ids.insert(f->id).second);
    CHECK(findFormula(f->id) == i);
    CHECK(f->group < groupCount());
    if (f->group < lastGroup) { printf("FAIL %s: out of group order\n", f->id); failures++; }
    lastGroup = f->group;
    CHECK(f->varCount >= 1 && f->varCount <= kMaxVars);
    CHECK(f->equationCount >= 1);
    text(f, "name", f->name);
    text(f, "keywords", f->keywords, false);
    text(f, "blurb", f->blurb);

    int inputs = 0;
    std::set<std::string> syms;
    for (int v = 0; v < f->varCount; v++) {
      const Var &x = f->vars[v];
      CHECK(syms.insert(x.sym).second);
      text(f, "label", x.label);
      text(f, "var name", x.name);
      text(f, "help", x.help);
      CHECK(x.dim < kDimCount);
      if (x.unit && unitOf(x.dim, x.unit) < 0) { printf("FAIL %s.%s: no unit %s\n", f->id, x.sym, x.unit); failures++; }
      if (!(x.flags & kConstant)) inputs++;
      if ((x.flags & kConstant) && x.preset == 0) { printf("FAIL %s.%s: constant with no preset\n", f->id, x.sym); failures++; }
      if (x.flags & kAcute) CHECK(x.dim == units::kAngle);
      if (x.presets) {
        std::string list = x.presets;
        size_t at = 0;
        while (at < list.size()) {
          size_t end = list.find('|', at);
          if (end == std::string::npos) end = list.size();
          const std::string item = list.substr(at, end - at);
          const size_t eq = item.find('=');
          char *rest = nullptr;
          const double v2 = eq == std::string::npos ? 0 : strtod(item.c_str() + eq + 1, &rest);
          checks++;
          if (eq == std::string::npos || eq == 0 || !rest || *rest || !std::isfinite(v2) ||
              ((x.flags & kPositive) && v2 <= 0)) {
            printf("FAIL %s.%s: preset \"%s\"\n", f->id, x.sym, item.c_str());
            failures++;
          }
          at = end + 1;
        }
      }
    }
    if (f->needed < 1 || f->needed > inputs) { printf("FAIL %s: needed %d of %d\n", f->id, f->needed, inputs); failures++; }

    // Equations parse, and every variable is in one.
    uint32_t used = 0;
    std::vector<const Node *> stack;
    for (int e = 0; e < f->equationCount; e++) {
      const Node *l = nullptr, *r = nullptr;
      checks++;
      if (!equation(f, e, &l, &r)) {
        printf("FAIL %s: equation %d does not parse: %s\n", f->id, e, f->equations[e]);
        failures++;
        continue;
      }
      stack.push_back(l);
      stack.push_back(r);
    }
    while (!stack.empty()) {
      const Node *n = stack.back();
      stack.pop_back();
      if (n->op == Node::Var) used |= 1u << n->var;
      if (n->a) stack.push_back(n->a);
      if (n->b) stack.push_back(n->b);
    }
    for (int v = 0; v < f->varCount; v++)
      if (!((used >> v) & 1)) { printf("FAIL %s: %s is in no equation\n", f->id, f->vars[v].sym); failures++; }

    // Diagram.
    checks++;
    if (!f->diagram) { printf("FAIL %s: no diagram\n", f->id); failures++; continue; }
    std::string why;
    if (!checkDiagram(f->diagram, &why)) { printf("FAIL %s diagram: %s\n", f->id, why.c_str()); failures++; }
    std::string bad;
    if (!drawable(f->diagram, &bad)) { printf("FAIL %s diagram: character %s\n", f->id, bad.c_str()); failures++; }
    scripts.insert(f->diagram);
  }
  for (int g = 0; g < groupCount(); g++) {
    int out[256];
    const int n = inGroup(g, out, 256);
    CHECK(n > 0);
    CHECK(group(g)->name && group(g)->blurb);
    printf("  %-24s %d\n", group(g)->name, n);
  }
  printf("  %d formulas, %d groups, %d drawings\n", formulaCount(), groupCount(), (int)scripts.size());
  CHECK(group(-1) == nullptr && group(groupCount()) == nullptr);
  CHECK(formula(-1) == nullptr && formula(formulaCount()) == nullptr);
  CHECK(findFormula("nothing") == -1);
  CHECK(!strcmp(group(0)->name, "Electrical"));
  CHECK(!strcmp(formula(0)->id, "ohm"));
  int q[32];
  const int nq = quickList(q, 32);
  CHECK(nq > 4 && q[0] == 0);
  CHECK(quickList(q, 2) == 2);

  std::string why;
  CHECK(checkDiagram("k 1;L 0 0 400 240;T 200 120 Hello", &why));
  CHECK(checkDiagram(nullptr, &why));
  CHECK(!checkDiagram("L 0 0 500 0", &why));
  CHECK(!checkDiagram("L 0 0 5", &why));
  CHECK(!checkDiagram("X 1 2", &why));
  CHECK(!checkDiagram("k 7", &why));
  CHECK(!checkDiagram("T 10 10", &why));
  CHECK(!checkDiagram("T 2 100 A long label off the side", &why));
  CHECK(!checkDiagram("P 1 1 2 2", &why));
  CHECK(!checkDiagram("C 10 10 50", &why));

  // Constants: CODATA.
  std::map<std::string, double> c;
  for (int i = 0; i < constantCount(); i++) c[constant(i)->sym] = constant(i)->value;
  CHECK(constant(-1) == nullptr);
  CHECK(c["c"] == 299792458.0);
  CHECK(near(c["h"] / 2 / M_PI, c["hbar"], 1e-9));
  CHECK(near(c["kB"] * c["NA"], c["Rgas"], 1e-9));
  CHECK(near(c["qe"] * c["NA"], c["F"], 1e-9));
  CHECK(near(1 / (c["eps0"] * c["mu0"]), c["c"] * c["c"], 1e-9));
  CHECK(near(2 * M_PI * M_PI * M_PI * M_PI * M_PI * c["kB"] * c["kB"] * c["kB"] * c["kB"] /
             (15 * c["h"] * c["h"] * c["h"] * c["c"] * c["c"]), c["sigma"], 1e-9));
}

// ---- Dimensions of every equation ----

struct Ex {  // powers of m, kg, s, A, K, mol, cd
  double p[7] = {0, 0, 0, 0, 0, 0, 0};
  bool ok = true;
};
static Ex ex(double m, double kg = 0, double s = 0, double A = 0, double K = 0, double mol = 0, double cd = 0) {
  Ex e;
  e.p[0] = m; e.p[1] = kg; e.p[2] = s; e.p[3] = A; e.p[4] = K; e.p[5] = mol; e.p[6] = cd;
  return e;
}
static bool sameEx(const Ex &a, const Ex &b) {
  for (int i = 0; i < 7; i++)
    if (std::fabs(a.p[i] - b.p[i]) > 1e-9) return false;
  return true;
}
static bool dimensionless(const Ex &a) { return sameEx(a, Ex()); }

static Ex dimEx(int d) {
  using namespace mt::units;
  switch (d) {
    case kLength: return ex(1);
    case kArea: return ex(2);
    case kVolume: return ex(3);
    case kMass: return ex(0, 1);
    case kTemperature: return ex(0, 0, 0, 0, 1);
    case kTime: return ex(0, 0, 1);
    case kSpeed: return ex(1, 0, -1);
    case kAcceleration: return ex(1, 0, -2);
    case kPressure: return ex(-1, 1, -2);
    case kEnergy: return ex(2, 1, -2);
    case kPower: return ex(2, 1, -3);
    case kForce: return ex(1, 1, -2);
    case kTorque: return ex(2, 1, -2);
    case kDataSize: return ex(0);
    case kDataRate: return ex(0, 0, -1);
    case kAngle: return ex(0);
    case kFrequency: return ex(0, 0, -1);
    case kFuelEconomy: return ex(-2);
    case kDensity: return ex(-3, 1);
    case kFlow: return ex(3, 0, -1);
    case kCurrent: return ex(0, 0, 0, 1);
    case kVoltage: return ex(2, 1, -3, -1);
    case kResistance: return ex(2, 1, -3, -2);
    case kCapacitance: return ex(-2, -1, 4, 2);
    case kNone: case kRatio: case kDecibel: return ex(0);
    case kTempDiff: return ex(0, 0, 0, 0, 1);
    case kCharge: return ex(0, 0, 1, 1);
    case kInductance: return ex(2, 1, -2, -2);
    case kMagField: return ex(0, 1, -2, -1);
    case kMagFlux: return ex(2, 1, -2, -1);
    case kConductance: return ex(-2, -1, 3, 2);
    case kResistivity: return ex(3, 1, -3, -2);
    case kAngularSpeed: return ex(0, 0, -1);
    case kAngularAccel: return ex(0, 0, -2);
    case kStiffness: return ex(0, 1, -2);
    case kMomentum: return ex(1, 1, -1);
    case kInertia: return ex(2, 1);
    case kAreaMoment: return ex(4);
    case kSpecificHeat: return ex(2, 0, -2, 0, -1);
    case kThermalCond: return ex(1, 1, -3, 0, -1);
    case kThermalRes: return ex(-2, -1, 3, 0, 1);
    case kHeatTransfer: return ex(0, 1, -3, 0, -1);
    case kExpansion: return ex(0, 0, 0, 0, -1);
    case kViscosity: return ex(-1, 1, -1);
    case kKinViscosity: return ex(2, 0, -1);
    case kAmount: return ex(0, 0, 0, 0, 0, 1);
    case kMolarity: return ex(-3, 0, 0, 0, 0, 1);
    case kMolarMass: return ex(0, 1, 0, 0, 0, -1);
    case kElectricField: return ex(1, 1, -3, -1);
    case kIntensity: return ex(0, 1, -3);
    case kIlluminance: return ex(-2, 0, 0, 0, 0, 0, 1);
    case kLuminousFlux: return ex(0, 0, 0, 0, 0, 0, 1);
    case kRadioactivity: return ex(0, 0, -1);
    case kSpecificEnergy: return ex(2, 0, -2);
    case kMolarEnergy: return ex(2, 1, -2, 0, 0, -1);
    case kLinearDensity: return ex(-1, 1);
  }
  Ex bad;
  bad.ok = false;
  return bad;
}

static Ex constEx(double value) {
  struct { const char *sym; Ex e; } table[] = {
    {"pi", ex(0)}, {"c", ex(1, 0, -1)}, {"h", ex(2, 1, -1)}, {"hbar", ex(2, 1, -1)},
    {"kB", ex(2, 1, -2, 0, -1)}, {"NA", ex(0, 0, 0, 0, 0, -1)}, {"Rgas", ex(2, 1, -2, 0, -1, -1)},
    {"qe", ex(0, 0, 1, 1)}, {"eps0", ex(-3, -1, 4, 2)}, {"mu0", ex(1, 1, -2, -2)},
    {"sigma", ex(0, 1, -3, 0, -4)}, {"Gnewton", ex(3, -1, -2)}, {"g0", ex(1, 0, -2)},
    {"F", ex(0, 0, 1, 1, 0, -1)}, {"bWien", ex(1, 0, 0, 0, 1)},
    {"c0", ex(-3, 0, 0, 0, 0, 1)},
  };
  for (int i = 0; i < constantCount(); i++) {
    if (constant(i)->value != value) continue;
    for (auto &t : table)
      if (!strcmp(t.sym, constant(i)->sym)) return t.e;
  }
  Ex bad;
  bad.ok = false;
  return bad;
}

static bool constantTree(const Node *n, double *out) {
  if (n->op == Node::Var) return false;
  if (n->a && !constantTree(n->a, out)) return false;
  if (n->b && !constantTree(n->b, out)) return false;
  double none[kMaxVars] = {};
  return evaluate(n, none, out);
}

static std::string reason;

static Ex walk(const Node *n, const Formula *f) {
  Ex a, b;
  if (n->a) a = walk(n->a, f);
  if (n->b) b = walk(n->b, f);
  Ex r;
  if (!a.ok || !b.ok) { r.ok = false; return r; }
  auto bad = [&](const char *why) { reason = why; r.ok = false; return r; };
  switch (n->op) {
    case Node::Num: return r;
    case Node::Var: return dimEx(f->vars[n->var].dim);
    case Node::Const: {
      r = constEx(n->value);
      if (!r.ok) reason = "unknown constant";
      return r;
    }
    case Node::Neg: return a;
    case Node::Add:
    case Node::Sub:
      if (!sameEx(a, b)) return bad("adding unlike dimensions");
      return a;
    case Node::Mul:
    case Node::Div:
      for (int i = 0; i < 7; i++) r.p[i] = a.p[i] + (n->op == Node::Mul ? b.p[i] : -b.p[i]);
      return r;
    case Node::Pow: {
      if (!dimensionless(b)) return bad("exponent with a dimension");
      double e = 0;
      if (constantTree(n->b, &e)) {
        for (int i = 0; i < 7; i++) r.p[i] = a.p[i] * e;
        return r;
      }
      if (!dimensionless(a)) return bad("variable power of a dimension");
      return r;
    }
    case Node::Func:
      if (!strcmp(n->name, "sqrt")) { for (int i = 0; i < 7; i++) r.p[i] = a.p[i] / 2; return r; }
      if (!strcmp(n->name, "cbrt")) { for (int i = 0; i < 7; i++) r.p[i] = a.p[i] / 3; return r; }
      if (!strcmp(n->name, "abs")) return a;
      if (!strcmp(n->name, "atan2")) {
        if (!sameEx(a, b)) return bad("atan2 of unlike dimensions");
        return r;
      }
      if (!dimensionless(a)) return bad("function of a dimension");
      return r;
  }
  return r;
}

static void testDimensions() {
  for (int i = 0; i < formulaCount(); i++) {
    const Formula *f = formula(i);
    for (int e = 0; e < f->equationCount; e++) {
      const Node *l = nullptr, *r = nullptr;
      if (!equation(f, e, &l, &r)) continue;
      reason.clear();
      const Ex a = walk(l, f), b = walk(r, f);
      checks++;
      if (!a.ok || !b.ok || !sameEx(a, b)) {
        printf("FAIL %s: dimensions of \"%s\": %s\n", f->id, f->equations[e], reason.empty() ? "sides differ" : reason.c_str());
        failures++;
      }
    }
  }
}

// ---- Known answers, worked by hand ----

struct Set {
  const char *sym;
  double v;
};

static void known(int line, const char *id, std::initializer_list<Set> in, std::initializer_list<Set> out, double rel = 1e-5) {
  checks++;
  const int i = findFormula(id);
  if (i < 0) { printf("FAIL line %d: no formula %s\n", line, id); failures++; return; }
  const Formula *f = formula(i);
  Sheet s;
  s.open(f);
  for (const Set &x : in) {
    const int v = varOf(f, x.sym);
    if (v < 0) { printf("FAIL line %d: %s has no %s\n", line, id, x.sym); failures++; return; }
    s.set(v, x.v);
  }
  for (const Set &x : out) {
    const int v = varOf(f, x.sym);
    if (v < 0) { printf("FAIL line %d: %s has no %s\n", line, id, x.sym); failures++; return; }
    if (!s.hasValue(v) || !near(s.value(v), x.v, rel)) {
      printf("FAIL line %d: %s: %s = %.9g (has=%d, error \"%s\"), want %.9g\n", line, id, x.sym, s.value(v),
             s.hasValue(v), s.error(), x.v);
      failures++;
    }
  }
}
#define KNOWN(...) known(__LINE__, __VA_ARGS__)

static double deg(double d) { return d * M_PI / 180; }

static void testKnown() {
  // Electrical.
  KNOWN("ohm", {{"V", 12}, {"R", 4}}, {{"I", 3}, {"P", 36}});
  KNOWN("ohm", {{"P", 36}, {"I", 3}}, {{"V", 12}, {"R", 4}});
  KNOWN("ohm", {{"P", 36}, {"R", 4}}, {{"V", 12}, {"I", 3}});
  KNOWN("r-series", {{"R1", 100}, {"R2", 220}}, {{"R", 320}});
  KNOWN("r-parallel", {{"R1", 100}, {"R2", 100}}, {{"R", 50}});
  KNOWN("r-parallel", {{"R", 50}, {"R1", 100}}, {{"R2", 100}});
  KNOWN("r-parallel3", {{"R1", 300}, {"R2", 300}, {"R3", 300}}, {{"R", 100}});
  KNOWN("r-series3", {{"R1", 100}, {"R2", 200}, {"R3", 300}}, {{"R", 600}});
  KNOWN("l-series", {{"L1", 1e-3}, {"L2", 2e-3}}, {{"L", 3e-3}});
  KNOWN("energy-use", {{"P", 1000}, {"t", 3600}}, {{"E", 3.6e6}});
  KNOWN("z-rc", {{"R", 100}, {"f", 1000}, {"C", 1e-6}}, {{"Z", 187.964}});
  KNOWN("z-rl", {{"R", 3}, {"f", 1000}, {"L", 4 / (2 * M_PI * 1000)}}, {{"Z", 5}});
  KNOWN("c-series", {{"C1", 2e-6}, {"C2", 2e-6}}, {{"C", 1e-6}});
  KNOWN("c-parallel", {{"C1", 2e-6}, {"C2", 3e-6}}, {{"C", 5e-6}});
  KNOWN("l-parallel", {{"L1", 2e-3}, {"L2", 2e-3}}, {{"L", 1e-3}});
  KNOWN("divider", {{"Vin", 12}, {"R1", 10000}, {"R2", 5000}}, {{"Vout", 4}});
  KNOWN("divider", {{"Vin", 5}, {"Vout", 3.3}, {"R2", 3300}}, {{"R1", 1700}});
  KNOWN("cdivider", {{"I", 3}, {"R1", 10}, {"R2", 20}}, {{"I1", 2}});
  KNOWN("led", {{"Vs", 5}, {"Vf", 2}, {"I", 0.02}}, {{"R", 150}, {"P", 0.06}});
  KNOWN("wire-r", {{"L", 100}, {"A", 1e-6}}, {{"R", 1.68}, {"d", 1.12838e-3}});
  KNOWN("vdrop", {{"I", 10}, {"L", 10}, {"A", 2.5e-6}}, {{"Vd", 1.344}});
  KNOWN("r-temp", {{"R0", 100}, {"dT", 50}}, {{"R", 119.65}});
  KNOWN("battery-life", {{"Q", 2.5 * 3600}, {"I", 0.1}}, {{"t", 90000}});
  KNOWN("battery-energy", {{"V", 12}, {"Q", 3600 * 50}}, {{"E", 2.16e6}});
  KNOWN("transformer", {{"Vp", 120}, {"Np", 1000}, {"Ns", 100}}, {{"Vs", 12}});
  KNOWN("transformer-i", {{"Ip", 1}, {"Np", 1000}, {"Ns", 100}}, {{"Is", 10}});
  KNOWN("rms", {{"Vpk", 170}}, {{"Vrms", 120.2082}, {"Vpp", 340}, {"Vavg", 108.2254}});
  KNOWN("rms", {{"Vrms", 120}}, {{"Vpk", 169.7056}});
  KNOWN("ac-power", {{"V", 120}, {"I", 10}, {"pf", 0.8}}, {{"P", 960}});
  KNOWN("three-phase", {{"V", 400}, {"I", 10}, {"pf", 0.85}}, {{"P", 5888.973}});
  KNOWN("power-triangle", {{"P", 3}, {"Q", 4}}, {{"S", 5}, {"pf", 0.6}, {"phi", deg(53.1301)}});
  KNOWN("efficiency", {{"Pout", 90}, {"Pin", 100}}, {{"eta", 0.9}});
  KNOWN("internal-r", {{"E", 1.5}, {"I", 1}, {"r", 0.2}}, {{"Vt", 1.3}});
  KNOWN("cap-energy", {{"C", 100e-6}, {"V", 10}}, {{"Q", 1e-3}, {"E", 5e-3}});
  KNOWN("cap-energy", {{"Q", 1e-3}, {"E", 5e-3}}, {{"C", 100e-6}, {"V", 10}});
  KNOWN("ind-energy", {{"L", 2e-3}, {"I", 10}}, {{"E", 0.1}});
  // Electronics.
  KNOWN("rc-tau", {{"R", 10000}, {"C", 100e-6}}, {{"tau", 1}});
  KNOWN("rl-tau", {{"L", 10e-3}, {"R", 100}}, {{"tau", 1e-4}});
  KNOWN("rc-charge", {{"Vs", 10}, {"t", 1}, {"R", 1000}, {"C", 1e-3}}, {{"V", 6.32121}});
  KNOWN("rc-charge", {{"Vs", 10}, {"V", 6.32120559}, {"R", 1000}, {"C", 1e-3}}, {{"t", 1}});
  KNOWN("rc-discharge", {{"V0", 10}, {"t", 1}, {"R", 1000}, {"C", 1e-3}}, {{"V", 3.67879}});
  KNOWN("rl-charge", {{"V", 10}, {"R", 10}, {"L", 1}, {"t", 0.1}}, {{"I", 0.632121}});
  KNOWN("xc", {{"f", 1000}, {"C", 1e-6}}, {{"Xc", 159.1549}});
  KNOWN("xl", {{"f", 1000}, {"L", 1e-3}}, {{"Xl", 6.283185}});
  KNOWN("z-rlc", {{"R", 3}, {"Xl", 10}, {"Xc", 6}}, {{"Z", 5}});
  KNOWN("resonance", {{"L", 1e-6}, {"C", 100e-12}}, {{"f", 15.91549e6}});
  KNOWN("q-series", {{"R", 10}, {"L", 1e-3}, {"C", 1e-7}}, {{"Q", 10}});
  KNOWN("q-bandwidth", {{"f", 1e6}, {"BW", 1e4}}, {{"Q", 100}});
  KNOWN("db-power", {{"P2", 100}, {"P1", 1}}, {{"G", 20}});
  KNOWN("db-voltage", {{"V2", 10}, {"V1", 1}}, {{"G", 20}});
  KNOWN("dbm", {{"P", 1}}, {{"L", 30}});
  KNOWN("dbm", {{"L", 0}}, {{"P", 1e-3}});
  KNOWN("555-astable", {{"R1", 1000}, {"R2", 10000}, {"C", 1e-6}}, {{"f", 68.5714}, {"D", 0.52381}});
  KNOWN("555-mono", {{"R", 10000}, {"C", 1e-5}}, {{"t", 0.11}});
  KNOWN("opamp-inv", {{"Vin", 1}, {"Rf", 100000}, {"Rin", 10000}}, {{"Vout", -10}});
  KNOWN("opamp-noninv", {{"Vin", 1}, {"Rf", 100000}, {"Rg", 10000}}, {{"Vout", 11}});
  KNOWN("rc-filter", {{"R", 1000}, {"C", 1e-6}}, {{"fc", 159.1549}});
  KNOWN("wavelength", {{"f", 100e6}}, {{"lambda", 2.997925}});
  KNOWN("dipole", {{"f", 14.2e6}}, {{"L", 10.02827}});
  KNOWN("quarter-wave", {{"f", 146e6}}, {{"L", 0.487676}});
  KNOWN("skin-depth", {{"f", 1e6}}, {{"delta", 6.5233e-5}}, 1e-3);
  KNOWN("fspl", {{"d", 1000}, {"f", 2.4e9}}, {{"L", 100.05}}, 1e-3);
  KNOWN("adc", {{"Vref", 5}, {"n", 10}}, {{"q", 0.0048828}});
  KNOWN("adc", {{"Vref", 4.096}, {"q", 0.001}}, {{"n", 12}});
  KNOWN("zener", {{"Vin", 12}, {"Vz", 5}, {"Iz", 0.01}, {"Il", 0.06}}, {{"R", 100}});
  KNOWN("regulator-heat", {{"Vin", 12}, {"Vout", 5}, {"I", 1}}, {{"P", 7}});
  KNOWN("junction-temp", {{"Ta", 298.15}, {"P", 1}, {"Rth", 60}}, {{"Tj", 358.15}});
  KNOWN("buck", {{"Vin", 12}, {"D", 0.25}}, {{"Vout", 3}});
  KNOWN("boost", {{"Vin", 5}, {"D", 0.5}}, {{"Vout", 10}});
  KNOWN("buck-ripple", {{"Vout", 5}, {"Vin", 10}, {"f", 1e5}, {"L", 100e-6}}, {{"dI", 0.25}});
  KNOWN("rectifier-ripple", {{"I", 1}, {"f", 60}, {"C", 0.01}}, {{"Vr", 0.83333}});
  // Fields.
  KNOWN("plate-cap", {{"A", 0.01}, {"d", 1e-3}}, {{"C", 8.854188e-11}});
  KNOWN("solenoid-l", {{"N", 100}, {"A", 1e-3}, {"l", 0.1}}, {{"L", 1.25664e-4}});
  KNOWN("solenoid-b", {{"N", 1000}, {"I", 1}, {"l", 1}}, {{"B", 1.25664e-3}});
  KNOWN("wire-field", {{"I", 10}, {"r", 0.1}}, {{"B", 2e-5}});
  KNOWN("wire-force", {{"B", 0.5}, {"I", 2}, {"L", 0.1}}, {{"F", 0.1}});
  KNOWN("coulomb", {{"q1", 1e-6}, {"q2", 1e-6}, {"r", 1}}, {{"F", 8.98755e-3}});
  KNOWN("e-field", {{"V", 100}, {"d", 0.01}}, {{"E", 10000}});
  KNOWN("faraday", {{"N", 100}, {"dPhi", 0.01}, {"t", 0.1}}, {{"emf", -10}});
  // Motion.
  KNOWN("suvat", {{"u", 0}, {"a", 2}, {"t", 3}}, {{"v", 6}, {"s", 9}});
  KNOWN("suvat", {{"u", 10}, {"v", 20}, {"t", 2}}, {{"a", 5}, {"s", 30}});
  KNOWN("suvat", {{"u", 0}, {"v", 10}, {"s", 25}}, {{"a", 2}, {"t", 5}});
  KNOWN("suvat", {{"u", 5}, {"a", 0}, {"t", 4}}, {{"v", 5}, {"s", 20}});
  KNOWN("suvat", {{"u", 5}, {"a", 0}, {"s", 20}}, {{"v", 5}, {"t", 4}});
  KNOWN("speed", {{"d", 100000}, {"t", 3600}}, {{"v", 27.7778}});
  KNOWN("free-fall", {{"h", 20}}, {{"t", 2.01962}, {"v", 19.8057}});
  KNOWN("projectile", {{"v", 20}, {"theta", deg(45)}}, {{"R", 40.7886}, {"H", 10.1972}, {"T", 2.88419}});
  KNOWN("vector", {{"vx", 3}, {"vy", 4}}, {{"v", 5}, {"theta", deg(53.1301)}});
  KNOWN("stopping", {{"v", 20}, {"tr", 1}, {"mu", 0.8}}, {{"d", 45.4929}});
  // Forces.
  KNOWN("fma", {{"m", 10}, {"a", 2}}, {{"F", 20}});
  KNOWN("weight", {{"m", 10}}, {{"W", 98.0665}});
  KNOWN("friction", {{"mu", 0.5}, {"N", 100}}, {{"F", 50}});
  KNOWN("incline", {{"m", 10}, {"theta", deg(30)}}, {{"Fp", 49.03325}, {"N", 84.92808}});
  KNOWN("incline-slide", {{"theta", deg(30)}, {"mu", 0.1}}, {{"a", 4.05401}});
  KNOWN("work", {{"F", 10}, {"d", 5}, {"theta", deg(60)}}, {{"W", 25}});
  KNOWN("kinetic", {{"m", 1500}, {"v", 30}}, {{"E", 675000}});
  KNOWN("potential", {{"m", 10}, {"h", 5}}, {{"E", 490.3325}});
  KNOWN("power", {{"W", 1000}, {"t", 10}}, {{"P", 100}});
  KNOWN("power-force", {{"F", 100}, {"v", 5}}, {{"P", 500}});
  KNOWN("momentum", {{"m", 2}, {"v", 3}}, {{"p", 6}});
  KNOWN("impulse", {{"F", 100}, {"t", 0.5}, {"m", 10}}, {{"dv", 5}});
  KNOWN("collision", {{"m1", 1}, {"u1", 4}, {"m2", 3}, {"u2", 0}}, {{"v", 1}});
  KNOWN("hooke", {{"k", 100}, {"x", 0.1}}, {{"F", 10}});
  KNOWN("spring-energy", {{"k", 100}, {"x", 0.1}}, {{"E", 0.5}});
  KNOWN("spring-period", {{"m", 1}, {"k", 100}}, {{"T", 0.628319}});
  KNOWN("pendulum", {{"L", 1}}, {{"T", 2.00641}});
  KNOWN("centripetal", {{"m", 2}, {"v", 3}, {"r", 3}}, {{"F", 6}});
  KNOWN("centripetal-a", {{"v", 10}, {"r", 5}}, {{"a", 20}, {"w", 2}});
  // Rotation.
  KNOWN("rpm", {{"w", 100 * M_PI}}, {{"f", 50}, {"T", 0.02}});
  KNOWN("rim-speed", {{"w", 10}, {"r", 0.5}}, {{"v", 5}});
  KNOWN("torque", {{"F", 50}, {"r", 0.3}}, {{"T", 15}});
  KNOWN("rot-energy", {{"I", 2}, {"w", 10}}, {{"E", 100}});
  KNOWN("inertia-disc", {{"m", 2}, {"r", 0.5}}, {{"I", 0.25}});
  KNOWN("inertia-sphere", {{"m", 5}, {"r", 1}}, {{"I", 2}});
  KNOWN("inertia-rod", {{"m", 3}, {"L", 2}}, {{"I", 1}});
  KNOWN("shaft-power", {{"T", 100}, {"w", 100 * M_PI}}, {{"P", 31415.93}});
  KNOWN("gears", {{"N1", 20}, {"N2", 40}, {"w1", 100}}, {{"w2", 50}});
  KNOWN("gear-torque", {{"N1", 20}, {"N2", 40}, {"T1", 10}}, {{"T2", 20}});
  KNOWN("pulleys", {{"d1", 0.1}, {"d2", 0.2}, {"w1", 100}}, {{"w2", 50}});
  KNOWN("belt-speed", {{"d", 0.1}, {"w", 1000 * M_PI / 30}}, {{"v", 5.23599}});
  KNOWN("belt-length", {{"C", 0.5}, {"D", 0.2}, {"d", 0.1}}, {{"L", 1.47624}});
  KNOWN("lever", {{"F1", 10}, {"d1", 2}, {"d2", 0.5}}, {{"F2", 40}});
  KNOWN("block-tackle", {{"W", 400}, {"n", 4}}, {{"F", 100}});
  KNOWN("bolt-torque", {{"d", 0.01}, {"F", 20000}}, {{"T", 40}});
  // Materials.
  KNOWN("density", {{"m", 7850}, {"V", 1}}, {{"rho", 7850}});
  KNOWN("stress", {{"F", 1000}, {"A", 1e-4}}, {{"sigma", 1e7}});
  KNOWN("strain", {{"dL", 0.001}, {"L", 1}}, {{"e", 0.001}});
  KNOWN("youngs", {{"sigma", 2e8}, {"e", 0.001}}, {{"E", 2e11}});
  KNOWN("bar-stretch", {{"F", 10000}, {"L", 1}, {"A", 1e-4}}, {{"dL", 5e-4}});
  KNOWN("shear", {{"F", 1000}, {"A", 1e-4}}, {{"tau", 1e7}});
  KNOWN("safety-factor", {{"Sy", 2.5e8}, {"sigma", 1e8}}, {{"n", 2.5}});
  KNOWN("cantilever", {{"F", 100}, {"L", 1}, {"I", 1e-8}}, {{"d", 0.0166667}});
  KNOWN("cantilever-udl", {{"F", 100}, {"L", 1}, {"I", 1e-8}}, {{"d", 0.00625}});
  KNOWN("beam-centre", {{"F", 100}, {"L", 1}, {"I", 1e-8}}, {{"d", 0.00104167}});
  KNOWN("beam-udl", {{"F", 100}, {"L", 1}, {"I", 1e-8}}, {{"d", 0.000651042}});
  KNOWN("bending-stress", {{"M", 100}, {"y", 0.01}, {"I", 1e-8}}, {{"sigma", 1e8}});
  KNOWN("i-rect", {{"b", 0.1}, {"h", 0.2}}, {{"I", 6.66667e-5}});
  KNOWN("i-tube", {{"D", 0.1}, {"d", 0.05}}, {{"I", 4.60194e-6}});
  KNOWN("shaft-twist", {{"T", 100}, {"L", 1}, {"d", 0.02}}, {{"phi", 0.0805848}});
  KNOWN("buckling", {{"I", 1e-8}, {"L", 1}}, {{"F", 19739.2}});
  KNOWN("expansion", {{"L", 10}, {"dT", 10}}, {{"dL", 0.0012}});
  KNOWN("hoop-stress", {{"p", 1e6}, {"r", 0.5}, {"t", 0.01}}, {{"sigma", 5e7}});
  KNOWN("coil-spring", {{"d", 0.002}, {"D", 0.02}, {"n", 10}}, {{"k", 1975}});
  // Fluids.
  KNOWN("pressure", {{"F", 100}, {"A", 0.01}}, {{"p", 10000}});
  KNOWN("hydrostatic", {{"h", 10}}, {{"p", 97870.4}});
  KNOWN("buoyancy", {{"V", 0.001}}, {{"F", 9.78704}});
  KNOWN("flow", {{"A", 0.01}, {"v", 2}}, {{"Q", 0.02}});
  KNOWN("pipe-flow", {{"d", 0.1}, {"v", 1}}, {{"Q", 0.00785398}});
  KNOWN("continuity", {{"A1", 2}, {"v1", 1}, {"A2", 1}}, {{"v2", 2}});
  KNOWN("bernoulli", {{"p1", 200000}, {"v1", 0}, {"h1", 0}, {"v2", 10}, {"h2", 0}}, {{"p2", 150100}});
  KNOWN("torricelli", {{"h", 5}}, {{"v", 9.90285}});
  KNOWN("reynolds", {{"v", 1}, {"d", 0.05}}, {{"Re", 49800.4}});
  KNOWN("poiseuille", {{"L", 1}, {"Q", 1e-6}, {"d", 0.01}}, {{"dp", 4.0822}}, 1e-3);
  KNOWN("darcy", {{"f", 0.02}, {"L", 100}, {"d", 0.1}, {"v", 2}}, {{"dp", 39920}});
  KNOWN("hydraulic-press", {{"F1", 100}, {"A1", 1}, {"A2", 10}}, {{"F2", 1000}});
  KNOWN("cylinder-force", {{"p", 6e5}, {"d", 0.05}}, {{"F", 1178.1}});
  KNOWN("drag", {{"v", 30}, {"Cd", 0.3}, {"A", 2.2}}, {{"F", 357.6}}, 1e-3);
  KNOWN("terminal", {{"m", 80}, {"Cd", 1}, {"A", 0.7}}, {{"v", 43.1477}});
  KNOWN("pump-power", {{"Q", 0.01}, {"H", 10}}, {{"P", 1398.1}}, 1e-3);
  // Heat.
  KNOWN("ideal-gas", {{"n", 1}, {"T", 273.15}, {"p", 101325}}, {{"V", 0.0224140}});
  KNOWN("combined-gas", {{"p1", 100}, {"V1", 2}, {"T1", 300}, {"V2", 1}, {"T2", 300}}, {{"p2", 200}});
  KNOWN("boyle", {{"p1", 100}, {"V1", 2}, {"V2", 1}}, {{"p2", 200}});
  KNOWN("heat", {{"m", 1}, {"dT", 80}}, {{"Q", 334880}});
  KNOWN("latent", {{"m", 1}}, {{"Q", 2.257e6}});
  KNOWN("flow-heat", {{"Q", 1e-4}, {"dT", 10}}, {{"P", 4177.6}}, 1e-3);
  KNOWN("conduction", {{"A", 10}, {"dT", 20}, {"d", 0.1}}, {{"P", 2000}});
  KNOWN("convection", {{"h", 10}, {"A", 2}, {"dT", 30}}, {{"P", 600}});
  KNOWN("radiation", {{"A", 1}, {"T", 400}, {"Ts", 300}}, {{"P", 893.1}}, 1e-3);
  KNOWN("carnot", {{"Th", 600}, {"Tc", 300}}, {{"eta", 0.5}});
  KNOWN("newton-cooling", {{"Ta", 300}, {"T0", 400}, {"k", 0.01}, {"t", 100}}, {{"T", 336.788}});
  // Waves.
  KNOWN("wave", {{"f", 440}, {"lambda", 0.78}}, {{"v", 343.2}});
  KNOWN("doppler", {{"fs", 1000}, {"vo", 0}, {"vs", 34.3}}, {{"fo", 1111.11}});
  KNOWN("sound-level", {{"I", 1e-6}}, {{"L", 60}});
  KNOWN("spl", {{"p", 1}}, {{"L", 93.9794}});
  KNOWN("inverse-square", {{"P", 100}, {"r", 1}}, {{"I", 7.95775}});
  KNOWN("string", {{"T", 100}, {"mu", 0.001}, {"L", 0.5}}, {{"f", 316.228}});
  KNOWN("sound-speed", {{"T", 293.15}}, {{"v", 343.2}}, 1e-3);
  // Light.
  KNOWN("photon", {{"lambda", 500e-9}}, {{"E", 3.97289e-19}, {"f", 5.99585e14}});
  KNOWN("snell", {{"t1", deg(30)}}, {{"t2", deg(19.4712)}});
  KNOWN("critical-angle", {{"n1", 1.5}, {"n2", 1}}, {{"tc", deg(41.8103)}});
  KNOWN("thin-lens", {{"f", 50}, {"do", 100}}, {{"di", 100}, {"M", -1}});
  KNOWN("thin-lens", {{"f", 100}, {"do", 50}}, {{"di", -100}, {"M", 2}});
  KNOWN("grating", {{"d", 2e-6}, {"m", 1}, {"lambda", 500e-9}}, {{"theta", deg(14.4775)}});
  KNOWN("double-slit", {{"lambda", 500e-9}, {"L", 1}, {"d", 1e-3}}, {{"y", 5e-4}});
  KNOWN("f-number", {{"f", 50}, {"D", 25}}, {{"N", 2}});
  KNOWN("angle-of-view", {{"s", 0.036}, {"f", 0.05}}, {{"a", deg(39.5978)}});
  KNOWN("wien", {{"T", 5772}}, {{"lambda", 5.02041e-7}});
  // Atoms, space.
  KNOWN("mass-energy", {{"m", 1}}, {{"E", 8.98755e16}});
  KNOWN("de-broglie", {{"m", 9.1093837e-31}, {"v", 1e6}}, {{"lambda", 7.2739e-10}});
  KNOWN("photoelectric", {{"f", 1e15}}, {{"K", 2.9761e-19}}, 1e-3);
  KNOWN("time-dilation", {{"t0", 1}, {"v", 0.6 * 299792458.0}}, {{"t", 1.25}});
  KNOWN("gravitation", {{"m1", 5.972e24}, {"m2", 7.342e22}, {"r", 3.844e8}}, {{"F", 1.98049e20}}, 1e-3);
  KNOWN("surface-gravity", {{"M", 5.972e24}, {"r", 6.371e6}}, {{"g", 9.82}}, 1e-3);
  KNOWN("orbital-speed", {{"M", 5.972e24}, {"r", 6.771e6}}, {{"v", 7672}}, 1e-3);
  KNOWN("orbital-period", {{"M", 5.972e24}, {"r", 4.2164e7}}, {{"T", 86165}}, 1e-3);
  KNOWN("escape", {{"M", 5.972e24}, {"r", 6.371e6}}, {{"v", 11185}}, 1e-3);
  KNOWN("kepler", {{"T1", 1}, {"a1", 1}, {"a2", 4}}, {{"T2", 8}});
  KNOWN("rocket", {{"ve", 3000}, {"m0", 100}, {"mf", 36.788}}, {{"dv", 3000}}, 1e-3);
  // Chemistry.
  KNOWN("moles", {{"m", 0.018015}, {"M", 0.018015}}, {{"n", 1}});
  KNOWN("molarity", {{"n", 1}, {"V", 0.001}}, {{"c", 1000}});
  KNOWN("dilution", {{"c1", 1000}, {"V1", 0.01}, {"V2", 0.1}}, {{"c2", 100}});
  KNOWN("ph", {{"H", 1}}, {{"pH", 3}});      // 1 mol/m^3 is 1e-3 mol/L
  KNOWN("ph", {{"pH", 7}}, {{"H", 1e-4}});
  KNOWN("arrhenius", {{"A", 1e13}, {"Ea", 50000}, {"T", 298.15}}, {{"k", 17393}}, 1e-3);
  KNOWN("half-life", {{"N0", 100}, {"t", 10}, {"th", 5}}, {{"N", 25}});
  KNOWN("half-life", {{"N0", 100}, {"N", 25}, {"th", 5}}, {{"t", 10}});
  KNOWN("activity", {{"lam", 0.01}, {"N", 1000}}, {{"A", 10}});
  KNOWN("nernst", {{"E0", 1.1}, {"T", 298.15}, {"z", 2}, {"Q", 10}}, {{"E", 1.07042}});
  KNOWN("electrolysis", {{"Q", 96485.33212 * 2}, {"M", 0.06355}, {"z", 2}}, {{"m", 0.06355}});
  // Geometry.
  KNOWN("circle", {{"r", 1}}, {{"d", 2}, {"C", 2 * M_PI}, {"A", M_PI}});
  KNOWN("circle", {{"A", M_PI}}, {{"r", 1}});
  KNOWN("arc", {{"r", 2}, {"theta", M_PI / 2}}, {{"s", M_PI}, {"A", M_PI}, {"c", 2.82843}});
  KNOWN("sphere", {{"r", 1}}, {{"V", 4.18879}, {"A", 12.5664}});
  KNOWN("cylinder", {{"r", 1}, {"h", 2}}, {{"V", 2 * M_PI}, {"A", 6 * M_PI}});
  KNOWN("cone", {{"r", 3}, {"h", 4}}, {{"s", 5}, {"V", 12 * M_PI}, {"A", 24 * M_PI}});
  KNOWN("rectangle", {{"w", 3}, {"h", 4}}, {{"A", 12}, {"P", 14}, {"d", 5}});
  KNOWN("box", {{"l", 1}, {"w", 2}, {"h", 3}}, {{"V", 6}, {"A", 22}});
  KNOWN("triangle-area", {{"b", 4}, {"h", 3}}, {{"A", 6}});
  KNOWN("pythagoras", {{"a", 3}, {"b", 4}}, {{"c", 5}});
  KNOWN("pythagoras", {{"a", 3}, {"c", 5}}, {{"b", 4}});
  KNOWN("right-triangle", {{"a", 3}, {"b", 4}}, {{"c", 5}, {"A", deg(36.8699)}, {"B", deg(53.1301)}});
  KNOWN("law-cosines", {{"a", 3}, {"b", 4}, {"C", M_PI / 2}}, {{"c", 5}});
  KNOWN("triangle", {{"a", 3}, {"b", 4}, {"c", 5}}, {{"C", M_PI / 2}});
  KNOWN("triangle", {{"a", 5}, {"b", 7}, {"C", deg(120)}}, {{"c", 10.44031}, {"A", deg(24.5036)}});
  KNOWN("slope", {{"rise", 1}, {"run", 12}}, {{"G", 1.0 / 12}, {"theta", deg(4.76364)}});
  KNOWN("quadratic", {{"a", 1}, {"b", -3}, {"c", 2}}, {{"x", 1}});
}

// ---- Solver behaviour ----

static void testSheet() {
  Sheet s;
  const Formula *ohm = formula(findFormula("ohm"));
  const int V = varOf(ohm, "V"), I = varOf(ohm, "I"), R = varOf(ohm, "R"), P = varOf(ohm, "P");
  s.open(ohm);
  CHECK(s.current() == ohm);
  CHECK(s.missing() == 2);
  CHECK(!*s.error());
  s.set(V, 12);
  CHECK(s.missing() == 1);
  CHECK(s.cell(V) == Cell::Entered && s.cell(I) == Cell::Empty);
  s.set(R, 4);
  CHECK(s.missing() == 0);
  CHECK(s.cell(I) == Cell::Solved && near(s.value(I), 3));
  CHECK(s.howLhs(V) == nullptr);
  CHECK(s.howLhs(I) && s.howLhs(I)->op == Node::Var && s.howLhs(I)->var == I);
  double vals[kMaxVars] = {};
  vals[V] = 12; vals[R] = 4;
  double out = 0;
  CHECK(evaluate(s.howRhs(I), vals, &out) && near(out, 3));

  // Filled in: retyping drops the oldest of the others.
  s.set(I, 6);          // V 12 entered first: V and R were inputs, so V goes
  CHECK(s.cell(V) == Cell::Solved && near(s.value(V), 24));
  CHECK(s.cell(R) == Cell::Entered && s.cell(I) == Cell::Entered);
  s.set(R, 2);          // R retyped, still two inputs
  CHECK(near(s.value(V), 12));
  s.set(P, 72);         // I is now the oldest
  CHECK(s.cell(I) == Cell::Solved && near(s.value(I), 6) && near(s.value(V), 12));

  // solveFor and clear.
  s.solveFor(R);
  CHECK(s.cell(R) != Cell::Entered);
  CHECK(s.missing() == 1);
  s.set(V, 12);
  CHECK(near(s.value(R), 2));
  s.clear(V);
  CHECK(!s.hasValue(V) && !s.hasValue(R) && s.missing() == 1);
  s.clearAll();
  CHECK(s.missing() == 2 && !s.hasValue(P));

  // kPositive.
  s.set(R, -4);
  CHECK(!strcmp(s.error(), "Needs R above 0"));
  CHECK(s.missing() == 0);
  s.set(R, 4);
  CHECK(!*s.error());
  // sqrt inversion takes the positive root and says nothing more when only that is allowed.
  s.set(P, 36);
  CHECK(near(s.value(I), 3) && s.altVar() < 0 && !*s.error());

  // A negative solve of a positive variable.
  const Formula *led = formula(findFormula("led"));
  s.open(led);
  s.set(varOf(led, "Vs"), 2);
  s.set(varOf(led, "Vf"), 3);
  s.set(varOf(led, "I"), 0.02);
  CHECK(!strcmp(s.error(), "Needs R above 0"));

  // Inconsistent entries.
  const Formula *rt = formula(findFormula("right-triangle"));
  s.open(rt);
  s.set(varOf(rt, "A"), deg(30));
  s.set(varOf(rt, "B"), deg(70));
  CHECK(!strcmp(s.error(), "These values don't fit together"));
  s.set(varOf(rt, "B"), deg(60));
  CHECK(!strcmp(s.error(), "Those values don't settle the rest"));

  // Constants.
  const Formula *w = formula(findFormula("weight"));
  const int m = varOf(w, "m"), W = varOf(w, "W"), g = varOf(w, "g");
  s.open(w);
  CHECK(s.cell(g) == Cell::Preset && near(s.value(g), 9.80665));
  CHECK(s.missing() == 1);
  s.set(m, 10);
  CHECK(near(s.value(W), 98.0665));
  s.set(g, 1.62);                       // the Moon; still only one entered
  CHECK(s.cell(g) == Cell::Preset && s.cell(m) == Cell::Entered && near(s.value(W), 16.2));
  s.clear(g);
  CHECK(near(s.value(g), 9.80665) && near(s.value(W), 98.0665));
  s.solveFor(g);
  CHECK(!s.hasValue(g) && s.missing() == 1);
  s.set(W, 16.2);
  CHECK(s.cell(g) == Cell::Solved && near(s.value(g), 1.62) && s.cell(m) == Cell::Entered);
  s.clearAll();
  CHECK(s.cell(g) == Cell::Preset);

  // Two solutions: the ball on the way up and on the way down.
  const Formula *su = formula(findFormula("suvat"));
  s.open(su);
  const int t = varOf(su, "t"), v = varOf(su, "v");
  s.set(varOf(su, "s"), 15);
  s.set(varOf(su, "u"), 20);
  s.set(varOf(su, "a"), -10);
  CHECK(near(s.value(t), 1) && near(s.value(v), 10));
  // The second way there is the other sign of v, with t = 3.
  CHECK(s.altVar() == v && near(s.altValue(), -10));
  CHECK(!strcmp(s.error(), "Two solutions: showing the positive one"));
  CHECK(s.howLhs(t) != nullptr);

  // A numeric solve shows the stored equation.
  const Formula *st = formula(findFormula("stopping"));
  s.open(st);
  const int sv = varOf(st, "v");
  s.set(varOf(st, "d"), 45.4929053);
  s.set(varOf(st, "tr"), 1);
  s.set(varOf(st, "mu"), 0.8);
  CHECK(near(s.value(sv), 20, 1e-5));
  CHECK(s.howLhs(sv) && (s.howLhs(sv)->op != Node::Var || s.howLhs(sv)->var != sv));

  // Projectile: the high and the low shot.
  const Formula *pj = formula(findFormula("projectile"));
  s.open(pj);
  s.set(varOf(pj, "v"), 20);
  s.set(varOf(pj, "R"), 20);
  const int th = varOf(pj, "theta");
  CHECK(s.altVar() == th);
  CHECK(near(s.value(th) + s.altValue(), M_PI / 2, 1e-9));

  // An obtuse triangle: asin alone would give the acute angle.
  const Formula *tr = formula(findFormula("triangle"));
  s.open(tr);
  s.set(varOf(tr, "a"), 3);
  s.set(varOf(tr, "b"), 4);
  s.set(varOf(tr, "c"), 6);
  CHECK(near(s.value(varOf(tr, "C")), deg(117.2796), 1e-5));
  CHECK(s.altVar() < 0);
  // Two sides and an angle not between them: the ambiguous case.
  s.open(tr);
  s.set(varOf(tr, "a"), 4);
  s.set(varOf(tr, "b"), 5);
  s.set(varOf(tr, "A"), deg(40));
  CHECK(s.altVar() == varOf(tr, "B"));
  // No triangle at all.
  s.open(tr);
  s.set(varOf(tr, "a"), 1);
  s.set(varOf(tr, "b"), 5);
  s.set(varOf(tr, "A"), deg(40));
  CHECK(*s.error() && s.missing() == 0);

  // Temperature differences.
  const Formula *h = formula(findFormula("heat"));
  s.open(h);
  double si = 0;
  CHECK(parseValue("10 C", h->vars[varOf(h, "dT")].dim, 0, &si));
  s.set(varOf(h, "dT"), si);
  s.set(varOf(h, "m"), 1);
  CHECK(near(s.value(varOf(h, "Q")), 41860));

  // Over-entering a formula with no solution.
  const Formula *q = formula(findFormula("quadratic"));
  s.open(q);
  s.set(varOf(q, "a"), 1);
  s.set(varOf(q, "b"), 0);
  s.set(varOf(q, "c"), 1);
  CHECK(!strcmp(s.error(), "No real solution for x"));
  s.set(varOf(q, "c"), -4);
  CHECK(near(s.value(varOf(q, "x")), 2) && s.altVar() == varOf(q, "x") && near(s.altValue(), -2));

  // Bad indices are harmless.
  s.set(99, 1);
  s.clear(-1);
  s.solveFor(99);
  CHECK(s.cell(99) == Cell::Empty && s.value(-1) == 0 && s.howLhs(99) == nullptr);
  s.open(nullptr);
  CHECK(s.current() == nullptr && s.missing() == 0);
  s.set(0, 1);
}

// ---- Every subset of every formula ----

// Subsets of `needed` that truly do not settle the rest: they say the same thing twice.
struct Degenerate {
  const char *id;
  const char *subset;   // symbols, in the formula's order, comma separated
};
static const Degenerate kDegenerate[] = {
  {"wire-r", "A,d"},                 // one wire's area and diameter are the same fact; L or R is left open
  {"led", "I,R,P"},                  // P = I^2 R: says nothing about the supply
  {"power-triangle", "pf,phi"},      // pf = cos(phi): no size given
  {"555-astable", "D,R1,R2"},        // D follows from R1 and R2: f and C are left open
  {"projectile", "H,T"},             // H = g T^2 / 8 for every launch
  {"right-triangle", "A,B"},         // same shape at any size
  {"triangle", "A,B,C"},             // same shape at any size
  {"slope", "G,theta"},              // both are the same slope
};

static bool isDegenerate(const Formula *f, const std::string &subset) {
  for (const Degenerate &d : kDegenerate)
    if (!strcmp(d.id, f->id) && subset == d.subset) return true;
  return false;
}

static std::mt19937 rng(12345);
static double uniform(double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); }
static double logUniform(double a, double b) { return std::exp(uniform(std::log(a), std::log(b))); }

// A plausible size for a variable, in SI.
static double sample(const Var &v) {
  using namespace mt::units;
  double lo = 1, hi = 100;
  switch (v.dim) {
    case kLength: lo = 0.01; hi = 100; break;
    case kArea: lo = 1e-4; hi = 10; break;
    case kVolume: lo = 1e-4; hi = 1; break;
    case kMass: lo = 0.01; hi = 1000; break;
    case kTemperature: return uniform(250, 1000);
    case kTime: lo = 0.01; hi = 1000; break;
    case kSpeed: lo = 0.1; hi = 100; break;
    case kAcceleration: lo = 0.1; hi = 50; break;
    case kPressure: lo = 1e3; hi = 1e7; break;
    case kEnergy: lo = 0.1; hi = 1e5; break;
    case kPower: lo = 0.1; hi = 1e4; break;
    case kForce: lo = 0.1; hi = 1e4; break;
    case kTorque: lo = 0.1; hi = 1000; break;
    case kDataRate: lo = 1e3; hi = 1e9; break;
    case kAngle:
      if (v.flags & kAcute) return uniform(0.1, 1.45);
      if (v.flags & kPositive) return uniform(0.2, 2.9);
      return uniform(-1.4, 1.4);
    case kFrequency: lo = 10; hi = 1e7; break;
    case kDensity: lo = 1; hi = 1e4; break;
    case kFlow: lo = 1e-5; hi = 1; break;
    case kCurrent: lo = 1e-3; hi = 100; break;
    case kVoltage: lo = 0.1; hi = 1000; break;
    case kResistance: lo = 1; hi = 1e6; break;
    case kCapacitance: lo = 1e-12; hi = 1e-3; break;
    case kNone: lo = 0.5; hi = 50; break;
    case kRatio: return uniform(0.1, 0.9);
    case kDecibel: lo = 1; hi = 60; break;
    case kTempDiff: lo = 1; hi = 100; break;
    case kCharge: lo = 1e-6; hi = 100; break;
    case kInductance: lo = 1e-6; hi = 1; break;
    case kMagField: lo = 1e-5; hi = 1; break;
    case kMagFlux: lo = 1e-5; hi = 1; break;
    case kStiffness: lo = 10; hi = 1e5; break;
    case kAreaMoment: lo = 1e-10; hi = 1e-4; break;
    case kMolarity: lo = 1e-3; hi = 1e3; break;
    case kMolarMass: lo = 1e-3; hi = 0.3; break;
    case kAmount: lo = 1e-3; hi = 100; break;
    default: break;
  }
  double x = logUniform(lo, hi);
  if (v.flags & kInteger) x = std::round(x) + 1;
  if (!(v.flags & (kPositive | kAcute)) && uniform(0, 1) < 0.25) x = -x;
  return x;
}

// Per-formula sizes where the generic ones will not do.
static double sampleFor(const Formula *f, const Var &v) {
  // Exponentials well short of saturating, where the inputs no longer carry the answer.
  for (const char *id : {"rc-charge", "rc-discharge", "rl-charge", "newton-cooling"})
    if (!strcmp(f->id, id) && v.dim != mt::units::kTemperature) return logUniform(0.3, 3);
  if (!strcmp(v.sym, "v") && !strcmp(f->id, "time-dilation")) return uniform(1e7, 2.9e8);
  if (!strcmp(v.sym, "K") && !strcmp(f->id, "photoelectric")) return logUniform(1e-20, 1e-18);
  return sample(v);
}

static bool allHold(const Sheet &s, const Formula *f, double *vals) {
  for (int v = 0; v < f->varCount; v++) vals[v] = s.value(v);
  for (int e = 0; e < f->equationCount; e++) {
    const Node *l = nullptr, *r = nullptr;
    if (!equation(f, e, &l, &r)) return false;
    double a = 0, b = 0;
    if (!evaluate(l, vals, &a) || !evaluate(r, vals, &b)) continue;   // t = (v-u)/a with a = 0
    // A side of plain zero has only the terms of the other to be judged against.
    if (a == 0 || b == 0) {
      if (std::fabs(a - b) > 1e-6) return false;
    } else if (std::fabs(a - b) > 1e-5 * std::fmax(std::fabs(a), std::fabs(b))) {
      return false;
    }
  }
  return true;
}

static bool wellConditioned(const Formula *f, const double *p);

// Finds a point on the formula: real values satisfying all its equations.
static bool findPoint(const Formula *f, const std::vector<int> &inputs, double *point) {
  Sheet s;
  std::vector<int> order = inputs;
  for (int attempt = 0; attempt < 5000; attempt++) {
    std::shuffle(order.begin(), order.end(), rng);
    s.open(f);
    for (int i = 0; i < f->needed; i++) s.set(order[i], sampleFor(f, f->vars[order[i]]));
    bool all = true;
    for (int v = 0; v < f->varCount; v++)
      if (!s.hasValue(v) || !std::isfinite(s.value(v))) all = false;
    if (!all || s.altVar() >= 0) continue;
    double vals[kMaxVars];
    if (!allHold(s, f, vals)) continue;
    for (int v = 0; v < f->varCount; v++) {
      point[v] = s.value(v);
      if ((f->vars[v].flags & kPositive) && point[v] <= 0) all = false;
    }
    if (all && wellConditioned(f, point)) return true;
  }
  return false;
}

// Points so close to a limit that a double cannot tell them from it.
static bool wellConditioned(const Formula *f, const double *p) {
  for (const char *id : {"z-rc", "z-rl", "z-rlc"}) {
    if (strcmp(f->id, id)) continue;
    const double r = p[varOf(f, "R")] / p[varOf(f, "Z")];
    if (r > 0.99 || r < 0.01) return false;
  }
  if (!strcmp(f->id, "projectile") && p[varOf(f, "theta")] > 1.4) return false;
  if (!strcmp(f->id, "buck-ripple") && p[varOf(f, "Vout")] > 0.99 * p[varOf(f, "Vin")]) return false;
  if (!strcmp(f->id, "arc") && (p[varOf(f, "theta")] > 6 || p[varOf(f, "theta")] < 0.01)) return false;
  if (!strcmp(f->id, "i-tube")) {
    const double r = p[varOf(f, "d")] / p[varOf(f, "D")];
    if (r > 0.95 || r < 0.05) return false;
  }
  if (!strcmp(f->id, "rectangle")) {
    const double r = p[varOf(f, "w")] / p[varOf(f, "h")];
    if (r > 100 || r < 0.01) return false;
  }
  if (!strcmp(f->id, "cone")) {
    const double r = p[varOf(f, "r")] / p[varOf(f, "h")];
    if (r > 100 || r < 0.01) return false;
  }
  return true;
}

static void subsets(int n, int k, int start, std::vector<int> &cur, std::vector<std::vector<int>> *out) {
  if ((int)cur.size() == k) {
    out->push_back(cur);
    return;
  }
  for (int i = start; i < n; i++) {
    cur.push_back(i);
    subsets(n, k, i + 1, cur, out);
    cur.pop_back();
  }
}

static int subsetsRun = 0;

// Enters the subset and checks the rest come back; false with a message if not.
static bool roundTrip(Sheet &s, const Formula *f, const std::vector<int> &entered, const double *point,
                      std::string *why) {
  s.open(f);
  for (int i : entered) s.set(i, point[i]);
  if (s.error()[0] && strncmp(s.error(), "Two solutions", 13)) {
    *why = s.error();
    return false;
  }
  bool differs = false;
  for (int v = 0; v < f->varCount; v++) {
    if (!s.hasValue(v)) {
      *why = std::string("no value for ") + f->vars[v].sym;
      return false;
    }
    if (!near(s.value(v), point[v], 1e-6) && std::fabs(s.value(v) - point[v]) > 1e-7 * std::fabs(point[v]) + 1e-12)
      differs = true;
  }
  if (differs) {
    // The other root is as good, as long as the sheet offered the one we started from.
    double vals[kMaxVars];
    if (s.altVar() < 0 || !allHold(s, f, vals)) {
      *why = "came back different";
      for (int v = 0; v < f->varCount; v++) {
        char b[80];
        snprintf(b, sizeof b, " %s=%.8g(%.8g)", f->vars[v].sym, s.value(v), point[v]);
        *why += b;
      }
      return false;
    }
    if (!near(s.altValue(), point[s.altVar()], 1e-6)) {
      // Several roots down the chain: what came back must still satisfy everything.
      *why = "";
    }
  }
  return true;
}

static void testSubsets(int rounds) {
  std::map<std::string, int> unused;
  for (const Degenerate &d : kDegenerate) unused[std::string(d.id) + ":" + d.subset] = 1;
  Sheet s;
  for (int i = 0; i < formulaCount(); i++) {
    const Formula *f = formula(i);
    std::vector<int> inputs;
    for (int v = 0; v < f->varCount; v++)
      if (!(f->vars[v].flags & kConstant)) inputs.push_back(v);
    std::vector<std::vector<int>> all;
    std::vector<int> cur;
    subsets((int)inputs.size(), f->needed, 0, cur, &all);
    int failedHere = 0;
    for (int round = 0; round < rounds && failedHere < 6; round++) {
      double point[kMaxVars];
      checks++;
      if (!findPoint(f, inputs, point)) {
        printf("FAIL %s: found no values that satisfy it\n", f->id);
        failures++;
        break;
      }
      for (const std::vector<int> &pick : all) {
        std::vector<int> entered;
        std::string name;
        for (int k : pick) {
          entered.push_back(inputs[k]);
          if (!name.empty()) name += ",";
          name += f->vars[inputs[k]].sym;
        }
        if (isDegenerate(f, name)) {
          unused.erase(std::string(f->id) + ":" + name);
          continue;
        }
        subsetsRun++;
        checks++;
        std::string why;
        if (!roundTrip(s, f, entered, point, &why)) {
          if (failedHere++ < 6) printf("FAIL %s: entering {%s}: %s\n", f->id, name.c_str(), why.c_str());
          failures++;
        }
      }
    }

    // A constant can be solved for: one more value entered, and it comes back.
    for (int c = 0; c < f->varCount; c++) {
      if (!(f->vars[c].flags & kConstant)) continue;
      double point[kMaxVars];
      if (!findPoint(f, inputs, point)) break;
      std::vector<std::vector<int>> more;
      std::vector<int> cur2;
      subsets((int)inputs.size(), f->needed + 1, 0, cur2, &more);
      bool any = false;
      for (const std::vector<int> &pick : more) {
        s.open(f);
        s.solveFor(c);
        for (int k : pick) s.set(inputs[k], point[inputs[k]]);
        if (s.hasValue(c) && near(s.value(c), point[c], 1e-6)) any = true;
      }
      checks++;
      if (!any && !more.empty()) {
        printf("FAIL %s: constant %s cannot be solved for\n", f->id, f->vars[c].sym);
        failures++;
      }
    }
  }
  for (auto &u : unused) {
    printf("FAIL allowlist entry %s never came up\n", u.first.c_str());
    failures++;
  }
  printf("  %d subsets solved\n", subsetsRun);
}

// ---- Search ----

static void find(int line, const char *query, const char *first) {
  checks++;
  int out[64];
  const int n = search(query, out, 64);
  if (n == 0 || strcmp(formula(out[0])->id, first)) {
    printf("FAIL line %d: search \"%s\" gave %d, first %s, want %s\n", line, query, n,
           n ? formula(out[0])->id : "(none)", first);
    failures++;
  }
}
#define FIND(...) find(__LINE__, __VA_ARGS__)

static void testSearch() {
  FIND("ohm", "ohm");
  FIND("Ohm's law", "ohm");
  FIND("watts", "ohm");
  FIND("kinetic", "kinetic");
  FIND("cap reactance", "xc");
  FIND("f=ma", "fma");
  FIND("F = m*a", "fma");
  FIND("led", "led");
  FIND("voltage divider", "divider");
  FIND("555", "555-astable");
  FIND("pendulum", "pendulum");
  FIND("snell", "snell");
  FIND("bernoulli", "bernoulli");
  FIND("pythagoras", "pythagoras");
  FIND("ideal gas", "ideal-gas");
  FIND("half life", "half-life");
  int out[8];
  CHECK(search("", out, 8) == 0);
  CHECK(search("   ", out, 8) == 0);
  CHECK(search("zzzzqqq", out, 8) == 0);
  CHECK(search("resistor", out, 2) == 2);
}

int main(int argc, char **argv) {
  const int rounds = argc > 1 ? atoi(argv[1]) : 3;
  testUnits();
  testLibrary();
  testDimensions();
  testKnown();
  testSheet();
  testSubsets(rounds);
  testSearch();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
