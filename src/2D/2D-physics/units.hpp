#pragma once

// SI constants and unit conversions.
// All unit conversions are handled here, please NEVER mix units outside this file, the program WILL break if you do. (also prob cause a memory leak)
namespace units {

constexpr double G_SI     = 6.67430e-11;    // m^3 kg^-1 s^-2, gravitational constant in SI units
constexpr double c_SI     = 299792458.0;     // m/s, speed of light in SI units
constexpr double MSUN_KG  = 1.98847e30;      // kg, mass of the sun in kilograms

// Convert solar masses to geometric mass parameter (meters): M_geom = G*M_kg/c^2
inline double solarMassToGeomMeters(double msun) { 
    return G_SI * msun * MSUN_KG / (c_SI * c_SI);
}

inline double metersToGeometric(double x) {
    return x;  // Already in geometric units when G=c=1, so this is a no-op.
}

} // namespace units
