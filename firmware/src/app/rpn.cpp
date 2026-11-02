#include "rpn.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace mt::rpn {

namespace {

constexpr uint32_t kMagic = 0x314e5052;  // "RPN1"
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;
constexpr int kEntryDigits = 16;  // a double holds about 16 significant digits
constexpr int kAllDigits = 12;

double bitsToD(uint64_t w) {
  double d;
  memcpy(&d, &w, sizeof d);
  return d;
}

uint64_t dToBits(double d) {
  if (d == 0) d = 0;  // no -0 on the stack: it would show as "-0"
  uint64_t w;
  memcpy(&w, &d, sizeof w);
  return w;
}

struct Label {
  const char *utf8, *ascii;
};

// In Op order.
const Label kLabels[] = {
    {"", ""},
    {"ENTER", "ENTER"}, {"x⇄y", "x<>y"}, {"R↓", "Rdn"}, {"R↑", "Rup"}, {"CLx", "CLx"},
    {"CLST", "CLST"}, {"LASTx", "LASTx"}, {"DROP", "DROP"}, {"UNDO", "UNDO"}, {"±", "CHS"},
    {"+", "+"}, {"−", "-"}, {"×", "*"}, {"÷", "/"}, {"yˣ", "y^x"}, {"ˣ√y", "xroot"},
    {"x²", "x^2"}, {"√x", "sqrt"}, {"1/x", "1/x"}, {"%", "%"}, {"Δ%", "d%"}, {"ABS", "ABS"},
    {"IP", "IP"}, {"FP", "FP"}, {"RND", "RND"}, {"MOD", "MOD"}, {"IDIV", "IDIV"},
    {"LN", "LN"}, {"eˣ", "e^x"}, {"LOG", "LOG"}, {"10ˣ", "10^x"}, {"LOG2", "LOG2"},
    {"2ˣ", "2^x"},
    {"SIN", "SIN"}, {"COS", "COS"}, {"TAN", "TAN"}, {"ASIN", "ASIN"}, {"ACOS", "ACOS"},
    {"ATAN", "ATAN"}, {"→POL", "->POL"}, {"→REC", "->REC"}, {"HYPOT", "HYPOT"},
    {"SINH", "SINH"}, {"COSH", "COSH"}, {"TANH", "TANH"}, {"ASINH", "ASINH"},
    {"ACOSH", "ACOSH"}, {"ATANH", "ATANH"},
    {"x!", "x!"}, {"Γ", "GAMMA"}, {"nCr", "nCr"}, {"nPr", "nPr"}, {"→DEG", "->DEG"},
    {"→RAD", "->RAD"}, {"→H.MS", "->HMS"}, {"H.MS→", "HMS->"},
    {"π", "pi"}, {"e", "e"}, {"RAN#", "RAND"},
    {"Σ+", "S+"}, {"Σ−", "S-"}, {"n", "n"}, {"x̄", "xbar"}, {"ȳ", "ybar"}, {"sx", "sx"},
    {"sy", "sy"}, {"SLOPE", "SLOPE"}, {"INTCP", "INTCP"}, {"CORR", "CORR"}, {"ŷ", "yhat"},
    {"CLΣ", "CLS"},
    {"CLREG", "CLREG"},
    {"AND", "AND"}, {"OR", "OR"}, {"XOR", "XOR"}, {"NOT", "NOT"}, {"SL", "SL"}, {"SR", "SR"},
};
static_assert(sizeof kLabels / sizeof kLabels[0] == (size_t)Op::kCount, "one label per Op");

int radixOf(Base b) {
  switch (b) {
    case Base::Hex: return 16;
    case Base::Oct: return 8;
    case Base::Bin: return 2;
    default: return 10;
  }
}

// v rounded to n significant digits: the digits (no point) and the decimal exponent.
void sigDigits(double v, int n, std::string &d, int &e) {
  char buf[48];
  snprintf(buf, sizeof buf, "%.*e", n - 1, fabs(v));
  d.clear();
  const char *p = buf;
  for (; *p && *p != 'e'; p++)
    if (*p >= '0' && *p <= '9') d += *p;
  e = *p ? atoi(p + 1) : 0;
}

// "1234567" -> "1,234,567"
std::string groupDigits(const std::string &s) {
  std::string out;
  int n = (int)s.size();
  for (int i = 0; i < n; i++) {
    if (i > 0 && (n - i) % 3 == 0) out += ',';
    out += s[i];
  }
  return out;
}

// Groups the integer part of a plain decimal like "-1234.5".
std::string groupNumber(const std::string &s) {
  size_t start = (s[0] == '-') ? 1 : 0;
  size_t dot = s.find('.');
  if (dot == std::string::npos) dot = s.size();
  return s.substr(0, start) + groupDigits(s.substr(start, dot - start)) + s.substr(dot);
}

std::string sciText(double v, int decimals) {
  std::string d;
  int e;
  sigDigits(v, decimals + 1, d, e);
  std::string s = v < 0 ? "-" : "";
  s += d[0];
  if (decimals > 0) s += "." + d.substr(1);
  return s + "E" + std::to_string(e);
}

std::string engText(double v, int decimals) {
  std::string d;
  int e;
  sigDigits(v, decimals + 1, d, e);
  int ee = e >= 0 ? e / 3 * 3 : -((-e + 2) / 3) * 3;
  int lead = e - ee + 1;  // 1..3 digits before the point
  while ((int)d.size() < lead) d += '0';
  std::string s = v < 0 ? "-" : "";
  s += d.substr(0, lead);
  if ((int)d.size() > lead) s += "." + d.substr(lead);
  return s + "E" + std::to_string(ee);
}

// SCI with as many of the decimals as fit in width.
std::string fitSci(double v, int decimals, int width) {
  std::string s;
  for (int n = decimals; n >= 0; n--) {
    s = sciText(v, n);
    if ((int)s.size() <= width) break;
  }
  return s;
}

}  // namespace

