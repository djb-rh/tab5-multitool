// Host test for the RPN engine: builds with the Mac's clang, no device.
//   test/rpn_test.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../firmware/src/app/rpn.h"

using namespace mt::rpn;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) CHECK(fabs((a) - (b)) <= 1e-9 * (fabs(b) > 1 ? fabs(b) : 1))
#define EQS(a, b) do { std::string s_ = (a); if (s_ != (b)) { \
  printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, s_.c_str(), (b)); failures++; } } while (0)

// Types a number the way a user would: "1.5e-3" -> 1 . 5 EEX CHS 3, "-2" ends with CHS.
static void type(Calc &c, const char *s) {
  bool neg = false;
  for (const char *p = s; *p; p++) {
    if (*p == '-' && p == s) neg = true;
    else if (*p == '-') c.chs();
    else if (*p == '.') c.point();
    else if (*p == 'e') c.eex();
    else if (*p >= 'A' && *p <= 'F') c.digit(*p - 'A' + 10);
    else c.digit(*p - '0');
  }
  if (neg) c.chs();
}

// "a b op": a ENTER b op
static double bin(const char *a, const char *b, Op op, Calc &c) {
  c.reset();
  type(c, a);
  c.apply(Op::Enter);
  type(c, b);
  c.apply(op);
  return c.level(0);
}

static double un(const char *a, Op op, Calc &c) {
  c.reset();
  type(c, a);
  c.apply(op);
  return c.level(0);
}

static void testEntry() {
  Calc c;
  type(c, "1.25");
  c.eex();
  c.digit(3);
  c.chs();
  EQS(c.entryText(), "1.25E-3");
  EQS(c.levelText(0, 20), "1.25E-3");
  CHECK(c.entering());
  c.apply(Op::Enter);
  CHECK(!c.entering());
  NEAR(c.level(0), 1.25e-3);
  NEAR(c.level(1), 1.25e-3);

  // Backspace through the exponent, then the mantissa.
  c.reset();
  type(c, "12");
  c.eex();
  c.digit(4);
  c.backspace();
  EQS(c.entryText(), "12E");
  c.backspace();
  EQS(c.entryText(), "12");
  c.backspace();
  c.backspace();
  CHECK(!c.entering());
  CHECK(c.level(0) == 0);
  CHECK(!c.liftEnabled());
  // Backspace with nothing typed is CLx.
  type(c, "7");
  c.apply(Op::Enter);
  c.backspace();
  CHECK(c.level(0) == 0 && c.level(1) == 7);

  // EEX on its own means 1E; point starts "0.".
  c.reset();
  c.eex();
  c.digit(3);
  c.apply(Op::Enter);
  CHECK(c.level(0) == 1000);
  c.reset();
  c.point();
  c.digit(5);
  EQS(c.entryText(), "0.5");
  c.point();  // a second point is ignored
  EQS(c.entryText(), "0.5");
  c.chs();
  EQS(c.entryText(), "-0.5");
  c.chs();
  c.apply(Op::Enter);
  CHECK(c.level(0) == 0.5);
  // Leading zeros don't pile up; exponent stops at 3 digits.
  c.reset();
  type(c, "007");
  EQS(c.entryText(), "7");
  c.eex();
  type(c, "1234");
  EQS(c.entryText(), "7E123");
  // Overflowing exponent: error, nothing changes.
  c.reset();
  type(c, "5");
  c.apply(Op::Enter);
  type(c, "9e999");
  c.apply(Op::Add);
  CHECK(c.error() == Err::Overflow);
  CHECK(c.level(0) == 5 && c.level(1) == 5);
  // Underflow is just 0.
  c.reset();
  type(c, "1e-999");
  c.apply(Op::Enter);
  CHECK(c.level(0) == 0 && c.error() == Err::None);
}

