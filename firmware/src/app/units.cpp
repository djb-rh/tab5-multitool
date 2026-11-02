#include "units.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mt {
namespace units {
namespace {

constexpr uint8_t C = kCommon;
constexpr uint8_t A = kApprox;

constexpr Unit L(const char *n, const char *s, const char *a, double f, uint8_t fl = 0) {
  return Unit{n, s, a, f, 0, Kind::Linear, fl};
}

// Exact definitions the tables build on (international yard and pound of 1959, US
// gallon = 231 in^3, imperial gallon = 4.54609 L, standard gravity, IT BTU).
constexpr double kPi = 3.14159265358979323846;
constexpr double kIn = 0.0254;
constexpr double kFt = 0.3048;
constexpr double kMi = 1609.344;
constexpr double kLb = 0.45359237;
constexpr double kOz = kLb / 16;
constexpr double kG0 = 9.80665;
constexpr double kLbf = 4.4482216152605;        // kLb * kG0
constexpr double kFtLbf = 1.3558179483314004;   // kFt * kLbf
constexpr double kIn3L = 0.016387064;           // litres per cubic inch
constexpr double kFt3L = 28.316846592;          // litres per cubic foot
constexpr double kGalL = 3.785411784;           // US gallon, litres
constexpr double kUkGalL = 4.54609;
constexpr double kBtu = 1055.05585262;          // joules
constexpr double kMmHg = 133.322387415;         // conventional mmHg (13.5951 g/cm^3 at g0)

// Each table's first comment line names its base unit: the unit with factor 1.

constexpr Unit kLengthU[] = {
  // base: metres
  L("Nanometers", "nm", "nanometer|nanometers|nanometre|nanometres", 1e-9),
  L("Micrometers", "um", "µm|μm|micron|microns|micrometer|micrometers|micrometre|micrometres", 1e-6),
  L("Millimeters", "mm", "millimeter|millimeters|millimetre|millimetres", 1e-3, C),
  L("Centimeters", "cm", "centimeter|centimeters|centimetre|centimetres", 1e-2, C),
  L("Meters", "m", "meter|meters|metre|metres", 1, C),
  L("Kilometers", "km", "kilometer|kilometers|kilometre|kilometres|kms", 1e3, C),
  L("Mils (thou)", "mil", "mils|thou|thous", kIn / 1000),
  L("Inches", "in", "inch|inches|\"|''|″|”", kIn, C),
  L("Feet", "ft", "foot|feet|'|′|’", kFt, C),
  L("Yards", "yd", "yard|yards|yds", 0.9144, C),
  L("Miles", "mi", "mile|miles", kMi, C),
  L("Nautical miles", "nmi", "NM|nautical mile|nautical miles", 1852),
};

constexpr Unit kAreaU[] = {
  // base: square metres
  L("Square millimeters", "mm^2", "mm2|mm²|sq mm|square millimeter|square millimeters|square millimetre|"
    "square millimetres", 1e-6),
  L("Square centimeters", "cm^2", "cm2|cm²|sq cm|square centimeter|square centimeters|square centimetre|"
    "square centimetres", 1e-4, C),
  L("Square meters", "m^2", "m2|m²|sq m|sqm|square meter|square meters|square metre|square metres", 1, C),
  L("Hectares", "ha", "hectare|hectares", 1e4, C),
  L("Square kilometers", "km^2", "km2|km²|sq km|square kilometer|square kilometers|square kilometre|"
    "square kilometres", 1e6, C),
  L("Square inches", "in^2", "in2|in²|sq in|sqin|square inch|square inches", kIn * kIn, C),
  L("Square feet", "ft^2", "ft2|ft²|sq ft|sqft|square foot|square feet", kFt * kFt, C),
  L("Square yards", "yd^2", "yd2|yd²|sq yd|sqyd|square yard|square yards", 0.83612736),
  L("Acres", "ac", "acre|acres", 4046.8564224, C),
  L("Square miles", "mi^2", "mi2|mi²|sq mi|sqmi|square mile|square miles", kMi * kMi, C),
  // 20 ft x 44 ft, by the rules: exact.
  L("Pickleball courts", "courts", "court|pickleball court|pickleball courts", 880 * kFt * kFt),
};

constexpr Unit kVolumeU[] = {
  // base: litres
  L("Milliliters", "mL", "ml|milliliter|milliliters|millilitre|millilitres", 1e-3, C),
  L("Cubic centimeters", "cm^3", "cm3|cm³|cc|cubic centimeter|cubic centimeters|cubic centimetre|"
    "cubic centimetres", 1e-3),
  L("Deciliters", "dL", "dl|deciliter|deciliters|decilitre|decilitres", 0.1),
  L("Liters", "L", "l|liter|liters|litre|litres|ltr", 1, C),
  L("Cubic meters", "m^3", "m3|m³|cubic meter|cubic meters|cubic metre|cubic metres", 1000, C),
  // "t" and "T" are the recipe shorthands; weak so "2 t" alone stays tonnes.
  L("Teaspoons", "tsp", "tsps|teaspoon|teaspoons|~t", kGalL / 768, C),
  L("Tablespoons", "tbsp", "tbsps|tbs|tbl|tablespoon|tablespoons|~T", kGalL / 256, C),
  L("US fluid ounces", "fl oz", "fl. oz|fl.oz|us fl oz|fluid ounce|fluid ounces|~oz|~ozs|~ounce|~ounces",
    kGalL / 128, C),
  L("US cups", "cup", "cups|c|us cup|us cups", kGalL / 16, C),
  L("US pints", "pt", "pint|pints|us pt|us pint|us pints", kGalL / 8, C),
  L("US quarts", "qt", "quart|quarts|us qt|us quart|us quarts", kGalL / 4, C),
  L("US gallons", "gal", "gals|gallon|gallons|us gal|us gallon|us gallons", kGalL, C),
  L("UK fluid ounces", "UK fl oz", "imp fl oz|uk fluid ounce|uk fluid ounces|imperial fluid ounce|"
    "imperial fluid ounces", kUkGalL / 160),
  L("UK pints", "UK pt", "imp pt|uk pint|uk pints|imperial pint|imperial pints", kUkGalL / 8),
  L("UK quarts", "UK qt", "imp qt|uk quart|uk quarts|imperial quart|imperial quarts", kUkGalL / 4),
  L("UK gallons", "UK gal", "imp gal|uk gallon|uk gallons|imperial gallon|imperial gallons", kUkGalL),
  L("Cubic inches", "in^3", "in3|in³|cu in|cubic inch|cubic inches", kIn3L, C),
  L("Cubic feet", "ft^3", "ft3|ft³|cu ft|cuft|cubic foot|cubic feet", kFt3L, C),
  L("Cubic yards", "yd^3", "yd3|yd³|cu yd|cubic yard|cubic yards", 27 * kFt3L),
  L("Barrels (oil)", "bbl", "barrel|barrels|bbls", 42 * kGalL),
};

constexpr Unit kMassU[] = {
  // base: kilograms
  L("Milligrams", "mg", "milligram|milligrams|milligramme|milligrammes", 1e-6),
  L("Grams", "g", "gm|gms|gram|grams|gramme|grammes", 1e-3, C),
  L("Kilograms", "kg", "kgs|kilo|kilos|kilogram|kilograms|kilogramme|kilogrammes", 1, C),
  L("Metric tonnes", "t", "tonne|tonnes|metric ton|metric tons", 1000),
  L("Grains", "gr", "grain|grains", kLb / 7000),
  L("Carats", "ct", "carat|carats", 2e-4),
  L("Ounces", "oz", "ozs|ounce|ounces", kOz, C),
  L("Pounds", "lb", "lbs|pound|pounds", kLb, C),
  L("Stone", "st", "stone|stones", 14 * kLb),
  L("Short tons (US)", "ton", "tons|short ton|short tons|us ton|us tons", 2000 * kLb),
  L("Long tons (UK)", "long ton", "long tons|uk ton|uk tons|imperial ton|imperial tons", 2240 * kLb),
  L("Troy ounces", "ozt", "oz t|troy oz|troy ounce|troy ounces", kLb / 7000 * 480),
};

constexpr Unit kTemperatureU[] = {
  // base: kelvin. Celsius and Fahrenheit are affine: b = (v + offset) * factor.
  {"Celsius", "C", "°C|ºC|degC|deg C|degree C|degrees C|celsius|centigrade", 1, 273.15, Kind::Affine, C},
  {"Fahrenheit", "F", "°F|ºF|degF|deg F|degree F|degrees F|fahrenheit", 5.0 / 9, 459.67, Kind::Affine, C},
  L("Kelvin", "K", "°K|kelvin|kelvins", 1, C),
  L("Rankine", "R", "°R|degR|deg R|rankine", 5.0 / 9),
};

constexpr Unit kTimeU[] = {
  // base: seconds
  L("Nanoseconds", "ns", "nsec|nanosecond|nanoseconds", 1e-9),
  L("Microseconds", "us", "µs|μs|usec|microsecond|microseconds", 1e-6),
  L("Milliseconds", "ms", "msec|millisecond|milliseconds", 1e-3, C),
  L("Seconds", "s", "sec|secs|second|seconds", 1, C),
  // "m" is weak: alone it is metres, but "1h 20m" only fits time.
  L("Minutes", "min", "mins|minute|minutes|~m", 60, C),
  L("Hours", "h", "hr|hrs|hour|hours", 3600, C),
  L("Days", "d", "day|days", 86400, C),
  L("Weeks", "wk", "wks|week|weeks", 604800, C),
  // Gregorian averages (365.2425 days a year).
  L("Months (average)", "mo", "mos|month|months", 2629746, A),
  L("Years (average)", "yr", "yrs|y|year|years", 31556952, A),
};

constexpr Unit kSpeedU[] = {
  // base: metres per second
  L("Meters per second", "m/s", "mps|m/sec|meters per second|metres per second", 1, C),
  L("Kilometers per hour", "km/h", "kph|kmh|kmph|km/hr|kilometers per hour|kilometres per hour", 1 / 3.6, C),
  L("Miles per hour", "mph", "mi/h|mi/hr|miles per hour", kMi / 3600, C),
  L("Feet per second", "ft/s", "fps|ft/sec|feet per second", kFt, C),
  L("Knots", "kn", "kt|kts|knot|knots", 1852.0 / 3600, C),
  L("Millimeters per second", "mm/s", "mm/sec|millimeters per second|millimetres per second", 1e-3),
  L("Millimeters per minute", "mm/min", "millimeters per minute|millimetres per minute", 1e-3 / 60),
  L("Inches per second", "in/s", "ips|in/sec|inches per second", kIn),
  L("Inches per minute", "in/min", "ipm|inches per minute", kIn / 60),
  // ISA sea level, 15 C. The real speed of sound moves with temperature.
  L("Mach (sea level, approx.)", "Mach", "mach", 340.294, A),
};

constexpr Unit kAccelerationU[] = {
  // base: metres per second squared
  L("Meters per second squared", "m/s^2", "m/s2|m/s²|meters per second squared|metres per second squared",
    1, C),
  L("Standard gravity", "g", "g0|gee|gees|g-force|gforce|standard gravity", kG0, C),
  L("Feet per second squared", "ft/s^2", "ft/s2|ft/s²|feet per second squared", kFt, C),
};

constexpr Unit kPressureU[] = {
  // base: pascals
  L("Pascals", "Pa", "pascal|pascals", 1, C),
  L("Hectopascals", "hPa", "hectopascal|hectopascals", 100),
  L("Kilopascals", "kPa", "kilopascal|kilopascals", 1e3, C),
  L("Megapascals", "MPa", "megapascal|megapascals|N/mm2|N/mm^2|N/mm²", 1e6),
  L("Millibars", "mbar", "mb|millibar|millibars", 100),
  L("Bars", "bar", "bars", 1e5, C),
  L("Pounds per square inch", "psi", "psia|psig|lbf/in2|lbf/in^2|lb/in2|lb/in^2|pounds per square inch",
    kLbf / (kIn * kIn), C),
  L("Atmospheres", "atm", "atmosphere|atmospheres", 101325, C),
  L("Torr", "Torr", "torr", 101325.0 / 760),
  L("Millimeters of mercury", "mmHg", "mm hg|millimeters of mercury|millimetres of mercury", kMmHg, C),
  L("Inches of mercury", "inHg", "in hg|in. hg|inches of mercury", kMmHg * 25.4, C),
  // Conventional inch of water: 1000 kg/m^3 at standard gravity.
  L("Inches of water", "inH2O", "in h2o|in wc|inwc|in. w.c.|inches of water", kIn * 1000 * kG0),
  L("Kilograms-force per cm^2", "kgf/cm^2", "kgf/cm2|kgf/cm²|kg/cm2|kg/cm^2|kg/cm²", kG0 * 1e4),
};

constexpr Unit kEnergyU[] = {
  // base: joules
  L("Joules", "J", "joule|joules", 1, C),
  L("Kilojoules", "kJ", "kilojoule|kilojoules", 1e3, C),
  L("Megajoules", "MJ", "megajoule|megajoules", 1e6),
  // Thermochemical calorie; US food labels use it (1 Cal = 1 kcal).
  L("Calories", "cal", "calorie|calories|small calorie|small calories", 4.184, C),
  L("Kilocalories (food Cal)", "kcal", "Cal|kilocalorie|kilocalories|food calorie|food calories", 4184, C),
  L("Watt-hours", "Wh", "watt hour|watt hours|watt-hour|watt-hours", 3600, C),
  L("Kilowatt-hours", "kWh", "kilowatt hour|kilowatt hours|kilowatt-hour|kilowatt-hours", 3.6e6, C),
  L("BTU", "BTU", "btus|british thermal unit|british thermal units", kBtu, C),
  L("Therms", "thm", "therm|therms", kBtu * 1e5),
  L("Foot-pounds", "ft-lbf", "ft-lb|ft lbf|ft lb|ftlb|ftlbf|ft·lbf|ft·lb|foot pound|foot pounds|"
    "foot-pound|foot-pounds", kFtLbf, C),
  L("Electronvolts", "eV", "electronvolt|electronvolts|electron volt|electron volts", 1.602176634e-19),
  L("Ergs", "erg", "ergs", 1e-7),
};

constexpr Unit kPowerU[] = {
  // base: watts
  L("Milliwatts", "mW", "milliwatt|milliwatts", 1e-3),
  L("Watts", "W", "watt|watts", 1, C),
  L("Kilowatts", "kW", "kilowatt|kilowatts", 1e3, C),
  L("Megawatts", "MW", "megawatt|megawatts", 1e6),
  // 550 ft-lbf/s.
  L("Horsepower", "hp", "bhp|hp(i)|horsepower|mechanical horsepower", 550 * kFtLbf, C),
  L("Metric horsepower", "PS", "cv|hp(m)|metric hp|metric horsepower", 75 * kG0, C),
  L("BTU per hour", "BTU/h", "btu/hr|btuh|btu per hour", kBtu / 3600, C),
  // 12,000 BTU/h; "ton" is weak so "2 ton" alone stays a weight.
  L("Tons of refrigeration", "TR", "rt|ton of refrigeration|tons of refrigeration|~ton|~tons",
    12000 * kBtu / 3600, C),
  L("Foot-pounds per second", "ft-lbf/s", "ft-lb/s|ft lbf/s|ft·lbf/s|foot pounds per second", kFtLbf),
};

constexpr Unit kForceU[] = {
  // base: newtons
  L("Newtons", "N", "newton|newtons", 1, C),
  L("Kilonewtons", "kN", "kilonewton|kilonewtons", 1e3, C),
  L("Dynes", "dyn", "dyne|dynes", 1e-5),
  // Weak "lb", "kg", "oz", "g": "10 lb" alone is mass, "10 lb to N" is force.
  L("Pounds-force", "lbf", "pound force|pounds force|pound-force|pounds-force|~lb|~lbs|~pound|~pounds",
    kLbf, C),
  L("Ounces-force", "ozf", "ounce force|ounces force|ounce-force|ounces-force|~oz", kLbf / 16),
  L("Kilograms-force", "kgf", "kp|kilopond|kilogram force|kilograms force|kilogram-force|kilograms-force|~kg",
    kG0, C),
  L("Grams-force", "gf", "gram force|grams force|gram-force|grams-force|~g", kG0 / 1000),
  L("Kips", "kip", "kips|kipf", 1000 * kLbf),
};

constexpr Unit kTorqueU[] = {
  // base: newton-metres. No "lb in"-style aliases with a bare space before "in":
  // "2 lb in kg" must stay a conversion.
  L("Newton-meters", "N-m", "Nm|n m|n·m|n*m|n.m|newton meter|newton meters|newton metre|newton metres|"
    "newton-meter|newton-meters|newton-metre|newton-metres", 1, C),
  L("Newton-centimeters", "N-cm", "Ncm|n cm|n·cm|newton centimeter|newton centimeters|newton-centimeter|"
    "newton-centimeters", 0.01),
  L("Kilogram-force meters", "kgf-m", "kgf m|kgf·m|kg-m|kgm|kg·m", kG0),
  L("Kilogram-force centimeters", "kgf-cm", "kgf cm|kgf·cm|kg-cm|kgcm|kg cm|kg·cm", kG0 / 100, C),
  L("Pound-force feet", "lbf-ft", "lb-ft|lb ft|lbft|lbf ft|lbf·ft|lb·ft|ft-lb|ft lb|ft·lb|ft-lbf|ftlb|"
    "pound feet|pound-feet|pound foot|pound-foot|foot-pound|foot-pounds|foot pound|foot pounds", kFtLbf, C),
  L("Pound-force inches", "lbf-in", "lb-in|lbin|lbf·in|lb·in|in-lb|in-lbf|inlb|in·lb|in·lbf|inch pound|"
    "inch pounds|inch-pound|inch-pounds|pound inch|pound inches|pound-inch|pound-inches", kLbf * kIn, C),
  L("Ounce-force inches", "ozf-in", "oz-in|ozin|oz·in|ozf·in|in-oz|ounce inch|ounce inches|ounce-inch|"
    "ounce-inches", kLbf / 16 * kIn, C),
};

constexpr Unit kDataSizeU[] = {
  // base: bytes. Lower-case "b" is bits, "B" bytes; exact case wins.
  L("Bits", "bit", "bits|b", 0.125, C),
  L("Kilobits", "kbit", "kb|kbits|kilobit|kilobits", 125),
  L("Megabits", "Mbit", "Mb|mbits|megabit|megabits", 1.25e5),
  L("Gigabits", "Gbit", "Gb|gbits|gigabit|gigabits", 1.25e8),
  L("Bytes", "B", "byte|bytes|octet|octets", 1, C),
  L("Kilobytes", "kB", "KB|kbyte|kbytes|kilobyte|kilobytes", 1e3, C),
  L("Megabytes", "MB", "mbyte|mbytes|megabyte|megabytes", 1e6, C),
  L("Gigabytes", "GB", "gbyte|gbytes|gig|gigs|gigabyte|gigabytes", 1e9, C),
  L("Terabytes", "TB", "tbyte|tbytes|terabyte|terabytes", 1e12, C),
  L("Petabytes", "PB", "petabyte|petabytes", 1e15),
  L("Kibibytes", "KiB", "kibibyte|kibibytes", 1024.0, C),
  L("Mebibytes", "MiB", "mebibyte|mebibytes", 1048576.0, C),
  L("Gibibytes", "GiB", "gibibyte|gibibytes", 1073741824.0, C),
  L("Tebibytes", "TiB", "tebibyte|tebibytes", 1099511627776.0, C),
};

constexpr Unit kDataRateU[] = {
  // base: bits per second
  L("Bits per second", "bit/s", "bps|b/s|bits/s|bits per second", 1, C),
  L("Kilobits per second", "kbit/s", "kbps|kb/s|kbits/s|kilobits per second", 1e3, C),
  L("Megabits per second", "Mbit/s", "Mbps|mbps|Mb/s|mbits/s|megabits per second", 1e6, C),
  L("Gigabits per second", "Gbit/s", "Gbps|gbps|Gb/s|gbits/s|gigabits per second", 1e9, C),
  L("Bytes per second", "B/s", "Bps|bytes/s|bytes per second", 8, C),
  L("Kilobytes per second", "kB/s", "kBps|KB/s|KBps|kilobytes per second", 8e3, C),
  L("Megabytes per second", "MB/s", "MBps|megabytes per second", 8e6, C),
  L("Gigabytes per second", "GB/s", "GBps|gigabytes per second", 8e9),
  L("Kibibytes per second", "KiB/s", "kibibytes per second", 8192.0),
  L("Mebibytes per second", "MiB/s", "mebibytes per second", 8388608.0, C),
};

constexpr Unit kAngleU[] = {
  // base: degrees (so the common units are exact)
  L("Degrees", "deg", "°|º|degs|degree|degrees", 1, C),
  L("Radians", "rad", "rads|radian|radians", 180 / kPi, C),
  L("Gradians", "grad", "grads|gon|gradian|gradians", 0.9),
  L("Arcminutes", "arcmin", "'|′|’|arcminute|arcminutes|arc minute|arc minutes|minute of arc|minutes of arc",
    1.0 / 60, C),
  L("Arcseconds", "arcsec", "\"|''|″|”|arcsecond|arcseconds|arc second|arc seconds|second of arc|"
    "seconds of arc", 1.0 / 3600, C),
  L("Turns", "turn", "turns|rev|revs|revolution|revolutions|rotation|rotations", 360, C),
  L("Milliradians", "mrad", "milliradian|milliradians", 0.18 / kPi),
};

constexpr Unit kFrequencyU[] = {
  // base: hertz
  L("Hertz", "Hz", "hertz|cps|cycles per second", 1, C),
  L("Kilohertz", "kHz", "kilohertz", 1e3, C),
  L("Megahertz", "MHz", "megahertz", 1e6, C),
  L("Gigahertz", "GHz", "gigahertz", 1e9, C),
  L("Revolutions per minute", "rpm", "rev/min|r/min|revs per minute|revolutions per minute", 1.0 / 60, C),
  L("Radians per second", "rad/s", "rad/sec|radians per second", 0.5 / kPi),
};

constexpr Unit kFuelEconomyU[] = {
  // base: kilometres per litre. L/100 km is the reciprocal: b = 100 / v.
  L("Miles per US gallon", "mpg", "mpg us|us mpg|mpg(us)|mi/gal|miles per gallon", kMi / 1000 / kGalL, C),
  L("Miles per UK gallon", "mpg UK", "uk mpg|mpg(uk)|imp mpg|mpg imp|miles per imperial gallon",
    kMi / 1000 / kUkGalL, C),
  L("Kilometers per liter", "km/L", "kmpl|km/l|kilometers per liter|kilometres per litre", 1, C),
  {"Liters per 100 km", "L/100 km", "l/100km|l/100 km|lp100km|l per 100 km|liters per 100 km|litres per 100 km",
   100, 0, Kind::Reciprocal, C},
};

constexpr Unit kDensityU[] = {
  // base: kilograms per cubic metre
  L("Kilograms per cubic meter", "kg/m^3", "kg/m3|kg/m³|kilograms per cubic meter|kilograms per cubic metre",
    1, C),
  L("Grams per cubic centimeter", "g/cm^3", "g/cm3|g/cm³|g/cc|g/ml|g/mL|kg/l|kg/L|grams per cubic centimeter|"
    "grams per milliliter", 1000, C),
  L("Grams per liter", "g/L", "g/l|grams per liter|grams per litre", 1),
  L("Pounds per cubic foot", "lb/ft^3", "lb/ft3|lb/ft³|lb/cu ft|pcf|pounds per cubic foot",
    kLb / (kFt3L / 1000), C),
  L("Pounds per cubic inch", "lb/in^3", "lb/in3|lb/in³|lb/cu in|pci|pounds per cubic inch",
    kLb / (kIn3L / 1000), C),
  L("Pounds per US gallon", "lb/gal", "lb/us gal|ppg|pounds per gallon", kLb / (kGalL / 1000), C),
  L("Ounces per cubic inch", "oz/in^3", "oz/in3|oz/in³|ounces per cubic inch", kOz / (kIn3L / 1000)),
  // Relative to water at its densest (999.972 kg/m^3); close enough to call 1000.
  L("Specific gravity (approx.)", "SG", "sg|specific gravity", 1000, A),
};

constexpr Unit kFlowU[] = {
  // base: litres per minute
  L("Milliliters per minute", "mL/min", "ml/min|milliliters per minute", 1e-3),
  L("Liters per minute", "L/min", "l/min|lpm|liters per minute|litres per minute", 1, C),
  L("Liters per second", "L/s", "l/s|lps|liters per second|litres per second", 60),
  L("Liters per hour", "L/h", "l/h|l/hr|lph|liters per hour|litres per hour", 1.0 / 60),
  L("Cubic meters per hour", "m^3/h", "m3/h|m³/h|m3/hr|cmh|cubic meters per hour|cubic metres per hour",
    1000.0 / 60, C),
  L("Cubic meters per second", "m^3/s", "m3/s|m³/s|cumec|cumecs|cubic meters per second", 60000),
  L("US gallons per minute", "gpm", "gal/min|us gpm|gallons per minute", kGalL, C),
  L("US gallons per hour", "gph", "gal/h|gal/hr|gallons per hour", kGalL / 60),
  L("UK gallons per minute", "UK gpm", "uk gal/min|imp gpm", kUkGalL),
  L("Cubic feet per minute", "CFM", "cfm|ft3/min|ft^3/min|cu ft/min|cubic feet per minute", kFt3L, C),
  L("Cubic feet per second", "cfs", "ft3/s|ft^3/s|cubic feet per second", kFt3L * 60),
};

constexpr Unit kCurrentU[] = {
  // base: amperes
  L("Microamps", "uA", "µA|μA|microamp|microamps|microampere|microamperes", 1e-6),
  L("Milliamps", "mA", "milliamp|milliamps|milliampere|milliamperes", 1e-3, C),
  L("Amps", "A", "amp|amps|ampere|amperes", 1, C),
  L("Kiloamps", "kA", "kiloamp|kiloamps|kiloampere|kiloamperes", 1e3),
};

constexpr Unit kVoltageU[] = {
  // base: volts
  L("Microvolts", "uV", "µV|μV|microvolt|microvolts", 1e-6),
  L("Millivolts", "mV", "millivolt|millivolts", 1e-3, C),
  L("Volts", "V", "volt|volts", 1, C),
  L("Kilovolts", "kV", "kilovolt|kilovolts", 1e3, C),
};

constexpr Unit kResistanceU[] = {
  // base: ohms. "mohm" is milli and "Mohm" mega: exact case decides.
  L("Milliohms", "mohm", "mΩ|milliohm|milliohms", 1e-3),
  L("Ohms", "ohm", "ohms|Ω", 1, C),
  L("Kilohms", "kohm", "kohms|kΩ|kilohm|kilohms|kiloohm|kiloohms", 1e3, C),
  L("Megohms", "Mohm", "Mohms|MΩ|meg|megohm|megohms|megaohm|megaohms", 1e6, C),
};

constexpr Unit kCapacitanceU[] = {
  // base: farads
  L("Picofarads", "pF", "puff|picofarad|picofarads", 1e-12, C),
  L("Nanofarads", "nF", "nanofarad|nanofarads", 1e-9, C),
  L("Microfarads", "uF", "µF|μF|mfd|microfarad|microfarads", 1e-6, C),
  L("Millifarads", "mF", "millifarad|millifarads", 1e-3),
  L("Farads", "F", "farad|farads", 1),
};

template <size_t N>
constexpr uint8_t countOf(const Unit (&)[N]) { return (uint8_t)N; }

// Default pairs are indices into the tables above.
constexpr Category kCats[] = {
  {"Length", kLengthU, countOf(kLengthU), 7, 2, nullptr},
  {"Area", kAreaU, countOf(kAreaU), 6, 2, nullptr},
  {"Volume", kVolumeU, countOf(kVolumeU), 8, 0, "US and UK measures differ: a US pint is 16 fl oz, a UK pint 20."},
  {"Mass", kMassU, countOf(kMassU), 7, 2, nullptr},
  {"Temperature", kTemperatureU, countOf(kTemperatureU), 1, 0, nullptr},
  {"Time", kTimeU, countOf(kTimeU), 5, 4, "Months and years are calendar averages."},
  {"Speed", kSpeedU, countOf(kSpeedU), 2, 1, "Mach is the speed of sound at sea level, 15 C (340.294 m/s)."},
  {"Acceleration", kAccelerationU, countOf(kAccelerationU), 1, 0, nullptr},
  {"Pressure", kPressureU, countOf(kPressureU), 6, 2, nullptr},
  {"Energy", kEnergyU, countOf(kEnergyU), 4, 1, "Food Calories (Cal) are kilocalories."},
  {"Power", kPowerU, countOf(kPowerU), 4, 2, nullptr},
  {"Force", kForceU, countOf(kForceU), 3, 0, nullptr},
  {"Torque", kTorqueU, countOf(kTorqueU), 4, 0, nullptr},
  {"Data size", kDataSizeU, countOf(kDataSizeU), 7, 12, "kB, MB, GB are powers of 1000; KiB, MiB, GiB of 1024."},
  {"Data rate", kDataRateU, countOf(kDataRateU), 2, 6, nullptr},
  {"Angle", kAngleU, countOf(kAngleU), 0, 1, nullptr},
  {"Frequency", kFrequencyU, countOf(kFrequencyU), 4, 0, nullptr},
  {"Fuel economy", kFuelEconomyU, countOf(kFuelEconomyU), 0, 3, "L/100 km is inverse: bigger numbers burn more."},
  {"Density", kDensityU, countOf(kDensityU), 1, 3, nullptr},
  {"Flow", kFlowU, countOf(kFlowU), 6, 1, nullptr},
  {"Current", kCurrentU, countOf(kCurrentU), 2, 1, nullptr},
  {"Voltage", kVoltageU, countOf(kVoltageU), 2, 1, nullptr},
  {"Resistance", kResistanceU, countOf(kResistanceU), 2, 1, nullptr},
  {"Capacitance", kCapacitanceU, countOf(kCapacitanceU), 2, 1, nullptr},
};
static_assert(sizeof kCats / sizeof kCats[0] == kCatCount, "kCats must match enum Cat");

// Typical US kitchen densities (King Arthur / USDA style), per US cup.
constexpr Ingredient kIngredients[] = {
  {"Water", 236.5882365},
  {"Milk", 242},
  {"All-purpose flour", 120},
  {"Bread flour", 127},
  {"Whole wheat flour", 113},
  {"Granulated sugar", 200},
  {"Brown sugar (packed)", 213},
  {"Powdered sugar", 113},
  {"Butter", 227},
  {"Vegetable oil", 218},
  {"Honey", 336},
  {"Maple syrup", 312},
  {"Rice (uncooked)", 185},
  {"Rolled oats", 89},
  {"Cocoa powder", 84},
  {"Chocolate chips", 170},
  {"Table salt", 288},
  {"Kosher salt (Diamond)", 135},
  {"Kosher salt (Morton)", 240},
};
constexpr int kIngredientCount = sizeof kIngredients / sizeof kIngredients[0];
constexpr double kCupL = kGalL / 16;

// ---- Text helpers (ASCII only: no locale, UTF-8 bytes compare as they are) ----

inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
inline bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
inline bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

const Unit *unitAt(int cat, int u) {
  if (cat < 0 || cat >= kCatCount || u < 0 || u >= kCats[cat].count) return nullptr;
  return &kCats[cat].units[u];
}

// Calls fn(alias, len, weak) for the name, the symbol and each '|' alias of u, until
// fn returns true.
template <typename Fn>
void forEachAlias(const Unit &u, Fn fn) {
  if (fn(u.name, strlen(u.name), false)) return;
  if (fn(u.symbol, strlen(u.symbol), false)) return;
  for (const char *a = u.aliases; *a;) {
    const char *e = strchr(a, '|');
    const size_t n = e ? (size_t)(e - a) : strlen(a);
    const bool weak = a[0] == '~';
    if (n > (weak ? 1u : 0u) && fn(a + weak, n - weak, weak)) return;
    if (!e) break;
    a = e + 1;
  }
}

// Matches alias a (n bytes) against text t. A space in the alias matches any run of
// whitespace or none. Returns the text length matched, 0 for no match; *exact is
// false if any letter differed in case. The match must end at a word boundary:
// "in" does not match "inch", and "ft3" does not match "ft3in" ("5ft3in" is 5 ft 3 in).
size_t matchAlias(const char *a, size_t n, const char *t, bool *exact) {
  size_t i = 0, j = 0;
  bool ex = true;
  while (i < n) {
    if (a[i] == ' ') {
      while (isSpace(t[j])) j++;
      i++;
      continue;
    }
    if (!t[j]) return 0;
    if (t[j] != a[i]) {
      if (lower(t[j]) != lower(a[i])) return 0;
      ex = false;
    }
    i++;
    j++;
  }
  const char last = a[n - 1], next = t[j];
  if ((isAlpha(last) || isDigit(last)) && isAlpha(next)) return 0;
  if (isDigit(last) && isDigit(next)) return 0;
  *exact = ex;
  return j;
}

// How well unit u names exactly the len bytes at t: 6 exact case, 4 any case, 2 weak
// alias in exact case, 1 weak alias in any case, 0 not at all.
int quality(const Unit &u, const char *t, size_t len) {
  int best = 0;
  forEachAlias(u, [&](const char *a, size_t n, bool weak) {
    bool exact = false;
    if (matchAlias(a, n, t, &exact) != len) return false;
    const int q = (weak ? 1 : 4) + (exact ? (weak ? 1 : 2) : 0);
    if (q > best) best = q;
    return best == 6;
  });
  return best;
}

// The longest unit name of any unit (in cat, or all with cat < 0) at t; 0 if none.
// Without digitTails, names that end letter-digit ("ft3", "m2") are skipped: the
// second reading of "5ft3 in", where the 3 is inches.
size_t longestUnitAt(const char *t, int cat, bool digitTails) {
  size_t best = 0;
  for (int c = cat < 0 ? 0 : cat; c < (cat < 0 ? (int)kCatCount : cat + 1); c++) {
    for (int k = 0; k < kCats[c].count; k++) {
      forEachAlias(kCats[c].units[k], [&](const char *a, size_t n, bool) {
        if (!digitTails && n >= 2 && isDigit(a[n - 1]) && isAlpha(a[n - 2])) return false;
        bool exact;
        const size_t m = matchAlias(a, n, t, &exact);
        if (m > best) best = m;
        return false;
      });
    }
  }
  return best;
}

// ---- Arithmetic ----

// Recursive descent over a prefix of the text. Stops at the first character that
// cannot continue the expression (a unit name, a quote) and reports where.
struct Expr {
  const char *p;
  const char *end;   // just past the last character used
  bool bad = false;
  int depth = 0;

