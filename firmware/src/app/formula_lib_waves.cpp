// The library: Waves & sound, Light & optics, Atoms & relativity, Gravity & space.
#include "formula_diagrams.h"
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {
namespace {

using namespace units;
constexpr uint8_t G1 = lib::kWaves;
constexpr uint8_t G2 = lib::kLight;
constexpr uint8_t G3 = lib::kAtoms;
constexpr uint8_t G4 = lib::kSpace;

// ---- Waves & sound ----

constexpr Var waveV[] = {
  V("v", "v", "Wave speed", "Sound in air 343 m/s, light 3e8 m/s.", kSpeed, nullptr, kPositive, kPresetSound),
  V("f", "f", "Frequency", "Cycles per second. Concert A is 440 Hz.", kFrequency),
  V("lambda", "λ", "Wavelength", "Crest to crest.", kLength),
};
constexpr const char *waveE[] = {"v = f*lambda"};

constexpr Var dopV[] = {
  V("fo", "f_{o}", "Heard frequency", "At the listener.", kFrequency),
  V("fs", "f_{s}", "Source frequency", "As sent.", kFrequency),
  S("vo", "v_{o}", "Listener speed", "Positive towards the source.", kSpeed),
  S("vs", "v_{s}", "Source speed", "Positive towards the listener. Below the wave speed.", kSpeed),
  K("c", "c", "Wave speed", "Sound in air 343 m/s.", kSpeed, 343, nullptr, kPresetSound),
};
constexpr const char *dopE[] = {"fo = fs*(c + vo)/(c - vs)"};

constexpr Var splIV[] = {
  S("L", "L", "Sound level", "Talking 60 dB, a rock band 110 dB.", kDecibel),
  V("I", "I", "Intensity", "Sound power per area.", kIntensity),
  K("I0", "I_{0}", "Reference", "The threshold of hearing, 1e-12 W/m^2.", kIntensity, 1e-12),
};
constexpr const char *splIE[] = {"L = 10*log10(I/I0)"};

constexpr Var splPV[] = {
  S("L", "L", "Sound pressure level", "1 Pa is 94 dB, which is how mics are calibrated.", kDecibel),
  V("p", "p", "Sound pressure", "RMS.", kPressure),
  K("p0", "p_{0}", "Reference", "20 uPa in air.", kPressure, 2e-5),
};
constexpr const char *splPE[] = {"L = 20*log10(p/p0)"};

constexpr Var invsqV[] = {
  V("I", "I", "Intensity", "At distance r. Sunlight on Earth is 1361 W/m^2.", kIntensity),
  V("P", "P", "Source power", "Radiated evenly every way.", kPower),
  V("r", "r", "Distance", "From the source.", kLength),
};
constexpr const char *invsqE[] = {"I = P/(4*pi*r^2)"};

constexpr Var strV[] = {
  V("f", "f", "Fundamental", "The string's lowest note.", kFrequency),
  V("L", "L", "Length", "Of the vibrating part.", kLength, "mm"),
  V("T", "T", "Tension", "A guitar string is 50 to 100 N.", kForce),
  V("mu", "μ", "Mass per length", "A steel E string is about 0.4 g/m.", kLinearDensity, "g/m"),
};
constexpr const char *strE[] = {"f = sqrt(T/mu)/(2*L)"};

constexpr Var sosV[] = {
  V("v", "v", "Speed of sound", "343 m/s in air at 20 C.", kSpeed),
  V("T", "T", "Temperature", "Of the gas.", kTemperature, "C"),
  K("gamma", "γ", "Heat capacity ratio", "1.4 for air.", kNone, 1.4, nullptr, kPresetGamma),
  K("M", "M", "Molar mass", "29 g/mol for air.", kMolarMass, 0.02897, "g/mol", kPresetGasMass),
};
constexpr const char *sosE[] = {"v = sqrt(gamma*Rgas*T/M)"};

constexpr Formula kWavesTable[] = {
  formula("wave", "Wave speed", G1, "frequency wavelength lambda f",
          "Speed, frequency and wavelength of any wave.", waveV, waveE, 2, dWave),
  formula("doppler", "Doppler shift", G1, "siren pitch moving source",
          "How motion shifts the frequency of sound.", dopV, dopE, 3, dDoppler),
  formula("sound-level", "Sound level", G1, "decibel db intensity loudness",
          "Sound intensity on the decibel scale.", splIV, splIE, 1, dSpeaker),
  formula("spl", "Sound pressure level", G1, "decibel db spl microphone",
          "Sound pressure on the decibel scale.", splPV, splPE, 1, dSpeaker),
  formula("inverse-square", "Inverse square law", G1, "intensity distance spreading sphere",
          "Power spreading evenly over a growing sphere.", invsqV, invsqE, 2, dSpreading),
  formula("string", "Vibrating string", G1, "guitar tension note pitch frequency",
          "The note of a stretched string.", strV, strE, 3, dString),
  formula("sound-speed", "Speed of sound in a gas", G1, "air temperature mach",
          "How the speed of sound depends on the gas and its temperature.", sosV, sosE, 1, dWave),
};

// ---- Light & optics ----

constexpr Var photV[] = {
  V("E", "E", "Photon energy", "Green light is 2.3 eV.", kEnergy, "eV"),
  V("f", "f", "Frequency", "Green light is 5.6e14 Hz.", kFrequency),
  V("lambda", "λ", "Wavelength", "Visible light spans 380 to 750 nm.", kLength, "nm"),
};
constexpr const char *photE[] = {"E = h*f", "E = h*c/lambda", "lambda = c/f"};

constexpr Var snellV[] = {
  K("n1", "n_{1}", "Index, first side", "Air 1, water 1.33, glass 1.5.", kNone, 1, nullptr, kPresetIndex),
  V("t1", "θ_{1}", "Angle in", "From the normal.", kAngle, "deg", kAcute),
  K("n2", "n_{2}", "Index, second side", "Air 1, water 1.33, glass 1.5.", kNone, 1.5, nullptr, kPresetIndex),
  V("t2", "θ_{2}", "Angle out", "From the normal.", kAngle, "deg", kAcute),
};
constexpr const char *snellE[] = {"n1*sin(t1) = n2*sin(t2)"};

constexpr Var critV[] = {
  V("tc", "θ_{c}", "Critical angle", "Beyond it, all the light reflects. Glass to air: 42 deg.", kAngle, "deg", kPositive | kAcute),
  V("n1", "n_{1}", "Index inside", "The denser side. Glass 1.5.", kNone, nullptr, kPositive, kPresetIndex),
  V("n2", "n_{2}", "Index outside", "The thinner side. Air 1.", kNone, nullptr, kPositive, kPresetIndex),
};
constexpr const char *critE[] = {"sin(tc) = n2/n1"};

constexpr Var lensV[] = {
  S("f", "f", "Focal length", "Positive for a converging lens, negative for diverging.", kLength, "mm"),
  S("do", "d_{o}", "Object distance", "From the lens.", kLength, "mm"),
  S("di", "d_{i}", "Image distance", "From the lens. Negative: a virtual image, same side.", kLength, "mm"),
  S("M", "M", "Magnification", "Negative when the image is upside down.", kNone),
};
constexpr const char *lensE[] = {
  "1/f = 1/do + 1/di", "M = -di/do", "do = f*(M - 1)/M", "di = f*(1 - M)",
};

constexpr Var gratV[] = {
  V("d", "d", "Line spacing", "1000 lines per mm is 1 um.", kLength, "um"),
  V("theta", "θ", "Diffraction angle", "Of that order.", kAngle, "deg", kPositive | kAcute),
  V("m", "m", "Order", "1, 2, 3...", kNone, nullptr, kPositive | kInteger),
  V("lambda", "λ", "Wavelength", "Red laser 650 nm.", kLength, "nm"),
};
constexpr const char *gratE[] = {"d*sin(theta) = m*lambda"};

constexpr Var slitV[] = {
  V("y", "y", "Fringe spacing", "On the screen.", kLength, "mm"),
  V("lambda", "λ", "Wavelength", "Red laser 650 nm.", kLength, "nm"),
  V("L", "L", "Screen distance", "From the slits.", kLength),
  V("d", "d", "Slit spacing", "Between the slits.", kLength, "mm"),
};
constexpr const char *slitE[] = {"y = lambda*L/d"};

constexpr Var fnumV[] = {
  V("N", "N", "f-number", "f/2.8, f/8 and so on. Smaller lets in more.", kNone),
  V("f", "f", "Focal length", "Of the lens.", kLength, "mm"),
  V("D", "D", "Aperture", "Diameter of the opening.", kLength, "mm"),
};
constexpr const char *fnumE[] = {"N = f/D"};

constexpr Var fovV[] = {
  V("a", "α", "Angle of view", "Across the sensor's width.", kAngle, "deg"),
  V("s", "s", "Sensor width", "Full frame 36 mm.", kLength, "mm"),
  V("f", "f", "Focal length", "Of the lens.", kLength, "mm"),
};
constexpr const char *fovE[] = {"a = 2*atan(s/(2*f))"};

constexpr Var wienV[] = {
  V("lambda", "λ", "Peak wavelength", "Where a hot body glows brightest. The Sun: 500 nm.", kLength, "nm"),
  V("T", "T", "Temperature", "Of the body. The Sun's surface is 5772 K.", kTemperature, "K"),
};
constexpr const char *wienE[] = {"lambda = bWien/T"};

constexpr Formula kLightTable[] = {
  formula("photon", "Photon energy", G2, "planck hf wavelength frequency ev light",
          "Energy of one particle of light.", photV, photE, 1, dPhoton),
  formula("snell", "Snell's law", G2, "refraction bending index",
          "How light bends crossing into another material.", snellV, snellE, 1, dRefraction),
  formula("critical-angle", "Critical angle", G2, "total internal reflection fibre optic",
          "The angle beyond which light cannot get out.", critV, critE, 2, dCritical),
  formula("thin-lens", "Thin lens", G2, "focal length image object mirror camera",
          "Where a thin lens puts the image. The same equation works for a curved mirror.",
          lensV, lensE, 2, dLens),
  formula("grating", "Diffraction grating", G2, "lines spectrum order bragg",
          "Where the colours go from a grating.", gratV, gratE, 3, dGrating),
  formula("double-slit", "Double slit", G2, "young interference fringes",
          "Spacing of the bright fringes, for small angles.", slitV, slitE, 3, dGrating),
  formula("f-number", "f-number", G2, "aperture camera lens stop",
          "A lens's focal length over its aperture.", fnumV, fnumE, 2, dCamera),
  formula("angle-of-view", "Angle of view", G2, "field fov camera lens sensor",
          "How wide a lens sees.", fovV, fovE, 2, dCamera),
  formula("wien", "Wien's law", G2, "blackbody peak colour temperature",
          "The colour a hot body glows brightest in.", wienV, wienE, 1, dBlackbody),
};

// ---- Atoms & relativity ----

constexpr Var emcV[] = {
  V("E", "E", "Energy", "1 kg is 9e16 J.", kEnergy),
  V("m", "m", "Mass", "Turned to energy.", kMass),
};
constexpr const char *emcE[] = {"E = m*c^2"};

constexpr Var debV[] = {
  V("lambda", "λ", "Wavelength", "Of the matter wave.", kLength, "nm"),
  V("m", "m", "Mass", "An electron is 9.109e-31 kg.", kMass, nullptr, kPositive,
    "Electron=9.1093837e-31|Proton=1.6726219e-27|Neutron=1.6749275e-27"),
  V("v", "v", "Speed", "Well below the speed of light.", kSpeed),
};
constexpr const char *debE[] = {"lambda = h/(m*v)"};

constexpr Var pheV[] = {
  V("K", "K", "Electron energy", "The most an electron leaves with.", kEnergy, "eV"),
  V("f", "f", "Light frequency", "UV is about 1e15 Hz.", kFrequency),
  K("W", "φ", "Work function", "To free an electron. Sodium 2.28 eV, zinc 4.3 eV.", kEnergy, 3.65e-19, "eV",
    "Caesium=3.36e-19|Sodium=3.65e-19|Zinc=6.89e-19|Copper=7.53e-19"),
};
constexpr const char *pheE[] = {"K = h*f - W"};

constexpr Var tdilV[] = {
  V("t", "t", "Moving clock, seen from here", "Longer than t0.", kTime),
  V("t0", "t_{0}", "Time on the clock", "As the moving clock counts it.", kTime),
  V("v", "v", "Speed", "Below c. 0.9 c stretches time by 2.3.", kSpeed),
};
constexpr const char *tdilE[] = {"t = t0/sqrt(1 - v^2/c^2)"};

constexpr Formula kAtomsTable[] = {
  formula("mass-energy", "Mass-energy", G3, "e=mc2 einstein relativity nuclear",
          "How much energy a mass is worth.", emcV, emcE, 1, dMassEnergy),
  formula("de-broglie", "de Broglie wavelength", G3, "matter wave quantum electron",
          "Every moving thing has a wavelength.", debV, debE, 2, dMatterWave),
  formula("photoelectric", "Photoelectric effect", G3, "einstein work function electron light",
          "The energy light gives an electron knocked from a metal.", pheV, pheE, 1, dPhotoelectric),
  formula("time-dilation", "Time dilation", G3, "relativity lorentz clock gamma twin",
          "Moving clocks run slow.", tdilV, tdilE, 2, dRocket),
};

// ---- Gravity & space ----

constexpr Var gravV[] = {
  V("F", "F", "Force", "Between the two.", kForce),
  V("m1", "m_{1}", "Mass 1", "First body.", kMass, nullptr, kPositive, kPresetPlanetMass),
  V("m2", "m_{2}", "Mass 2", "Second body.", kMass, nullptr, kPositive, kPresetPlanetMass),
  V("r", "r", "Distance", "Between the centres. Earth to Moon: 384,400 km.", kLength, "km"),
};
constexpr const char *gravE[] = {"F = Gnewton*m1*m2/r^2"};

constexpr Var surfV[] = {
  V("g", "g", "Surface gravity", "Earth 9.81 m/s^2.", kAcceleration),
  V("M", "M", "Planet mass", "Earth 5.97e24 kg.", kMass, nullptr, kPositive, kPresetPlanetMass),
  V("r", "r", "Radius", "Earth 6371 km.", kLength, "km", kPositive, kPresetPlanetRadius),
};
constexpr const char *surfE[] = {"g = Gnewton*M/r^2"};

constexpr Var orbV[] = {
  V("v", "v", "Orbital speed", "The ISS moves at 7.7 km/s.", kSpeed, "km/h"),
  V("M", "M", "Central mass", "Earth 5.97e24 kg.", kMass, nullptr, kPositive, kPresetPlanetMass),
  V("r", "r", "Orbit radius", "From the planet's centre, not the ground.", kLength, "km"),
};
constexpr const char *orbE[] = {"v = sqrt(Gnewton*M/r)"};

constexpr Var orbTV[] = {
  V("T", "T", "Period", "One orbit. The ISS takes 93 minutes.", kTime, "h"),
  V("r", "r", "Orbit radius", "From the planet's centre. GEO is 42,164 km.", kLength, "km"),
  V("M", "M", "Central mass", "Earth 5.97e24 kg.", kMass, nullptr, kPositive, kPresetPlanetMass),
};
constexpr const char *orbTE[] = {"T = 2*pi*sqrt(r^3/(Gnewton*M))"};

constexpr Var escV[] = {
  V("v", "v", "Escape speed", "Earth 11.2 km/s, Moon 2.4 km/s.", kSpeed, "km/h"),
  V("M", "M", "Planet mass", "Earth 5.97e24 kg.", kMass, nullptr, kPositive, kPresetPlanetMass),
  V("r", "r", "Distance", "From the centre; the radius, from the ground.", kLength, "km", kPositive, kPresetPlanetRadius),
};
constexpr const char *escE[] = {"v = sqrt(2*Gnewton*M/r)"};

constexpr Var kepV[] = {
  V("T1", "T_{1}", "Period 1", "Of the first orbit.", kTime, "d"),
  V("a1", "a_{1}", "Size of orbit 1", "Semi-major axis.", kLength, "km"),
  V("T2", "T_{2}", "Period 2", "Of the second orbit, round the same body.", kTime, "d"),
  V("a2", "a_{2}", "Size of orbit 2", "Semi-major axis.", kLength, "km"),
};
constexpr const char *kepE[] = {"(T1/T2)^2 = (a1/a2)^3"};

constexpr Var rockV[] = {
  V("dv", "Δv", "Change of speed", "Reaching low Earth orbit takes about 9.4 km/s.", kSpeed),
  V("ve", "v_{e}", "Exhaust speed", "Kerosene 3 km/s, hydrogen 4.4 km/s.", kSpeed),
  V("m0", "m_{0}", "Start mass", "Fuelled.", kMass),
  V("mf", "m_{f}", "End mass", "After the burn.", kMass),
};
constexpr const char *rockE[] = {"dv = ve*ln(m0/mf)"};

constexpr Formula kSpaceTable[] = {
  formula("gravitation", "Newton's gravity", G4, "universal attraction law force inverse square",
          "Force between two masses.", gravV, gravE, 3, dMasses),
  formula("surface-gravity", "Surface gravity", G4, "planet weight acceleration",
          "The g of a planet from its mass and size.", surfV, surfE, 2, dPlanet),
  formula("orbital-speed", "Orbital speed", G4, "circular satellite velocity",
          "The speed of a circular orbit.", orbV, orbE, 2, dOrbit),
  formula("orbital-period", "Orbital period", G4, "circular satellite kepler geostationary",
          "The time round a circular orbit.", orbTV, orbTE, 2, dOrbit),
  formula("escape", "Escape velocity", G4, "speed leave planet rocket",
          "The speed to leave for good, without further thrust.", escV, escE, 2, dPlanet),
  formula("kepler", "Kepler's third law", G4, "period semi-major axis orbit planets",
          "Comparing two orbits round the same body.", kepV, kepE, 3, dTwoOrbits),
  formula("rocket", "Rocket equation", G4, "tsiolkovsky delta v exhaust fuel mass",
          "What fuel buys you in speed.", rockV, rockE, 3, dRocketBurn),
};

}  // namespace

Part wavesPart() { return partOf(kWavesTable); }
Part lightPart() { return partOf(kLightTable); }
Part atomsPart() { return partOf(kAtomsTable); }
Part spacePart() { return partOf(kSpaceTable); }

}  // namespace lib
}  // namespace formulas
}  // namespace mt