static void testStackLift() {
  Calc c;
  // 3 ENTER 4 + -> 7, LASTx 4
  CHECK(bin("3", "4", Op::Add, c) == 7);
  CHECK(c.lastX() == 4);
  // After an op, a new number lifts: 7 then 2 -> Y=7
  type(c, "2");
  c.apply(Op::Mul);
  CHECK(c.level(0) == 14);
  // T copies down on a drop.
  c.reset();
  type(c, "1"); c.apply(Op::Enter);
  type(c, "2"); c.apply(Op::Enter);
  type(c, "3"); c.apply(Op::Enter);
  type(c, "4");
  CHECK(c.level(0) == 0);  // a placeholder while typing
  c.commitEntry();
  CHECK(c.level(3) == 1 && c.level(0) == 4);
  c.apply(Op::Add);
  CHECK(c.level(0) == 7 && c.level(1) == 2 && c.level(2) == 1 && c.level(3) == 1);
  // ENTER ENTER fills the stack with a constant (classic trick): 2 ENTER ENTER ENTER × × ×
  c.reset();
  type(c, "2");
  c.apply(Op::Enter); c.apply(Op::Enter); c.apply(Op::Enter);
  c.apply(Op::Mul); c.apply(Op::Mul); c.apply(Op::Mul);
  CHECK(c.level(0) == 16);
  // ENTER then a number overwrites X (lift disabled).
  c.reset();
  type(c, "5");
  c.apply(Op::Enter);
  type(c, "6");
  CHECK(c.level(1) == 5 && c.level(2) == 0);
  // CLx disables lift too.
  c.apply(Op::Clx);
  type(c, "9");
  c.apply(Op::Add);
  CHECK(c.level(0) == 14);
  // Swap, roll down, roll up.
  c.reset();
  type(c, "1"); c.apply(Op::Enter);
  type(c, "2"); c.apply(Op::Enter);
  type(c, "3"); c.apply(Op::Enter);
  type(c, "4");
  c.apply(Op::Swap);
  CHECK(c.level(0) == 3 && c.level(1) == 4);
  c.apply(Op::RollDown);
  CHECK(c.level(0) == 4 && c.level(3) == 3);
  c.apply(Op::RollUp);
  CHECK(c.level(0) == 3 && c.level(1) == 4);
  // RCL after ENTER replaces X; LASTx pushes.
  c.reset();
  type(c, "8");
  c.sto(3);
  type(c, "1");
  c.apply(Op::Enter);
  c.rcl(3);
  CHECK(c.level(0) == 8 && c.level(1) == 1 && c.level(2) == 8);
  c.apply(Op::Sqrt);
  c.apply(Op::LastX);
  CHECK(c.level(0) == 8 && c.level(1) == sqrt(8.0));
  // CHS on a result leaves LASTx alone.
  c.apply(Op::Chs);
  CHECK(c.level(0) == -8 && c.lastX() == 8);
  // CLST
  c.apply(Op::ClearStack);
  CHECK(c.level(0) == 0 && c.level(1) == 0 && c.depth() == 4);
  // Classic DROP copies T down.
  c.reset();
  type(c, "1"); c.apply(Op::Enter);
  type(c, "2"); c.apply(Op::Enter);
  type(c, "3"); c.apply(Op::Enter);
  type(c, "4");
  c.apply(Op::Drop);
  CHECK(c.level(0) == 3 && c.level(2) == 1 && c.level(3) == 1);
}

static void testBigStack() {
  Calc c;
  c.setBigStack(true);
  c.apply(Op::ClearStack);
  CHECK(c.depth() == 0);
  EQS(c.levelText(0, 10), "");
  c.apply(Op::Add);
  CHECK(c.error() == Err::TooFew);
  for (int i = 1; i <= 6; i++) {
    type(c, std::to_string(i).c_str());
    c.apply(Op::Enter);
  }
  // ENTER after 6 duplicated it: 7 levels.
  CHECK(c.depth() == 7 && c.level(0) == 6 && c.level(6) == 1);
  c.apply(Op::Drop);
  CHECK(c.depth() == 6);
  c.apply(Op::Add);  // 5+6
  CHECK(c.depth() == 5 && c.level(0) == 11 && c.level(4) == 1);
  c.apply(Op::RollDown);
  CHECK(c.level(0) == 4 && c.level(4) == 11);
  c.apply(Op::RollUp);
  CHECK(c.level(0) == 11);
  // Fills up to kMaxDepth and stops growing.
  for (int i = 0; i < kMaxDepth + 10; i++) c.apply(Op::Pi);
  CHECK(c.depth() == kMaxDepth);
  // Back to classic keeps X..T.
  c.reset();
  c.setBigStack(true);
  type(c, "1"); c.apply(Op::Enter);
  type(c, "2"); c.apply(Op::Enter);
  type(c, "3"); c.apply(Op::Enter);
  type(c, "4"); c.apply(Op::Enter);
  type(c, "5");
  CHECK(c.depth() == 9);  // started with 4 zeros
  c.setBigStack(false);
  CHECK(c.depth() == 4 && c.level(0) == 5 && c.level(3) == 2);
  // Shrinking below 4 pads with zeros.
  c.setBigStack(true);
  c.apply(Op::Drop); c.apply(Op::Drop); c.apply(Op::Drop);
  CHECK(c.depth() == 1);
  c.setBigStack(false);
  CHECK(c.depth() == 4 && c.level(0) == 2 && c.level(1) == 0);
}

