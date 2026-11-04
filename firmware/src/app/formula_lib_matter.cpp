// The library: Materials & structures, Fluids, Heat & gases.
#include "formula_diagrams.h"
#include "formula_lib.h"

namespace mt {
namespace formulas {
namespace lib {
namespace {

using namespace units;
constexpr uint8_t G1 = lib::kMaterials;
constexpr uint8_t G2 = lib::kFluids;
constexpr uint8_t G3 = lib::kHeat;

// ---- Materials & structures ----

constexpr Var densV[] = {
  V("rho", "ρ", "Density", "Water 1000 kg/m^3, steel 7850.", kDensity, nullptr, kPositive, kPresetDensity),
  V("m", "m", "Mass", "Of the piece.", kMass),
  V("V", "V", "Volume", "Of the piece.", kVolume, "L"),
};
constexpr const char *densE[] = {"rho = m/V"};

constexpr Var stressV[] = {
  S("sigma", "σ", "Stress", "Force per area. Mild steel yields near 250 MPa.", kPressure, "MPa"),
  S("F", "F", "Force", "Pulling on the bar.", kForce, "kN"),
  V("A", "A", "Cross-section", "Of the bar.", kArea, "mm^2"),
};
constexpr const char *stressE[] = {"sigma = F/A"};

constexpr Var strainV[] = {
  S("e", "ε", "Strain", "Stretch as a share of length. Steel yields at about 0.1%.", kRatio, "%"),
  S("dL", "ΔL", "Stretch", "Change in length.", kLength, "mm"),
  V("L", "L", "Length", "Before stretching.", kLength),
};
constexpr const char *strainE[] = {"e = dL/L"};

constexpr Var youngV[] = {
  V("E", "E", "Young's modulus", "Stiffness of the material. Steel 200 GPa, aluminium 69 GPa.", kPressure, "MPa",
    kPositive, kPresetYoung),
  V("sigma", "σ", "Stress", "Force per area.", kPressure, "MPa"),
  V("e", "ε", "Strain", "Stretch as a share of length.", kRatio, "%"),
};
constexpr const char *youngE[] = {"E = sigma/e"};

constexpr Var barV[] = {
  V("dL", "ΔL", "Stretch", "How much longer the bar gets.", kLength, "mm"),
  V("F", "F", "Force", "Along the bar.", kForce, "kN"),
  V("L", "L", "Length", "Of the bar.", kLength),
  V("A", "A", "Cross-section", "Of the bar.", kArea, "mm^2"),
  K("E", "E", "Young's modulus", "Stiffness of the material. Steel 200 GPa.", kPressure, 2e11, "MPa", kPresetYoung),
};
constexpr const char *barE[] = {"dL = F*L/(A*E)"};

constexpr Var shearV[] = {
  V("tau", "τ", "Shear stress", "Sideways force per area.", kPressure, "MPa"),
  V("F", "F", "Shear force", "Across the section.", kForce, "kN"),
  V("A", "A", "Area", "Of the section being sheared. A bolt's is its cross-section.", kArea, "mm^2"),
};
constexpr const char *shearE[] = {"tau = F/A"};

constexpr Var fosV[] = {
  V("n", "n", "Factor of safety", "2 for well-known loads, 4 or more if people depend on it.", kNone),
  V("Sy", "S_{y}", "Strength", "Yield strength of the material.", kPressure, "MPa", kPositive, kPresetYield),
  V("sigma", "σ", "Working stress", "The stress it will see.", kPressure, "MPa"),
};
constexpr const char *fosE[] = {"n = Sy/sigma"};

constexpr Var cantV[] = {
  V("d", "δ", "Deflection", "At the tip.", kLength, "mm"),
  V("F", "F", "End load", "At the tip.", kForce),
  V("L", "L", "Length", "From the wall to the tip.", kLength),
  K("E", "E", "Young's modulus", "Stiffness of the material. Steel 200 GPa.", kPressure, 2e11, "MPa", kPresetYoung),
  V("I", "I", "Second moment of area", "Stiffness of the shape. See Rectangle section.", kAreaMoment, "mm^4"),
};
constexpr const char *cantE[] = {"d = F*L^3/(3*E*I)"};
constexpr const char *cantUE[] = {"d = F*L^3/(8*E*I)"};

constexpr Var ssV[] = {
  V("d", "δ", "Deflection", "At the middle.", kLength, "mm"),
  V("F", "F", "Load", "Total load on the beam.", kForce),
  V("L", "L", "Span", "Between the supports.", kLength),
  K("E", "E", "Young's modulus", "Stiffness of the material. Steel 200 GPa.", kPressure, 2e11, "MPa", kPresetYoung),
  V("I", "I", "Second moment of area", "Stiffness of the shape. See Rectangle section.", kAreaMoment, "mm^4"),
};
constexpr const char *ssE[] = {"d = F*L^3/(48*E*I)"};
constexpr const char *ssUE[] = {"d = 5*F*L^3/(384*E*I)"};

constexpr Var bendV[] = {
  V("sigma", "σ", "Bending stress", "At the surface.", kPressure, "MPa"),
  V("M", "M", "Bending moment", "At that point of the beam.", kTorque),
  V("y", "y", "Distance from centre", "To the surface. Half the depth for a symmetric beam.", kLength, "mm"),
  V("I", "I", "Second moment of area", "Of the section.", kAreaMoment, "mm^4"),
};
constexpr const char *bendE[] = {"sigma = M*y/I"};

constexpr Var irectV[] = {
  V("I", "I", "Second moment of area", "About the middle, bending the depth.", kAreaMoment, "mm^4"),
  V("b", "b", "Width", "Across the section.", kLength, "mm"),
  V("h", "h", "Depth", "In the direction of the load. It counts as the cube.", kLength, "mm"),
};
constexpr const char *irectE[] = {"I = b*h^3/12"};

constexpr Var icircV[] = {
  V("I", "I", "Second moment of area", "About a diameter.", kAreaMoment, "mm^4"),
  V("D", "D", "Outside diameter", "Of the tube or bar.", kLength, "mm"),
  V("d", "d", "Inside diameter", "Of the tube. Make it small for a solid bar.", kLength, "mm"),
};
constexpr const char *icircE[] = {"I = pi*(D^4 - d^4)/64"};

constexpr Var twistV[] = {
  V("phi", "φ", "Twist", "End to end.", kAngle, "deg"),
  V("T", "T", "Torque", "In the shaft.", kTorque),
  V("L", "L", "Length", "Of the shaft.", kLength),
  K("G", "G", "Shear modulus", "Steel 79 GPa, aluminium 26 GPa.", kPressure, 7.9e10, "MPa", kPresetShear),
  V("d", "d", "Diameter", "Of a solid round shaft.", kLength, "mm"),
};
constexpr const char *twistE[] = {"phi = 32*T*L/(pi*G*d^4)"};

constexpr Var eulerV[] = {
  V("F", "F", "Buckling load", "Where a slim column gives way. Both ends pinned.", kForce, "kN"),
  K("E", "E", "Young's modulus", "Stiffness of the material. Steel 200 GPa.", kPressure, 2e11, "MPa", kPresetYoung),
  V("I", "I", "Second moment of area", "The weakest way round.", kAreaMoment, "mm^4"),
  V("L", "L", "Length", "Between the pins.", kLength),
};
constexpr const char *eulerE[] = {"F = pi^2*E*I/L^2"};

constexpr Var expV[] = {
  S("dL", "ΔL", "Change in length", "A 10 m steel rail grows 1.2 mm per 10 C.", kLength, "mm"),
  K("alpha", "α", "Expansion coefficient", "Per kelvin. Steel 12 ppm/K, aluminium 23.", kExpansion, 1.2e-5, "ppm/K", kPresetExpansion),
  V("L", "L", "Length", "Before the change.", kLength),
  S("dT", "ΔT", "Temperature change", "How much it warms.", kTempDiff, "C deg"),
};
constexpr const char *expE[] = {"dL = alpha*L*dT"};

constexpr Var hoopV[] = {
  V("sigma", "σ", "Hoop stress", "Round the wall.", kPressure, "MPa"),
  V("p", "p", "Pressure", "Inside, above outside.", kPressure, "bar"),
  V("r", "r", "Radius", "Of the tank.", kLength, "mm"),
  V("t", "t", "Wall thickness", "Thin compared to the radius.", kLength, "mm"),
};
constexpr const char *hoopE[] = {"sigma = p*r/t"};

constexpr Var coilV[] = {
  V("k", "k", "Spring rate", "Force per deflection.", kStiffness, "N/mm"),
  K("G", "G", "Shear modulus", "Spring steel 79 GPa.", kPressure, 7.9e10, "MPa", kPresetShear),
  V("d", "d", "Wire diameter", "Of the spring wire.", kLength, "mm"),
  V("D", "D", "Coil diameter", "To the middle of the wire.", kLength, "mm"),
  V("n", "n", "Active coils", "Those free to flex.", kNone),
};
constexpr const char *coilE[] = {"k = G*d^4/(8*D^3*n)"};

constexpr Formula kMaterialsTable[] = {
  formula("density", "Density", G1, "mass volume weight material specific gravity",
          "Mass per volume.", densV, densE, 2, dScale),
  formula("stress", "Stress", G1, "tensile compressive force area sigma pressure",
          "Force spread over a section.", stressV, stressE, 2, dBarPull),
  formula("strain", "Strain", G1, "stretch elongation percent",
          "Stretch as a share of the length.", strainV, strainE, 2, dBarPull),
  formula("youngs", "Young's modulus", G1, "elastic stiffness hooke modulus of elasticity",
          "How stress and strain relate in the elastic range.", youngV, youngE, 2, dBarPull),
  formula("bar-stretch", "Bar stretch", G1, "elongation axial tension rod cable",
          "How much a bar stretches under a load.", barV, barE, 3, dBarPull),
  formula("shear", "Shear stress", G1, "bolt pin rivet tau",
          "Sideways force spread over a section.", shearV, shearE, 2, dShear),
  formula("safety-factor", "Factor of safety", G1, "fos margin yield strength design",
          "How far below the material's limit a part works.", fosV, fosE, 2, dBarPull),
  formula("cantilever", "Cantilever, end load", G1, "beam deflection bend shelf",
          "A beam fixed at one end, with a load at the other.", cantV, cantE, 3, dCantilever),
  formula("cantilever-udl", "Cantilever, even load", G1, "beam deflection udl distributed",
          "A beam fixed at one end, with the total load F spread along it.", cantV, cantUE, 3, dCantileverU),
  formula("beam-centre", "Beam, centre load", G1, "simply supported deflection bending",
          "A beam on two supports, with a load in the middle.", ssV, ssE, 3, dBeam),
  formula("beam-udl", "Beam, even load", G1, "simply supported deflection udl distributed",
          "A beam on two supports, with the total load F spread along it.", ssV, ssUE, 3, dBeamU),
  formula("bending-stress", "Bending stress", G1, "beam flexure my/i",
          "The stress bending puts in the surface of a beam.", bendV, bendE, 3, dSectionRect),
  formula("i-rect", "Rectangle section", G1, "second moment of area beam stiffness joist",
          "Second moment of area of a rectangle, for bending.", irectV, irectE, 2, dSectionRect),
  formula("i-tube", "Tube section", G1, "second moment of area pipe round bar",
          "Second moment of area of a round tube, for bending.", icircV, icircE, 2, dSectionTube),
  formula("shaft-twist", "Shaft twist", G1, "torsion angle torsional",
          "How far a solid round shaft winds up under torque.", twistV, twistE, 3, dTorsion),
  formula("buckling", "Column buckling", G1, "euler strut compression slender",
          "The load at which a slim column with pinned ends bows out.", eulerV, eulerE, 2, dColumn),
  formula("expansion", "Thermal expansion", G1, "linear growth temperature rail gap",
          "How much things grow as they warm.", expV, expE, 2, dExpansion),
  formula("hoop-stress", "Hoop stress", G1, "pressure vessel tank pipe wall thin",
          "Stress in the wall of a thin cylindrical tank.", hoopV, hoopE, 3, dVessel),
  formula("coil-spring", "Coil spring rate", G1, "compression helical wire design",
          "The rate of a coil spring from its wire and size.", coilV, coilE, 3, dCoilSpring),
};

// ---- Fluids ----

constexpr Var presV[] = {
  V("p", "p", "Pressure", "Force per area. Air at sea level is 101 kPa, a car tyre 2.2 bar.", kPressure, "kPa"),
  V("F", "F", "Force", "Square to the surface.", kForce),
  V("A", "A", "Area", "It acts on.", kArea, "cm^2"),
};
constexpr const char *presE[] = {"p = F/A"};

constexpr Var hydroV[] = {
  V("p", "p", "Pressure", "From the fluid above: gauge pressure.", kPressure, "kPa"),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  V("h", "h", "Depth", "Below the surface.", kLength),
  kVarG,
};
constexpr const char *hydroE[] = {"p = rho*g*h"};

constexpr Var buoyV[] = {
  V("F", "F", "Buoyant force", "Upward, equal to the weight of fluid pushed aside.", kForce),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  V("V", "V", "Displaced volume", "Of the part underwater.", kVolume, "L"),
  kVarG,
};
constexpr const char *buoyE[] = {"F = rho*g*V"};

constexpr Var flowV[] = {
  V("Q", "Q", "Flow rate", "A garden hose gives about 20 L/min.", kFlow, "L/min"),
  V("A", "A", "Pipe area", "Inside the pipe.", kArea, "cm^2"),
  V("v", "v", "Flow speed", "Water in a pipe runs at 1 to 3 m/s.", kSpeed),
};
constexpr const char *flowE[] = {"Q = A*v"};

constexpr Var pipeV[] = {
  V("Q", "Q", "Flow rate", "Through a round pipe.", kFlow, "L/min"),
  V("d", "d", "Pipe bore", "Inside diameter.", kLength, "mm"),
  V("v", "v", "Flow speed", "Water in a pipe runs at 1 to 3 m/s.", kSpeed),
};
constexpr const char *pipeE[] = {"Q = pi*d^2/4*v"};

constexpr Var contV[] = {
  V("A1", "A_{1}", "Area 1", "Of the pipe at point 1.", kArea, "cm^2"),
  V("v1", "v_{1}", "Speed 1", "At point 1.", kSpeed),
  V("A2", "A_{2}", "Area 2", "Of the pipe at point 2.", kArea, "cm^2"),
  V("v2", "v_{2}", "Speed 2", "At point 2.", kSpeed),
};
constexpr const char *contE[] = {"A1*v1 = A2*v2"};

constexpr Var bernV[] = {
  S("p1", "p_{1}", "Pressure 1", "At point 1.", kPressure, "kPa"),
  S("v1", "v_{1}", "Speed 1", "At point 1.", kSpeed),
  S("h1", "h_{1}", "Height 1", "Of point 1.", kLength),
  S("p2", "p_{2}", "Pressure 2", "At point 2.", kPressure, "kPa"),
  S("v2", "v_{2}", "Speed 2", "At point 2.", kSpeed),
  S("h2", "h_{2}", "Height 2", "Of point 2.", kLength),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  kVarG,
};
constexpr const char *bernE[] = {"p1 + rho*v1^2/2 + rho*g*h1 = p2 + rho*v2^2/2 + rho*g*h2"};

constexpr Var torrV[] = {
  V("v", "v", "Jet speed", "Out of the hole.", kSpeed),
  V("h", "h", "Head", "Height of the surface above the hole.", kLength),
  kVarG,
};
constexpr const char *torrE[] = {"v = sqrt(2*g*h)"};

constexpr Var reV[] = {
  V("Re", "Re", "Reynolds number", "Pipe flow is smooth below about 2300 and turbulent above 4000.", kNone),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  V("v", "v", "Flow speed", "Mean speed.", kSpeed),
  V("d", "d", "Pipe bore", "Or the length of the object.", kLength, "mm"),
  K("mu", "μ", "Viscosity", "Water 1 mPa-s, air 0.018, light oil about 300.", kViscosity, 1.002e-3, "mPa-s", kPresetViscosity),
};
constexpr const char *reE[] = {"Re = rho*v*d/mu"};

constexpr Var hpV[] = {
  V("dp", "Δp", "Pressure drop", "Along the pipe.", kPressure, "kPa"),
  K("mu", "μ", "Viscosity", "Water 1 mPa-s.", kViscosity, 1.002e-3, "mPa-s", kPresetViscosity),
  V("L", "L", "Pipe length", "End to end.", kLength),
  V("Q", "Q", "Flow rate", "Through the pipe.", kFlow, "L/min"),
  V("d", "d", "Pipe bore", "Counts as the fourth power.", kLength, "mm"),
};
constexpr const char *hpE[] = {"dp = 128*mu*L*Q/(pi*d^4)"};

constexpr Var dwV[] = {
  V("dp", "Δp", "Pressure drop", "Along the pipe.", kPressure, "kPa"),
  V("f", "f", "Friction factor", "Darcy's: about 0.02 in turbulent flow.", kNone),
  V("L", "L", "Pipe length", "End to end.", kLength),
  V("d", "d", "Pipe bore", "Inside diameter.", kLength, "mm"),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  V("v", "v", "Flow speed", "Mean speed.", kSpeed),
};
constexpr const char *dwE[] = {"dp = f*L/d*rho*v^2/2"};

constexpr Var hydrV[] = {
  V("F1", "F_{1}", "Input force", "On the small piston.", kForce),
  V("A1", "A_{1}", "Input area", "Small piston.", kArea, "cm^2"),
  V("F2", "F_{2}", "Output force", "From the large piston.", kForce),
  V("A2", "A_{2}", "Output area", "Large piston.", kArea, "cm^2"),
};
constexpr const char *hydrE[] = {"F1/A1 = F2/A2"};

constexpr Var cylV[] = {
  V("F", "F", "Rod force", "Pushing out.", kForce, "kN"),
  V("p", "p", "Pressure", "Shop air 6 bar, hydraulics 200 bar.", kPressure, "bar"),
  V("d", "d", "Bore", "Inside the cylinder.", kLength, "mm"),
};
constexpr const char *cylE[] = {"F = p*pi*d^2/4"};

constexpr Var dragV[] = {
  V("F", "F", "Drag force", "Opposing motion through a fluid.", kForce),
  K("rho", "ρ", "Fluid density", "Air 1.2 kg/m^3.", kDensity, 1.204, nullptr, kPresetFluidDensity),
  V("v", "v", "Speed", "Through the fluid.", kSpeed, "km/h"),
  V("Cd", "C_{d}", "Drag coefficient", "A car 0.3, a sphere 0.47, a cyclist 0.9.", kNone, nullptr, kPositive,
    "Streamlined=0.04|Car=0.3|Sphere=0.47|Cube=1.05|Cyclist=0.9|Flat plate=1.28"),
  V("A", "A", "Frontal area", "Seen from the front. A car is about 2.2 m^2.", kArea),
};
constexpr const char *dragE[] = {"F = rho*v^2*Cd*A/2"};

constexpr Var termV[] = {
  V("v", "v", "Terminal speed", "When drag equals weight. A skydiver: 55 m/s.", kSpeed),
  V("m", "m", "Mass", "Of the falling thing.", kMass),
  K("rho", "ρ", "Air density", "1.2 kg/m^3 at sea level.", kDensity, 1.204, nullptr, kPresetFluidDensity),
  V("Cd", "C_{d}", "Drag coefficient", "Sphere 0.47, skydiver 1.0.", kNone),
  V("A", "A", "Frontal area", "A skydiver is about 0.7 m^2.", kArea),
  kVarG,
};
constexpr const char *termE[] = {"v = sqrt(2*m*g/(rho*Cd*A))"};

constexpr Var pumpV[] = {
  V("P", "P", "Shaft power", "Into the pump.", kPower),
  K("rho", "ρ", "Fluid density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  V("Q", "Q", "Flow rate", "Delivered.", kFlow, "L/min"),
  V("H", "H", "Head", "Height the pump lifts to.", kLength),
  K("eta", "η", "Efficiency", "Of the pump. 0.6 to 0.8.", kRatio, 0.7, "%"),
  kVarG,
};
constexpr const char *pumpE[] = {"P = rho*g*Q*H/eta"};

constexpr Formula kFluidsTable[] = {
  formula("pressure", "Pressure", G2, "force area psi bar pascal",
          "Force spread over an area.", presV, presE, 2, dRam),
  formula("hydrostatic", "Hydrostatic pressure", G2, "depth water column head diver tank",
          "Pressure from the weight of the fluid above.", hydroV, hydroE, 1, dDepth),
  formula("buoyancy", "Buoyancy", G2, "archimedes float displaced upthrust",
          "The upward push on anything in a fluid.", buoyV, buoyE, 1, dFloat),
  formula("flow", "Flow rate", G2, "pipe volume speed discharge",
          "Flow is area times speed.", flowV, flowE, 2, dPipe),
  formula("pipe-flow", "Flow in a round pipe", G2, "bore diameter speed sizing",
          "Flow from a pipe's bore and the speed in it.", pipeV, pipeE, 2, dPipe),
  formula("continuity", "Continuity", G2, "pipe narrowing nozzle speed area",
          "What goes in must come out: narrow means fast.", contV, contE, 3, dVenturi),
  formula("bernoulli", "Bernoulli's equation", G2, "pressure speed height energy fluid",
          "Energy along a streamline of steady, frictionless flow.", bernV, bernE, 5, dBernoulli),
  formula("torricelli", "Draining tank", G2, "torricelli jet hole outflow",
          "Speed of a jet out of a hole.", torrV, torrE, 1, dDepth),
  formula("reynolds", "Reynolds number", G2, "laminar turbulent flow regime",
          "Tells smooth flow from turbulent.", reV, reE, 2, dPipe),
  formula("poiseuille", "Hagen-Poiseuille", G2, "laminar viscous pipe pressure drop tube",
          "Pressure lost in smooth flow in a round pipe.", hpV, hpE, 3, dPipeDrop),
  formula("darcy", "Darcy-Weisbach", G2, "turbulent friction factor pipe pressure drop head loss",
          "Pressure lost to friction in pipe flow of any kind.", dwV, dwE, 4, dPipeDrop),
  formula("hydraulic-press", "Hydraulic press", G2, "jack pascal force multiplier piston",
          "Pressure is shared, so a big piston gets a big force.", hydrV, hydrE, 3, dPress),
  formula("cylinder-force", "Cylinder force", G2, "pneumatic hydraulic ram actuator air",
          "Force of a ram from pressure and bore.", cylV, cylE, 2, dRam),
  formula("drag", "Drag", G2, "air resistance aerodynamic wind cd",
          "Force of moving through a fluid at speed.", dragV, dragE, 3, dDrag),
  formula("terminal", "Terminal velocity", G2, "skydiver falling drag air",
          "The steady speed of a falling body.", termV, termE, 3, dTerminal),
  formula("pump-power", "Pump power", G2, "head flow motor sizing water",
          "Shaft power to lift a flow.", pumpV, pumpE, 2, dPump),
};

// ---- Heat & gases ----

constexpr Var gasV[] = {
  V("p", "p", "Pressure", "Absolute. The atmosphere is 101 kPa.", kPressure, "kPa"),
  V("V", "V", "Volume", "Of the gas.", kVolume, "L"),
  V("n", "n", "Amount", "A mole at room conditions fills about 24 L.", kAmount),
  V("T", "T", "Temperature", "Absolute: enter C or F freely, it uses kelvin.", kTemperature, "C"),
};
constexpr const char *gasE[] = {"p*V = n*Rgas*T"};

constexpr Var combV[] = {
  V("p1", "p_{1}", "Pressure before", "Absolute.", kPressure, "kPa"),
  V("V1", "V_{1}", "Volume before", "Of the gas.", kVolume, "L"),
  V("T1", "T_{1}", "Temperature before", "Absolute temperature.", kTemperature, "C"),
  V("p2", "p_{2}", "Pressure after", "Absolute.", kPressure, "kPa"),
  V("V2", "V_{2}", "Volume after", "Of the gas.", kVolume, "L"),
  V("T2", "T_{2}", "Temperature after", "Absolute temperature.", kTemperature, "C"),
};
constexpr const char *combE[] = {"p1*V1/T1 = p2*V2/T2"};

constexpr Var boyleV[] = {
  V("p1", "p_{1}", "Pressure before", "Absolute.", kPressure, "kPa"),
  V("V1", "V_{1}", "Volume before", "Of the gas.", kVolume, "L"),
  V("p2", "p_{2}", "Pressure after", "Absolute.", kPressure, "kPa"),
  V("V2", "V_{2}", "Volume after", "Of the gas.", kVolume, "L"),
};
constexpr const char *boyleE[] = {"p1*V1 = p2*V2"};

constexpr Var heatV[] = {
  S("Q", "Q", "Heat", "Taking 1 L of water from 20 to 100 C needs 335 kJ.", kEnergy, "kJ"),
  V("m", "m", "Mass", "What is heated.", kMass),
  K("c", "c", "Specific heat", "Water 4186 J/(kg-K), aluminium 897.", kSpecificHeat, 4186, nullptr, kPresetSpecificHeat),
  S("dT", "ΔT", "Temperature rise", "Difference, so 10 C of rise is 10 K.", kTempDiff, "C deg"),
};
constexpr const char *heatE[] = {"Q = m*c*dT"};

constexpr Var latV[] = {
  V("Q", "Q", "Heat", "To melt or boil it, with no change in temperature.", kEnergy, "kJ"),
  V("m", "m", "Mass", "What changes state.", kMass),
  K("L", "L", "Latent heat", "Water: 334 kJ/kg melting, 2257 kJ/kg boiling.", kSpecificEnergy, 2.257e6, "kJ/kg", kPresetLatent),
};
constexpr const char *latE[] = {"Q = m*L"};

constexpr Var flowhV[] = {
  V("P", "P", "Heat rate", "Carried by the flow.", kPower, "kW"),
  V("Q", "Q", "Flow rate", "Of the fluid.", kFlow, "L/min"),
  K("rho", "ρ", "Density", "Water 998 kg/m^3.", kDensity, 998, nullptr, kPresetFluidDensity),
  K("c", "c", "Specific heat", "Water 4186 J/(kg-K).", kSpecificHeat, 4186, nullptr, kPresetSpecificHeat),
  V("dT", "ΔT", "Temperature rise", "From inlet to outlet.", kTempDiff, "C deg"),
};
constexpr const char *flowhE[] = {"P = rho*Q*c*dT"};

constexpr Var condV[] = {
  V("P", "P", "Heat flow", "Through the wall, in watts.", kPower),
  K("k", "k", "Thermal conductivity", "Copper 401, glass 1, foam 0.03 W/(m-K).", kThermalCond, 1.0, nullptr, kPresetConductivity),
  V("A", "A", "Area", "Of the wall.", kArea),
  V("dT", "ΔT", "Temperature difference", "From one face to the other.", kTempDiff, "C deg"),
  V("d", "d", "Thickness", "Of the wall.", kLength, "mm"),
};
constexpr const char *condE[] = {"P = k*A*dT/d"};

constexpr Var convV[] = {
  V("P", "P", "Heat flow", "Into the moving fluid.", kPower),
  V("h", "h", "Film coefficient", "Still air 10, moving air 50, water 1000 W/(m^2-K).", kHeatTransfer, nullptr, kPositive,
    "Still air=10|Moving air=50|Still water=500|Moving water=3000"),
  V("A", "A", "Area", "Of the surface.", kArea),
  V("dT", "ΔT", "Temperature difference", "Between surface and fluid.", kTempDiff, "C deg"),
};
constexpr const char *convE[] = {"P = h*A*dT"};

constexpr Var radV[] = {
  V("P", "P", "Net radiated heat", "From the hot surface to its cooler surroundings.", kPower),
  K("eps", "ε", "Emissivity", "1 for a perfect radiator; polished metal 0.05.", kNone, 0.9, nullptr, kPresetEmissivity),
  V("A", "A", "Area", "Of the surface.", kArea),
  V("T", "T", "Surface temperature", "Of the hot body.", kTemperature, "C"),
  V("Ts", "T_{s}", "Surroundings", "What it sees around it.", kTemperature, "C"),
};
constexpr const char *radE[] = {"P = eps*sigma*A*(T^4 - Ts^4)"};

constexpr Var carnotV[] = {
  V("eta", "η", "Best efficiency", "Of any heat engine between those temperatures.", kRatio, "%"),
  V("Th", "T_{h}", "Hot temperature", "Of the source. Must be absolute; enter in C or F freely.", kTemperature, "C"),
  V("Tc", "T_{c}", "Cold temperature", "Of the sink.", kTemperature, "C"),
};
constexpr const char *carnotE[] = {"eta = 1 - Tc/Th"};

constexpr Var coolV[] = {
  V("T", "T", "Temperature now", "At time t.", kTemperature, "C"),
  V("Ta", "T_{a}", "Surroundings", "Room temperature.", kTemperature, "C"),
  V("T0", "T_{0}", "Start temperature", "At time 0.", kTemperature, "C"),
  V("k", "k", "Cooling rate", "Per second. A coffee cup is about 0.0005.", kFrequency),
  V("t", "t", "Time", "Elapsed.", kTime, "min"),
};
constexpr const char *coolE[] = {"T = Ta + (T0 - Ta)*exp(-k*t)"};

constexpr Formula kHeatTable[] = {
  formula("ideal-gas", "Ideal gas law", G3, "pv nrt moles pressure volume temperature",
          "How pressure, volume, amount and temperature tie together.", gasV, gasE, 3, dGas),
  formula("combined-gas", "Combined gas law", G3, "charles gay lussac before after",
          "A fixed amount of gas, before and after a change.", combV, combE, 5, dGas2),
  formula("boyle", "Boyle's law", G3, "pressure volume squeeze isothermal",
          "Squeeze a gas at steady temperature and the pressure rises.", boyleV, boyleE, 3, dGas2),
  formula("heat", "Heat capacity", G3, "specific heat energy mcdt warming kettle",
          "The heat to warm something up.", heatV, heatE, 2, dHeating),
  formula("latent", "Latent heat", G3, "melting boiling evaporation fusion vaporisation",
          "The heat to melt or boil something, at steady temperature.", latV, latE, 1, dHeating),
  formula("flow-heat", "Heat in a flow", G3, "water loop radiator cooling hvac",
          "Heat carried by a stream of fluid.", flowhV, flowhE, 2, dHeatedPipe),
  formula("conduction", "Conduction", G3, "fourier insulation wall heat loss",
          "Heat through a solid wall.", condV, condE, 3, dConduction),
  formula("convection", "Convection", G3, "newton heat transfer air cooling heatsink",
          "Heat from a surface to a moving fluid.", convV, convE, 3, dConvection),
  formula("radiation", "Thermal radiation", G3, "stefan boltzmann infrared blackbody emissivity",
          "Heat radiated from a hot surface to cooler surroundings.", radV, radE, 3, dRadiation),
  formula("carnot", "Carnot efficiency", G3, "heat engine limit second law thermodynamics",
          "The best any heat engine can do between two temperatures.", carnotV, carnotE, 2, dEngine),
  formula("newton-cooling", "Newton's cooling", G3, "coffee temperature decay exponential",
          "Things cool quickly at first, then slowly towards the room.", coolV, coolE, 4, dCooling),
};

}  // namespace

Part materialsPart() { return partOf(kMaterialsTable); }
Part fluidsPart() { return partOf(kFluidsTable); }
Part heatPart() { return partOf(kHeatTable); }

}  // namespace lib
}  // namespace formulas
}  // namespace mt