const char *opLabel(Op op) { return op < Op::kCount ? kLabels[(int)op].utf8 : ""; }
const char *opAscii(Op op) { return op < Op::kCount ? kLabels[(int)op].ascii : ""; }

const char *errText(Err e) {
  switch (e) {
    case Err::None: return "";
    case Err::DivZero: return "Divide by 0";
    case Err::Invalid: return "Invalid input";
    case Err::Overflow: return "Overflow";
    case Err::TooFew: return "Too few arguments";
    case Err::Stat: return "Stat error";
    case Err::RealOnly: return "Not in base mode";
    case Err::BaseOnly: return "Base mode only";
    case Err::BadReg: return "Bad register";
  }
  return "";
}

const char *angleName(Angle a) {
  return a == Angle::Deg ? "DEG" : a == Angle::Rad ? "RAD" : "GRAD";
}

const char *fmtName(Fmt f) {
  static const char *const names[] = {"ALL", "FIX", "SCI", "ENG"};
  return names[(int)f & 3];
}

const char *baseName(Base b) {
  static const char *const names[] = {"", "DEC", "HEX", "OCT", "BIN"};
  return (int)b <= 4 ? names[(int)b] : "";
}

std::string formatReal(double v, Fmt fmt, int digits, int width, bool group) {
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v < 0 ? "-Inf" : "Inf";
  if (v == 0) v = 0;
  if (digits < 0) digits = 0;
  if (digits > kMaxDigits) digits = kMaxDigits;
  switch (fmt) {
    case Fmt::Fix: {
      if (fabs(v) < 1e15) {
        char buf[48];
        snprintf(buf, sizeof buf, "%.*f", digits, v);
        std::string s = buf;
        // Too small to show any digit: SCI, as the HP does.
        bool lost = v != 0 && strtod(buf, nullptr) == 0;
        if (group) s = groupNumber(s);
        if (!lost && (int)s.size() <= width) return s;
      }
      return fitSci(v, digits, width);
    }
    case Fmt::Sci: return fitSci(v, digits, width);
    case Fmt::Eng: {
      std::string s;
      for (int n = digits; n >= 0; n--) {
        s = engText(v, n);
        if ((int)s.size() <= width) break;
      }
      return s;
    }
    case Fmt::All: break;
  }
  if (v == 0) return "0";
  // Fewest digits that read back as v, up to 12.
  std::string d;
  int e, n = 1;
  for (; n < kAllDigits; n++) {
    char buf[48];
    snprintf(buf, sizeof buf, "%.*e", n - 1, v);
    if (strtod(buf, nullptr) == v) break;
  }
  sigDigits(v, n, d, e);
  while (d.size() > 1 && d.back() == '0') d.pop_back();
  if (e < kAllDigits && e >= -9) {
    std::string ip, fp;
    if (e >= 0) {
      ip = d.substr(0, e + 1);
      while ((int)ip.size() < e + 1) ip += '0';
      if ((int)d.size() > e + 1) fp = d.substr(e + 1);
    } else {
      ip = "0";
      fp = std::string(-e - 1, '0') + d;
    }
    if (group) ip = groupDigits(ip);
    std::string s = (v < 0 ? "-" : "") + ip + (fp.empty() ? "" : "." + fp);
    if ((int)s.size() <= width) return s;
  }
  return fitSci(v, (int)d.size() - 1, width);
}

