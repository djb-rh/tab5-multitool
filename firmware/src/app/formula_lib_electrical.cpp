// The library: Electrical, Electronics & radio, Fields & magnetism.
#include "formula_diagrams.h"
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {
namespace {

using namespace units;
constexpr uint8_t G1 = lib::kElectrical;
constexpr uint8_t G2 = lib::kElectronics;
constexpr uint8_t G3 = lib::kFields;

// ---- Electrical ----

constexpr Var ohmV[] = {
  V("V", "V", "Voltage", "Across the part. A AA cell is 1.5 V, a car 12 V, US mains 120 V.", kVoltage),
  V("I", "I", "Current", "Through the part. An LED takes 20 mA, a kettle about 10 A.", kCurrent),
  V("R", "R", "Resistance", "How hard it is for current to flow. Resistors run from ohms to megohms.", kResistance),
  V("P", "P", "Power", "Heat or work per second. A small resistor is rated 0.25 W, a heater 1500 W.", kPower),
};
constexpr const char *ohmE[] = {"V = I*R", "P = V*I", "P = I^2*R", "P = V^2/R"};

constexpr Var rser2V[] = {
  V("R", "R", "Total resistance", "What the pair looks like from outside.", kResistance),
  V("R1", "R_{1}", "Resistor 1", "A 1 kohm resistor is a common value.", kResistance),
  V("R2", "R_{2}", "Resistor 2", "The second resistor in the chain.", kResistance),
};
constexpr const char *rser2E[] = {"R = R1 + R2"};

constexpr Var rser3V[] = {
  V("R", "R", "Total resistance", "What the chain looks like from outside.", kResistance),
  V("R1", "R_{1}", "Resistor 1", "First in the chain.", kResistance),
  V("R2", "R_{2}", "Resistor 2", "Second in the chain.", kResistance),
  V("R3", "R_{3}", "Resistor 3", "Third in the chain.", kResistance),
};
constexpr const char *rser3E[] = {"R = R1 + R2 + R3"};

constexpr const char *rpar2E[] = {"R = R1*R2/(R1 + R2)", "1/R = 1/R1 + 1/R2"};
constexpr const char *rpar3E[] = {"R = 1/(1/R1 + 1/R2 + 1/R3)", "1/R = 1/R1 + 1/R2 + 1/R3"};

constexpr Var cpar2V[] = {
  V("C", "C", "Total capacitance", "What the pair acts like.", kCapacitance, "uF"),
  V("C1", "C_{1}", "Capacitor 1", "100 nF is the usual decoupling value.", kCapacitance, "uF"),
  V("C2", "C_{2}", "Capacitor 2", "The second capacitor.", kCapacitance, "uF"),
};
constexpr const char *cpar2E[] = {"C = C1 + C2"};
constexpr const char *cser2E[] = {"C = C1*C2/(C1 + C2)", "1/C = 1/C1 + 1/C2"};

constexpr Var lser2V[] = {
  V("L", "L", "Total inductance", "What the pair acts like, with no coupling between them.", kInductance, "uH"),
  V("L1", "L_{1}", "Inductor 1", "Power inductors run from 1 uH to 1 mH.", kInductance, "uH"),
  V("L2", "L_{2}", "Inductor 2", "The second inductor.", kInductance, "uH"),
};
constexpr const char *lser2E[] = {"L = L1 + L2"};
constexpr const char *lpar2E[] = {"L = L1*L2/(L1 + L2)", "1/L = 1/L1 + 1/L2"};

constexpr Var divV[] = {
  S("Vin", "V_{in}", "Input voltage", "Across the whole divider. 5 V, 12 V...", kVoltage),
  S("Vout", "V_{out}", "Output voltage", "Across R2, with nothing loading it.", kVoltage),
  V("R1", "R_{1}", "Top resistor", "From the input to the output.", kResistance, "kohm"),
  V("R2", "R_{2}", "Bottom resistor", "From the output to ground.", kResistance, "kohm"),
};
constexpr const char *divE[] = {
  "Vout = Vin*R2/(R1 + R2)", "Vin = Vout*(R1 + R2)/R2", "R1 = R2*(Vin - Vout)/Vout",
  "R2 = R1*Vout/(Vin - Vout)",
};

constexpr Var cdivV[] = {
  V("I", "I", "Total current", "Going into the pair.", kCurrent),
  V("I1", "I_{1}", "Current in R1", "The smaller resistor takes the larger share.", kCurrent),
  V("R1", "R_{1}", "Resistor 1", "The branch whose current is I1.", kResistance),
  V("R2", "R_{2}", "Resistor 2", "The other branch.", kResistance),
};
constexpr const char *cdivE[] = {
  "I1 = I*R2/(R1 + R2)", "I = I1*(R1 + R2)/R2", "R1 = R2*(I - I1)/I1", "R2 = R1*I1/(I - I1)",
};

constexpr Var ledV[] = {
  V("Vs", "V_{s}", "Supply voltage", "What feeds the LED and its resistor. 3.3 V, 5 V, 12 V.", kVoltage),
  V("Vf", "V_{f}", "LED forward voltage", "Red about 2 V, green 2.2 V, blue and white 3.1 V.", kVoltage, nullptr,
    kPositive, "Red=2.0|Yellow=2.1|Green=2.2|Blue=3.1|White=3.1|Infrared=1.3"),
  V("I", "I", "LED current", "Indicators run at 2 to 20 mA.", kCurrent, "mA"),
  V("R", "R", "Series resistor", "Pick the next standard value up.", kResistance),
  V("P", "P", "Resistor power", "Heat in the resistor. Choose a rating about twice this.", kPower, "mW"),
};
constexpr const char *ledE[] = {"R = (Vs - Vf)/I", "P = I^2*R", "P = (Vs - Vf)*I", "P = (Vs - Vf)^2/R"};

constexpr Var wireV[] = {
  V("R", "R", "Resistance", "End to end. 18 AWG copper is about 21 milliohm per metre.", kResistance),
  K("rho", "ρ", "Resistivity", "A property of the metal, measured in ohm-metres. Copper 1.68e-8.",
    kResistivity, 1.68e-8, nullptr, kPresetResistivity),
  V("L", "L", "Length", "Of the wire, one way.", kLength),
  V("A", "A", "Cross-section", "Of the metal. 1.5 mm^2 is house lighting wire.", kArea, "mm^2"),
  V("d", "d", "Diameter", "Of the metal, without the insulation.", kLength, "mm"),
};
constexpr const char *wireE[] = {"R = rho*L/A", "A = pi*d^2/4", "R = 4*rho*L/(pi*d^2)"};

constexpr Var vdropV[] = {
  V("Vd", "V_{d}", "Voltage drop", "Lost in the cable. Keep it under about 3% of the supply.", kVoltage),
  V("I", "I", "Current", "Drawn by the load.", kCurrent),
  V("L", "L", "Cable length", "One way: the current travels there and back.", kLength),
  V("A", "A", "Cross-section", "Of one conductor. 2.5 mm^2 is a common power cable.", kArea, "mm^2"),
  K("rho", "ρ", "Resistivity", "A property of the metal, measured in ohm-metres. Copper 1.68e-8.",
    kResistivity, 1.68e-8, nullptr, kPresetResistivity),
};
constexpr const char *vdropE[] = {"Vd = 2*I*rho*L/A"};

constexpr Var rtempV[] = {
  V("R", "R", "Resistance when hot", "At the new temperature.", kResistance),
  V("R0", "R_{0}", "Resistance at reference", "Usually quoted at 20 C.", kResistance),
  K("alpha", "α", "Temperature coefficient", "Fractional change per kelvin. Copper 0.00393.",
    kExpansion, 0.00393, nullptr, kPresetTempCoeff),
  S("dT", "ΔT", "Temperature change", "Above the reference temperature.", kTempDiff, "C deg"),
};
constexpr const char *rtempE[] = {"R = R0*(1 + alpha*dT)"};

constexpr Var batV[] = {
  V("t", "t", "Run time", "From full to empty.", kTime, "h"),
  V("Q", "Q", "Capacity", "On the label. A AA is about 2500 mAh, a phone 4000 mAh.", kCharge, "mAh"),
  V("I", "I", "Load current", "Average draw of the device.", kCurrent, "mA"),
};
constexpr const char *batE[] = {"t = Q/I"};

constexpr Var batEV[] = {
  V("E", "E", "Stored energy", "A phone battery holds about 15 Wh, a car battery 600 Wh.", kEnergy, "Wh"),
  V("V", "V", "Nominal voltage", "Li-ion 3.7 V per cell, lead-acid 12 V.", kVoltage),
  V("Q", "Q", "Capacity", "The rating in Ah or mAh.", kCharge, "Ah"),
};
constexpr const char *batEE[] = {"E = V*Q"};

constexpr Var energyV[] = {
  V("E", "E", "Energy", "What the meter bills. A kettle boil is about 0.1 kWh.", kEnergy, "kWh"),
  V("P", "P", "Power", "The appliance's rating. A fridge averages 100 W.", kPower),
  V("t", "t", "Time", "How long it runs.", kTime, "h"),
};
constexpr const char *energyE[] = {"E = P*t"};

constexpr Var txV[] = {
  V("Vp", "V_{p}", "Primary voltage", "What the transformer is fed with.", kVoltage),
  V("Vs", "V_{s}", "Secondary voltage", "What comes out, unloaded.", kVoltage),
  V("Np", "N_{p}", "Primary turns", "Turns on the input winding.", kNone, nullptr, kPositive | kInteger),
  V("Ns", "N_{s}", "Secondary turns", "Turns on the output winding.", kNone, nullptr, kPositive | kInteger),
};
constexpr const char *txE[] = {"Vs/Vp = Ns/Np"};

constexpr Var txiV[] = {
  V("Ip", "I_{p}", "Primary current", "Drawn from the supply.", kCurrent),
  V("Is", "I_{s}", "Secondary current", "Delivered to the load.", kCurrent),
  V("Np", "N_{p}", "Primary turns", "Turns on the input winding.", kNone, nullptr, kPositive | kInteger),
  V("Ns", "N_{s}", "Secondary turns", "Turns on the output winding.", kNone, nullptr, kPositive | kInteger),
};
constexpr const char *txiE[] = {"Is/Ip = Np/Ns"};

constexpr Var rmsV[] = {
  V("Vrms", "V_{rms}", "RMS voltage", "The heating value and what a meter shows. US mains 120 V.", kVoltage),
  V("Vpk", "V_{pk}", "Peak voltage", "From zero to the top of the wave. 170 V for 120 V mains.", kVoltage),
  V("Vpp", "V_{pp}", "Peak-to-peak", "Bottom to top, as read off a scope.", kVoltage),
  V("Vavg", "V_{avg}", "Rectified average", "Mean of the wave with its negative half flipped.", kVoltage),
};
constexpr const char *rmsE[] = {"Vrms = Vpk/sqrt(2)", "Vpp = 2*Vpk", "Vavg = 2*Vpk/pi"};

constexpr Var acpV[] = {
  V("P", "P", "Real power", "Doing work, in watts.", kPower),
  V("V", "V", "RMS voltage", "US mains 120 V, Europe 230 V.", kVoltage),
  V("I", "I", "RMS current", "As read by a clamp meter.", kCurrent),
  V("pf", "pf", "Power factor", "1 for a heater, about 0.8 for a motor.", kRatio),
};
constexpr const char *acpE[] = {"P = V*I*pf"};

constexpr Var p3V[] = {
  V("P", "P", "Real power", "Total of the three phases.", kPower, "kW"),
  V("V", "V_{L}", "Line voltage", "Between two lines: 208, 400 or 480 V.", kVoltage),
  V("I", "I_{L}", "Line current", "In each line.", kCurrent),
  V("pf", "pf", "Power factor", "1 for heaters, about 0.85 for loaded motors.", kRatio),
};
constexpr const char *p3E[] = {"P = sqrt(3)*V*I*pf"};

constexpr Var ptriV[] = {
  V("P", "P", "Real power", "Does the work, in watts.", kPower),
  V("Q", "Q", "Reactive power", "Sloshes back and forth; in VAR, shown here in W.", kPower),
  V("S", "S", "Apparent power", "V times I; in VA, shown here in W.", kPower),
  V("pf", "pf", "Power factor", "P over S. 1 is ideal.", kRatio),
  V("phi", "φ", "Phase angle", "Between voltage and current.", kAngle, "deg", kPositive | kAcute),
};
constexpr const char *ptriE[] = {
  "S = sqrt(P^2 + Q^2)", "P = S*pf", "pf = cos(phi)", "Q = S*sin(phi)", "tan(phi) = Q/P",
};

constexpr Var effV[] = {
  V("eta", "η", "Efficiency", "Out over in. A good motor is 90%, a buck converter 95%.", kRatio, "%"),
  V("Pout", "P_{out}", "Power out", "Useful power delivered.", kPower),
  V("Pin", "P_{in}", "Power in", "Power taken from the source.", kPower),
};
constexpr const char *effE[] = {"eta = Pout/Pin"};

constexpr Var intrV[] = {
  V("Vt", "V_{t}", "Terminal voltage", "What you measure under load.", kVoltage),
  V("E", "E", "Open-circuit voltage", "With nothing connected. 1.5 V for a fresh AA.", kVoltage),
  S("I", "I", "Load current", "Drawn from the source.", kCurrent),
  V("r", "r", "Internal resistance", "A AA about 0.2 ohm, a car battery 0.01 ohm.", kResistance),
};
constexpr const char *intrE[] = {"Vt = E - I*r"};

constexpr Formula kElectricalTable[] = {
  formula("ohm", "Ohm's law and power", G1,
          "ohms watts volts amps voltage current resistance power vir pvi joule heating dissipation",
          "Voltage, current, resistance and power in a resistive part. Any two give the other two.",
          ohmV, ohmE, 2, dOhm),
  formula("r-series", "Resistors in series", G1, "add sum chain total two",
          "End to end, resistances add. The same current flows through both.", rser2V, rser2E, 2, dRSeries),
  formula("r-parallel", "Resistors in parallel", G1, "product over sum shunt total two",
          "Side by side, the total is less than either one. Both see the same voltage.",
          rser2V, rpar2E, 2, dRParallel),
  formula("r-series3", "Three resistors in series", G1, "add sum chain total",
          "End to end, resistances add.", rser3V, rser3E, 3, dRSeries3),
  formula("r-parallel3", "Three resistors in parallel", G1, "shunt total",
          "Side by side, conductances add, so the reciprocals of the resistances do.",
          rser3V, rpar3E, 3, dRParallel3),
  formula("divider", "Voltage divider", G1, "potential divider attenuator sensor bias",
          "Two resistors tapping a fraction of a voltage. Assumes nothing heavy loads the output.",
          divV, divE, 3, dDivider),
  formula("cdivider", "Current divider", G1, "split parallel branch share shunt",
          "How a current splits between two parallel resistors.", cdivV, cdivE, 3, dRParallel),
  formula("led", "LED resistor", G1, "diode series limiting current light indicator",
          "The series resistor that sets an LED's current, and the heat it has to shed.",
          ledV, ledE, 3, dLed),
  formula("c-parallel", "Capacitors in parallel", G1, "add sum total",
          "Side by side, capacitances add.", cpar2V, cpar2E, 2, dCParallel),
  formula("c-series", "Capacitors in series", G1, "total product over sum",
          "End to end, the total is less than either one.", cpar2V, cser2E, 2, dCSeries),
  formula("l-series", "Inductors in series", G1, "add sum total coil",
          "End to end, inductances add, as long as their fields do not couple.", lser2V, lser2E, 2, dLSeries),
  formula("l-parallel", "Inductors in parallel", G1, "total coil",
          "Side by side, the total is less than either one, as long as their fields do not couple.",
          lser2V, lpar2E, 2, dLParallel),
  formula("wire-r", "Wire resistance", G1, "resistivity gauge awg copper cable conductor",
          "Resistance from the metal, its length and its thickness.", wireV, wireE, 2, dWire),
  formula("vdrop", "Cable voltage drop", G1, "wire loss run length gauge sizing",
          "Volts lost in a two-conductor cable, out and back.", vdropV, vdropE, 3, dVdrop),
  formula("r-temp", "Resistance change with temperature", G1, "tempco coefficient alpha hot",
          "Metals resist more as they warm. Linear, so good for a hundred kelvin or so.",
          rtempV, rtempE, 2, dOhm),
  formula("battery-life", "Battery life", G1, "runtime capacity mah ah hours discharge",
          "Run time from capacity and load. Real cells give less at high current.", batV, batE, 2, dBattery),
  formula("battery-energy", "Battery energy", G1, "wh watt hours stored capacity",
          "Energy stored in a battery from its voltage and capacity.", batEV, batEE, 2, dBattery),
  formula("energy-use", "Electrical energy", G1, "kwh bill consumption power time",
          "Energy used at a steady power. 1 kWh is 3.6 MJ.", energyV, energyE, 2, dAcLoad),
  formula("transformer", "Transformer voltage", G1, "turns ratio step down up primary secondary winding",
          "Voltage goes with turns, for an ideal transformer.", txV, txE, 3, dTransformer),
  formula("transformer-i", "Transformer current", G1, "turns ratio amps primary secondary winding",
          "Current goes against turns: power in equals power out.", txiV, txiE, 3, dTransformer),
  formula("rms", "RMS and peak", G1, "sine ac average peak to peak pp mains waveform",
          "The readings of a sine wave. These ratios are for a sine only.", rmsV, rmsE, 1, dRms),
  formula("ac-power", "AC power", G1, "single phase watts real pf",
          "Real power in a single-phase AC load.", acpV, acpE, 3, dAcLoad),
  formula("three-phase", "Three-phase power", G1, "3 phase motor kw watts line root 3",
          "Real power in a balanced three-phase load, from line voltage and line current.",
          p3V, p3E, 3, dThreePhase),
  formula("power-triangle", "Power factor and the power triangle", G1,
          "pf real reactive apparent var va phase angle correction",
          "How real, reactive and apparent power fit a right triangle.", ptriV, ptriE, 2, dPowerTriangle),
  formula("efficiency", "Efficiency", G1, "losses power in out eta",
          "The share of the power going in that comes out as something useful.", effV, effE, 2, dBlackBox),
  formula("internal-r", "Internal resistance", G1, "battery sag esr source terminal load",
          "Why a source's voltage sags as it delivers current.", intrV, intrE, 3, dInternalR),
};

// ---- Electronics & radio ----

constexpr Var rcV[] = {
  V("tau", "τ", "Time constant", "After one τ a capacitor has moved 63% of the way.", kTime, "ms"),
  V("R", "R", "Resistance", "In series with the capacitor.", kResistance, "kohm"),
  V("C", "C", "Capacitance", "100 nF, 10 uF...", kCapacitance, "uF"),
};
constexpr const char *rcE[] = {"tau = R*C"};

constexpr Var rlV[] = {
  V("tau", "τ", "Time constant", "After one τ the current has moved 63% of the way.", kTime, "ms"),
  V("L", "L", "Inductance", "A power choke might be 100 uH.", kInductance, "mH"),
  V("R", "R", "Resistance", "In series, including the coil's own.", kResistance),
};
constexpr const char *rlE[] = {"tau = L/R"};

constexpr Var rcchV[] = {
  V("V", "V", "Capacitor voltage", "At time t, starting from zero. Below Vs.", kVoltage),
  V("Vs", "V_{s}", "Supply voltage", "What it is charging towards.", kVoltage),
  V("t", "t", "Time", "Since the switch closed.", kTime, "ms"),
  V("R", "R", "Resistance", "In series with the capacitor.", kResistance, "kohm"),
  V("C", "C", "Capacitance", "The capacitor being charged.", kCapacitance, "uF"),
};
constexpr const char *rcchE[] = {"V = Vs*(1 - exp(-t/(R*C)))"};

constexpr Var rcdisV[] = {
  V("V", "V", "Capacitor voltage", "At time t. Below V0.", kVoltage),
  V("V0", "V_{0}", "Starting voltage", "When the discharge began.", kVoltage),
  V("t", "t", "Time", "Since the discharge began.", kTime, "ms"),
  V("R", "R", "Resistance", "What it discharges through.", kResistance, "kohm"),
  V("C", "C", "Capacitance", "The capacitor.", kCapacitance, "uF"),
};
constexpr const char *rcdisE[] = {"V = V0*exp(-t/(R*C))"};

constexpr Var rlchV[] = {
  V("I", "I", "Current", "At time t, starting from zero. Below V/R.", kCurrent, "mA"),
  V("V", "V", "Supply voltage", "Step applied to the coil.", kVoltage),
  V("R", "R", "Resistance", "In series, including the coil's own.", kResistance),
  V("L", "L", "Inductance", "The coil.", kInductance, "mH"),
  V("t", "t", "Time", "Since the switch closed.", kTime, "ms"),
};
constexpr const char *rlchE[] = {"I = V/R*(1 - exp(-R*t/L))"};

constexpr Var xcV[] = {
  V("Xc", "X_{C}", "Capacitive reactance", "AC opposition, in ohms. Falls as frequency rises.", kResistance),
  V("f", "f", "Frequency", "Of the signal. 60 Hz mains, 1 kHz audio.", kFrequency, "kHz"),
  V("C", "C", "Capacitance", "The capacitor.", kCapacitance, "uF"),
};
constexpr const char *xcE[] = {"Xc = 1/(2*pi*f*C)"};

constexpr Var xlV[] = {
  V("Xl", "X_{L}", "Inductive reactance", "AC opposition, in ohms. Rises with frequency.", kResistance),
  V("f", "f", "Frequency", "Of the signal.", kFrequency, "kHz"),
  V("L", "L", "Inductance", "The coil.", kInductance, "mH"),
};
constexpr const char *xlE[] = {"Xl = 2*pi*f*L"};

constexpr Var zrlcV[] = {
  V("Z", "Z", "Impedance", "Magnitude of the total AC opposition.", kResistance),
  V("R", "R", "Resistance", "The resistive part.", kResistance),
  V("Xl", "X_{L}", "Inductive reactance", "From the coil: 2πfL.", kResistance),
  V("Xc", "X_{C}", "Capacitive reactance", "From the capacitor: 1/(2πfC).", kResistance),
};
constexpr const char *zrlcE[] = {"Z = sqrt(R^2 + (Xl - Xc)^2)"};

constexpr Var zrcV[] = {
  V("Z", "Z", "Impedance", "Of the series pair. Above R.", kResistance),
  V("R", "R", "Resistance", "In series.", kResistance),
  V("f", "f", "Frequency", "Of the signal.", kFrequency, "kHz"),
  V("C", "C", "Capacitance", "In series.", kCapacitance, "uF"),
};
constexpr const char *zrcE[] = {"Z = sqrt(R^2 + 1/(2*pi*f*C)^2)"};

constexpr Var zrlV[] = {
  V("Z", "Z", "Impedance", "Of the series pair. Above R.", kResistance),
  V("R", "R", "Resistance", "In series.", kResistance),
  V("f", "f", "Frequency", "Of the signal.", kFrequency, "kHz"),
  V("L", "L", "Inductance", "In series.", kInductance, "mH"),
};
constexpr const char *zrlE[] = {"Z = sqrt(R^2 + (2*pi*f*L)^2)"};

constexpr Var resV[] = {
  V("f", "f", "Resonant frequency", "Where X_L equals X_C.", kFrequency, "MHz"),
  V("L", "L", "Inductance", "The coil of the tuned circuit.", kInductance, "uH"),
  V("C", "C", "Capacitance", "The capacitor of the tuned circuit.", kCapacitance, "pF"),
};
constexpr const char *resE[] = {"f = 1/(2*pi*sqrt(L*C))"};

constexpr Var qsV[] = {
  V("Q", "Q", "Q factor", "Sharpness of a resonance. 10 to 100 is typical.", kNone),
  V("R", "R", "Series resistance", "The losses. Smaller gives a higher Q.", kResistance),
  V("L", "L", "Inductance", "Of the tuned circuit.", kInductance, "uH"),
  V("C", "C", "Capacitance", "Of the tuned circuit.", kCapacitance, "pF"),
};
constexpr const char *qsE[] = {"Q = sqrt(L/C)/R"};

constexpr Var qbwV[] = {
  V("Q", "Q", "Q factor", "Sharpness of a resonance.", kNone),
  V("f", "f_{0}", "Centre frequency", "The peak of the resonance.", kFrequency, "MHz"),
  V("BW", "BW", "Bandwidth", "Between the two -3 dB points.", kFrequency, "kHz"),
};
constexpr const char *qbwE[] = {"Q = f/BW"};

constexpr Var dbpV[] = {
  S("G", "G", "Gain", "+3 dB is about double power, +10 dB ten times, -3 dB half.", kDecibel),
  V("P2", "P_{2}", "Power out", "The power being compared.", kPower),
  V("P1", "P_{1}", "Power in", "The reference power.", kPower),
};
constexpr const char *dbpE[] = {"G = 10*log10(P2/P1)"};

constexpr Var dbvV[] = {
  S("G", "G", "Gain", "+6 dB is about double voltage, +20 dB ten times.", kDecibel),
  V("V2", "V_{2}", "Voltage out", "The voltage being compared.", kVoltage),
  V("V1", "V_{1}", "Voltage in", "The reference voltage, into the same impedance.", kVoltage),
};
constexpr const char *dbvE[] = {"G = 20*log10(V2/V1)"};

constexpr Var dbmV[] = {
  S("L", "L", "Level", "0 dBm is 1 mW. A Wi-Fi radio sends about +20 dBm.", kDecibel),
  V("P", "P", "Power", "The actual power.", kPower, "mW"),
  K("P0", "P_{0}", "Reference power", "1 mW for dBm; change to 1 W for dBW.", kPower, 1e-3, "mW"),
};
constexpr const char *dbmE[] = {"L = 10*log10(P/P0)"};

constexpr Var t555V[] = {
  V("f", "f", "Frequency", "Output of the astable.", kFrequency, "kHz"),
  V("D", "D", "Duty cycle", "High time as a share of the period. Always over 50% here.", kRatio, "%"),
  V("R1", "R_{1}", "Resistor R1", "Pin 7 to the supply. Keep it above 1 kohm.", kResistance, "kohm"),
  V("R2", "R_{2}", "Resistor R2", "Pin 7 to pins 2 and 6.", kResistance, "kohm"),
  V("C", "C", "Timing capacitor", "Pins 2 and 6 to ground.", kCapacitance, "uF"),
};
constexpr const char *t555E[] = {
  "f = 1.44/((R1 + 2*R2)*C)", "D = (R1 + R2)/(R1 + 2*R2)",
  "R2 = (1 - D)*1.44/(f*C)", "R1 = (2*D - 1)*1.44/(f*C)", "R1 = R2*(2*D - 1)/(1 - D)",
};

constexpr Var m555V[] = {
  V("t", "t", "Pulse width", "How long the output stays high.", kTime, "ms"),
  V("R", "R", "Timing resistor", "Between the supply and pins 6 and 7.", kResistance, "kohm"),
  V("C", "C", "Timing capacitor", "Pin 6 to ground.", kCapacitance, "uF"),
};
constexpr const char *m555E[] = {"t = 1.1*R*C"};

constexpr Var oaiV[] = {
  S("Vout", "V_{out}", "Output voltage", "Inverted, and limited by the supply rails.", kVoltage),
  S("Vin", "V_{in}", "Input voltage", "Into Rin.", kVoltage),
  V("Rf", "R_{f}", "Feedback resistor", "Output back to the inverting input.", kResistance, "kohm"),
  V("Rin", "R_{in}", "Input resistor", "Sets the input impedance too.", kResistance, "kohm"),
};
constexpr const char *oaiE[] = {"Vout = -Vin*Rf/Rin"};

constexpr Var oanV[] = {
  S("Vout", "V_{out}", "Output voltage", "Limited by the supply rails.", kVoltage),
  S("Vin", "V_{in}", "Input voltage", "On the + input.", kVoltage),
  V("Rf", "R_{f}", "Feedback resistor", "Output to the inverting input.", kResistance, "kohm"),
  V("Rg", "R_{g}", "Ground resistor", "Inverting input to ground.", kResistance, "kohm"),
};
constexpr const char *oanE[] = {"Vout = Vin*(1 + Rf/Rg)"};

constexpr Var fcrV[] = {
  V("fc", "f_{c}", "Cutoff frequency", "The -3 dB point.", kFrequency, "kHz"),
  V("R", "R", "Resistance", "Of the filter.", kResistance, "kohm"),
  V("C", "C", "Capacitance", "Of the filter.", kCapacitance, "nF"),
};
constexpr const char *fcrE[] = {"fc = 1/(2*pi*R*C)"};

constexpr Var lamV[] = {
  V("lambda", "λ", "Wavelength", "One full cycle in space. The 2 m band is 2 m.", kLength),
  V("f", "f", "Frequency", "FM radio is 100 MHz, Wi-Fi 2.4 GHz.", kFrequency, "MHz"),
  K("VF", "VF", "Velocity factor", "Speed along the line as a share of c. 1 in free space.", kRatio, 1, nullptr,
    "Free space=1|Bare wire=0.95|Ladder line=0.91|Twin lead=0.82|Foam coax=0.8|RG-58 coax=0.66"),
};
constexpr const char *lamE[] = {"lambda = VF*c/f"};

constexpr Var dipV[] = {
  V("L", "L", "Total length", "Tip to tip. About 10 m for 14 MHz.", kLength),
  V("f", "f", "Frequency", "Where it should resonate.", kFrequency, "MHz"),
  K("K", "K", "End effect", "How much shorter than a free-space half wave. 0.95 for thin wire.", kRatio, 0.95, nullptr,
    kPresetVelocityFactor),
};
constexpr const char *dipE[] = {"L = K*c/(2*f)"};

constexpr const char *qwE[] = {"L = K*c/(4*f)"};
constexpr Var qwV[] = {
  V("L", "L", "Element length", "Base to tip, over a ground plane.", kLength),
  V("f", "f", "Frequency", "Where it should resonate.", kFrequency, "MHz"),
  K("K", "K", "End effect", "How much shorter than a free-space quarter wave. 0.95 for thin wire.", kRatio, 0.95, nullptr,
    kPresetVelocityFactor),
};

constexpr Var skinV[] = {
  V("delta", "δ", "Skin depth", "RF current stays this close to the surface. Copper at 1 MHz: 65 um.", kLength, "um"),
  K("rho", "ρ", "Resistivity", "Of the conductor. Copper 1.68e-8 ohm-m.", kResistivity, 1.68e-8, nullptr, kPresetResistivity),
  V("f", "f", "Frequency", "Of the current.", kFrequency, "MHz"),
  K("mur", "μ_{r}", "Relative permeability", "1 for copper and aluminium, far more for iron.", kNone, 1, nullptr, kPresetPermeability),
};
constexpr const char *skinE[] = {"delta = sqrt(rho/(pi*f*mu0*mur))"};

constexpr Var fsplV[] = {
  V("L", "L", "Path loss", "Between ideal antennas. 2.4 GHz over 100 m is about 80 dB.", kDecibel),
  V("d", "d", "Distance", "Line of sight.", kLength, "km"),
  V("f", "f", "Frequency", "Of the signal.", kFrequency, "MHz"),
};
constexpr const char *fsplE[] = {"L = 20*log10(4*pi*d*f/c)"};

constexpr Var adcV[] = {
  V("q", "q", "Step size", "One count of the converter.", kVoltage, "mV"),
  V("Vref", "V_{ref}", "Reference voltage", "Full scale. 3.3 V or 5 V.", kVoltage),
  V("n", "n", "Bits", "Resolution. An Arduino has 10, an ESP32 12.", kNone, nullptr, kPositive | kInteger),
};
constexpr const char *adcE[] = {"q = Vref/2^n"};

constexpr Var zenV[] = {
  V("R", "R", "Series resistor", "Between the supply and the zener.", kResistance),
  V("Vin", "V_{in}", "Supply voltage", "Use the lowest it will reach.", kVoltage),
  V("Vz", "V_{z}", "Zener voltage", "3.3, 5.1, 12 V...", kVoltage),
  V("Iz", "I_{z}", "Zener current", "Keep a few mA flowing to hold regulation.", kCurrent, "mA"),
  V("Il", "I_{L}", "Load current", "The largest the load will take.", kCurrent, "mA"),
};
constexpr const char *zenE[] = {"R = (Vin - Vz)/(Iz + Il)"};

constexpr Var ldoV[] = {
  V("P", "P", "Heat in the regulator", "A TO-220 with no heatsink manages about 1 W.", kPower),
  V("Vin", "V_{in}", "Input voltage", "Into the regulator.", kVoltage),
  V("Vout", "V_{out}", "Output voltage", "Regulated output.", kVoltage),
  V("I", "I", "Load current", "Through the regulator.", kCurrent, "mA"),
};
constexpr const char *ldoE[] = {"P = (Vin - Vout)*I"};

constexpr Var tjV[] = {
  V("Tj", "T_{j}", "Junction temperature", "The silicon itself. Keep it under about 125 C.", kTemperature, "C"),
  V("Ta", "T_{a}", "Ambient temperature", "The air around the part. 25 C on the bench.", kTemperature, "C"),
  V("P", "P", "Power", "Heat being dissipated.", kPower),
  V("Rth", "θ_{JA}", "Thermal resistance", "Kelvin of rise per watt. A bare TO-220 is about 60 K/W.", kThermalRes),
};
constexpr const char *tjE[] = {"Tj = Ta + P*Rth"};

constexpr Var buckV[] = {
  V("D", "D", "Duty cycle", "Share of the time the switch is on.", kRatio, "%"),
  V("Vout", "V_{out}", "Output voltage", "Lower than the input.", kVoltage),
  V("Vin", "V_{in}", "Input voltage", "Into the converter.", kVoltage),
};
constexpr const char *buckE[] = {"Vout = Vin*D"};

constexpr Var boostV[] = {
  V("D", "D", "Duty cycle", "Share of the time the switch is on.", kRatio, "%"),
  V("Vout", "V_{out}", "Output voltage", "Higher than the input.", kVoltage),
  V("Vin", "V_{in}", "Input voltage", "Into the converter.", kVoltage),
};
constexpr const char *boostE[] = {"Vout = Vin/(1 - D)"};

constexpr Var ripIV[] = {
  V("dI", "ΔI", "Ripple current", "Peak to peak in the inductor. Aim for 20 to 40% of the load.", kCurrent, "mA"),
  V("Vout", "V_{out}", "Output voltage", "Of the buck converter.", kVoltage),
  V("Vin", "V_{in}", "Input voltage", "Of the buck converter.", kVoltage),
  V("f", "f", "Switching frequency", "100 kHz to 2 MHz.", kFrequency, "kHz"),
  V("L", "L", "Inductance", "Of the output inductor.", kInductance, "uH"),
};
constexpr const char *ripIE[] = {"dI = Vout*(1 - Vout/Vin)/(f*L)"};

constexpr Var ripVV[] = {
  V("Vr", "V_{r}", "Ripple voltage", "Peak to peak on the reservoir capacitor.", kVoltage),
  V("I", "I", "Load current", "Drawn from the capacitor.", kCurrent, "mA"),
  V("f", "f", "Mains frequency", "60 Hz or 50 Hz.", kFrequency),
  V("C", "C", "Reservoir capacitor", "After the bridge. 1000 uF per amp or more.", kCapacitance, "uF"),
};
constexpr const char *ripVE[] = {"Vr = I/(2*f*C)"};

constexpr Formula kElectronicsTable[] = {
  formula("rc-tau", "RC time constant", G2, "tau delay timing",
          "How fast a capacitor charges through a resistor.", rcV, rcE, 2, dRC),
  formula("rl-tau", "RL time constant", G2, "tau coil inductor",
          "How fast current builds in a coil.", rlV, rlE, 2, dRL),
  formula("rc-charge", "RC charging", G2, "capacitor exponential step response",
          "Voltage on a capacitor charging through a resistor from zero.", rcchV, rcchE, 4, dRC),
  formula("rc-discharge", "RC discharge", G2, "capacitor exponential decay bleed",
          "Voltage on a capacitor discharging through a resistor.", rcdisV, rcdisE, 4, dRCDis),
  formula("rl-charge", "RL current rise", G2, "inductor coil exponential step response",
          "Current in a coil after a voltage is switched across it.", rlchV, rlchE, 4, dRL),
  formula("xc", "Capacitive reactance", G2, "capacitor impedance ac xc",
          "A capacitor's opposition to AC. Current leads the voltage by 90 deg.", xcV, xcE, 2, dRCac),
  formula("xl", "Inductive reactance", G2, "inductor coil impedance ac xl",
          "An inductor's opposition to AC. Current lags the voltage by 90 deg.", xlV, xlE, 2, dRLac),
  formula("z-rlc", "Series RLC impedance", G2, "ac reactance z",
          "Total AC opposition of a series R, L and C, from the reactances.", zrlcV, zrlcE, 3, dRLC),
  formula("z-rc", "RC impedance", G2, "series ac z capacitor",
          "AC impedance of a resistor and capacitor in series.", zrcV, zrcE, 3, dRCac),
  formula("z-rl", "RL impedance", G2, "series ac z coil inductor",
          "AC impedance of a resistor and coil in series.", zrlV, zrlE, 3, dRLac),
  formula("resonance", "LC resonance", G2, "tuned circuit tank frequency radio",
          "The frequency a coil and capacitor ring at.", resV, resE, 2, dTank),
  formula("q-series", "Q of a series tuned circuit", G2, "quality factor rlc",
          "How sharp a series LC circuit's resonance is, from its losses.", qsV, qsE, 3, dRLC),
  formula("q-bandwidth", "Q and bandwidth", G2, "quality factor filter 3 db",
          "The width of a resonance from its Q.", qbwV, qbwE, 2, dResCurve),
  formula("db-power", "Decibels from power", G2, "db ratio gain loss",
          "A power ratio on a log scale.", dbpV, dbpE, 2, dAmp),
  formula("db-voltage", "Decibels from voltage", G2, "db ratio gain loss amplifier",
          "A voltage ratio on a log scale, in the same impedance.", dbvV, dbvE, 2, dAmp),
  formula("dbm", "dBm", G2, "db milliwatt rf level power dbw",
          "An absolute power level, in decibels above 1 mW.", dbmV, dbmE, 1, dAcLoad),
  formula("555-astable", "555 timer, astable", G2, "oscillator ne555 duty frequency clock",
          "Frequency and duty cycle of the classic 555 oscillator.", t555V, t555E, 3, d555Astable),
  formula("555-mono", "555 timer, one-shot", G2, "monostable ne555 pulse delay",
          "Pulse length of a 555 one-shot.", m555V, m555E, 2, d555Mono),
  formula("opamp-inv", "Inverting op-amp", G2, "gain amplifier feedback",
          "Gain set by two resistors. The output is flipped.", oaiV, oaiE, 3, dOpInv),
  formula("opamp-noninv", "Non-inverting op-amp", G2, "gain amplifier feedback",
          "Gain set by two resistors; high input impedance, and never below 1.", oanV, oanE, 3, dOpNon),
  formula("rc-filter", "RC filter cutoff", G2, "low pass high pass fc frequency",
          "The -3 dB point of a first-order RC filter, low-pass or high-pass.", fcrV, fcrE, 2, dLowPass),
  formula("wavelength", "Radio wavelength", G2, "frequency meters band velocity factor feedline",
          "Length of one wave in space or along a transmission line.", lamV, lamE, 1, dWave),
  formula("dipole", "Half-wave dipole", G2, "antenna length ham radio wire aerial",
          "Length of a wire dipole. In feet this is the old 468 over f(MHz).", dipV, dipE, 1, dDipole),
  formula("quarter-wave", "Quarter-wave vertical", G2, "antenna length ham radio ground plane whip",
          "Length of a quarter-wave element over a ground plane.", qwV, qwE, 1, dVertical),
  formula("skin-depth", "Skin depth", G2, "rf conductor current surface litz",
          "How deep high-frequency current flows in a conductor.", skinV, skinE, 1, dSkin),
  formula("fspl", "Free-space path loss", G2, "rf link budget radio range",
          "How much signal spreads away between two ideal antennas in the open.", fsplV, fsplE, 2, dLink),
  formula("adc", "ADC step size", G2, "resolution bits lsb converter dac quantization",
          "The voltage of one count of a converter.", adcV, adcE, 2, dAdc),
  formula("zener", "Zener regulator", G2, "shunt reference diode resistor",
          "The series resistor for a simple zener supply.", zenV, zenE, 4, dZener),
  formula("regulator-heat", "Linear regulator heat", G2, "ldo 7805 dissipation power loss",
          "What a linear regulator burns off: the voltage it drops, times the current.", ldoV, ldoE, 3, dConverter),
  formula("junction-temp", "Junction temperature", G2, "heatsink thermal resistance theta",
          "How hot a part runs for the heat it dissipates.", tjV, tjE, 3, dHeatsink),
  formula("buck", "Buck converter", G2, "step down smps switching duty",
          "Ideal output of a step-down switching converter.", buckV, buckE, 2, dConverter),
  formula("boost", "Boost converter", G2, "step up smps switching duty",
          "Ideal output of a step-up switching converter.", boostV, boostE, 2, dConverter),
  formula("buck-ripple", "Buck inductor ripple", G2, "smps switching inductor selection",
          "Peak-to-peak ripple in a buck converter's inductor.", ripIV, ripIE, 4, dConverter),
  formula("rectifier-ripple", "Rectifier ripple", G2, "bridge smoothing reservoir capacitor psu",
          "Ripple on a full-wave rectifier's reservoir capacitor. Use f = mains frequency.",
          ripVV, ripVE, 3, dRectifier),
};

// ---- Fields & magnetism ----

constexpr Var capqV[] = {
  V("Q", "Q", "Charge", "Stored on the plates.", kCharge, "uC"),
  V("C", "C", "Capacitance", "100 uF is a typical supply capacitor.", kCapacitance, "uF"),
  V("V", "V", "Voltage", "Across the capacitor.", kVoltage),
  V("E", "E", "Energy", "Stored in the field. A camera flash holds about 10 J.", kEnergy),
};
constexpr const char *capqE[] = {"Q = C*V", "E = C*V^2/2", "E = Q*V/2", "E = Q^2/(2*C)"};

constexpr Var indeV[] = {
  V("E", "E", "Energy", "Stored in the magnetic field.", kEnergy),
  V("L", "L", "Inductance", "The coil.", kInductance, "mH"),
  V("I", "I", "Current", "Through the coil.", kCurrent),
};
constexpr const char *indeE[] = {"E = L*I^2/2"};

constexpr Var ppV[] = {
  V("C", "C", "Capacitance", "Of the two plates.", kCapacitance, "pF"),
  K("er", "ε_{r}", "Relative permittivity", "Of what is between the plates. Air 1, FR-4 4.4.", kNone, 1, nullptr, kPresetDielectric),
  V("A", "A", "Plate area", "Where the plates overlap.", kArea, "cm^2"),
  V("d", "d", "Gap", "Between the plates.", kLength, "mm"),
};
constexpr const char *ppE[] = {"C = eps0*er*A/d"};

constexpr Var solV[] = {
  V("L", "L", "Inductance", "Of a long air-core coil.", kInductance, "uH"),
  K("mur", "μ_{r}", "Core permeability", "1 for air, thousands for iron.", kNone, 1, nullptr, kPresetPermeability),
  V("N", "N", "Turns", "Number of turns.", kNone, nullptr, kPositive | kInteger),
  V("A", "A", "Coil area", "Cross-section of the coil.", kArea, "cm^2"),
  V("l", "l", "Coil length", "Should be well over the diameter.", kLength, "cm"),
};
constexpr const char *solE[] = {"L = mu0*mur*N^2*A/l"};

constexpr Var solbV[] = {
  V("B", "B", "Field inside", "Strong magnets are about 1 T, the Earth 50 uT.", kMagField, "mT"),
  K("mur", "μ_{r}", "Core permeability", "1 for air, thousands for iron.", kNone, 1, nullptr, kPresetPermeability),
  V("N", "N", "Turns", "Number of turns.", kNone, nullptr, kPositive | kInteger),
  V("I", "I", "Current", "Through the coil.", kCurrent),
  V("l", "l", "Coil length", "End to end.", kLength, "cm"),
};
constexpr const char *solbE[] = {"B = mu0*mur*N*I/l"};

constexpr Var wirebV[] = {
  V("B", "B", "Field", "At distance r from a long straight wire.", kMagField, "uT"),
  V("I", "I", "Current", "In the wire.", kCurrent),
  V("r", "r", "Distance", "From the wire.", kLength, "cm"),
};
constexpr const char *wirebE[] = {"B = mu0*I/(2*pi*r)"};

constexpr Var flV[] = {
  V("F", "F", "Force", "Sideways on the wire: this is what turns a motor.", kForce),
  V("B", "B", "Field", "Crossing the wire at right angles.", kMagField, "mT"),
  V("I", "I", "Current", "In the wire.", kCurrent),
  V("L", "L", "Length in field", "Of the wire.", kLength, "cm"),
};
constexpr const char *flE[] = {"F = B*I*L"};

constexpr Var coulV[] = {
  V("F", "F", "Force", "Between the charges.", kForce),
  V("q1", "q_{1}", "Charge 1", "Size of the first charge.", kCharge, "nC"),
  V("q2", "q_{2}", "Charge 2", "Size of the second charge.", kCharge, "nC"),
  V("r", "r", "Distance", "Between the centres.", kLength, "cm"),
};
constexpr const char *coulE[] = {"F = q1*q2/(4*pi*eps0*r^2)"};

constexpr Var efV[] = {
  V("E", "E", "Field strength", "Air breaks down at about 3 kV/mm.", kElectricField),
  V("V", "V", "Voltage", "Between the plates.", kVoltage),
  V("d", "d", "Gap", "Between the plates.", kLength, "mm"),
};
constexpr const char *efE[] = {"E = V/d"};

constexpr Var emfV[] = {
  S("emf", "ε", "Induced voltage", "Across the coil, while the flux changes.", kVoltage),
  V("N", "N", "Turns", "Number of turns.", kNone, nullptr, kPositive | kInteger),
  S("dPhi", "ΔΦ", "Flux change", "Over the time t.", kMagFlux, "mWb"),
  V("t", "t", "Time", "Of the change.", kTime, "ms"),
};
constexpr const char *emfE[] = {"emf = -N*dPhi/t"};

constexpr Formula kFieldsTable[] = {
  formula("cap-energy", "Capacitor charge and energy", G3, "stored joules coulombs q cv",
          "What a capacitor holds at a given voltage. Any two give the rest.", capqV, capqE, 2, dCapacitor),
  formula("ind-energy", "Inductor energy", G3, "stored coil joules",
          "Energy stored in a coil's magnetic field.", indeV, indeE, 2, dInductor),
  formula("plate-cap", "Parallel-plate capacitor", G3, "capacitance dielectric permittivity pcb",
          "Capacitance from plate area and gap, for a gap small against the plates.", ppV, ppE, 2, dPlates),
  formula("solenoid-l", "Solenoid inductance", G3, "coil air core turns winding",
          "Inductance of a long, thin coil.", solV, solE, 3, dSolenoid),
  formula("solenoid-b", "Solenoid field", G3, "electromagnet coil turns magnetic",
          "Magnetic field inside a long coil.", solbV, solbE, 3, dSolenoid),
  formula("wire-field", "Field round a wire", G3, "magnetic ampere straight conductor",
          "Magnetic field near a long straight current.", wirebV, wirebE, 2, dWireField),
  formula("wire-force", "Force on a wire", G3, "motor magnetic bil lorentz",
          "Sideways force on a current in a magnetic field.", flV, flE, 3, dWireForce),
  formula("coulomb", "Coulomb's law", G3, "electrostatic charge attraction repulsion",
          "Force between two point charges.", coulV, coulE, 3, dCharges),
  formula("e-field", "Electric field between plates", G3, "voltage gradient breakdown",
          "Field strength between two parallel plates.", efV, efE, 2, dPlates),
  formula("faraday", "Faraday's law", G3, "induction emf generator flux coil lenz",
          "A changing flux through a coil induces a voltage, opposing the change.", emfV, emfE, 3, dFaraday),
};

}  // namespace

Part electricalPart() { return partOf(kElectricalTable); }
Part electronicsPart() { return partOf(kElectronicsTable); }
Part fieldsPart() { return partOf(kFieldsTable); }

}  // namespace lib
}  // namespace formulas
}  // namespace mt
