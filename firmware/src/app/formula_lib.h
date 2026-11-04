// Inside the Formulas library: how formulas.cpp finds the tables, and the helpers the
// formula_lib_*.cpp files write them with. The app uses formulas.h, not this.
#pragma once

#include <cstddef>

#include "formulas.h"

namespace mt {
namespace formulas {
namespace lib {

// Group indices, in the order the app lists them.
enum GroupId : uint8_t {
  kElectrical, kElectronics, kFields, kMotion, kForces, kRotation, kMaterials, kFluids,
  kHeat, kWaves, kLight, kAtoms, kSpace, kChemistry, kGeometry, kGroupTotal
};

// A slice of the formula list, one per library file.
struct Part {
  const Formula *formulas;
  int count;
};
int partCount();
Part part(int i);
int groupTotal();
const Group *groups();
// Null-terminated list of formula ids.
const char *const *quickIds();

// Each library file's table.
Part electricalPart();
Part electronicsPart();
Part fieldsPart();
Part motionPart();
Part forcesPart();
Part rotationPart();
Part materialsPart();
Part fluidsPart();
Part heatPart();
Part wavesPart();
Part lightPart();
Part atomsPart();
Part spacePart();
Part chemistryPart();
Part geometryPart();

// ---- Writing a table ----

// A variable; positive unless flags say otherwise.
constexpr Var V(const char *sym, const char *label, const char *name, const char *help, uint8_t dim,
                const char *unit = nullptr, uint8_t flags = kPositive, const char *presets = nullptr) {
  return Var{sym, label, name, help, dim, unit, flags, 0, presets};
}
// A variable that may be zero or negative.
constexpr Var S(const char *sym, const char *label, const char *name, const char *help, uint8_t dim,
                const char *unit = nullptr, const char *presets = nullptr) {
  return Var{sym, label, name, help, dim, unit, 0, 0, presets};
}
// A kConstant: starts at `preset`.
constexpr Var K(const char *sym, const char *label, const char *name, const char *help, uint8_t dim,
                double preset, const char *unit = nullptr, const char *presets = nullptr) {
  return Var{sym, label, name, help, dim, unit, (uint8_t)(kPositive | kConstant), preset, presets};
}

// ---- Handy values, in SI, shared by several formulas ----

constexpr const char *kPresetGravity =
    "Earth=9.80665|Moon=1.62|Mars=3.71|Venus=8.87|Mercury=3.70|Jupiter=24.79|Saturn=10.44|"
    "Uranus=8.69|Neptune=11.15|Sun=274";
constexpr const char *kPresetResistivity =
    "Silver=1.59e-8|Copper=1.68e-8|Gold=2.44e-8|Aluminium=2.65e-8|Tungsten=5.6e-8|Brass=7e-8|"
    "Iron=9.71e-8|Carbon steel=1.43e-7|Constantan=4.9e-7|Stainless steel=6.9e-7|Nichrome=1.1e-6";
constexpr const char *kPresetTempCoeff =
    "Copper=0.00393|Aluminium=0.00429|Silver=0.0038|Tungsten=0.0045|Iron=0.00651|Nichrome=0.0004";
constexpr const char *kPresetDensity =
    "Water=998|Seawater=1025|Air=1.204|Ice=917|Petrol=745|Oil=880|Mercury=13534|Pine=500|Oak=750|"
    "PLA=1240|Concrete=2400|Glass=2500|Aluminium=2700|Steel=7850|Brass=8500|Copper=8960|Lead=11340|Gold=19300";
constexpr const char *kPresetFluidDensity =
    "Water=998|Seawater=1025|Air=1.204|Helium=0.166|Petrol=745|Oil=880|Mercury=13534";
constexpr const char *kPresetSpecificHeat =
    "Water=4186|Ice=2090|Steam=2010|Air=1005|Ethanol=2440|Oil=1900|Aluminium=897|Glass=840|"
    "Concrete=880|Steel=490|Iron=449|Copper=385|Lead=129";
constexpr const char *kPresetLatent =
    "Water melting=3.34e5|Water boiling=2.257e6|Ethanol boiling=8.46e5|Ammonia boiling=1.37e6|"
    "Aluminium melting=3.97e5|Lead melting=2.3e4";
constexpr const char *kPresetConductivity =
    "Copper=401|Aluminium=237|Brass=109|Steel=50|Stainless steel=16|Concrete=1.4|Glass=1.0|"
    "Brick=0.7|Water=0.6|Wood=0.12|Fiberglass=0.04|Polystyrene foam=0.033|Air=0.026";
constexpr const char *kPresetYoung =
    "Steel=2e11|Stainless steel=1.93e11|Copper=1.17e11|Titanium=1.16e11|Brass=1e11|Aluminium=6.9e10|"
    "Glass=7e10|Concrete=3e10|Oak=1.1e10|PLA=3.5e9|ABS=2.3e9|Rubber=5e7";
constexpr const char *kPresetShear =
    "Steel=7.9e10|Stainless steel=7.7e10|Copper=4.4e10|Brass=3.7e10|Aluminium=2.6e10";
constexpr const char *kPresetYield =
    "Mild steel=2.5e8|4140 steel=6.5e8|6061-T6 aluminium=2.76e8|Brass=2e8|Oak=4e7|PLA=5e7";
constexpr const char *kPresetExpansion =
    "Invar=1.2e-6|Pyrex=3.3e-6|Glass=9e-6|Steel=1.2e-5|Concrete=1.2e-5|Copper=1.7e-5|"
    "Stainless steel=1.7e-5|Brass=1.9e-5|Aluminium=2.3e-5|PLA=6.8e-5";
constexpr const char *kPresetIndex =
    "Vacuum=1|Air=1.000293|Ice=1.31|Water=1.333|Acrylic=1.49|Crown glass=1.52|Flint glass=1.62|"
    "Sapphire=1.77|Diamond=2.417";
constexpr const char *kPresetSound =
    "Air 20 C=343|Air 0 C=331|Helium=1007|Water=1482|Seawater=1522|Glass=5640|Steel=5960|Aluminium=6420";
constexpr const char *kPresetFriction =
    "Rubber on dry road=0.8|Rubber on wet road=0.5|Steel on steel=0.6|Wood on wood=0.4|"
    "Steel on oiled steel=0.1|Teflon on steel=0.04|Rubber on ice=0.15";
constexpr const char *kPresetViscosity =
    "Water=1.002e-3|Air=1.81e-5|Ethanol=1.2e-3|Mercury=1.53e-3|SAE 30 oil=0.29|Glycerine=1.41|Honey=10";
constexpr const char *kPresetDielectric =
    "Vacuum=1|Air=1.0006|PTFE=2.1|Polyethylene=2.25|Paper=3.5|FR-4 board=4.4|Mica=6|Alumina=9.8|Water=80";
constexpr const char *kPresetPermeability =
    "Air=1|Nickel=600|Ferrite=2000|Iron=5000";
constexpr const char *kPresetVelocityFactor =
    "Bare wire=0.95|Ladder line=0.91|Twin lead=0.82|Foam coax=0.8|RG-58 coax=0.66";
constexpr const char *kPresetEmissivity =
    "Polished aluminium=0.05|Oxidised steel=0.8|Brick=0.9|Black paint=0.95|Human skin=0.98";
constexpr const char *kPresetPlanetMass =
    "Moon=7.342e22|Mars=6.417e23|Venus=4.867e24|Earth=5.972e24|Saturn=5.683e26|Jupiter=1.898e27|Sun=1.989e30";
constexpr const char *kPresetPlanetRadius =
    "Moon=1.737e6|Mars=3.39e6|Venus=6.052e6|Earth=6.371e6|Saturn=5.823e7|Jupiter=6.991e7|Sun=6.957e8";
constexpr const char *kPresetGamma = "Air=1.4|Helium=1.66|Argon=1.67|CO2=1.29|Steam=1.33";
constexpr const char *kPresetGasMass = "Air=0.02897|Helium=0.004003|Nitrogen=0.02801|Oxygen=0.032|CO2=0.04401";

// Gravity, the same everywhere it appears.
constexpr Var kVarG = K("g", "g", "Gravity", "Pull of gravity. Earth 9.81 m/s^2, Moon 1.62.",
                        units::kAcceleration, 9.80665, nullptr, kPresetGravity);

template <size_t NV, size_t NE>
constexpr Formula formula(const char *id, const char *name, uint8_t group, const char *keywords,
                          const char *blurb, const Var (&vars)[NV], const char *const (&eqs)[NE],
                          uint8_t needed, const char *diagram) {
  static_assert(NV <= (size_t)kMaxVars, "too many variables");
  return Formula{id, name, group, keywords, blurb, vars, (uint8_t)NV, eqs, (uint8_t)NE, needed, diagram};
}

template <size_t N>
constexpr Part partOf(const Formula (&table)[N]) {
  return Part{table, (int)N};
}

}  // namespace lib
}  // namespace formulas
}  // namespace mt