std::string formatWord(uint64_t w, Base b, int width, bool group) {
  std::string s;
  if (b == Base::Real || b == Base::Dec) {
    bool neg = (int64_t)w < 0;
    uint64_t m = neg ? 0 - w : w;  // also right for INT64_MIN
    char buf[24];
    snprintf(buf, sizeof buf, "%llu", (unsigned long long)m);
    s = group ? groupDigits(buf) : buf;
    if (neg) s = "-" + s;
  } else {
    int shift = b == Base::Hex ? 4 : b == Base::Oct ? 3 : 1;
    uint64_t mask = (1u << shift) - 1;
    do {
      s.insert(s.begin(), "0123456789ABCDEF"[w & mask]);
      w >>= shift;
    } while (w);
  }
  if ((int)s.size() > width && width > 2) s = ".." + s.substr(s.size() - (width - 2));
  return s;
}

// ---- Calc ----

Calc::Calc() { reset(); }

void Calc::reset() {
  memset(&s_, 0, sizeof s_);
  s_.magic = kMagic;
  s_.size = sizeof(State);
  s_.angle = (uint8_t)Angle::Deg;
  s_.fmt = (uint8_t)Fmt::All;
  s_.digits = 4;
  s_.base = (uint8_t)Base::Real;
  s_.depth = 4;
  s_.lift = 1;
  s_.rng = 0x9E3779B97F4A7C15ull;
  undo_ = s_;
  hasUndo_ = false;
  err_ = Err::None;
  entering_ = inExp_ = false;
  mant_.clear();
  exp_.clear();
}

double Calc::valueOf(Cell c) const { return baseMode() ? (double)(int64_t)c : bitsToD(c); }
double Calc::x() const { return bitsToD(s_.stack[0]); }
double Calc::y() const { return bitsToD(s_.stack[1]); }
void Calc::setX(double v) { s_.stack[0] = dToBits(v); }

double Calc::level(int i) const { return i >= 0 && i < s_.depth ? valueOf(s_.stack[i]) : 0; }
uint64_t Calc::levelWord(int i) const { return i >= 0 && i < s_.depth ? s_.stack[i] : 0; }
double Calc::lastX() const { return valueOf(s_.lastX); }
double Calc::reg(int i) const { return i >= 0 && i < kRegs ? valueOf(s_.reg[i]) : 0; }

void Calc::setFormat(Fmt f, int digits) {
  s_.fmt = (uint8_t)f;
  s_.digits = (uint8_t)(digits < 0 ? 0 : digits > kMaxDigits ? kMaxDigits : digits);
}

void Calc::seed(uint64_t v) { s_.rng = v ? v : 0x9E3779B97F4A7C15ull; }

uint64_t Calc::nextRand() {  // xorshift64*
  uint64_t r = s_.rng;
  r ^= r >> 12;
  r ^= r << 25;
  r ^= r >> 27;
  s_.rng = r;
  return r * 0x2545F4914F6CDD1Dull;
}

bool Calc::need(int n) { return s_.depth >= n || fail(Err::TooFew); }

void Calc::push(Cell c) {
  int n = s_.bigStack ? (s_.depth < kMaxDepth ? s_.depth + 1 : kMaxDepth) : 4;
  memmove(s_.stack + 1, s_.stack, (n - 1) * sizeof(Cell));
  s_.stack[0] = c;
  s_.depth = n;
}

void Calc::pushValue(Cell c) {
  if (s_.lift || s_.depth == 0) push(c);
  else s_.stack[0] = c;
  s_.lift = 1;
}

void Calc::drop() {
  if (s_.bigStack) {
    if (s_.depth == 0) return;
    memmove(s_.stack, s_.stack + 1, (s_.depth - 1) * sizeof(Cell));
    s_.stack[--s_.depth] = 0;
  } else {
    memmove(s_.stack, s_.stack + 1, 3 * sizeof(Cell));  // T stays: it copies down
  }
}

// ---- Number entry ----

void Calc::beginEntry() {
  undo_ = s_;  // undo drops the number being typed
  hasUndo_ = true;
  pushValue(0);  // placeholder in X; commitEntry() fills it
  entering_ = true;
  inExp_ = false;
  mant_.clear();
  exp_.clear();
}

void Calc::digit(int d) {
  err_ = Err::None;
  int radix = radixOf(base());
  if (d < 0 || d >= radix) return;
  if (!entering_) beginEntry();
  char ch = "0123456789ABCDEF"[d];
  if (inExp_) {
    std::string digits = exp_[0] == '-' ? exp_.substr(1) : exp_;
    if (digits.size() < 3) exp_ += ch;  // E-308..E308
    return;
  }
  if (mant_ == "0" || mant_ == "-0") mant_.pop_back();
  if (baseMode()) {
    // Refuse the digit if the word would overflow (DEC keeps to int64 range).
    uint64_t v = 0, limit = base() == Base::Dec ? (uint64_t)INT64_MAX : UINT64_MAX;
    for (char c : mant_ + ch) {
      if (c == '-') continue;
      uint64_t dv = c <= '9' ? c - '0' : c - 'A' + 10;
      if (v > (limit - dv) / radix) return;
      v = v * radix + dv;
    }
  } else {
    int n = 0;
    for (char c : mant_) n += c >= '0' && c <= '9';
    if (n >= kEntryDigits) return;
  }
  mant_ += ch;
}