  // Operator after optional spaces: returns its byte length and sets *op, or 0.
  int peekOp(char *op) const {
    const char *q = p;
    while (*q == ' ' || *q == '\t') q++;
    switch (*q) {
      case '+': case '*': case '/': case '^': *op = *q; return (int)(q - p) + 1;
      case '-': if (q[1] == '>') return 0; *op = '-'; return (int)(q - p) + 1;  // "->" separates
    }
    // UTF-8 × ÷ −
    if ((uint8_t)q[0] == 0xC3 && (uint8_t)q[1] == 0x97) { *op = '*'; return (int)(q - p) + 2; }
    if ((uint8_t)q[0] == 0xC3 && (uint8_t)q[1] == 0xB7) { *op = '/'; return (int)(q - p) + 2; }
    if ((uint8_t)q[0] == 0xE2 && (uint8_t)q[1] == 0x88 && (uint8_t)q[2] == 0x92) {
      *op = '-';
      return (int)(q - p) + 3;
    }
    return 0;
  }

  double number() {
    const char *q = p;
    char buf[64];
    size_t n = 0;
    bool digits = false;
    while (isDigit(*q) || *q == ',') {
      // A comma only as a thousands separator: exactly three digits follow.
      if (*q == ',') {
        if (!digits || !isDigit(q[1]) || !isDigit(q[2]) || !isDigit(q[3]) || isDigit(q[4])) break;
        q++;
        continue;
      }
      if (n < sizeof buf - 1) buf[n++] = *q;
      digits = true;
      q++;
    }
    if (*q == '.') {
      if (n < sizeof buf - 1) buf[n++] = '.';
      q++;
      while (isDigit(*q)) {
        if (n < sizeof buf - 1) buf[n++] = *q;
        digits = true;
        q++;
      }
    }
    if (!digits) { bad = true; return 0; }
    // An exponent only when digits follow: "1eV" is one electronvolt.
    if ((*q == 'e' || *q == 'E') &&
        (isDigit(q[1]) || ((q[1] == '+' || q[1] == '-') && isDigit(q[2])))) {
      do {
        if (n < sizeof buf - 1) buf[n++] = *q;
        q++;
      } while (isDigit(*q) || ((*q == '+' || *q == '-') && (q[-1] == 'e' || q[-1] == 'E')));
    }
    buf[n] = 0;
    p = end = q;
    return strtod(buf, nullptr);
  }

