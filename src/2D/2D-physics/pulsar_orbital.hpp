#pragma once
/*
 * pulsar_orbital.hpp — Physics model for a neutron star (pulsar) orbiting a
 * Schwarzschild black hole.
 *
 * The geodesic integration is delegated to the existing OrbitingBody / RK4 pipeline.
 * This file adds:
 *   1. PulsarState       — NS intrinsic properties + adiabatic GW-decay bookkeeping.
 *   2. PulsarOrbitalData — Four observable categories with rolling history deques.
 *   3. Inline physics functions for GW emission (Peters 1964), Shapiro delay,
 *      gravitational redshift, Roche limit, NS binding energy.
 *
 * Units: everything is in geometric units (G = c = 1) unless noted.
 *   1 geometric metre  ≡  1 m physical (since G = c = 1)
 *   Time in geometric metres: t_seconds = t_geom / c_SI
 *
 * GW inspiral is applied adiabatically: each simulation frame the body's
 * conserved E and L are updated to match the decayed (a, e), so the geodesic
 * integration naturally continues on the new orbit without resetting position.
 *
 * A visualization scale factor GW_VIS_SCALE accelerates the inspiral so it is
 * perceptible on human timescales.  All *displayed* quantities (da/dt, merger
 * time, etc.) are the real, unscaled physical values.
 */

#include <cmath>
#include <deque>
#include <utility>
#include <algorithm>
#include <string>
#include "units.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Neutron star / pulsar physical properties ─────────────────────────────────

struct PulsarState {
    double massSolar  = 1.4;        // NS mass (solar masses)
    double massGeom   = 0.0;        // NS mass (geometric metres); computed by init()
    double radiusM    = 1.0e4;      // NS physical radius (m) — ~10 km
    double spinPeriod = 0.033;      // Rotation period (seconds), Crab pulsar-like
    double magField   = 1.0e12;     // Surface dipole B-field (Gauss)

    // Orbital elements tracked here separately from the OrbitingBody's E/L.
    double a = 0.0;                 // current semi-major axis (geometric metres)
    double e = 0.0;                 // current eccentricity

    // Spin dynamics (updated every frame by updatePulsar)
    double spinOmega        = 0.0;  // current spin angular velocity (rad/s, SI)
    double spinPhase        = 0.0;  // accumulated visual spin phase (rad)
    double precPhase        = 0.0;  // accumulated geodetic precession phase (rad)
    double lightCylRadius_m = 0.0;  // R_LC = c/spinOmega (metres)

    bool active = false;

    // GW inspiral time-scale multiplier (visual only — da/dt, de/dt are real physics
    // multiplied by this before applying to the orbit).
    static constexpr double GW_VIS_SCALE  = 1.0e4;
    // Unipolar inductor orbital coupling amplifier (for data panel display).
    static constexpr double MAG_VIS_SCALE = 1.0e14;

    void init(double a0, double e0) {
        massGeom        = units::solarMassToGeomMeters(massSolar);
        a               = a0;
        e               = e0;
        active          = true;
        spinOmega       = 2.0 * M_PI / spinPeriod;
        spinPhase       = 0.0;
        precPhase       = 0.0;
        lightCylRadius_m = units::c_SI / spinOmega;
    }
};

// ── Four-category observational data ─────────────────────────────────────────

struct PulsarOrbitalData {

    // ── 1. Pulsar Timing ───────────────────────────────────────────────────
    double shapiroDelay_us   = 0.0;  // Shapiro delay at current orbital phase (μs)
    double shapiroMax_us     = 0.0;  // Peak Shapiro delay at superior conjunction (μs)
    double shapiro_ema_us    = 0.0;  // EMA of shapiroDelay used for residual baseline
    double timingResidual_us = 0.0;  // deviation from EMA baseline (oscillates with orbit)
    double gravRedshift      = 1.0;  // ν_obs / ν_emit  (< 1 → gravitational redshift)
    double dopplerFactor     = 1.0;  // Doppler contribution to observed frequency

    // ── 2. Orbital Evolution ───────────────────────────────────────────────
    double semiMajorAxis_M = 0.0;   // a / M_BH
    double eccentricity    = 0.0;   // current e
    double period_s        = 0.0;   // orbital period (seconds, real physics)
    double dadt_real       = 0.0;   // real (unscaled) da/dt (m per m coord-time)
    double timeToMerger_s  = 0.0;   // estimated Peters merger time, unscaled, seconds