void Calc::point() {
  err_ = Err::None;
  if (baseMode()) return;
  if (!entering_) beginEntry();
  if (inExp_ || mant_.find('.') != std::string::npos) return;
  if (mant_.empty() || mant_ == "-") mant_ += '0';
  mant_ += '.';
}

void Calc::eex() {
  err_ = Err::None;
  if (baseMode()) return;
  if (!entering_) beginEntry();
  if (inExp_) return;
  // A mantissa of nothing or plain zeros becomes 1, so EEX 3 means 1000.
  if (mant_.find_first_of("123456789") == std::string::npos && mant_.find('.') == std::string::npos)
    mant_ = (mant_[0] == '-' ? "-1" : "1");
  inExp_ = true;
  exp_.clear();
}

void Calc::chs() {
  err_ = Err::None;
  if (!entering_) {
    apply(Op::Chs);
    return;
  }
  if (inExp_) {
    if (!exp_.empty() && exp_[0] == '-') exp_.erase(0, 1);
    else exp_.insert(0, "-");
  } else if (baseMode() && base() != Base::Dec) {
    apply(Op::Chs);  // raw-bit bases have no typed sign: negate the word
  } else {
    if (mant_[0] == '-') mant_.erase(0, 1);
    else mant_.insert(0, "-");
  }
}

void Calc::backspace() {
  err_ = Err::None;
  if (!entering_) {
    apply(Op::Clx);
    return;
  }
  if (inExp_) {
    if (!exp_.empty()) exp_.pop_back();
    else inExp_ = false;
    return;
  }
  mant_.pop_back();
  if (mant_.empty() || mant_ == "-") {
    entering_ = false;
    s_.stack[0] = 0;
    s_.lift = 0;
  }
}

std::string Calc::entryText() const {
  if (!entering_) return "";
  return inExp_ ? mant_ + "E" + exp_ : mant_;
}

bool Calc::commitEntry() {
  if (!entering_) return true;
  entering_ = false;
  Cell c;
  if (baseMode()) {
    uint64_t v = 0;
    int radix = radixOf(base());
    for (char ch : mant_)
      if (ch != '-') v = v * radix + (ch <= '9' ? ch - '0' : ch - 'A' + 10);
    c = mant_[0] == '-' ? 0 - v : v;
  } else {
    std::string t = mant_;
    if (inExp_ && exp_.find_first_of("0123456789") != std::string::npos) t += "e" + exp_;
    double v = strtod(t.c_str(), nullptr);  // underflow gives 0, which is fine
    if (!std::isfinite(v)) {
      s_ = undo_;
      return fail(Err::Overflow);
    }
    c = dToBits(v);
  }
  s_.stack[0] = c;
  s_.lift = 1;
  return true;
}

// ---- Operations ----

void Calc::apply(Op op) {
  err_ = Err::None;
  if (op == Op::Undo) {
    if (entering_) {
      entering_ = false;
      s_ = undo_;
      return;
    }
    if (!hasUndo_) return;
    // Display settings aren't part of what an undo takes back.
    State now = s_;
    s_ = undo_;
    undo_ = now;
    s_.angle = now.angle;
    s_.fmt = now.fmt;
    s_.digits = now.digits;
    s_.group = now.group;
    return;
  }
  if (!commitEntry()) return;
  State before = s_;
  bool ok = exec(op) && checkFinite();
  if (!ok) {
    Err e = err_;
    s_ = before;
    err_ = e;
    return;
  }
  undo_ = before;
  hasUndo_ = true;
}

// Real mode: anything that came out NaN or Inf turns into an error.
bool Calc::checkFinite() {
  if (baseMode()) return true;
  auto bad = [&](Cell c) {
    double v = bitsToD(c);
    if (std::isfinite(v)) return false;
    fail(std::isnan(v) ? Err::Invalid : Err::Overflow);
    return true;
  };
  for (int i = 0; i < s_.depth; i++)
    if (bad(s_.stack[i])) return false;
  for (Cell c : s_.reg)
    if (bad(c)) return false;
  double sums[] = {s_.sn, s_.sx, s_.sy, s_.sxx, s_.syy, s_.sxy};
  for (double v : sums)
    if (bad(dToBits(v))) return false;
  return !bad(s_.lastX);
}

