// The calculator's engine: an HP-style RPN machine with no screen or device
// code, so it also builds on the Mac for test/rpn_test.sh.
//
// Keys go in through digit()/point()/eex()/chs()/backspace() while a number
// is typed, and apply()/sto()/rcl() for everything else. Any of those clears
// the last error first, so the UI shows errorText() until the next key.
// A failed operation leaves every value as it was: NaN and Inf never reach
// the stack or the registers.
//
// Stack: classic X Y Z T with HP stack lift (ENTER and CLx disable it for the
// next number, most operations enable it, T copies down on a drop). The big
// stack holds up to kMaxDepth levels, grows on push, shrinks on drop and can
// be empty; an operation with too few levels gives "Too few arguments".
//
// Base mode (DEC/HEX/OCT/BIN) works on 64-bit two's complement words. DEC
// shows them signed, the others as raw bits. Switching in truncates reals
// toward zero (saturating); switching out turns words back into reals.
// Words wrap on overflow, as on an HP-16C.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace mt::rpn {

constexpr int kMaxDepth = 100;  // big stack
constexpr int kRegs = 10;       // STO/RCL 0..9
constexpr int kMaxDigits = 11;  // FIX/SCI/ENG n

enum class Op : uint8_t {
  None,  // "no arithmetic" for sto()/rcl()
  // Stack
  Enter, Swap, RollDown, RollUp, Clx, ClearStack, LastX, Drop, Undo, Chs,
  // Arithmetic. Two-argument ops take Y then X: Sub is y-x, Pow is y^x, Root is
  // the x-th root of y, Mod is y mod x (sign of x), Pct is y*x/100 (Y kept),
  // DeltaPct is 100*(x-y)/y (Y kept), Round rounds X to the display format.
  Add, Sub, Mul, Div, Pow, Root, Sq, Sqrt, Inv, Pct, DeltaPct, Abs, IntPart, FracPart, Round,
  Mod, IntDiv,
  // Logs and powers
  Ln, Exp, Log, Alog, Log2, Exp2,
  // Trig in the angle mode. ToPolar: x,y in X,Y -> r in X, angle in Y; ToRect the reverse.
  Sin, Cos, Tan, Asin, Acos, Atan, ToPolar, ToRect, Hypot,
  Sinh, Cosh, Tanh, Asinh, Acosh, Atanh,
  // Fact is x! (gamma(x+1) for non-integers); Comb/Perm take n in Y, r in X.
  // ToHms/FromHms convert decimal hours to and from HP's H.MMSS.
  Fact, Gamma, Comb, Perm, ToDeg, ToRad, ToHms, FromHms,
  // Constants (they push, like a typed number)
  Pi, E, Rand,
  // Statistics on x,y pairs (x in X, y in Y). SigmaPlus/Minus leave n in X and
  // disable lift; the others push their result, except Yhat, which takes x from X.
  SigmaPlus, SigmaMinus, StatN, MeanX, MeanY, StdDevX, StdDevY, Slope, Intercept, Corr, Yhat,
  ClearSigma,
  ClearRegs,
  // Base mode only. Shl/Shr shift X one bit (logical).
  And, Or, Xor, Not, Shl, Shr,
  kCount
};

enum class Angle : uint8_t { Deg, Rad, Grad };
enum class Fmt : uint8_t { All, Fix, Sci, Eng };
// Real is ordinary floating point; the rest are base (integer) mode.
enum class Base : uint8_t { Real, Dec, Hex, Oct, Bin };

enum class Err : uint8_t {
  None, DivZero, Invalid, Overflow, TooFew, Stat, RealOnly, BaseOnly, BadReg
};

// Keycap labels. opLabel() may use UTF-8 (×, ÷, √, π, Σ+, x⇄y, R↓...);
// opAscii() is plain ASCII for fonts without those.
const char *opLabel(Op op);
const char *opAscii(Op op);
const char *errText(Err e);  // "" for Err::None
const char *angleName(Angle a);  // "DEG" "RAD" "GRAD"
const char *fmtName(Fmt f);      // "ALL" "FIX" "SCI" "ENG"
const char *baseName(Base b);    // "" for Real, else "DEC" "HEX" "OCT" "BIN"

// Formats a real in at most width characters. FIX falls back to SCI when the
// number is too big for the width or too small to show; ALL shows up to 12
// significant digits, plain when it fits. group adds thousands separators
// (FIX/ALL plain only). Gives "Inf", "-Inf", "NaN" for those.
std::string formatReal(double v, Fmt fmt, int digits, int width, bool group = false);
// Formats a base-mode word: signed for Dec, raw bits otherwise. If too wide,
// the high digits are cut and the text starts with "..".
std::string formatWord(uint64_t w, Base b, int width, bool group = false);