    // ── 3. Gravitational Waves ─────────────────────────────────────────────
    double gwStrain        = 0.0;   // dimensionless strain h (face-on, at 1 kpc)
    double gwFreq_Hz       = 0.0;   // f_GW = 2 × f_orbital (Hz)
    double gwPower_ergs    = 0.0;   // GW luminosity (erg/s)
    double chirpMass_solar = 0.0;   // chirp mass M_chirp (solar masses)

    // ── 4. Structural Integrity ────────────────────────────────────────────
    double rocheLimit_M    = 0.0;   // Roche limit radius / M_BH
    double currentRadius_M = 0.0;   // current orbital radius / M_BH
    double tidalForce_geom = 0.0;   // 2 M / r³  (tidal stretch per unit separation)
    double bindingEnergy_J = 0.0;   // NS gravitational self-binding energy (J)
    double rocheMargin     = 1.0;   // r / r_Roche  (< 1.0 → disruption)
    bool   rocheViolated   = false; // currently inside Roche limit
    bool   disrupted       = false; // permanent disruption flag

    // ── 5. Spin & Magnetosphere ────────────────────────────────────────────
    double spinPeriod_s     = 0.033; // current (vis-scaled) spin period (s)
    double Pdot             = 0.0;   // real spin-down rate (s/s, positive → slowing)
    double spinDownLum_ergs = 0.0;   // magnetic dipole spin-down luminosity (erg/s)
    double lightCylRadius_M = 0.0;   // light cylinder radius / M_BH
    bool   inLightCylinder  = false; // orbital radius < R_LC
    double geodeticRate_deg = 0.0;   // geodetic precession rate (degrees/year, real)

    // ── 6. Magnetic Coupling (unipolar inductor) ─────────────────────────
    // The BH sweeps through the NS dipole field → induced EMF → resistive dissipation.
    double magPower_ergs    = 0.0;   // unipolar inductor power (erg/s)
    double magDadt_real     = 0.0;   // additional da/dt from magnetic torque (m/m, unscaled)

    // ── History deques for sparkline-style display ─────────────────────────
    // Each entry: (normalised_time, value)
    static constexpr size_t MAX_HIST = 350;

    std::deque<std::pair<float,float>> shapiroHistory;    // → shapiroDelay_us
    std::deque<std::pair<float,float>> residualHistory;   // → timingResidual_us
    std::deque<std::pair<float,float>> aHistory;          // → a / a0
    std::deque<std::pair<float,float>> eHistory;          // → eccentricity
    std::deque<std::pair<float,float>> THistory;          // → period / T0
    std::deque<std::pair<float,float>> hHistory;          // → h / h0
    std::deque<std::pair<float,float>> fHistory;          // → gwFreq_Hz
    std::deque<std::pair<float,float>> rocheHistory;      // → rocheMargin

    // Reference values for normalisation (set once at scenario start)
    double a0        = 1.0;
    double T0        = 1.0;
    double h0        = 1.0;
    double coordTime = 0.0;   // accumulated simulation coordinate time (geom metres)

    void pushHistory() {
        float t  = (T0 > 1e-30) ? (float)(coordTime / T0) : 0.0f;
        auto push1 = [](std::deque<std::pair<float,float>>& dq, float tt, float v) {
            dq.push_back({tt, v});
            if (dq.size() > MAX_HIST) dq.pop_front();
        };
        push1(shapiroHistory,  t, (float)shapiroDelay_us);
        push1(residualHistory, t, (float)timingResidual_us);
        push1(aHistory,        t, (a0 > 1e-30) ? (float)(semiMajorAxis_M / a0) : 1.0f);
        push1(eHistory,        t, (float)eccentricity);
        push1(THistory,        t, (T0 > 1e-30) ? (float)(period_s / T0) : 1.0f);
        push1(hHistory,        t, (h0 > 1e-30) ? (float)(gwStrain   / h0) : 1.0f);
        push1(fHistory,        t, (float)gwFreq_Hz);
        push1(rocheHistory,    t, (float)rocheMargin);
    }
};

// ── Physics functions ─────────────────────────────────────────────────────────