static void testArith() {
  Calc c;
  CHECK(bin("7", "2", Op::Sub, c) == 5);
  CHECK(bin("7", "2", Op::Div, c) == 3.5);
  CHECK(bin("2", "10", Op::Pow, c) == 1024);
  NEAR(bin("27", "3", Op::Root, c), 3);
  CHECK(bin("-8", "3", Op::Root, c) == -2);
  NEAR(bin("16", "4", Op::Root, c), 2);
  bin("-16", "4", Op::Root, c);
  CHECK(c.error() == Err::Invalid);
  CHECK(un("9", Op::Sq, c) == 81);
  CHECK(un("4", Op::Inv, c) == 0.25);
  CHECK(bin("200", "15", Op::Pct, c) == 30);
  CHECK(c.level(1) == 200);  // Y kept
  CHECK(bin("80", "100", Op::DeltaPct, c) == 25);
  CHECK(un("-3.75", Op::Abs, c) == 3.75);
  CHECK(un("-3.75", Op::IntPart, c) == -3);
  CHECK(un("-3.75", Op::FracPart, c) == -0.75);
  CHECK(bin("7", "3", Op::Mod, c) == 1);
  CHECK(bin("-7", "3", Op::Mod, c) == 2);
  CHECK(bin("7", "-3", Op::Mod, c) == -2);
  CHECK(bin("7", "2", Op::IntDiv, c) == 3);
  CHECK(bin("-7", "2", Op::IntDiv, c) == -3);
  CHECK(bin("3", "4", Op::Hypot, c) == 5);
  // RND to FIX 2 / SCI 2
  c.reset();
  c.setFormat(Fmt::Fix, 2);
  type(c, "3.14159");
  c.apply(Op::Round);
  CHECK(c.level(0) == 3.14);
  c.setFormat(Fmt::Sci, 2);
  type(c, "123456");
  c.apply(Op::Round);
  CHECK(c.level(0) == 123000);
}

static void testErrors() {
  Calc c;
  // Divide by 0 leaves the stack as it was.
  bin("5", "0", Op::Div, c);
  CHECK(c.error() == Err::DivZero);
  EQS(c.errorText(), "Divide by 0");
  CHECK(c.level(0) == 0 && c.level(1) == 5);
  // The error clears on the next key.
  c.digit(1);
  CHECK(c.error() == Err::None);
  un("-1", Op::Sqrt, c);
  CHECK(c.error() == Err::Invalid && c.level(0) == -1);
  un("0", Op::Ln, c);
  CHECK(c.error() == Err::Invalid);
  un("0", Op::Log, c);
  CHECK(c.error() == Err::Invalid);
  un("0", Op::Inv, c);
  CHECK(c.error() == Err::DivZero);
  un("1000", Op::Exp, c);
  CHECK(c.error() == Err::Overflow && c.level(0) == 1000);
  bin("1e300", "1e300", Op::Mul, c);
  CHECK(c.error() == Err::Overflow && c.level(0) == 1e300);
  CHECK(c.lastX() == 0);  // LASTx untouched by a failed op
  un("171", Op::Fact, c);
  CHECK(c.error() == Err::Overflow);
  un("-2", Op::Fact, c);
  CHECK(c.error() == Err::Invalid);
  bin("-8", "0.5", Op::Pow, c);
  CHECK(c.error() == Err::Invalid);
  bin("0", "-1", Op::Pow, c);
  CHECK(c.error() == Err::DivZero);
  un("2", Op::Asin, c);
  CHECK(c.error() == Err::Invalid);
  un("1", Op::Atanh, c);
  CHECK(c.error() == Err::Invalid);
  un("0.5", Op::Acosh, c);
  CHECK(c.error() == Err::Invalid);
  // STO÷0 leaves the register alone.
  c.reset();
  type(c, "6");
  c.sto(1);
  c.apply(Op::Clx);
  c.sto(1, Op::Div);
  CHECK(c.error() == Err::DivZero && c.reg(1) == 6);
  c.sto(10);
  CHECK(c.error() == Err::BadReg);
  // Base-only ops in real mode.
  bin("6", "3", Op::And, c);
  CHECK(c.error() == Err::BaseOnly && c.level(0) == 3);
  un("6", Op::Not, c);
  CHECK(c.error() == Err::BaseOnly);
}

