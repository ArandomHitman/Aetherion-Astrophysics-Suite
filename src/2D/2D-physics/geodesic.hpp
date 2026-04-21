#pragma once
#include "schwarzschild.hpp"
#include "integrator.hpp"

/*--------- Null geodesic (Binet equation) ---------*/
// The Binet substitution u = 1/r is genuinely clever — it turns the messy r(φ) ODE into a nice polynomial form.
// d²u/dφ² + u = 3Mu²   (the Schwarzschild Binet equation for null geodesics)
// the right-hand side 3Mu² is the GR correction; without it you get straight lines (Newtonian).
// I did not derive this myself, in case that wasn't obvious.
// State for photon ray tracing: d²u/dφ² + u = 3Mu², u = 1/r

/*--------- Timelike geodesic (proper time parameterization) ---------*/
struct GeodesicState { // du du du du Max Verstappen
    double u;        
    double du_dphi;  

    GeodesicState operator+(const GeodesicState& o) const {
        return { u + o.u, du_dphi + o.du_dphi };
    }
    GeodesicState operator*(double s) const {
        return { u * s, du_dphi * s };
    }
};

inline GeodesicState stepNullGeodesic( // I totally did not just find this from stackoverflow...
    const Schwarzschild& bh,
    const GeodesicState& state,
    double dphi)
{
    return rk4_step(state, dphi, [&](const GeodesicState& s) -> GeodesicState {
        // derivative of u is just du/dphi, and derivative of du/dphi is the Binet RHS: -u + 3Mu²
        // written out like this it looks deceptively simple for something that handles light bending near a black hole
        return { s.du_dphi, -s.u + 3.0 * bh.M * s.u * s.u };
    });
}

/*--------- Timelike geodesic (proper time parameterization) ---------*/
// For massive particles: uses conserved E, L and the exact Schwarzschild
// geodesic equations:
//   dr/dτ  = v_r              (radial proper velocity)
//   dφ/dτ  = L/r²             (angular velocity in proper time)
//   dv_r/dτ = -M/r² + L²/r³ - 3ML²/r⁴   (from V_eff derivative)
// three equations, three unknowns. straightforward to integrate with RK4.
// the only subtle part is that this is parameterized by proper time τ, not coordinate time t.
// the distinction matters a lot near the horizon where they diverge wildly from each other,
// which is also where everything goes sideways numerically. just GR things.
struct TimelikeState {
    double r;     // radial coordinate
    double phi;   // azimuthal angle
    double vr;    // dr/dτ

    TimelikeState operator+(const TimelikeState& o) const {
        return { r + o.r, phi + o.phi, vr + o.vr };
    }
    TimelikeState operator*(double s) const { // scale the rate of change in state, not the state itself, since r, phi, vr are not linear variables
        return { r * s, phi * s, vr * s };
    }
};

// Step a timelike geodesic by proper time dτ using RK4.
// L is the conserved specific angular momentum.
inline TimelikeState stepTimelikeGeodesic(
    const Schwarzschild& bh,
    const TimelikeState& state,
    double L,
    double dtau)
{
    // Clamp r to just outside horizon before stepping — L/r² and
    // radialAcceleration both diverge as r → 2M, producing NaN/Inf.
    const double rMin = bh.horizon() * 1.01;
    TimelikeState clamped = state;
    if (clamped.r < rMin) clamped.r = rMin;

    auto result = rk4_step(clamped, dtau, [&](const TimelikeState& s) -> TimelikeState {
        double r_safe = std::max(s.r, rMin);
        return {
            s.vr,                                   // dr/dτ
            L / (r_safe * r_safe),                  // dφ/dτ
            bh.radialAcceleration(r_safe, L)        // dv_r/dτ
        };
    });

    // Post-step NaN/Inf guard — if the integrator produced garbage,
    // freeze the state rather than propagating poison values.
    if (!std::isfinite(result.r) || !std::isfinite(result.phi) || !std::isfinite(result.vr)) {
        return state; // return unchanged
    }

    return result;
}
