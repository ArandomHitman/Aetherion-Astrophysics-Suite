#pragma once
#include <cmath>
#include <algorithm>

// Encapsulates all Schwarzschild black hole math (geometric units: G = c = 1).
// This is the scientific API — all metric-level computations live here.
struct Schwarzschild {
    double M = 1.0;

    /*--------- Characteristic radii ---------*/
    double horizon() const { return 2.0 * M; }          // r_s = 2M, the classic Schwarzschild radius. simplest result in all of GR and yet here we are building a whole app around it
    double photonSphere() const { return 3.0 * M; }     // r=3M -- photons can orbit here indefinitely (in theory). in practice any tiny perturbation sends them spiraling in or out, it's an unstable equilibrium
    double isco() const { return 6.0 * M; }             // innermost stable circular orbit -- inside this, stable orbits don't exist and stuff just... plunges in. the simulation makes this very visible
    double criticalImpact() const { return 3.0 * std::sqrt(3.0) * M; } // b_crit = 3√3·M. photons with b < this get captured no matter what. derived from the photon sphere condition, which is a fun derivation if you have an afternoon

    // Metric function f(r) = 1 - 2M/r
    // goes to zero at the horizon (r=2M) and negative inside it. if this returns something negative you're inside the event horizon, which is fine for the math but the physics gets weird
    double f(double r) const { return 1.0 - 2.0 * M / r; }

    /*--------- Time dilation & redshift ---------*/
    // diverges as r → 2M, which is correct — a clock at the horizon runs infinitely slowly relative to infinity
    // the max(1e-12, ...) is just so we don't divide by zero when someone queries exactly at the horizon, which they will
    double timeDilation(double r) const {
        return 1.0 / std::sqrt(std::max(1e-12, f(r)));
    }

    // Gravitational redshift factor: ν_obs/ν_emit for static source at r_emit, observer at r_obs
    double redshift(double r_emitter, double r_observer) const {
        return std::sqrt(
            std::max(1e-12, f(r_observer)) /
            std::max(1e-12, f(r_emitter))
        );
    }

    /*--------- Effective potentials ---------*/
    // Timelike: V_eff = (1 - 2M/r)(1 + L²/r²)
    double Veff_timelike(double r, double L) const {
        return f(r) * (1.0 + L * L / (r * r));
    }

    // Null: V_eff = (1 - 2M/r) L²/r²
    double Veff_null(double r, double L) const {
        return f(r) * L * L / (r * r);
    }

    /*--------- Radial acceleration for timelike geodesic (proper time) ---------*/
    // d²r/dτ² = -M/r²  (Newtonian gravity)
    //         + L²/r³  (centrifugal repulsion, same as Newtonian)
    //         - 3ML²/r⁴  (GR correction term — this is the whole reason orbits inside ISCO are unstable)
    // that last term is small at large r but absolutely dominates near the horizon. without it, RK4 orbits near 3M look stable and they really aren't.
    double radialAcceleration(double r, double L) const {
        double r2 = r * r, r3 = r2 * r, r4 = r3 * r;
        return -M / r2 + L * L / r3 - 3.0 * M * L * L / r4;
    }

    /*--------- Conserved quantities ---------*/
    struct OrbitalParams { double E; double L; };

    // E, L for circular orbit at coordinate radius r (requires r > 3M)
    // NOTE: returns {1.0, 0.0} as a fallback for r ≤ 3M, which is kind of a lie (that's not a valid orbit)
    // but at least it doesn't crash. probably should return an error flag but I didn't want to complicate the API
    OrbitalParams circularOrbitEL(double r) const {
        double denom = r - 3.0 * M;
        if (denom <= 0.0) return {1.0, 0.0};
        double L2 = M * r * r / denom;
        double E2 = (r - 2.0 * M) * (r - 2.0 * M) / (r * denom);
        return { std::sqrt(std::max(0.0, E2)), std::sqrt(std::max(0.0, L2)) };
    }