static void testScience() {
  Calc c;
  NEAR(un("1", Op::Exp, c), M_E);
  NEAR(un("100", Op::Log, c), 2);
  NEAR(un("3", Op::Alog, c), 1000);
  NEAR(un("1024", Op::Log2, c), 10);
  NEAR(un("10", Op::Exp2, c), 1024);
  NEAR(un("2.718281828459045", Op::Ln, c), 1);
  // DEG: exact quarter turns.
  CHECK(un("180", Op::Sin, c) == 0);
  CHECK(un("90", Op::Cos, c) == 0);
  CHECK(un("-90", Op::Sin, c) == -1);
  CHECK(un("720", Op::Cos, c) == 1);
  NEAR(un("30", Op::Sin, c), 0.5);
  NEAR(un("45", Op::Tan, c), 1);
  un("90", Op::Tan, c);
  CHECK(c.error() == Err::Invalid);
  NEAR(un("0.5", Op::Asin, c), 30);
  NEAR(un("1", Op::Atan, c), 45);
  NEAR(un("-1", Op::Acos, c), 180);
  // RAD and GRAD
  c.reset();
  c.setAngle(Angle::Rad);
  type(c, "1");
  c.apply(Op::Atan);
  NEAR(c.level(0), M_PI / 4);
  c.apply(Op::Sin);
  NEAR(c.level(0), sqrt(0.5));
  c.reset();
  c.setAngle(Angle::Grad);
  type(c, "100");
  c.apply(Op::Sin);
  CHECK(c.level(0) == 1);
  type(c, "1");
  c.apply(Op::Atan);
  NEAR(c.level(0), 50);
  // Polar/rect: 4 ENTER 3 (y=4, x=3) -> r=5, angle 53.13°
  c.reset();
  type(c, "4");
  c.apply(Op::Enter);
  type(c, "3");
  c.apply(Op::ToPolar);
  NEAR(c.level(0), 5);
  NEAR(c.level(1), atan2(4.0, 3.0) * 180 / M_PI);
  CHECK(c.lastX() == 3);
  c.apply(Op::ToRect);
  NEAR(c.level(0), 3);
  NEAR(c.level(1), 4);
  // Hyperbolics
  NEAR(un("1", Op::Sinh, c), sinh(1.0));
  NEAR(un("1", Op::Cosh, c), cosh(1.0));
  NEAR(un("1", Op::Tanh, c), tanh(1.0));
  NEAR(un("1", Op::Asinh, c), asinh(1.0));
  NEAR(un("2", Op::Acosh, c), acosh(2.0));
  NEAR(un("0.5", Op::Atanh, c), atanh(0.5));
  // Factorial, gamma, combinations
  CHECK(un("0", Op::Fact, c) == 1);
  CHECK(un("10", Op::Fact, c) == 3628800);
  NEAR(un("0.5", Op::Fact, c), sqrt(M_PI) / 2);
  NEAR(un("5", Op::Gamma, c), 24);
  un("0", Op::Gamma, c);
  CHECK(c.error() == Err::Invalid);
  CHECK(bin("52", "5", Op::Comb, c) == 2598960);
  CHECK(bin("10", "3", Op::Perm, c) == 720);
  CHECK(bin("5", "0", Op::Comb, c) == 1);
  bin("3", "5", Op::Comb, c);
  CHECK(c.error() == Err::Invalid);
  bin("1e15", "4e14", Op::Comb, c);  // huge: overflows quickly, doesn't hang
  CHECK(c.error() == Err::Overflow);
  // Angle conversions, H.MS
  NEAR(un("3.141592653589793", Op::ToDeg, c), 180);
  NEAR(un("180", Op::ToRad, c), M_PI);
  NEAR(un("1.5", Op::ToHms, c), 1.30);
  NEAR(un("1.3", Op::FromHms, c), 1.5);
  NEAR(un("2.453", Op::FromHms, c), 2 + 45.5 / 60);
  NEAR(un("2.758333333333333", Op::ToHms, c), 2.453);
  NEAR(un("-1.75", Op::ToHms, c), -1.45);
  NEAR(un("0.9999999999", Op::ToHms, c), 1.0);  // 59.9999996 s rounds, carries into the hour
  // Constants and random
  c.reset();
  c.apply(Op::Pi);
  c.apply(Op::E);
  CHECK(c.level(1) == M_PI && c.level(0) == M_E);
  c.seed(42);
  for (int i = 0; i < 1000; i++) {
    c.apply(Op::Rand);
    double r = c.level(0);
    if (!(r >= 0 && r < 1)) { CHECK(r >= 0 && r < 1); break; }
  }
}