bool Calc::exec(Op op) {
  Cell *st = s_.stack;
  switch (op) {
    case Op::None: return true;
    case Op::Enter:
      if (!need(1)) return false;
      push(st[0]);
      s_.lift = 0;
      return true;
    case Op::Swap:
      if (!need(2)) return false;
      std::swap(st[0], st[1]);
      break;
    case Op::RollDown: {
      if (!need(1)) return false;
      Cell c = st[0];
      memmove(st, st + 1, (s_.depth - 1) * sizeof(Cell));
      st[s_.depth - 1] = c;
      break;
    }
    case Op::RollUp: {
      if (!need(1)) return false;
      Cell c = st[s_.depth - 1];
      memmove(st + 1, st, (s_.depth - 1) * sizeof(Cell));
      st[0] = c;
      break;
    }
    case Op::Clx:
      if (s_.depth) st[0] = 0;
      s_.lift = 0;
      return true;
    case Op::ClearStack:
      memset(st, 0, sizeof s_.stack);
      if (s_.bigStack) s_.depth = 0;
      s_.lift = 0;
      return true;
    case Op::LastX: pushValue(s_.lastX); return true;
    case Op::Drop:
      if (!need(1)) return false;
      drop();
      break;
    case Op::ClearRegs: memset(s_.reg, 0, sizeof s_.reg); return true;
    case Op::ClearSigma: s_.sn = s_.sx = s_.sy = s_.sxx = s_.syy = s_.sxy = 0; return true;
    case Op::Chs:  // sign change leaves LASTx and lift alone, as on the HP
      if (!need(1)) return false;
      if (baseMode()) st[0] = 0 - st[0];
      else setX(-x());
      return true;
    case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: case Op::Mod: case Op::IntDiv:
    case Op::Pow: case Op::Root: case Op::Hypot: case Op::Comb: case Op::Perm:
    case Op::And: case Op::Or: case Op::Xor: {
      Cell r;
      if (!need(2) || !arith2(op, st[1], st[0], r)) return false;
      s_.lastX = st[0];
      drop();
      st[0] = r;
      break;
    }
    default: return baseMode() ? execBase(op) : execReal(op);
  }
  s_.lift = 1;
  return true;
}

bool Calc::execBase(Op op) {
  if (op != Op::Sq && op != Op::Abs && op != Op::Not && op != Op::Shl && op != Op::Shr)
    return fail(Err::RealOnly);
  if (!need(1)) return false;
  uint64_t a = s_.stack[0], r;
  switch (op) {
    case Op::Sq: r = a * a; break;  // wraps, as everything in base mode does
    case Op::Abs: r = (int64_t)a < 0 ? 0 - a : a; break;
    case Op::Not: r = ~a; break;
    case Op::Shl: r = a << 1; break;
    case Op::Shr: r = a >> 1; break;
    default: return fail(Err::RealOnly);
  }
  s_.lastX = a;
  s_.stack[0] = r;
  s_.lift = 1;
  return true;
}

bool Calc::arith2(Op op, Cell yc, Cell xc, Cell &out) {
  if (!baseMode()) {
    double r;
    if (!realBinary(op, bitsToD(yc), bitsToD(xc), r)) return false;
    out = dToBits(r);
    return true;
  }
  int64_t y = (int64_t)yc, x = (int64_t)xc;
  switch (op) {
    case Op::Add: out = yc + xc; return true;
    case Op::Sub: out = yc - xc; return true;
    case Op::Mul: out = yc * xc; return true;
    case Op::Div: case Op::IntDiv:
      if (x == 0) return fail(Err::DivZero);
      out = (x == -1) ? 0 - yc : (uint64_t)(y / x);  // INT64_MIN / -1 wraps instead of trapping
      return true;
    case Op::Mod: {
      if (x == 0) return fail(Err::DivZero);
      int64_t r = x == -1 ? 0 : y % x;
      if (r != 0 && (r < 0) != (x < 0)) r += x;
      out = (uint64_t)r;
      return true;
    }
    case Op::And: out = yc & xc; return true;
    case Op::Or: out = yc | xc; return true;
    case Op::Xor: out = yc ^ xc; return true;
    default: return fail(Err::RealOnly);
  }
}