  double primary() {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '(') {
      if (++depth > 32) { bad = true; return 0; }
      p++;
      const double v = sum();
      while (*p == ' ' || *p == '\t') p++;
      if (bad || *p != ')') { bad = true; return 0; }
      p++;
      end = p;
      depth--;
      return v;
    }
    return number();
  }

  double unary() {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-' || *p == '+') {
      const bool neg = *p == '-';
      p++;
      const double v = unary();
      return neg ? -v : v;
    }
    if ((uint8_t)p[0] == 0xE2 && (uint8_t)p[1] == 0x88 && (uint8_t)p[2] == 0x92) {
      p += 3;
      return -unary();
    }
    return power();
  }

  double power() {
    const double b = primary();
    if (bad) return 0;
    char op;
    const int n = peekOp(&op);
    if (n && op == '^') {
      p += n;
      if (++depth > 32) { bad = true; return 0; }
      const double e = unary();  // right associative: 2^3^2 = 2^9
      depth--;
      return bad ? 0 : std::pow(b, e);
    }
    return b;
  }

  double product() {
    double v = unary();
    char op;
    int n;
    while (!bad && (n = peekOp(&op)) && (op == '*' || op == '/')) {
      p += n;
      const double r = unary();
      if (bad) return 0;
      if (op == '/') {
        if (r == 0) { bad = true; return 0; }
        v /= r;
      } else {
        v *= r;
      }
    }
    return v;
  }

