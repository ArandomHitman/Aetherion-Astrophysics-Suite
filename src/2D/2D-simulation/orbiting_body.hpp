#pragma once
#include "../2D-physics/schwarzschild.hpp"
#include "../2D-physics/geodesic.hpp"
#include "../2D-utils/presets_2d.hpp"
#include "research_data.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <deque>
#include <utility>
#include <string>

// Measured physical quantities attached to a body.
struct Measurement {
    double timeDilation     = 1.0;   // dt/dτ = E/f(r)
    double redshift         = 1.0;   // 1+z total (gravitational + Doppler)
    double orbitalVelocity  = 0.0;   // local 3-velocity magnitude
    double dopplerFactor    = 1.0;   // Doppler-only contribution
    double properTime       = 0.0;   // accumulated proper time
    double coordinateTime   = 0.0;   // accumulated coordinate time
};

// An orbiting body integrated on exact Schwarzschild timelike geodesics.
// Uses conserved specific energy E and angular momentum L.
struct OrbitingBody {
    // Conserved quantities
    double E  = 1.0;   // specific energy (E/mc²)
    double L  = 0.0;   // specific angular momentum (L/(mc))

    // Geodesic state
    double r   = 10.0;
    double phi = 0.0;
    double vr  = 0.0;  // dr/dτ

    // User-facing parameters (for UI display and reinit)
    double nominalA = 10.0;
    double nominalEcc = 0.3;

    // Galaxy system metadata
    std::string    label;
    GalaxyBodyType bodyType = GalaxyBodyType::Star;
    bool           isGalaxyBody = false;

    Measurement measurement;
    bool captured = false;

    // Research tracking
    PrecessionTracker      precession;
    ConservationTracker    conservation;
    NumericalErrorTracker  errorTracker;

    // Orbit trail
    static constexpr size_t MAX_TRAIL = 3000;
    std::deque<std::pair<double,double>> trail;
    int trailCounter = 0;

    OrbitingBody(const Schwarzschild& bh, double a = 10.0, double ecc = 0.3) {
        initFromKeplerian(bh, a, ecc);
    }

    // Returns true if periapsis was clamped to outside ISCO.
    bool initFromKeplerian(const Schwarzschild& bh, double a, double ecc) {
        nominalA = a;
        nominalEcc = ecc;
        captured = false;
        bool clamped = false;

        double r_peri = a * (1.0 - ecc);
        double r_apo  = a * (1.0 + ecc);

        // Enforce periapsis outside ISCO for stability
        if (r_peri < bh.isco() + 0.1 * bh.M) {
            r_peri = bh.isco() + 0.1 * bh.M;
            r_apo = std::max(r_apo, r_peri + 0.1 * bh.M);
            clamped = true;
        }

        auto params = bh.boundOrbitEL(r_peri, r_apo);
        E = params.E;
        L = params.L;

        // Start at apoapsis, radially stationary
        r   = r_apo;
        phi = 0.0;
        vr  = 0.0;
        measurement = Measurement{};
        trail.clear();
        trail.emplace_back(worldX(), worldY());
        trailCounter = 0;

        // Init research trackers
        precession.reset();
        conservation.init(E);
        errorTracker.reset();
        return clamped;
    }

    // Init for circular orbit at exact radius (for ISCO tests)
    // "exact" is doing some heavy lifting here, we give it the exactly correct E and L for a circular orbit,
    // then immediately kick vr by a tiny amount in startISCOTest() to see what happens.
    // spoiler: the unstable one falls in. every single time. It's 3am, but goddamn that's satisfying...
    void initCircularOrbit(const Schwarzschild& bh, double r0) {
        auto params = bh.circularOrbitEL(r0);
        E = params.E;
        L = params.L;
        r = r0;
        phi = 0.0;
        vr = 0.0;
        nominalA = r0;
        nominalEcc = 0.0;
        captured = false;
        measurement = Measurement{};
        trail.clear();
        trail.emplace_back(worldX(), worldY());
        trailCounter = 0;
        precession.reset();
        conservation.init(E);
        errorTracker.reset();
    }

    // Init for radial infall from rest at r0 (L=0)
    void initRadialInfall(const Schwarzschild& bh, double r0) {
        // E = sqrt(f(r0)) is the specific energy for an object at rest at r0.
        // dropping from rest at finite r rather than infinity means E < 1, which is correct —
        // it takes energy to have been assembled at r0 against gravity.
        // L=0 means pure radial infall — no angular momentum, just falls straight in.
        // this is one of those cases where the GR result is genuinely different from Newtonian:
        // the proper-time fall is finite even though coordinate time diverges at the horizon.
        double fr = bh.f(r0);
        E = std::sqrt(std::max(1e-15, fr));  // Energy for rest at r0
        L = 0.0;
        r = r0;
        phi = 0.0;
        vr = 0.0;
        nominalA = r0;
        nominalEcc = 1.0;
        captured = false;
        measurement = Measurement{};
        trail.clear();
        trail.emplace_back(worldX(), worldY());
        trailCounter = 0;
        precession.reset();
        conservation.init(E);
        errorTracker.reset();
    }