// Peters (1964) eccentricity enhancement: f(e) = (1 + 73/24 e² + 37/96 e⁴) / (1−e²)^(7/2)
// Used in da/dt and GW power formulas.
inline double pulsarPetersF(double e) {
    double e2  = e * e;
    double num = 1.0 + (73.0/24.0)*e2 + (37.0/96.0)*e2*e2;
    double den = std::pow(std::max(1.0 - e2, 1e-12), 3.5);
    return num / den;
}

// Roche limit: r_Roche = 2.46 R_NS (M_BH / M_NS)^(1/3).
// Arguments in geometric metres; R_NS in physical metres (= geometric metres when G=c=1).
inline double pulsarRocheLimit(double M_BH, double M_NS, double R_NS) {
    if (M_NS < 1e-30) return 0.0;
    return 2.46 * R_NS * std::cbrt(M_BH / M_NS);
}

// Peters da/dt in geometric units (G = c = 1).
// da/dt = -(64/5) m1 m2 (m1+m2) / a³ × f(e)
inline double pulsarDadt(double a, double e, double m1, double m2) {
    if (a < 1e-30) return 0.0;
    return -(64.0/5.0) * m1 * m2 * (m1 + m2) / (a*a*a) * pulsarPetersF(e);
}

// Peters de/dt in geometric units.
// de/dt = -(304/15) e m1 m2 (m1+m2) / [a⁴ (1−e²)^(5/2)] × (1 + 121/304 e²)
inline double pulsarDedt(double a, double e, double m1, double m2) {
    if (a < 1e-30 || e < 1e-15) return 0.0;
    double a4  = a * a * a * a;
    double inv = 1.0 / std::pow(std::max(1.0 - e*e, 1e-12), 2.5);
    return -(304.0/15.0) * e * m1 * m2 * (m1 + m2) / a4 * inv
           * (1.0 + (121.0/304.0)*e*e);
}

// Orbital period in geometric units: T = 2π √(a³ / M_total)
inline double pulsarOrbitalPeriodGeom(double a, double M_total) {
    if (a < 1e-30 || M_total < 1e-30) return 1e-30;
    return 2.0 * M_PI * std::sqrt(a*a*a / M_total);
}

// GW strain amplitude (face-on, sky-averaged with eccentricity correction), at 1 kpc reference.
// h ≈ 4 μ ω² a² / d × √(1 + 73/24 e² + 37/96 e⁴)
inline double pulsarGWStrain(double a, double e, double m1, double m2) {
    constexpr double dist_ref = 3.086e19;  // 1 kpc in metres
    if (a < 1e-30) return 0.0;
    double M_tot = m1 + m2;
    double mu    = m1 * m2 / M_tot;
    double T     = pulsarOrbitalPeriodGeom(a, M_tot);
    double omega = 2.0 * M_PI / T;
    double h     = 4.0 * mu * omega * omega * a * a / dist_ref;
    double e2    = e * e;
    double enh   = std::sqrt(1.0 + (73.0/24.0)*e2 + (37.0/96.0)*e2*e2);
    return h * enh;
}

// GW frequency in Hz: f_GW = 2 f_orbital = 2 c / T_geom
inline double pulsarGWFreqHz(double a, double M_total) {
    double T_geom = pulsarOrbitalPeriodGeom(a, M_total);
    if (T_geom < 1e-30) return 0.0;
    return 2.0 * units::c_SI / T_geom;  // convert geom-time period to real Hz
}

// GW luminosity in erg/s.
// P_geom = (32/5) μ² (m1+m2) / a⁵ f(e)  [geometric units]
// P_SI   = P_geom × c⁵/G  ≈  P_geom × 3.628e52 W  =  P_geom × 3.628e59 erg/s
inline double pulsarGWPowerErgs(double a, double e, double m1, double m2) {
    if (a < 1e-30) return 0.0;
    double mu      = m1 * m2 / (m1 + m2);
    double P_geom  = (32.0/5.0) * mu * mu * (m1 + m2)
                     / (a*a*a*a*a) * pulsarPetersF(e);
    constexpr double c5_over_G_ergs = 3.628e59;  // c^5/G in erg/s
    return P_geom * c5_over_G_ergs;
}