bool Calc::realBinary(Op op, double y, double x, double &r) {
  switch (op) {
    case Op::Add: r = y + x; return true;
    case Op::Sub: r = y - x; return true;
    case Op::Mul: r = y * x; return true;
    case Op::Div:
      if (x == 0) return fail(Err::DivZero);
      r = y / x;
      return true;
    case Op::Pow:
      if (y == 0 && x <= 0) return fail(x == 0 ? Err::Invalid : Err::DivZero);
      if (y < 0 && x != floor(x)) return fail(Err::Invalid);
      r = pow(y, x);
      return true;
    case Op::Root: {  // x-th root of y
      if (x == 0 || (y == 0 && x < 0)) return fail(Err::DivZero);
      bool oddInt = x == floor(x) && fmod(fabs(x), 2) == 1;
      if (y < 0 && !oddInt) return fail(Err::Invalid);
      double a = fabs(y);
      r = x == 2 ? sqrt(a) : x == 3 ? cbrt(a) : pow(a, 1 / x);  // exact cube roots
      if (y < 0) r = -r;
      return true;
    }
    case Op::Mod:
      if (x == 0) return fail(Err::DivZero);
      r = fmod(y, x);
      if (r != 0 && (r < 0) != (x < 0)) r += x;  // sign of x, as on the HP-42S
      return true;
    case Op::IntDiv:
      if (x == 0) return fail(Err::DivZero);
      r = trunc(y / x);
      return true;
    case Op::Hypot: r = hypot(y, x); return true;
    case Op::Comb: case Op::Perm: {  // n in y, r in x
      if (y != floor(y) || x != floor(x) || x < 0 || x > y || y > 9007199254740992.0)
        return fail(Err::Invalid);
      double k = x;
      if (op == Op::Comb && y - k < k) k = y - k;
      r = 1;
      for (double i = 1; i <= k && std::isfinite(r); i++)
        r = op == Op::Perm ? r * (y - i + 1) : r * (y - k + i) / i;
      if (op == Op::Comb && r < 9007199254740992.0) r = round(r);
      return true;
    }
    case Op::And: case Op::Or: case Op::Xor: return fail(Err::BaseOnly);
    default: return fail(Err::Invalid);
  }
}

double Calc::toAngle(double rad) const {
  switch (angle()) {
    case Angle::Deg: return rad * (180 / kPi);
    case Angle::Grad: return rad * (200 / kPi);
    default: return rad;
  }
}

// sin/cos/tan in the angle mode. In DEG and GRAD whole quarter turns come out
// exact (sin 180 = 0, cos 90 = 0) and tan of an odd quarter is an error.
double Calc::sinD(double a, int which, bool &ok) const {
  ok = true;
  double quarter = angle() == Angle::Deg ? 90 : angle() == Angle::Grad ? 100 : 0;
  if (quarter) {
    a = fmod(a, 4 * quarter);  // exact, and keeps big angles accurate
    double q = a / quarter;
    if (q == floor(q)) {
      int k = ((int)q % 4 + 4) % 4;
      static const double s[] = {0, 1, 0, -1}, c[] = {1, 0, -1, 0};
      if (which == 2 && (k & 1)) ok = false;
      return which == 0 ? s[k] : which == 1 ? c[k] : 0;
    }
    a *= kPi / (2 * quarter);
  }
  return which == 0 ? sin(a) : which == 1 ? cos(a) : tan(a);
}

bool Calc::linReg(double &m, double &b, double *r) const {
  double n = s_.sn;
  double dx = n * s_.sxx - s_.sx * s_.sx, dy = n * s_.syy - s_.sy * s_.sy;
  double cxy = n * s_.sxy - s_.sx * s_.sy;
  if (n < 2 || dx <= 0) return false;
  m = cxy / dx;
  b = (s_.sy - m * s_.sx) / n;
  if (r) {
    if (dy <= 0) return false;
    *r = cxy / sqrt(dx * dy);
  }
  return true;
}