  double sum() {
    double v = product();
    char op;
    int n;
    while (!bad && (n = peekOp(&op)) && (op == '+' || op == '-')) {
      p += n;
      const double r = product();
      if (bad) return 0;
      const double s = op == '+' ? v + r : v - r;
      // 0.1 + 0.2 - 0.3 is 5.6e-17 in binary; call cancellation that complete zero.
      v = std::fabs(s) < 1e-14 * std::fmax(std::fabs(v), std::fabs(r)) ? 0 : s;
    }
    return v;
  }
};

// ---- Tokens for typed input ----

enum TokType : uint8_t { kNum, kUnitTok, kInTok, kSepTok };

struct Tok {
  TokType type;
  bool isInt;    // a plain whole number ("2"), may start a mixed number
  bool isFrac;   // a plain proper fraction ("3/8"), may end one
  double value;
  const char *p; // unit text
  uint8_t len;
};

constexpr int kMaxToks = 24;

struct Toks {
  Tok t[kMaxToks];
  int n = 0;
};

bool startsNumber(const char *p) {
  if (isDigit(*p) || *p == '(') return true;
  if (*p == '.') return isDigit(p[1]);
  // Signs, possibly several ("--3"), then a number or a bracket.
  const char *q = p;
  bool sign = false;
  for (;;) {
    if (*q == '-' || *q == '+') q++;
    else if ((uint8_t)q[0] == 0xE2 && (uint8_t)q[1] == 0x88 && (uint8_t)q[2] == 0x92) q += 3;
    else break;
    sign = true;
    while (*q == ' ') q++;
  }
  return sign && (isDigit(*q) || *q == '(' || (*q == '.' && isDigit(q[1])));
}