static void testRegisters() {
  Calc c;
  type(c, "10");
  c.sto(0);
  type(c, "5");
  c.sto(0, Op::Add);
  CHECK(c.reg(0) == 15);
  c.sto(0, Op::Mul);
  CHECK(c.reg(0) == 75);
  c.sto(0, Op::Sub);
  CHECK(c.reg(0) == 70);
  c.sto(0, Op::Div);
  CHECK(c.reg(0) == 14);
  c.rcl(0, Op::Add);
  CHECK(c.level(0) == 19 && c.lastX() == 5);
  c.rcl(0);
  CHECK(c.level(0) == 14 && c.level(1) == 19);
  c.apply(Op::ClearRegs);
  CHECK(c.reg(0) == 0);
}

static void testStats() {
  Calc c;
  // (x, y) = (1,2) (2,4.1) (3,5.9) (4,8.2): enter y ENTER x Σ+
  const double xs[] = {1, 2, 3, 4}, ys[] = {2, 4.1, 5.9, 8.2};
  char buf[32];
  for (int i = 0; i < 4; i++) {
    snprintf(buf, sizeof buf, "%g", ys[i]);
    type(c, buf);
    c.apply(Op::Enter);
    snprintf(buf, sizeof buf, "%g", xs[i]);
    type(c, buf);
    c.apply(Op::SigmaPlus);
    CHECK(c.level(0) == i + 1);
  }
  CHECK(c.statCount() == 4);
  CHECK(!c.liftEnabled());
  c.apply(Op::MeanX);  // replaces n (lift disabled)
  CHECK(c.level(0) == 2.5);
  c.apply(Op::MeanY);
  NEAR(c.level(0), 5.05);
  c.apply(Op::StdDevX);
  NEAR(c.level(0), sqrt(5.0 / 3));
  c.apply(Op::Slope);
  NEAR(c.level(0), 2.04);
  c.apply(Op::Intercept);
  NEAR(c.level(0), -0.05);
  c.apply(Op::Corr);
  CHECK(c.level(0) > 0.998 && c.level(0) <= 1);
  type(c, "5");
  c.apply(Op::Yhat);
  NEAR(c.level(0), 2.04 * 5 - 0.05);
  c.apply(Op::StdDevY);
  CHECK(c.level(0) > 0);
  // Σ− removes a point.
  type(c, "8.2");
  c.apply(Op::Enter);
  type(c, "4");
  c.apply(Op::SigmaMinus);
  CHECK(c.level(0) == 3);
  c.apply(Op::MeanX);
  NEAR(c.level(0), 2);
  c.apply(Op::ClearSigma);
  c.apply(Op::MeanX);
  CHECK(c.error() == Err::Stat);
  c.apply(Op::StatN);
  CHECK(c.level(0) == 0);
}