    // E, L for bound orbit from periapsis r_p and apoapsis r_a
    // Uses exact Schwarzschild turning-point equations:
    //   E² = f(r)(1 + L²/r²) at both r_p and r_a
    // this is actually some fairly gnarly algebra — you solve two simultaneous equations for E² and L².
    // I sat with pen and paper for a while on this one. the fallback to circularOrbitEL covers the edge cases
    // where the two radii are nearly identical (which would make denom blow up).
    // For very high eccentricity orbits where r_peri approaches 3M (photon sphere),
    // the orbital dynamics become unstable. Clamp r_peri to be safely above 3M.
    OrbitalParams boundOrbitEL(double r_peri, double r_apo) const {
        const double MIN_PERI = 3.0 * M * 1.01;  // Clamp periapsis to just above photon sphere
        
        if (r_peri < MIN_PERI) {
            r_peri = MIN_PERI;  // Clamp and continue (implicit circular fallback if r_peri > r_apo now)
        }

        if (std::abs(r_peri - r_apo) < 1e-8 * M)
            return circularOrbitEL(0.5 * (r_peri + r_apo));

        double fp = f(r_peri), fa = f(r_apo);
        double rp2 = r_peri * r_peri, ra2 = r_apo * r_apo;
        double denom = fp / rp2 - fa / ra2;
        if (std::abs(denom) < 1e-30)
            return circularOrbitEL(0.5 * (r_peri + r_apo));

        double L2 = (fa - fp) / denom;
        if (L2 <= 0.0)
            return circularOrbitEL(0.5 * (r_peri + r_apo));

        double E2 = fp * (1.0 + L2 / rp2);
        return { std::sqrt(std::max(0.0, E2)), std::sqrt(L2) };
    }

    /*--------- Escape velocity ---------*/
    // Relativistic escape velocity at radius r (as fraction of c)
    // technically this is the Newtonian expression and happens to give the right answer here — the GR derivation
    // gives the same formula for a radially infalling particle from rest at infinity. convenient coincidence.
    double escapeVelocity(double r) const {
        return std::sqrt(2.0 * M / r);
    }

    /*--------- Tidal force ---------*/
    // Tidal acceleration per unit separation along radial direction
    // scales as 1/r³ so it gets absolutely vicious near the horizon.
    // real astrophysical tidal disruption events (TDEs) are some of the brightest things in the universe.
    // our simulation just turns a body label red, which is considerably less dramatic.
    double tidalForce(double r) const {
        return 2.0 * M / (r * r * r);
    }

    // Tidal stress on an object of size objectSize at radius r
    double tidalStress(double r, double objectSize) const {
        return tidalForce(r) * objectSize;
    }

    // Tidal disruption threshold: returns true if tidal force exceeds
    // self-gravity approximation for a body of mass m_obj, radius R_obj
    // this is the super-simplified Roche limit approximation assuming a uniform-density sphere.
    // real tidal disruption is way messier — this just tells you "approximately yes/no" which is good enough for display purposes
    bool tidallyDisrupted(double r, double m_obj, double R_obj) const {
        if (m_obj <= 0.0 || R_obj <= 0.0) return false;
        double f_tidal = tidalForce(r) * R_obj;
        double f_self  = m_obj / (R_obj * R_obj);
        return f_tidal > f_self;
    }

    /*--------- Bondi accretion radius ---------*/
    // cs = sound speed as fraction of c
    // the Bondi radius is where the black hole's gravity "wins" over the thermal pressure of the gas.
    // inside this radius, gas will tend to accrete. outside it, gas mostly doesn't care about the BH.
    // fun to show in the galaxy preset. less fun to explain at parties.
    double bondiRadius(double cs) const {
        if (cs <= 0.0) return 1e30; // physically meaningless but at least we don't divide by zero and explode
        return 2.0 * M / (cs * cs);
    }

