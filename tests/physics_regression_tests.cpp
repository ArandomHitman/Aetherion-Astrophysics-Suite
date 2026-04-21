#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "src/2D/2D-physics/geodesic.hpp"
#include "src/2D/2D-physics/schwarzschild.hpp"
#include "src/2D/2D-physics/units.hpp"
#include "src/2D/2D-simulation/photon.hpp"

namespace {

bool testSolarGrazingDeflection() {
    // Reference: weak-field GR prediction for a grazing solar ray is ~1.75 arcsec.
    // In geometric units (G=c=1): alpha ~= 4M / b.
    Schwarzschild bh;
    bh.M = units::solarMassToGeomMeters(1.0);

    constexpr double solarRadiusMeters = 6.9634e8;
    constexpr double expectedArcsec = 1.75;
    constexpr double expectedDeg = expectedArcsec / 3600.0;

    Photon photon;
    photon.impactParameter = solarRadiusMeters;
    photon.computePath(bh, /*rMax=*/1.0e12, /*dphi=*/0.0015);

    if (photon.captured) {
        std::cerr << "[FAIL] solar deflection: photon was unexpectedly captured\n";
        return false;
    }

    const double measuredDeg = std::abs(photon.deflectionDeg);
    const double absErrDeg = std::abs(measuredDeg - expectedDeg);

    // Conservative beta tolerance: 0.12 arcsec allows discretization + finite-rMax effects.
    constexpr double toleranceDeg = 0.12 / 3600.0;
    if (absErrDeg > toleranceDeg) {
        std::cerr << "[FAIL] solar deflection: expected ~" << expectedDeg
                  << " deg (" << expectedArcsec << " arcsec), got " << measuredDeg
                  << " deg, abs err " << absErrDeg << " deg\n";
        return false;
    }

    std::cout << "[PASS] solar deflection: " << measuredDeg << " deg (target "
              << expectedDeg << " deg)\n";
    return true;
}

bool testTimelikeConservationLongRun() {
    Schwarzschild bh;
    bh.M = 1.0;

    // Moderately eccentric bound orbit, safely outside strong-instability region.
    const double rPeri = 12.0 * bh.M;
    const double rApo = 28.0 * bh.M;
    auto params = bh.boundOrbitEL(rPeri, rApo);

    TimelikeState state{rPeri, 0.0, 0.0};
    const double E0 = bh.computeEnergy(state.r, state.vr, params.L);
    const double L0 = params.L;

    constexpr double dtau = 0.02;
    constexpr int steps = 60000;

    double maxRelEnergyDrift = 0.0;
    double maxRelLDrift = 0.0;

    TimelikeState prev = state;
    for (int i = 0; i < steps; ++i) {
        state = stepTimelikeGeodesic(bh, state, params.L, dtau);

        if (!std::isfinite(state.r) || !std::isfinite(state.phi) || !std::isfinite(state.vr)) {
            std::cerr << "[FAIL] conservation: non-finite state at step " << i << "\n";
            return false;
        }

        const double E = bh.computeEnergy(state.r, state.vr, params.L);
        const double relEDrift = std::abs(E - E0) / std::max(1e-15, std::abs(E0));
        maxRelEnergyDrift = std::max(maxRelEnergyDrift, relEDrift);

        // Infer L numerically from dphi/dtau to check angular momentum consistency.
        const double dphi = state.phi - prev.phi;
        const double rAvg = 0.5 * (state.r + prev.r);
        const double Linferred = (dphi / dtau) * rAvg * rAvg;
        const double relLDrift = std::abs(Linferred - L0) / std::max(1e-15, std::abs(L0));
        maxRelLDrift = std::max(maxRelLDrift, relLDrift);

        prev = state;
    }

    // Beta gate: generous enough for RK4 long runs, strict enough to catch regressions.
    constexpr double maxEnergyDriftAllowed = 5e-3;
    constexpr double maxLDriftAllowed = 2e-2;

    if (maxRelEnergyDrift > maxEnergyDriftAllowed) {
        std::cerr << "[FAIL] conservation: max relative energy drift " << maxRelEnergyDrift
                  << " exceeds " << maxEnergyDriftAllowed << "\n";
        return false;
    }
    if (maxRelLDrift > maxLDriftAllowed) {
        std::cerr << "[FAIL] conservation: max relative angular momentum drift " << maxRelLDrift
                  << " exceeds " << maxLDriftAllowed << "\n";
        return false;
    }

    std::cout << "[PASS] conservation: max dE/E=" << maxRelEnergyDrift
              << ", max dL/L=" << maxRelLDrift << "\n";
    return true;
}

} // namespace

int main() {
    bool ok = true;

    ok = testSolarGrazingDeflection() && ok;
    ok = testTimelikeConservationLongRun() && ok;

    if (!ok) {
        std::cerr << "Physics regression tests FAILED\n";
        return EXIT_FAILURE;
    }

    std::cout << "Physics regression tests PASSED\n";
    return EXIT_SUCCESS;
}