// Reads digits; returns the count and the value.
int readInt(const char *p, long long *v) {
  int n = 0;
  *v = 0;
  while (isDigit(p[n]) && n < 15) *v = *v * 10 + (p[n++] - '0');
  return isDigit(p[n]) ? 0 : n;
}

// Tape-measure mixed number "2-3/8" (a proper fraction after the dash, and no
// operator after it, so "5-3/8*2" is still arithmetic). Returns length, 0 if not one.
int tapeNumber(const char *p, double *v) {
  long long a, b, c;
  int i = readInt(p, &a);
  if (!i || p[i] != '-') return 0;
  int j = readInt(p + i + 1, &b);
  if (!j || p[i + 1 + j] != '/') return 0;
  const int k0 = i + 1 + j + 1;
  int k = readInt(p + k0, &c);
  if (!k || c == 0 || b >= c) return 0;
  const char *q = p + k0 + k;
  if (*q == '.') return 0;
  const char *r = q;
  while (*r == ' ') r++;
  if (*r == '*' || *r == '/' || *r == '^' || *r == '+' || (*r == '-' && r[1] != '>') || *r == '(') return 0;
  *v = (double)a + (double)b / (double)c;
  return (int)(q - p);
}

bool isSimpleFraction(const char *s, const char *e) {
  while (s < e && *s == ' ') s++;
  while (e > s && e[-1] == ' ') e--;
  long long a, b;
  int i = readInt(s, &a);
  if (!i) return false;
  const char *q = s + i;
  while (*q == ' ') q++;
  if (*q != '/') return false;
  q++;
  while (*q == ' ') q++;
  int j = readInt(q, &b);
  return j && q + j == e && b > 0 && a < b;
}