    // Sound speed from gas temperature (returns cs/c)
    static double soundSpeedFromTemp(double T_kelvin) {
        constexpr double kB = 1.380649e-23;     // J/K
        constexpr double mu = 0.6;               // mean molecular weight for fully ionized plasma (roughly half hydrogen half helium)
        constexpr double mH = 1.6726219e-27;     // hydrogen mass kg
        constexpr double c  = 299792458.0;       // m/s
        // TODO: this assumes an ideal gas, which is... optimistic for accreting plasma near a black hole.
        // good enough for Bondi radius order-of-magnitude estimates though.
        double cs = std::sqrt(kB * T_kelvin / (mu * mH));
        return cs / c;
    }

    /*--------- Compute energy from state ---------*/
    // E² = vr² + f(r)(1 + L²/r²) for timelike geodesics
    // this is just the first integral of the geodesic equations rearranged.
    // we recompute E from the current state periodically to measure how much the integrator has
    // drifted from the true conserved value. ideally this should return exactly E_initial forever.
    // it does not. RK4 is not a symplectic integrator and makes no promises about energy conservation.
    // this is fine. this is fine. the drift is small. it's fine.
    double computeEnergy(double r, double vr, double L) const {
        return std::sqrt(std::max(0.0,
            vr * vr + f(r) * (1.0 + L * L / (r * r))));
    }

    /*--------- Theoretical precession (weak-field limit) ---------*/
    // Δφ = 6πM / a(1-e²) per orbit — this is the famous GR precession formula (same one that explains Mercury's orbit)
    // IMPORTANT: weak-field approximation only! valid when a >> M.
    // for tight orbits near the ISCO the simulation's measured precession will be significantly larger than this prediction
    // and that's not a bug — it's the actual physics working correctly. the formula just doesn't apply there.
    double theoreticalPrecession(double a, double ecc) const {
        double denom = a * (1.0 - ecc * ecc);
        if (denom <= 0.0) return 0.0;
        return 6.0 * M_PI * M / denom;
    }

    /*--------- Gravitational redshift at r (observer at infinity) ---------*/
    double gravitationalRedshiftZ(double r) const {
        double fr = f(r);
        if (fr <= 1e-12) return 1e10;
        return 1.0 / std::sqrt(fr) - 1.0;
    }

    /*--------- ISCO stability classification ---------*/
    // Returns "Stable", "Critical", or "Unstable" for orbit at r
    // the 1% margins are intentional — without them, an orbit sitting right at 6M would flicker between
    // "Stable" and "Unstable" every frame due to floating-point noise, which looks terrible in the data panel
    const char* stabilityClassification(double r) const {
        double r_isco = isco();
        if (r > r_isco * 1.01) return "Stable";
        if (r < r_isco * 0.99) return "Unstable";
        return "Critical";
    }

    /*--------- Null geodesic periapsis finder ---------*/
    // bisection search on r²/f(r) = b² — the turning point condition for a null geodesic with impact parameter b
    // 200 iterations is overkill (you hit machine precision around iteration 50-60) but iterations are cheap
    // and I'd rather not have subtle accuracy issues from cutting it short
    // NOTE: returns -1.0 if b ≤ b_crit (photon is captured before reaching a turning point at all)
    double findPeriapsis(double b) const {
        if (b <= criticalImpact()) return -1.0;

        auto g = [this, b](double r) -> double {
            return (r * r) / f(r) - b * b;
        };

        // lo = 3M because the turning point is always outside the photon sphere for b > b_crit
        // hi = max(b*10, 1e5*M) — for very large b the turning point approaches r ≈ b, so we need hi to scale with b
        double lo = 3.0 * M;
        double hi = std::max(b * 10.0, 1e5 * M);

        for (int iter = 0; iter < 200; ++iter) {
            double mid = 0.5 * (lo + hi);
            if (g(mid) > 0) hi = mid; else lo = mid;
        }
        return 0.5 * (lo + hi);
    }
};