// Everything worth keeping across restarts, as one flat blob (about 970
// bytes) for NVS. Cells hold a double's bits in real mode, an int64 in base mode.
struct State {
  uint32_t magic;
  uint16_t size;        // sizeof(State): a different build's blob is refused
  uint8_t angle;        // Angle
  uint8_t fmt;          // Fmt
  uint8_t digits;       // 0..kMaxDigits
  uint8_t base;         // Base
  uint8_t bigStack;     // 0/1
  uint8_t group;        // 0/1, digit grouping
  uint8_t lift;         // stack lift enabled
  uint8_t depth;        // levels in use: 4 in classic, 0..kMaxDepth big
  uint8_t pad[2];
  uint64_t stack[kMaxDepth];  // [0] is X
  uint64_t lastX;
  uint64_t reg[kRegs];
  double sn, sx, sy, sxx, syy, sxy;  // statistics sums
  uint64_t rng;                      // RAN# state
};

class Calc {
 public:
  Calc();
  void reset();  // everything back to power-on defaults

  // Number entry. digit() takes 0..9, and 10..15 for A..F in HEX. eex() and
  // point() are ignored in base mode. chs() toggles the mantissa or exponent
  // sign while typing (in HEX/OCT/BIN it ends the entry and negates X), else
  // it negates X. backspace() with nothing typed is CLx; rubbing out the
  // whole number leaves X = 0 with lift disabled, as on the HP-42S.
  void digit(int d);
  void point();
  void eex();
  void chs();
  void backspace();
  // Ends entry, putting the typed number in X. Every operation does this itself.
  // False if the number overflowed (the entry is dropped, error set).
  bool commitEntry();

  void apply(Op op);
  // STO/RCL register 0..9. arith is None (plain) or Add/Sub/Mul/Div:
  // STO+ is reg = reg + x; RCL+ is x = x + reg.
  void sto(int reg, Op arith = Op::None);
  void rcl(int reg, Op arith = Op::None);

  // Modes. setBase() ends entry and converts the stack, LASTx and registers
  // (undoable). Leaving the big stack keeps X..T, padding with zeros.
  void setAngle(Angle a) { s_.angle = (uint8_t)a; }
  void setFormat(Fmt f, int digits);
  void setBase(Base b);
  void setBigStack(bool on);
  void setGrouping(bool on) { s_.group = on; }
  void seed(uint64_t seed);

  Angle angle() const { return (Angle)s_.angle; }
  Fmt format() const { return (Fmt)s_.fmt; }
  int digits() const { return s_.digits; }
  Base base() const { return (Base)s_.base; }
  bool baseMode() const { return s_.base != (uint8_t)Base::Real; }
  bool bigStack() const { return s_.bigStack; }
  bool grouping() const { return s_.group; }
  bool liftEnabled() const { return s_.lift; }

  // The stack. depth() is 4 in classic mode. level(0) is X. While a number is
  // being typed X holds a placeholder; show entryText() there instead
  // (levelText() does). Values come back as doubles in base mode too.
  int depth() const { return s_.depth; }
  double level(int i) const;
  uint64_t levelWord(int i) const;  // raw cell: the int64 bits in base mode
  double lastX() const;
  double reg(int i) const;
  int statCount() const { return (int)s_.sn; }

  bool entering() const { return entering_; }
  // The typed text, e.g. "-1.25E-3" or "1.25E" just after EEX; the UI adds a cursor.
  std::string entryText() const;
  // Level i formatted for the current mode in width characters (the entry text
  // for X while typing, "" for a level the big stack doesn't have).
  std::string levelText(int i, int width) const;
  std::string format(double v, int width) const;

  Err error() const { return err_; }
  const char *errorText() const { return errText(err_); }
  void clearError() { err_ = Err::None; }
  bool canUndo() const { return hasUndo_; }

  // Persistence. The typed number isn't part of the state: commitEntry() first
  // to keep it. load() checks magic, size and ranges, and changes nothing if wrong.
  const State &state() const { return s_; }
  bool load(const void *data, size_t len);

 private:
  using Cell = uint64_t;
  bool exec(Op op);
  bool execReal(Op op);
  bool execBase(Op op);
  bool realUnary(Op op, double a, double &r);
  bool realBinary(Op op, double y, double x, double &r);
  bool arith2(Op op, Cell y, Cell x, Cell &r);  // either mode
  bool linReg(double &m, double &b, double *r) const;
  bool checkFinite();
  bool fail(Err e) { err_ = e; return false; }
  bool need(int n);
  void beginEntry();
  void push(Cell c);
  void pushValue(Cell c);  // a recalled or computed value: lifts only if lift is on
  void drop();
  double x() const;
  double y() const;
  void setX(double v);
  double valueOf(Cell c) const;
  double sinD(double a, int which, bool &ok) const;  // which: 0 sin, 1 cos, 2 tan
  double toAngle(double rad) const;
  uint64_t nextRand();

  State s_;
  State undo_;
  bool hasUndo_ = false;
  Err err_ = Err::None;
  bool entering_ = false;
  bool inExp_ = false;
  std::string mant_, exp_;
};

}  // namespace mt::rpn