bool isPlainInt(const char *s, const char *e) {
  if (s < e && (*s == '-' || *s == '+')) s++;
  if (s == e) return false;
  for (; s < e; s++) if (!isDigit(*s)) return false;
  return true;
}

// Separator word at p in a whole line: "to", "into", "as", "->", "→", "=", "=>".
int separatorAt(const char *p) {
  if (p[0] == '-' && p[1] == '>') return 2;
  if (p[0] == '=') return p[1] == '>' ? 2 : 1;
  if ((uint8_t)p[0] == 0xE2 && (uint8_t)p[1] == 0x86 && (uint8_t)p[2] == 0x92) return 3;
  static const char *const kWords[] = {"into", "to", "as"};
  for (const char *w : kWords) {
    const size_t n = strlen(w);
    size_t i = 0;
    while (i < n && lower(p[i]) == w[i]) i++;
    if (i == n && !isAlpha(p[n])) return (int)n;
  }
  return 0;
}

// Splits text into numbers, unit names and (in a whole line) separators. A unit name
// is the longest name that matches; units only from cat when cat >= 0.
bool tokenize(const char *s, int cat, bool line, bool digitTails, Toks *out) {
  out->n = 0;
  const char *p = s;
  for (;;) {
    while (isSpace(*p) || *p == ',' || *p == '?') p++;
    if (!*p) return true;
    if (out->n == kMaxToks) return false;
    Tok &t = out->t[out->n];
    t = Tok{kNum, false, false, 0, p, 0};
    int n;
    if (line && (n = separatorAt(p))) {
      t.type = kSepTok;
      p += n;
    } else if (startsNumber(p)) {
      double v;
      if ((n = tapeNumber(p, &v))) {
        t.value = v;
        p += n;
      } else {
        Expr e{p, p};
        t.value = e.sum();
        if (e.bad || !std::isfinite(t.value)) return false;
        t.isInt = isPlainInt(p, e.end);
        t.isFrac = isSimpleFraction(p, e.end);
        p = e.end;
      }
    } else {
      const size_t m = longestUnitAt(p, cat, digitTails);
      if (!m || m > 255) return false;
      t.type = (line && m == 2 && lower(p[0]) == 'i' && lower(p[1]) == 'n') ? kInTok : kUnitTok;
      t.len = (uint8_t)m;
      p += m;
      // "lbs." "in." "oz." - a trailing full stop that is not a decimal point.
      if (*p == '.' && !isDigit(p[1])) p++;
    }
    out->n++;
  }
}

// Picks the category every unit token fits, preferring exact-case matches then the
// earlier category, and the best unit for each token in it.
bool resolve(const Tok *const *u, int n, int fixedCat, int *catOut, int *unitsOut) {
  int bestCat = -1, bestScore = 0;
  const int c0 = fixedCat < 0 ? 0 : fixedCat;
  const int c1 = fixedCat < 0 ? (int)kCatCount : fixedCat + 1;
  for (int c = c0; c < c1; c++) {
    int score = 0;
    for (int i = 0; i < n && score >= 0; i++) {
      int q = 0;
      for (int k = 0; k < kCats[c].count; k++) {
        const int qk = quality(kCats[c].units[k], u[i]->p, u[i]->len);
        if (qk > q) q = qk;
      }
      score = q ? score + q : -1;
    }
    if (score > bestScore) {
      bestScore = score;
      bestCat = c;
    }
  }
  if (bestCat < 0) return false;
  for (int i = 0; i < n; i++) {
    int q = 0;
    for (int k = 0; k < kCats[bestCat].count; k++) {
      const int qk = quality(kCats[bestCat].units[k], u[i]->p, u[i]->len);
      if (qk > q) {
        q = qk;
        unitsOut[i] = k;
      }
    }
  }
  *catOut = bestCat;
  return true;
}

struct Pairs {
  double v[kMaxToks];
  const Tok *unit[kMaxToks];
  int n = 0;
  bool bare = false;  // one number with no unit
};

// Reads tokens [a, b) as number+unit pairs, or a single bare number.
bool readPairs(const Toks &tk, int a, int b, Pairs *out) {
  out->n = 0;
  out->bare = false;
  int i = a;
  while (i < b) {
    if (tk.t[i].type != kNum) return false;
    double v = tk.t[i].value;
    // Mixed number "2 3/8".
    if (i + 1 < b && tk.t[i].isInt && tk.t[i + 1].type == kNum && tk.t[i + 1].isFrac) {
      v = v < 0 ? v - tk.t[i + 1].value : v + tk.t[i + 1].value;
      i++;
    }
    i++;
    if (i < b && (tk.t[i].type == kUnitTok || tk.t[i].type == kInTok)) {
      out->v[out->n] = v;
      out->unit[out->n++] = &tk.t[i++];
    } else if (i == b && out->n == 0) {
      out->v[0] = v;
      out->bare = true;
      return true;
    } else {
      return false;
    }
  }
  return out->n > 0;
}