static void testFormat() {
  EQS(formatReal(1234.5678, Fmt::Fix, 2, 20), "1234.57");
  EQS(formatReal(1234.5678, Fmt::Fix, 2, 20, true), "1,234.57");
  EQS(formatReal(-1234567.0, Fmt::Fix, 0, 20, true), "-1,234,567");
  EQS(formatReal(0, Fmt::Fix, 4, 20), "0.0000");
  EQS(formatReal(-0.0, Fmt::Fix, 2, 20), "0.00");
  EQS(formatReal(0.00001, Fmt::Fix, 2, 20), "1.00E-5");    // too small for FIX
  EQS(formatReal(-0.001, Fmt::Fix, 2, 20), "-1.00E-3");
  EQS(formatReal(1e20, Fmt::Fix, 2, 20), "1.00E20");       // too big for FIX
  EQS(formatReal(123456789.0, Fmt::Fix, 4, 10), "1.2346E8");  // too wide
  EQS(formatReal(1234.5678, Fmt::Sci, 3, 20), "1.235E3");
  EQS(formatReal(-0.00012345, Fmt::Sci, 2, 20), "-1.23E-4");
  EQS(formatReal(1, Fmt::Sci, 0, 20), "1E0");
  EQS(formatReal(12345.678, Fmt::Eng, 3, 20), "12.35E3");
  EQS(formatReal(0.0012345, Fmt::Eng, 2, 20), "1.23E-3");
  EQS(formatReal(123456, Fmt::Eng, 2, 20), "123E3");
  EQS(formatReal(1e-7, Fmt::Eng, 2, 20), "100E-9");
  EQS(formatReal(999.96, Fmt::Eng, 3, 20), "1.000E3");
  EQS(formatReal(0.1, Fmt::All, 0, 20), "0.1");
  EQS(formatReal(1.0 / 3, Fmt::All, 0, 20), "0.333333333333");
  EQS(formatReal(0.1 + 0.2, Fmt::All, 0, 20), "0.3");
  EQS(formatReal(100, Fmt::All, 0, 20), "100");
  EQS(formatReal(-2.5e-3, Fmt::All, 0, 20), "-0.0025");
  EQS(formatReal(6.02214076e23, Fmt::All, 0, 20), "6.02214076E23");
  EQS(formatReal(1e-12, Fmt::All, 0, 20), "1E-12");
  EQS(formatReal(1234567.0, Fmt::All, 0, 20, true), "1,234,567");
  EQS(formatReal(1.0 / 3, Fmt::All, 0, 8), "3.333E-1");
  EQS(formatReal(INFINITY, Fmt::All, 0, 20), "Inf");
  EQS(formatReal(-INFINITY, Fmt::Fix, 2, 20), "-Inf");
  EQS(formatReal(NAN, Fmt::Sci, 2, 20), "NaN");
  EQS(formatReal(1.5e-300, Fmt::Sci, 1, 20), "1.5E-300");
  // Every format stays within width for awkward values.
  const double vals[] = {1.0 / 3, -1.0 / 3, 1e-300, -9.87654321e299, 123456789012345.0, 5e-324};
  for (double v : vals)
    for (int f = 0; f < 4; f++)
      for (int d = 0; d <= kMaxDigits; d++) {
        std::string s = formatReal(v, (Fmt)f, d, 12);
        if (s.size() > 12) { printf("FAIL width %g %d %d -> %s\n", v, f, d, s.c_str()); failures++; }
      }
  // Calc::levelText uses the current mode.
  Calc c;
  c.setFormat(Fmt::Fix, 3);
  c.apply(Op::Pi);
  EQS(c.levelText(0, 20), "3.142");
  EQS(c.levelText(1, 20), "0.000");
}

