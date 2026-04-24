/*--------------- HEADERS ---------------*/
#pragma once
#include "../2D-physics/schwarzschild.hpp"
#include "../2D-physics/geodesic.hpp"
#include "../2D-core/types.hpp"
#include <vector>
#include <cmath>
#include <chrono>

/*---------------- main struct for photons ------------------*/
struct Photon {
    double impactParameter = 0.0;
    bool captured = false;
    std::vector<Vec2> path;      // downsampled for display
    std::vector<Vec2> fullPath;  // full-resolution for export

    // deflection tracking logic 
    double deflectionAngle = 0.0;   // total deflection in radians
    double deflectionDeg   = 0.0;   // total deflection in degrees
    double phiInfinity     = 0.0;   // angle swept periapsis as photon approaches infinity (used for deflection calculation)

    void computePath(const Schwarzschild& bh, double rMax = 1e5, double dphi = 0.0025) { // logic here is a little scuffed, 
        path.clear(); // but basically we integrate the photon's path in small angular steps until it either escapes (r → ∞) or is captured (crosses the event horizon). 
        fullPath.clear();
        captured = false; // We track the path points for rendering, and we also keep track of the deflection angle and the angle at which the photon escapes to infinity for analysis. 
        // The integration uses the null geodesic equations in Schwarzschild spacetime, and we have to be careful to handle the asymptotic behavior correctly to get accurate deflection angles.

        double b = std::abs(impactParameter);
        if (b <= bh.criticalImpact()) { captured = true; return; }

        double rmin = bh.findPeriapsis(b);
        if (rmin <= 0.0) { captured = true; return; }

        // Initial conditions at periapsis (phi=0, du/dphi=0)
        GeodesicState state = { 1.0 / rmin, 0.0 };
        double phi = 0.0;

        // Integrate outgoing half from periapsis
        std::vector<std::pair<double,double>> half;  // (r, phi)
        half.emplace_back(1.0 / state.u, phi);

        // Track last valid outgoing state for asymptotic extrapolation.
        // This is always the most recent step where u > 0 and the photon
        // is heading outward (du/dφ < 0). 
        double ext_phi = phi;
        double ext_u   = state.u;
        double ext_du  = state.du_dphi;

        const double horizonU = 1.0 / (bh.horizon() * 1.001); // capture threshold
        const int maxSteps = 200000; // Call it overenginering, call it paranoia, but I want to avoid infinite loops, thank you very much.
        auto startTime = std::chrono::steady_clock::now();
        constexpr int BUDGET_CHECK_INTERVAL = 512;
        constexpr auto TIME_BUDGET = std::chrono::milliseconds(50);
        for (int i = 0; i < maxSteps; ++i) {
            // Check time budget periodically to avoid stalling the frame
            if ((i & (BUDGET_CHECK_INTERVAL - 1)) == 0 && i > 0) {
                auto now = std::chrono::steady_clock::now();
                if (now - startTime > TIME_BUDGET) break;
            }
            state = stepNullGeodesic(bh, state, dphi);
            phi += dphi;

            // NOTE: Escape: u → 0 means that r → ∞
            if (state.u <= 0.0) break;

            double r = 1.0 / state.u;

            // NOTE: Capture: if the photon crosses the horizon (with a small safety margin) we consider it captured. We check this inside 
            // the loop because the photon could be on an unstable orbit and hover near the horizon for a while before finally crossing it, 
            // and we want to capture that behavior accurately.
            if (state.u >= horizonU) { captured = true; break; }

            // Save outgoing state for extrapolation (u > 0 guaranteed here,
            // du < 0 once photon has passed periapsis and is heading out).
            if (state.du_dphi < 0.0) {
                ext_phi = phi;
                ext_u   = state.u;
                ext_du  = state.du_dphi;
            }

            half.emplace_back(r, phi);

            // Escaped to max distance
            if (r >= rMax) break;
        }

        if (captured) return;

        // Extrapolate φ to r = ∞ using the asymptotic flat-space solution.
        // Far from the black hole the Binet equation reduces to d²u/dφ² + u ≈ 0
        // of which the solution is u = A cos(φ − φ₀).  The remaining angle from an
        // outgoing state (u, du/dφ) to the zero-crossing u = 0 (i.e. r → ∞) is
        // atan2(u, −du/dφ).  Using the last saved outgoing state makes this
        // sub-step accurate regardless of whether the loop terminated because
        // u crossed zero or because r exceeded rMax.


        double phi_inf = ext_phi + std::atan2(ext_u, -ext_du);

        double phi_offset = M_PI + phi_inf;

        // Deflection = 2·φ_∞ - π  (straight line sweeps π)
        phiInfinity     = phi_inf;
        deflectionAngle = 2.0 * phi_inf - M_PI;
        deflectionDeg   = deflectionAngle * 180.0 / M_PI;

        // Build full-resolution path for export BEFORE downsampling.
        // Cap to MAX_EXPORT_HALF to prevent unbounded allocation on near-critical-impact photons.
        // Previously had no cap, so at 120 rays × 200K steps this would cause a memory leak nearing ~500 MB.
        // MAX_EXPORT_HALF gives 5× more detail than display while keeping memory bounded.
        // [FIXED 2026-04-24: added stride-sampled cap, was reserve(2*half.size()) with no limit]
        static constexpr int MAX_EXPORT_HALF = 5000;
        const std::vector<std::pair<double,double>>* exportSrc = &half;
        std::vector<std::pair<double,double>> exportSampled;
        if ((int)half.size() > MAX_EXPORT_HALF) {
            exportSampled.reserve(MAX_EXPORT_HALF);
            float stride = float(half.size() - 1) / float(MAX_EXPORT_HALF - 1);
            for (int s = 0; s < MAX_EXPORT_HALF; ++s)
                exportSampled.push_back(half[size_t(s * stride + 0.5f)]);
            exportSrc = &exportSampled;
        }
        fullPath.reserve(2 * exportSrc->size());
        for (int i = (int)exportSrc->size() - 1; i >= 0; --i) {
            double r = (*exportSrc)[i].first;
            double phi_local = -(*exportSrc)[i].second + phi_offset;
            fullPath.push_back({
                (float)(r * std::cos(phi_local)),
                (float)(r * std::sin(phi_local))
            });
        }
        for (size_t i = 0; i < exportSrc->size(); ++i) {
            double r = (*exportSrc)[i].first;
            double phi_local = (*exportSrc)[i].second + phi_offset;
            fullPath.push_back({
                (float)(r * std::cos(phi_local)),
                (float)(r * std::sin(phi_local))
            });
        }

        // Downsample half-path for display: cap to MAX_DISPLAY_HALF points to
        // prevent huge allocations for strongly-lensed near-critical-impact
        // photons (which can accumulate up to 200K integration steps).
        // I learned this the hard way by running it on Sgr A* and watching the process balloon.
        // do not remove this limit.
        static constexpr int MAX_DISPLAY_HALF = 1000; 
        if ((int)half.size() > MAX_DISPLAY_HALF) {
            std::vector<std::pair<double,double>> sampled;
            sampled.reserve(MAX_DISPLAY_HALF);
            float stride = float(half.size() - 1) / float(MAX_DISPLAY_HALF - 1);
            for (int s = 0; s < MAX_DISPLAY_HALF; ++s)
                sampled.push_back(half[size_t(s * stride + 0.5f)]);
            half = std::move(sampled);
        }
        path.reserve(2 * half.size());

        // Build full path: incoming (mirror of outgoing) then outgoing
        for (int i = (int)half.size() - 1; i >= 0; --i) {
            double r = half[i].first;
            double phi_local = -half[i].second + phi_offset;
            path.push_back({
                (float)(r * std::cos(phi_local)),
                (float)(r * std::sin(phi_local))
            });
        }
        for (size_t i = 0; i < half.size(); ++i) { // for the size of the half vector, push back the full path of the photon, which is the mirror of the outgoing path followed by the outgoing path itself
            double r = half[i].first;
            double phi_local = half[i].second + phi_offset;
            path.push_back({
                (float)(r * std::cos(phi_local)),
                (float)(r * std::sin(phi_local))
            });
        }
    }
};  // fun fact! I spent 10 minutes trying to figure out why my code wouldn't compile, including rewriting a quarter of this file, only to find that everything
    // was caused by forgetting this semicolon! 