// Adds the pairs up in `to` units. Only linear units add.
bool sumPairs(const Pairs &pr, int cat, const int *units, int to, double *out) {
  bool ok = true;
  if (pr.n == 1) {
    *out = convert(pr.v[0], cat, units[0], to, &ok);
    return ok;
  }
  double base = 0;
  for (int i = 0; i < pr.n; i++) {
    if (unitAt(cat, units[i])->kind != Kind::Linear) return false;
    base += toBase(pr.v[i], cat, units[i], &ok);
    if (!ok) return false;
  }
  *out = fromBase(base, cat, to, &ok);
  return ok;
}

// Trims trailing zeros after a decimal point, then the point itself.
void trimZeros(char *s) {
  if (!strchr(s, '.')) return;
  size_t n = strlen(s);
  while (n && s[n - 1] == '0') s[--n] = 0;
  if (n && s[n - 1] == '.') s[--n] = 0;
}

}  // namespace

// ---- Tables ----

int categoryCount() { return kCatCount; }

const Category *category(int cat) { return (cat >= 0 && cat < kCatCount) ? &kCats[cat] : nullptr; }

int unitCount(int cat) { return (cat >= 0 && cat < kCatCount) ? kCats[cat].count : 0; }

const Unit *unit(int cat, int index) { return unitAt(cat, index); }

int findCategory(const char *name) {
  if (!name) return -1;
  for (int c = 0; c < kCatCount; c++) {
    const char *a = kCats[c].name, *b = name;
    while (*a && lower(*a) == lower(*b)) a++, b++;
    if (!*a && !*b) return c;
  }
  return -1;
}

// ---- Conversion ----

double toBase(double v, int cat, int u, bool *ok) {
  const Unit *x = unitAt(cat, u);
  double b = 0;
  if (x && std::isfinite(v)) {
    switch (x->kind) {
      case Kind::Linear: b = v * x->factor; break;
      case Kind::Affine: b = (v + x->offset) * x->factor; break;
      case Kind::Reciprocal: b = v == 0 ? INFINITY : x->factor / v; break;
    }
  }
  const bool good = x && std::isfinite(v) && std::isfinite(b);
  if (ok) *ok = good;
  return good ? b : 0;
}

double fromBase(double b, int cat, int u, bool *ok) {
  const Unit *x = unitAt(cat, u);
  double v = 0;
  if (x && std::isfinite(b)) {
    switch (x->kind) {
      case Kind::Linear: v = b / x->factor; break;
      case Kind::Affine:
        v = b / x->factor - x->offset;
        // 32 F -> 273.15 K -> 0 C leaves 5.7e-14 behind; that is zero.
        if (std::fabs(v) < 1e-12 * x->offset) v = 0;
        break;
      case Kind::Reciprocal: v = b == 0 ? INFINITY : x->factor / b; break;
    }
  }
  const bool good = x && std::isfinite(b) && std::isfinite(v);
  if (ok) *ok = good;
  return good ? v : 0;
}

double convert(double value, int cat, int from, int to, bool *ok) {
  if (!unitAt(cat, from) || !unitAt(cat, to) || !std::isfinite(value)) {
    if (ok) *ok = false;
    return 0;
  }
  if (from == to) {
    if (ok) *ok = true;
    return value;
  }
  bool good;
  const double b = toBase(value, cat, from, &good);
  const double r = good ? fromBase(b, cat, to, &good) : 0;
  if (ok) *ok = good;
  return good ? r : 0;
}

// ---- Cooking ----

int ingredientCount() { return kIngredientCount; }

const Ingredient *ingredient(int i) { return (i >= 0 && i < kIngredientCount) ? &kIngredients[i] : nullptr; }

double convertIngredient(double value, int ing, int fromCat, int fromUnit, int toCat, int toUnit, bool *ok) {
  const bool catsOk = (fromCat == kVolume || fromCat == kMass) && (toCat == kVolume || toCat == kMass);
  if (!ingredient(ing) || !catsOk) {
    if (ok) *ok = false;
    return 0;
  }
  if (fromCat == toCat) return convert(value, fromCat, fromUnit, toUnit, ok);
  const double kgPerL = kIngredients[ing].gramsPerCup / 1000 / kCupL;
  bool good;
  double b = toBase(value, fromCat, fromUnit, &good);  // litres or kilograms
  if (good) b = fromCat == kVolume ? b * kgPerL : b / kgPerL;
  const double r = good ? fromBase(b, toCat, toUnit, &good) : 0;
  if (ok) *ok = good;
  return good ? r : 0;
}

// ---- Typed input ----

UnitRef findUnit(const char *text, int cat) {
  UnitRef r;
  if (!text || cat >= kCatCount) return r;
  while (isSpace(*text)) text++;
  size_t len = strlen(text);
  while (len && isSpace(text[len - 1])) len--;
  if (!len) return r;
  int bestQ = 0;
  for (int c = cat < 0 ? 0 : cat; c < (cat < 0 ? (int)kCatCount : cat + 1); c++) {
    for (int k = 0; k < kCats[c].count; k++) {
      const int q = quality(kCats[c].units[k], text, len);
      if (q > bestQ) {
        bestQ = q;
        r.cat = c;
        r.unit = k;
      }
    }
  }
  return r;
}

bool evalExpression(const char *text, double *out) {
  if (!text) return false;
  Toks tk;
  Pairs pr;
  if (!tokenize(text, -1, false, true, &tk) || !readPairs(tk, 0, tk.n, &pr) || !pr.bare) return false;
  *out = pr.v[0];
  return true;
}

bool parseQuantity(const char *text, int cat, int unitIndex, double *out) {
  if (!text || !unitAt(cat, unitIndex)) return false;
  for (int pass = 0; pass < 2; pass++) {
    Toks tk;
    Pairs pr;
    if (!tokenize(text, cat, false, pass == 0, &tk) || !readPairs(tk, 0, tk.n, &pr)) continue;
    if (pr.bare) {
      *out = pr.v[0];
      return true;
    }
    int c, units[kMaxToks];
    if (resolve(pr.unit, pr.n, cat, &c, units) && sumPairs(pr, cat, units, unitIndex, out)) return true;
  }
  return false;
}

namespace {

LineResult parseLineTokens(const Toks &tk) {
  LineResult r;
  r.error = "Not understood";
  const int n = tk.n;
  if (!n) return r;

  // Candidate readings: left tokens [0, leftEnd) are the quantity, token `target`
  // (or none, -1) the unit to convert to.
  struct Reading { int leftEnd, target; };
  Reading cand[kMaxToks + 2];
  int nc = 0;
  int sep = -1, seps = 0;
  for (int i = 0; i < n; i++) {
    if (tk.t[i].type == kSepTok) {
      sep = i;
      seps++;
    }
  }
  if (seps > 1) return r;
  if (seps == 1) {
    if (sep == n - 1) cand[nc++] = {sep, -1};          // "5 ft =": no target typed
    else if (sep == n - 2) cand[nc++] = {sep, n - 1};
    else return r;
  } else {
    // "in" as the separator: it must be next to last, since the target is one unit
    // ("100 F in C", and the second "in" of "3 in in mm").
    if (n >= 3 && tk.t[n - 1].type != kNum && tk.t[n - 2].type == kInTok) cand[nc++] = {n - 2, n - 1};
    // No separator: the last unit is the target ("5in mm", "3/8 in mm").
    if (n >= 3 && tk.t[n - 1].type != kNum) cand[nc++] = {n - 1, n - 1};
    cand[nc++] = {n, -1};
  }

  bool sawMismatch = false;
  for (int ci = 0; ci < nc; ci++) {
    const Reading &rd = cand[ci];
    Pairs pr;
    if (!readPairs(tk, 0, rd.leftEnd, &pr) || pr.bare) continue;
    if (rd.target >= 0 && tk.t[rd.target].type == kNum) continue;
    const Tok *u[kMaxToks + 1];
    int units[kMaxToks + 1];
    for (int i = 0; i < pr.n; i++) u[i] = pr.unit[i];
    int nu = pr.n;
    if (rd.target >= 0) u[nu++] = &tk.t[rd.target];
    int cat;
    if (!resolve(u, nu, -1, &cat, units)) {
      sawMismatch = true;
      continue;
    }
    double value;
    if (!sumPairs(pr, cat, units, units[0], &value)) continue;
    r.ok = true;
    r.cat = cat;
    r.from = units[0];
    r.hasTarget = rd.target >= 0;
    r.to = r.hasTarget ? units[nu - 1] : r.from;
    r.value = value;
    r.result = convert(value, cat, r.from, r.to, &r.resultOk);
    r.error = r.resultOk ? "" : "Result is infinite";
    return r;
  }
  if (sawMismatch) r.error = "Units don't match";
  return r;
}

}  // namespace