bool Calc::realUnary(Op op, double a, double &r) {
  bool ok = true;
  switch (op) {
    case Op::Sq: r = a * a; break;
    case Op::Sqrt:
      if (a < 0) return fail(Err::Invalid);
      r = sqrt(a);
      break;
    case Op::Inv:
      if (a == 0) return fail(Err::DivZero);
      r = 1 / a;
      break;
    case Op::Abs: r = fabs(a); break;
    case Op::IntPart: r = trunc(a); break;
    case Op::FracPart: r = a - trunc(a); break;
    case Op::Round: {
      // Rounds to what the display shows (ALL shows everything: no change).
      char buf[400];
      if (format() == Fmt::All) r = a;
      else if (format() == Fmt::Fix && fabs(a) < 1e15) snprintf(buf, sizeof buf, "%.*f", digits(), a);
      else snprintf(buf, sizeof buf, "%.*e", digits(), a);
      if (format() != Fmt::All) r = strtod(buf, nullptr);
      break;
    }
    case Op::Ln: case Op::Log: case Op::Log2:
      if (a <= 0) return fail(Err::Invalid);
      r = op == Op::Ln ? log(a) : op == Op::Log ? log10(a) : log2(a);
      break;
    case Op::Exp: r = exp(a); break;
    case Op::Alog: r = pow(10.0, a); break;
    case Op::Exp2: r = exp2(a); break;
    case Op::Sin: r = sinD(a, 0, ok); break;
    case Op::Cos: r = sinD(a, 1, ok); break;
    case Op::Tan: r = sinD(a, 2, ok); break;
    case Op::Asin: case Op::Acos:
      if (fabs(a) > 1) return fail(Err::Invalid);
      r = toAngle(op == Op::Asin ? asin(a) : acos(a));
      break;
    case Op::Atan: r = toAngle(atan(a)); break;
    case Op::Sinh: r = sinh(a); break;
    case Op::Cosh: r = cosh(a); break;
    case Op::Tanh: r = tanh(a); break;
    case Op::Asinh: r = asinh(a); break;
    case Op::Acosh:
      if (a < 1) return fail(Err::Invalid);
      r = acosh(a);
      break;
    case Op::Atanh:
      if (fabs(a) >= 1) return fail(Err::Invalid);
      r = atanh(a);
      break;
    case Op::Fact:
      if (a == floor(a)) {  // integers by multiplication: exact up to 22!
        if (a < 0) return fail(Err::Invalid);
        if (a > 170) return fail(Err::Overflow);
        r = 1;
        for (int i = 2; i <= (int)a; i++) r *= i;
      } else {
        r = tgamma(a + 1);
      }
      break;
    case Op::Gamma:
      if (a <= 0 && a == floor(a)) return fail(Err::Invalid);
      r = tgamma(a);
      break;
    case Op::ToDeg: r = a * (180 / kPi); break;
    case Op::ToRad: r = a * (kPi / 180); break;
    case Op::ToHms: {  // decimal hours -> H.MMSS
      double v = fabs(a), h = floor(v), mm = (v - h) * 60, m = floor(mm);
      double s = round((mm - m) * 60 * 1e6) / 1e6;  // 59.9999999 is really 60
      if (s >= 60) s -= 60, m++;
      if (m >= 60) m -= 60, h++;
      r = h + m / 100 + s / 10000;
      if (a < 0) r = -r;
      break;
    }
    case Op::FromHms: {  // H.MMSS -> decimal hours
      double v = fabs(a), h = floor(v), mm = (v - h) * 100;
      double m = floor(mm + 1e-6);  // 1.30 is 29.99999... minutes in binary
      double s = (mm - m) * 100;
      if (s < 0) s = 0;
      r = h + m / 60 + s / 3600;
      if (a < 0) r = -r;
      break;
    }
    case Op::Yhat: {
      double m, b;
      if (!linReg(m, b, nullptr)) return fail(Err::Stat);
      r = m * a + b;
      break;
    }
    default: return fail(Err::Invalid);
  }
  return ok || fail(Err::Invalid);
}

bool Calc::execReal(Op op) {
  double r, m, b;
  switch (op) {
    case Op::Pct: case Op::DeltaPct:  // Y stays, as on the HP
      if (!need(2)) return false;
      if (op == Op::Pct) r = y() * x() / 100;
      else if (y() == 0) return fail(Err::DivZero);
      else r = 100 * (x() - y()) / y();
      s_.lastX = s_.stack[0];
      setX(r);
      break;
    case Op::ToPolar: case Op::ToRect: {
      if (!need(2)) return false;
      double a, c;
      if (op == Op::ToPolar) {
        a = hypot(x(), y());
        c = toAngle(atan2(y(), x()));
      } else {
        bool ok;
        double cs = sinD(y(), 1, ok), sn = sinD(y(), 0, ok);
        a = x() * cs;
        c = x() * sn;
      }
      s_.lastX = s_.stack[0];
      setX(a);
      s_.stack[1] = dToBits(c);
      break;
    }
    case Op::Pi: pushValue(dToBits(kPi)); break;
    case Op::E: pushValue(dToBits(kE)); break;
    case Op::Rand: pushValue(dToBits((nextRand() >> 11) * (1.0 / 9007199254740992.0))); break;
    case Op::SigmaPlus: case Op::SigmaMinus: {
      if (!need(2)) return false;
      double k = op == Op::SigmaPlus ? 1 : -1, xv = x(), yv = y();
      s_.sn += k;
      s_.sx += k * xv;
      s_.sy += k * yv;
      s_.sxx += k * xv * xv;
      s_.syy += k * yv * yv;
      s_.sxy += k * xv * yv;
      s_.lastX = s_.stack[0];
      setX(s_.sn);
      s_.lift = 0;  // the next number replaces n
      return true;
    }
    case Op::StatN: pushValue(dToBits(s_.sn)); break;
    case Op::MeanX: case Op::MeanY:
      if (s_.sn <= 0) return fail(Err::Stat);
      pushValue(dToBits((op == Op::MeanX ? s_.sx : s_.sy) / s_.sn));
      break;
    case Op::StdDevX: case Op::StdDevY: {
      if (s_.sn < 2) return fail(Err::Stat);
      double sum = op == Op::StdDevX ? s_.sx : s_.sy, sq = op == Op::StdDevX ? s_.sxx : s_.syy;
      double var = (sq - sum * sum / s_.sn) / (s_.sn - 1);
      pushValue(dToBits(sqrt(var > 0 ? var : 0)));  // rounding can make it a hair negative
      break;
    }
    case Op::Slope: case Op::Intercept:
      if (!linReg(m, b, nullptr)) return fail(Err::Stat);
      pushValue(dToBits(op == Op::Slope ? m : b));
      break;
    case Op::Corr:
      if (!linReg(m, b, &r)) return fail(Err::Stat);
      pushValue(dToBits(r));
      break;
    case Op::Not: case Op::Shl: case Op::Shr: return fail(Err::BaseOnly);
    default:  // one-argument functions of X
      if (!need(1) || !realUnary(op, x(), r)) return false;
      s_.lastX = s_.stack[0];
      setX(r);
      break;
  }
  s_.lift = 1;
  return true;
}

