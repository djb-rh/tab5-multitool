// The library's index: the groups, the order of its parts and the quick list. The
// formulas are in formula_lib_*.cpp; their drawings in formula_diagrams.h.
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {

namespace {

constexpr Group kGroups[] = {
  {"Electrical", "Ohm's law, power, R C L, AC", 0xE8B23A},
  {"Electronics & radio", "Timing, filters, op-amps, dB, antennas", 0x4FB0E8},
  {"Fields & magnetism", "Charge, coils, magnets", 0x9A7BE0},
  {"Motion", "Speed, acceleration, projectiles", 0x58C878},
  {"Forces & energy", "Newton, friction, work, springs", 0xE8804A},
  {"Rotation & machines", "Torque, shafts, gears, levers", 0xD86A8E},
  {"Materials & structures", "Stress, beams, shafts, expansion", 0x9AA8B8},
  {"Fluids", "Pressure, flow, drag", 0x3AC0C0},
  {"Heat & gases", "Gas laws, heat flow, engines", 0xE85A4A},
  {"Waves & sound", "Frequency, decibels, Doppler", 0x7AC8E8},
  {"Light & optics", "Lenses, refraction, photons", 0xE8D85A},
  {"Atoms & relativity", "E = mc^2, matter waves, levels", 0xB88AE0},
  {"Gravity & space", "Orbits, escape, rockets", 0x6A88E8},
  {"Chemistry", "Moles, pH, rates, decay", 0x78D0A0},
  {"Geometry", "Shapes, solids, triangles", 0xC8B088},
};
static_assert(sizeof kGroups / sizeof kGroups[0] == kGroupTotal, "kGroups must match enum GroupId");

constexpr const char *kQuick[] = {
  "ohm", "r-parallel", "divider", "led", "wire-r", "rc-tau", "xc", "resonance",
  "suvat", "fma", "kinetic", "shaft-power", "pressure", "ideal-gas", "wave", "right-triangle",
  nullptr,
};

}  // namespace

int groupTotal() { return kGroupTotal; }
const Group *groups() { return kGroups; }
const char *const *quickIds() { return kQuick; }

int partCount() { return kGroupTotal; }

Part part(int i) {
  switch (i) {
    case kElectrical: return electricalPart();
    case kElectronics: return electronicsPart();
    case kFields: return fieldsPart();
    case kMotion: return motionPart();
    case kForces: return forcesPart();
    case kRotation: return rotationPart();
    case kMaterials: return materialsPart();
    case kFluids: return fluidsPart();
    case kHeat: return heatPart();
    case kWaves: return wavesPart();
    case kLight: return lightPart();
    case kAtoms: return atomsPart();
    case kSpace: return spacePart();
    case kChemistry: return chemistryPart();
    case kGeometry: return geometryPart();
    default: return Part{nullptr, 0};
  }
}

}  // namespace lib
}  // namespace formulas
}  // namespace mt