    double worldX() const { return r * std::cos(phi); }
    double worldY() const { return r * std::sin(phi); }
    double radius() const { return r; }

    void update(const Schwarzschild& bh, double dt) {
        if (captured) return;

        double fr = bh.f(r);
        if (fr <= 1e-10) { captured = true; return; }

        // Precession detection (before integration step)
        precession.detectPeriapsis(vr, phi);

        // Convert coordinate time dt → proper time dτ:  dτ = dt·f(r)/E
        double dtau_total = dt * fr / E;

        // Accumulate coordinate time
        measurement.coordinateTime += dt;

        // Adaptive sub-stepping for numerical stability
        // Scale step size with M so that nSteps stays reasonable at any mass
        // the 0.05*M step limit was found by trial and error — any larger and energy starts drifting
        // noticeably near the horizon. any smaller and we're burning CPU for no benefit at large r.
        double maxStep = 0.05 * bh.M;
        int nSteps = std::max(1, static_cast<int>(std::abs(dtau_total) / maxStep) + 1);
        nSteps = std::min(nSteps, 4000);  // safety cap — if we ever hit 4000 something is very wrong and we'd rather be slightly inaccurate than freeze the frame
        double h = dtau_total / nSteps;

        TimelikeState state = { r, phi, vr };
        for (int i = 0; i < nSteps; ++i) {
            // RK4 error estimation via step-doubling (periodically)
            // step-doubling: take one full step, then two half-steps, compare results.
            // error ≈ (single - double_half) / 15, where the /15 comes from Richardson extrapolation for a 4th-order method.
            // we only do this every checkInterval steps because it's essentially 3x the normal computation cost —
            // doing it every step would tank performance for no real gain
            if (errorTracker.shouldCheck()) {
                auto singleStep = stepTimelikeGeodesic(bh, state, L, h);
                auto halfStep1  = stepTimelikeGeodesic(bh, state, L, h * 0.5);
                auto halfStep2  = stepTimelikeGeodesic(bh, halfStep1, L, h * 0.5);
                double error = std::abs(singleStep.r - halfStep2.r) / 15.0;
                errorTracker.recordError(error);
                state = singleStep;  // keep standard result
            } else {
                state = stepTimelikeGeodesic(bh, state, L, h);
            }

        // Capture detection: crossed or touched the horizon
        // 1.001 is a small margin because floating point arithmetic near r=2M is a nightmare.
        // in principle we should never have r < horizon mid-step if the sub-stepping is tight enough,
        // but "in principle" and "in practice" are very different things when you're multiplying
        // doubles together a million times near a singularity.
        if (state.r <= bh.horizon() * 1.001) {
                captured = true;
                r = state.r; phi = state.phi; vr = state.vr;
                return;
            }
        }

        r   = state.r;
        phi = state.phi;
        vr  = state.vr;
        measurement.properTime += dtau_total;

        // Record orbit trail (every few steps)
        // only record every other step to keep memory under control.
        // at 60fps with up to 4000 sub-steps per frame we'd fill MAX_TRAIL in well under a second otherwise,
        // and then you'd spend more time popping from the deque than actually simulating
        trailCounter++;
        if (trailCounter % 2 == 0) {
            trail.emplace_back(worldX(), worldY());
            if (trail.size() > MAX_TRAIL)
                trail.pop_front();
        }

        updateMeasurements(bh);

        // Update energy conservation tracker
        double E_now = bh.computeEnergy(r, vr, L);
        conservation.update(E_now, measurement.properTime);
    }

    void updateMeasurements(const Schwarzschild& bh) {
        double fr = bh.f(r);
        if (fr <= 1e-10) return; // inside or at the horizon, all measurements are garbage here anyway

        // Lorentz factor as seen by static observer at r:  γ = E/f(r)
        // this is the time dilation relative to coordinate time, NOT relative to infinity.
        // the display shows it as dt/dτ which is the same thing but phrased more intuitively.
        double gamma = E / fr;
        double v2 = std::max(0.0, 1.0 - 1.0 / (gamma * gamma));
        measurement.orbitalVelocity = std::sqrt(std::min(v2, 0.9999));
        measurement.timeDilation = gamma; 

        // Local velocity components (in FIDO frame):
        //   v_r_local   = v_r / E         (radial)
        //   v_phi_local = L·√f / (r·E)    (tangential)
        double sqf = std::sqrt(fr);
        double vr_local   = vr / E;
        double vphi_local = L * sqf / (r * E); // warcrime? probably!

        // Velocity component away from observer at (-∞, 0):
        // In local (r̂, φ̂) frame, "away" direction = cos(φ) r̂ - sin(φ) φ̂
        double beta_away = vr_local * std::cos(phi) - vphi_local * std::sin(phi);

        // Total redshift (GR exact for this geometry):
        //   1+z = γ·(1 + β_away) / √f = E·(1 + β_away) / f
        measurement.redshift = E * (1.0 + beta_away) / fr;
        measurement.dopplerFactor = gamma * (1.0 + beta_away);
    }
};