// Chirp mass in solar masses: M_c = (m1 m2)^(3/5) / (m1+m2)^(1/5)
inline double pulsarChirpMassSolar(double m1_geom, double m2_geom) {
    constexpr double MSUN_GEOM = 1476.25;  // G M_sun / c² in metres
    double Mc_geom = std::pow(m1_geom * m2_geom, 3.0/5.0)
                   / std::pow(m1_geom + m2_geom, 1.0/5.0);
    return Mc_geom / MSUN_GEOM;
}

// Peters circular-orbit merger time estimate (unscaled), in seconds.
// T_merge ≈ (5/256) a⁴ / [m1 m2 (m1+m2) f(e)]
inline double pulsarMergerTimeS(double a, double e, double m1, double m2) {
    if (a < 1e-30 || m1 < 1e-30 || m2 < 1e-30) return 0.0;
    double fe      = std::max(1.0, pulsarPetersF(e));  // ≥1; larger e → faster merge
    double T_geom  = (5.0/256.0) * std::pow(a, 4.0) / (m1 * m2 * (m1 + m2)) / fe;
    return T_geom / units::c_SI;
}

// Shapiro delay (μs) for a pulsar at orbital angle phi, around BH of mass M_BH.
// Observer is in the +x direction; superior conjunction (maximum delay) at phi = π.
// Formula: Δt = 2 M_BH [ln 2 − ln(max(ε, 1 + cos φ))]  → 0 at φ=0, peaks near φ=π.
inline double pulsarShapiroDelayUs(double phi, double M_BH) {
    constexpr double eps = 0.02;  // clamp at ~6° from exact superior conjunction
    double arg    = std::max(eps, 1.0 + std::cos(phi));
    double dt_geo = 2.0 * M_BH * (std::log(2.0) - std::log(arg));
    return dt_geo / units::c_SI * 1.0e6;  // → μs
}

// Peak Shapiro delay at the clamped superior conjunction (μs).
inline double pulsarShapiroMaxUs(double M_BH) {
    constexpr double eps = 0.02;
    double dt_geo = 2.0 * M_BH * (std::log(2.0) - std::log(eps));
    return dt_geo / units::c_SI * 1.0e6;
}

// Gravitational redshift factor: ν_obs / ν_emit = √(1 − 2M/r)
inline double pulsarGravRedshift(double r, double M_BH) {
    double fr = 1.0 - 2.0 * M_BH / std::max(r, 2.0 * M_BH * 1.001);
    return std::sqrt(std::max(0.0, fr));
}

// Approximate Doppler factor for body orbiting at radius r, angle phi.
// v_circ ≈ √(M/r); factor = 1 + v_circ cos(phi)  (observer in +x direction)
inline double pulsarDopplerFactor(double r, double phi, double M_BH) {
    double v_circ = std::sqrt(M_BH / std::max(r, 2.0 * M_BH * 1.001));
    return 1.0 + v_circ * std::cos(phi);
}

// NS gravitational binding energy in Joules (virial estimate: 0.6 G M²/R).
// E_bind_geom = 0.6 M_NS² / R_NS  (geometric units)
// E_bind_J    = E_bind_geom × c⁴/G  (conversion factor c⁴/G ≈ 6.083e43 J/m)
inline double pulsarBindingEnergyJ(double M_NS_geom, double R_NS_m) {
    if (R_NS_m < 1e-30) return 0.0;
    double E_geom = 0.6 * M_NS_geom * M_NS_geom / R_NS_m;
    constexpr double c4_over_G = 6.083e43;  // c^4/G in J/m
    return E_geom * c4_over_G;
}

// ── Spin & magnetosphere physics ─────────────────────────────────────────────

// Magnetic dipole spin-down rate dΩ/dt (rad/s²).
// Uses CGS throughout: B in Gauss, R in cm, I in g·cm².
// P_sd = B² R⁶ Ω⁴ / (6 c³)  →  dΩ/dt = −P_sd / (I Ω)
inline double pulsarSpinDownRate(double B_G, double R_NS_m, double Omega_radps) {
    if (Omega_radps < 1e-10) return 0.0;
    double B   = B_G;
    double R   = R_NS_m * 1.0e2;                          // m → cm
    double c3  = 2.998e10 * 2.998e10 * 2.998e10;          // (cm/s)³
    double M_g = 1.4 * units::MSUN_KG * 1.0e3;            // NS mass in grams
    double I   = 0.4 * M_g * R * R;                       // moment of inertia (g·cm²)
    double Psd = B * B * std::pow(R, 6.0) * std::pow(Omega_radps, 4.0) / (6.0 * c3);
    return -Psd / (I * Omega_radps);                       // rad/s²
}