// ---- Registers ----

void Calc::sto(int reg, Op arith) {
  err_ = Err::None;
  if (!commitEntry()) return;
  if (reg < 0 || reg >= kRegs) return (void)fail(Err::BadReg);
  if (!need(1)) return;
  State before = s_;
  bool ok = true;
  if (arith == Op::None) s_.reg[reg] = s_.stack[0];
  else if (arith == Op::Add || arith == Op::Sub || arith == Op::Mul || arith == Op::Div)
    ok = arith2(arith, s_.reg[reg], s_.stack[0], s_.reg[reg]);
  else ok = fail(Err::Invalid);
  s_.lift = 1;
  if (!ok || !checkFinite()) {
    Err e = err_;
    s_ = before;
    err_ = e;
    return;
  }
  undo_ = before;
  hasUndo_ = true;
}

void Calc::rcl(int reg, Op arith) {
  err_ = Err::None;
  if (!commitEntry()) return;
  if (reg < 0 || reg >= kRegs) return (void)fail(Err::BadReg);
  State before = s_;
  bool ok = true;
  if (arith == Op::None) {
    pushValue(s_.reg[reg]);
  } else if (arith == Op::Add || arith == Op::Sub || arith == Op::Mul || arith == Op::Div) {
    Cell r;
    ok = need(1) && arith2(arith, s_.stack[0], s_.reg[reg], r);
    if (ok) {
      s_.lastX = s_.stack[0];
      s_.stack[0] = r;
      s_.lift = 1;
    }
  } else {
    ok = fail(Err::Invalid);
  }
  if (!ok || !checkFinite()) {
    Err e = err_;
    s_ = before;
    err_ = e;
    return;
  }
  undo_ = before;
  hasUndo_ = true;
}

// ---- Modes ----

void Calc::setBase(Base b) {
  if (!commitEntry() || b == base() || (int)b > (int)Base::Bin) return;
  err_ = Err::None;
  undo_ = s_;
  hasUndo_ = true;
  bool from = baseMode(), to = b != Base::Real;
  auto conv = [&](Cell &c) {
    if (!from && to) {  // truncate toward zero, saturating at the int64 limits
      double v = trunc(bitsToD(c));
      int64_t i = v >= 9223372036854775808.0 ? INT64_MAX
                  : v <= -9223372036854775808.0 ? INT64_MIN : (int64_t)v;
      c = (uint64_t)i;
    } else if (from && !to) {
      c = dToBits((double)(int64_t)c);
    }
  };
  for (Cell &c : s_.stack) conv(c);
  for (Cell &c : s_.reg) conv(c);
  conv(s_.lastX);
  s_.base = (uint8_t)b;
}

void Calc::setBigStack(bool on) {
  commitEntry();
  if (on == bigStack()) return;
  if (!on) {
    for (int i = s_.depth; i < 4; i++) s_.stack[i] = 0;
    for (int i = 4; i < kMaxDepth; i++) s_.stack[i] = 0;
    s_.depth = 4;
  }
  s_.bigStack = on;
}

std::string Calc::format(double v, int width) const {
  return formatReal(v, format(), digits(), width, grouping());
}

std::string Calc::levelText(int i, int width) const {
  if (i == 0 && entering_) return entryText();
  if (i < 0 || i >= s_.depth) return "";
  if (baseMode()) return formatWord(s_.stack[i], base(), width, grouping());
  return format(bitsToD(s_.stack[i]), width);
}

bool Calc::load(const void *data, size_t len) {
  if (!data || len != sizeof(State)) return false;
  State t;
  memcpy(&t, data, sizeof t);
  if (t.magic != kMagic || t.size != sizeof(State) || t.angle > 2 || t.fmt > 3 ||
      t.digits > kMaxDigits || t.base > 4 || t.bigStack > 1 || t.lift > 1 ||
      t.depth > (t.bigStack ? kMaxDepth : 4) || (!t.bigStack && t.depth != 4))
    return false;
  State old = s_;
  s_ = t;
  if (!checkFinite()) {
    s_ = old;
    err_ = Err::None;
    return false;
  }
  entering_ = inExp_ = false;
  hasUndo_ = false;
  err_ = Err::None;
  return true;
}

}  // namespace mt::rpn