LineResult parseLine(const char *text) {
  LineResult r;
  r.error = "Not understood";
  if (!text) return r;
  for (int pass = 0; pass < 2; pass++) {
    Toks tk;
    if (!tokenize(text, -1, true, pass == 0, &tk)) continue;
    const LineResult p = parseLineTokens(tk);
    if (p.ok) return p;
    if (pass == 0) r = p;  // the first reading's reason is the one to show
  }
  return r;
}

// ---- Display ----

std::string formatNumber(double v, const NumberFormat &f) {
  if (std::isnan(v)) return "NaN";
  if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
  if (v == 0) return "0";
  const int digits = f.digits < 1 ? 1 : f.digits > 17 ? 17 : f.digits;
  char buf[96];
  snprintf(buf, sizeof buf, "%.*e", digits - 1, v);
  char *e = strchr(buf, 'e');
  const int exp = atoi(e + 1);
  if (exp >= f.sciHigh || exp < f.sciLow) {
    *e = 0;
    trimZeros(buf);
    std::string s = buf;
    s += f.timesTen ? " x 10^" : "e";
    s += std::to_string(exp);
    return s;
  }
  // Print the already rounded value, so 99999 to 4 digits is 100000, not 99999.
  const double rounded = strtod(buf, nullptr);
  const int dec = digits - 1 - exp > 0 ? digits - 1 - exp : 0;
  snprintf(buf, sizeof buf, "%.*f", dec, rounded);
  trimZeros(buf);
  // A tiny negative never reaches here as zero (sciLow catches it), but be sure.
  if (!strcmp(buf, "-0")) return "0";
  if (!f.grouping) return buf;
  const char *s = buf;
  std::string out;
  if (*s == '-') out += *s++;
  const char *dot = strchr(s, '.');
  const int intLen = dot ? (int)(dot - s) : (int)strlen(s);
  for (int i = 0; i < intLen; i++) {
    if (i && (intLen - i) % 3 == 0) out += ',';
    out += s[i];
  }
  if (dot) out += dot;
  return out;
}

std::string formatWithUnit(double v, int cat, int u, const NumberFormat &f) {
  const Unit *x = unitAt(cat, u);
  std::string s = formatNumber(v, f);
  if (x) {
    s += ' ';
    s += x->symbol;
  }
  return s;
}

Fraction nearestFraction(double v, int maxDen) {
  Fraction fr;
  if (!std::isfinite(v)) return fr;
  if (maxDen < 1) maxDen = 1;
  if (maxDen > 1024) maxDen = 1024;
  const double a = std::fabs(v);
  if (a > 1e15) {  // beyond exact long long ticks; not a tape-measure number anyway
    fr.negative = v < 0;
    fr.whole = (long long)std::floor(a + 0.5);
    fr.error = v - (fr.negative ? -1.0 : 1.0) * (double)fr.whole;
    fr.exact = false;
    return fr;
  }
  const long long ticks = (long long)std::floor(a * maxDen + 0.5);
  fr.whole = ticks / maxDen;
  long long num = ticks % maxDen, den = maxDen;
  long long g = den;
  for (long long x = num; x;) {  // gcd(num, den)
    const long long t = g % x;
    g = x;
    x = t;
  }
  if (num) {
    num /= g;
    den /= g;
  }
  fr.num = (int)num;
  fr.den = num ? (int)den : 1;
  fr.negative = v < 0 && ticks != 0;
  const double approx = (double)ticks / maxDen;
  fr.error = (v < 0 ? -1 : 1) * (a - approx);
  fr.exact = std::fabs(fr.error) <= 1e-9 * (a > 1 ? a : 1);
  if (fr.exact) fr.error = 0;
  return fr;
}

namespace {

std::string fractionText(const Fraction &fr) {
  std::string s;
  if (!fr.exact) s += '~';
  if (fr.negative) s += '-';
  if (fr.whole || !fr.num) s += std::to_string(fr.whole);
  if (fr.num) {
    if (fr.whole) s += ' ';
    s += std::to_string(fr.num) + "/" + std::to_string(fr.den);
  }
  return s;
}

}  // namespace

std::string formatFraction(double v, int maxDen, bool showError) {
  if (!std::isfinite(v)) return formatNumber(v);
  const Fraction fr = nearestFraction(v, maxDen);
  std::string s = fractionText(fr);
  if (showError && !fr.exact) {
    NumberFormat nf;
    nf.digits = 2;
    s += " (";
    if (fr.error > 0) s += '+';
    s += formatNumber(fr.error, nf) + ")";
  }
  return s;
}

std::string formatFeetInches(double inches, int fracDen, int decimals) {
  if (!std::isfinite(inches) || std::fabs(inches) > 1e13) return formatNumber(inches) + "\"";
  const bool neg = inches < 0;
  const double a = std::fabs(inches);
  long long feet;
  std::string in;
  bool any;
  if (fracDen > 0) {
    if (fracDen > 1024) fracDen = 1024;
    // Round once, in whole ticks, so 11.999" becomes 1' 0" rather than 0' 12".
    const long long ticks = (long long)std::floor(a * fracDen + 0.5);
    const long long perFoot = 12LL * fracDen;
    feet = ticks / perFoot;
    const Fraction fr = nearestFraction((double)(ticks % perFoot) / fracDen, fracDen);
    in = fractionText(fr);
    any = ticks != 0;
    if (std::fabs((double)ticks / fracDen - a) > 1e-9 * (a > 1 ? a : 1)) in = "~" + in;
  } else {
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    const double scale = std::pow(10.0, decimals);
    const long long ticks = (long long)std::floor(a * scale + 0.5);
    const long long perFoot = (long long)(12 * scale);
    feet = ticks / perFoot;
    char buf[48];
    snprintf(buf, sizeof buf, "%.*f", decimals, (double)(ticks % perFoot) / scale);
    trimZeros(buf);
    in = buf;
    any = ticks != 0;
  }
  // "~" belongs in front of the whole thing, not inside it.
  const bool approx = !in.empty() && in[0] == '~';
  if (approx) in.erase(0, 1);
  std::string s = approx ? "~" : "";
  if (neg && any) s += '-';
  if (feet) s += std::to_string(feet) + "' ";
  s += in + "\"";
  return s;
}

std::string formatDuration(double seconds) {
  if (!std::isfinite(seconds) || std::fabs(seconds) >= 1e12) return formatNumber(seconds) + " s";
  long long ms = (long long)std::floor(std::fabs(seconds) * 1000 + 0.5);
  const bool neg = seconds < 0 && ms;
  const long long days = ms / 86400000;
  ms %= 86400000;
  const int h = (int)(ms / 3600000), m = (int)(ms / 60000 % 60), s = (int)(ms / 1000 % 60);
  const int frac = (int)(ms % 1000);
  char buf[64];
  int n;
  if (days) n = snprintf(buf, sizeof buf, "%s%lldd %02d:%02d:%02d", neg ? "-" : "", days, h, m, s);
  else n = snprintf(buf, sizeof buf, "%s%d:%02d:%02d", neg ? "-" : "", h, m, s);
  if (frac && n > 0 && n < (int)sizeof buf - 5) {
    snprintf(buf + n, sizeof buf - n, ".%03d", frac);
    size_t len = strlen(buf);
    while (buf[len - 1] == '0') buf[--len] = 0;
  }
  return buf;
}

}  // namespace units
}  // namespace mt
