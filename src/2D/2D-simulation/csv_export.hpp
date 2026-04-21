#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include "research_data.hpp"

// Forward declarations — avoid circular includes
struct Photon;
struct OrbitingBody;

namespace CSVExport {

// Export orbit trail (worldX, worldY, r, phi)
template<typename TrailContainer>
inline bool exportOrbitData(
    const std::string& filename,
    const TrailContainer& trail)
{
    std::ofstream out(filename);
    if (!out) return false;
    out << "worldX,worldY,radius,phi\n";
    for (const auto& [x, y] : trail) {
        double r = std::hypot(x, y);
        double p = std::atan2(y, x);
        out << x << "," << y << "," << r << "," << p << "\n";
    }
    return true;
}

// Export deflection table (b, deflection_rad, deflection_deg, captured)
inline bool exportDeflectionTable(
    const std::string& filename,
    const std::vector<PhotonDeflection>& table)
{
    std::ofstream out(filename);
    if (!out) return false;
    out << "impact_parameter,deflection_rad,deflection_deg,captured\n";
    for (const auto& d : table) {
        out << d.impactParameter << ","
            << d.deflectionAngle << ","
            << d.deflectionDeg << ","
            << (d.captured ? 1 : 0) << "\n";
    }
    return true;
}

// Export energy conservation drift history
inline bool exportConservationHistory(
    const std::string& filename,
    const ConservationTracker& tracker)
{
    std::ofstream out(filename);
    if (!out) return false;
    out << "proper_time,energy_drift_relative\n";
    for (const auto& [t, d] : tracker.driftHistory) {
        out << t << "," << d << "\n";
    }
    return true;
}

// Export precession data
inline bool exportPrecessionData(
    const std::string& filename,
    const PrecessionTracker& tracker)
{
    std::ofstream out(filename);
    if (!out) return false;
    out << "orbit_number,periapsis_phi_rad,precession_rad,precession_deg\n";
    for (size_t i = 0; i < tracker.precessionPerOrbit.size(); ++i) {
        out << (i + 1) << ","
            << (i + 1 < tracker.periapsisAngles.size() ? tracker.periapsisAngles[i+1] : 0.0) << ","
            << tracker.precessionPerOrbit[i] << ","
            << (tracker.precessionPerOrbit[i] * 180.0 / M_PI) << "\n";
    }
    return true;
}

} // namespace CSVExport