static void testBase() {
  Calc c;
  type(c, "-12.7");
  c.apply(Op::Enter);
  type(c, "1e30");  // saturates
  c.setBase(Base::Dec);
  CHECK(c.baseMode());
  CHECK(c.level(1) == -12);
  CHECK((int64_t)c.levelWord(0) == INT64_MAX);
  EQS(c.levelText(1, 20), "-12");
  c.setBase(Base::Hex);
  EQS(c.levelText(1, 20), "FFFFFFFFFFFFFFF4");
  EQS(c.levelText(1, 8), "..FFFFF4");
  EQS(baseName(c.base()), "HEX");
  // Entry in HEX: A..F digits, no point.
  c.reset();
  c.setBase(Base::Hex);
  type(c, "FF");
  c.point();
  c.eex();
  EQS(c.entryText(), "FF");
  c.apply(Op::Enter);
  type(c, "0F");
  c.apply(Op::And);
  CHECK(c.levelWord(0) == 0xF);
  type(c, "F0");
  c.apply(Op::Or);
  CHECK(c.levelWord(0) == 0xFF);
  type(c, "AA");
  c.apply(Op::Xor);
  CHECK(c.levelWord(0) == 0x55);
  c.apply(Op::Shl);
  CHECK(c.levelWord(0) == 0xAA);
  c.apply(Op::Shr);
  c.apply(Op::Shr);
  CHECK(c.levelWord(0) == 0x2A);
  c.apply(Op::Not);
  CHECK(c.levelWord(0) == ~0x2Aull);
  c.digit(16);  // not a digit
  CHECK(!c.entering());
  // 16 hex digits fit, the 17th is refused.
  c.reset();
  c.setBase(Base::Hex);
  type(c, "FFFFFFFFFFFFFFFFF");
  EQS(c.entryText(), "FFFFFFFFFFFFFFFF");
  c.apply(Op::Enter);
  CHECK(c.levelWord(0) == UINT64_MAX);
  c.chs();  // -(-1) = 1
  CHECK(c.levelWord(0) == 1);
  // BIN and OCT
  c.reset();
  c.setBase(Base::Bin);
  type(c, "1012");  // 2 is refused
  c.apply(Op::Enter);
  CHECK(c.levelWord(0) == 5);
  EQS(c.levelText(0, 20), "101");
  c.setBase(Base::Oct);
  EQS(c.levelText(0, 20), "5");
  type(c, "17");
  c.apply(Op::Mul);
  CHECK(c.levelWord(0) == 75);
  EQS(c.levelText(0, 20), "113");
  // DEC integer arithmetic: truncating divide, wrap, mod sign of divisor.
  c.reset();
  c.setBase(Base::Dec);
  type(c, "-7");
  EQS(c.entryText(), "-7");
  c.apply(Op::Enter);
  type(c, "2");
  c.apply(Op::Div);
  CHECK((int64_t)c.levelWord(0) == -3);
  type(c, "0");
  c.apply(Op::Div);
  CHECK(c.error() == Err::DivZero);
  c.apply(Op::Clx);
  type(c, "9223372036854775807");
  c.apply(Op::Enter);
  type(c, "1");
  c.apply(Op::Add);
  CHECK((int64_t)c.levelWord(0) == INT64_MIN);
  type(c, "-1");
  c.apply(Op::Div);
  CHECK((int64_t)c.levelWord(0) == INT64_MIN);
  c.reset();
  c.setBase(Base::Dec);
  type(c, "-7");
  c.apply(Op::Enter);
  type(c, "3");
  c.apply(Op::Mod);
  CHECK((int64_t)c.levelWord(0) == 2);
  c.setGrouping(true);
  type(c, "1234567");
  EQS(c.levelText(0, 20), "1234567");  // still typing
  c.apply(Op::Enter);
  EQS(c.levelText(0, 20), "1,234,567");
  // Real-only ops refuse.
  c.apply(Op::Sqrt);
  CHECK(c.error() == Err::RealOnly);
  c.apply(Op::Pi);
  CHECK(c.error() == Err::RealOnly);
  // Registers convert both ways; back to real.
  c.sto(2);
  c.setBase(Base::Real);
  CHECK(c.reg(2) == 1234567 && c.level(0) == 1234567);
  // setBase is undoable.
  c.reset();
  type(c, "2.5");
  c.setBase(Base::Dec);
  CHECK(c.level(0) == 2);
  c.apply(Op::Undo);
  CHECK(!c.baseMode() && c.level(0) == 2.5);
}