// Light cylinder radius in metres: R_LC = c / Ω
inline double pulsarLightCylRadius_m(double Omega_radps) {
    return (Omega_radps > 1e-10) ? units::c_SI / Omega_radps : 1.0e30;
}

// Spin-down luminosity in erg/s: L_sd = B² R⁶ Ω⁴ / (6 c³)
inline double pulsarSpinDownLum_ergs(double B_G, double R_NS_m, double Omega_radps) {
    double B  = B_G;
    double R  = R_NS_m * 1.0e2;
    double c3 = 2.998e10 * 2.998e10 * 2.998e10;
    return B * B * std::pow(R, 6.0) * std::pow(Omega_radps, 4.0) / (6.0 * c3);
}

// Geodetic (de Sitter) precession rate in rad/s (SI).
// Ω_geo = (3/2) (GM_BH / c² r) × v_orb / r
// Geometric units (G=c=1): Ω_geo_geom = (3/2) M^(3/2) / a^(5/2)  [rad/m]
// Convert to rad/s by multiplying by c_SI.
inline double pulsarGeodeticPrecRate_radps(double a_geom, double M_BH_geom) {
    if (a_geom < 1e-10) return 0.0;
    double O_geom = 1.5 * std::pow(M_BH_geom, 1.5) / std::pow(a_geom, 2.5);
    return O_geom * units::c_SI;  // rad/s
}

// Unipolar inductor power (Watts, SI).
// When the BH sweeps through the NS rotating magnetosphere, it acts as a
// conductor with surface impedance Z₀ = 377 Ω (membrane paradigm).
// P_uni ≈ r_s_BH² × v_orb² × B_orb² / (Z₀ μ₀ c)
// B_orb = B_surf × (R_NS / r_orb)³  (equatorial dipole field at orbital radius)
// v_orb ≈ √(G M_BH / r_orb)  ≈  √(M_BH_geom) × c_SI / √(r_orb)  [m/s]
inline double pulsarUnipolePower_W(double r_orb_geom, double M_BH_geom,
                                    double B_surf_G, double R_NS_m) {
    // Guard: orbital radius must exceed NS radius for dipole formula to be valid.
    if (r_orb_geom < R_NS_m || r_orb_geom < 1e-10 || M_BH_geom < 1e-10) return 0.0;
    double B_orb_T  = (B_surf_G * 1.0e-4) * std::pow(R_NS_m / r_orb_geom, 3.0);
    double v_orb_ms = std::sqrt(M_BH_geom) * units::c_SI / std::sqrt(r_orb_geom);
    double r_s_m    = 2.0 * M_BH_geom;       // BH Schwarzschild radius (geometric m)
    constexpr double Z0  = 376.73;             // impedance of free space (Ω)
    constexpr double mu0 = 1.2566370614e-6;   // permeability of free space (H/m)
    return r_s_m * r_s_m * v_orb_ms * v_orb_ms
         * B_orb_T * B_orb_T / (Z0 * mu0 * units::c_SI);
}

// Additional da/dt from unipolar inductor orbital coupling (geometric m per m).
// P_uni [W] → P_geom = P_uni / (c⁵/G)  [geometric watts]
// E_orb = −M_BH M_NS / (2a)  →  da/dt = −2a² P_geom / (M_BH M_NS)
inline double pulsarMagDadt(double a_geom, double M_BH_geom, double M_NS_geom,
                             double B_surf_G, double R_NS_m) {
    if (a_geom < 1e-30 || M_BH_geom < 1e-30 || M_NS_geom < 1e-30) return 0.0;
    double P_W    = pulsarUnipolePower_W(a_geom, M_BH_geom, B_surf_G, R_NS_m);
    constexpr double c5_G = 3.6277e52;  // c⁵/G in Watts
    double P_geom = P_W / c5_G;
    return -2.0 * a_geom * a_geom * P_geom / (M_BH_geom * M_NS_geom);
}