static void testUndo() {
  Calc c;
  type(c, "3");
  c.apply(Op::Enter);
  type(c, "4");
  c.apply(Op::Mul);
  CHECK(c.level(0) == 12);
  c.apply(Op::Undo);
  CHECK(c.level(0) == 4 && c.level(1) == 3);
  c.apply(Op::Undo);  // undo the undo
  CHECK(c.level(0) == 12);
  // Undo while typing drops the number.
  type(c, "99");
  c.apply(Op::Undo);
  CHECK(!c.entering() && c.level(0) == 12 && c.level(1) == 0);
  // Undo restores registers too, but not the display mode.
  c.reset();
  type(c, "5");
  c.sto(4);
  c.setFormat(Fmt::Fix, 2);
  c.apply(Op::Undo);
  CHECK(c.reg(4) == 0 && c.format() == Fmt::Fix);
  // A failed op doesn't replace the undo point.
  c.reset();
  type(c, "2");
  c.apply(Op::Sq);
  c.apply(Op::Ln);
  type(c, "0");
  c.apply(Op::Div);
  CHECK(c.error() == Err::DivZero);
  c.apply(Op::Undo);  // takes back the 0 typed for the failed divide
  NEAR(c.level(0), log(4.0));
  CHECK(c.level(1) == 0);
}

static void testSaveLoad() {
  Calc c;
  c.setAngle(Angle::Rad);
  c.setFormat(Fmt::Eng, 3);
  c.setBigStack(true);
  for (int i = 0; i < 12; i++) {
    type(c, std::to_string(i).c_str());
    c.apply(Op::Enter);
  }
  type(c, "42");
  c.sto(9);
  type(c, "3"); c.apply(Op::Enter);
  type(c, "1"); c.apply(Op::SigmaPlus);
  State blob = c.state();
  CHECK(sizeof(State) < 1500);
  Calc d;
  CHECK(d.load(&blob, sizeof blob));
  CHECK(d.angle() == Angle::Rad && d.format() == Fmt::Eng && d.digits() == 3 && d.bigStack());
  CHECK(d.depth() == c.depth() && d.reg(9) == 42 && d.statCount() == 1);
  for (int i = 0; i < c.depth(); i++) CHECK(d.level(i) == c.level(i));
  // Bad blobs change nothing.
  State bad = blob;
  bad.magic ^= 1;
  CHECK(!d.load(&bad, sizeof bad));
  bad = blob;
  bad.depth = 200;
  CHECK(!d.load(&bad, sizeof bad));
  bad = blob;
  double nan = NAN;
  memcpy(&bad.stack[0], &nan, 8);
  CHECK(!d.load(&bad, sizeof bad));
  CHECK(!d.load(&blob, sizeof blob - 1));
  CHECK(d.reg(9) == 42);
}

static void testLabels() {
  for (int i = 1; i < (int)Op::kCount; i++) {
    const char *a = opAscii((Op)i);
    CHECK(*opLabel((Op)i) && *a);
    for (const char *p = a; *p; p++)
      if ((unsigned char)*p > 126) { printf("FAIL non-ASCII label for op %d\n", i); failures++; }
  }
  EQS(opLabel(Op::Mul), "×");
  EQS(opAscii(Op::Swap), "x<>y");
  EQS(opLabel(Op::SigmaPlus), "Σ+");
  EQS(angleName(Angle::Grad), "GRAD");
  EQS(fmtName(Fmt::Sci), "SCI");
  EQS(errText(Err::Overflow), "Overflow");
}

int main() {
  testEntry();
  testStackLift();
  testBigStack();
  testArith();
  testErrors();
  testScience();
  testRegisters();
  testStats();
  testFormat();
  testBase();
  testUndo();
  testSaveLoad();
  testLabels();
  printf(failures ? "%d failure(s)\n" : "rpn: all passed\n", failures);
  return failures ? 1 : 0;
}
