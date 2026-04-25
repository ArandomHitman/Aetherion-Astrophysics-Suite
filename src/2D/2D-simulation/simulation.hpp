#pragma once
#include "black_hole.hpp"
#include "photon.hpp"
#include "orbiting_body.hpp"
#include "research_data.hpp"
#include "csv_export.hpp"
#include "FITS_export.hpp"
#include "../2D-utils/presets_2d.hpp"
#include "../2D-physics/geodesic.hpp"
#include "../2D-physics/pulsar_orbital.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

// TODO: pack all viable headers into single header hpp files for 2d, 3d, and launcher
// hours wasted trying to fix linux package diff issues: 7

struct SimParams {
    int    numRays        = 120;
    double rMaxIntegrate  = 1e5;
    double pixelsPerM     = 60.0;
    float  timeScale      = 1.0f;   // user-controllable speed multiplier
};

/*--------- Research scenario types ---------*/
enum class ResearchScenario {
    None,
    ISCOTest,          // Particles at 5M, 6M, 7M
    PhotonSphereTest,  // Photon near critical b
    RadialInfall,      // Particle dropped from rest
    TidalDisruption,   // Body in tidal zone
    PulsarOrbital      // Neutron star inspiraling via GW emission
};

// Central simulation manager — orchestrates the black hole world.
class Simulation {
public:
    BlackHole                  bh;
    std::vector<Photon>        photons;
    std::vector<OrbitingBody>  bodies;
    SimParams                  params;
    bool                       galaxySystemActive = false;
    int                        activePresetIdx    = -1;

    // Research mode state
    LensingData                lensingData;
    std::vector<PhotonSphereTestResult> photonSphereResults;
    std::vector<ISCOTestResult>         iscoResults;
    ResearchScenario           activeScenario = ResearchScenario::None;
    int                        selectedBodyIdx = 0;
    double                     gasTemperatureK = 1e7;
    std::string                exportName      = "untitled_data"; ///< used as export subfolder name

    // Pulsar orbital scenario state
    PulsarState        pulsarState;
    PulsarOrbitalData  pulsarData;
    bool               tidalDisrupted = false; // set when tidal test body crosses disruption threshold

    Simulation() {
        bodies.emplace_back(bh.metric, 10.0 * bh.metric.M, 0.3);
    }

    void update(double dt) {
        // Scale simulation time with M so animations run at similar visual
        // speed regardless of BH mass.  For the default M=1 case, dtSim=dt.
        // dt * M is the correct normalization: orbital period ∝ M, so this
        // keeps the fraction-of-orbit-per-frame mass-independent.
        double dtSim = dt * bh.metric.M * static_cast<double>(params.timeScale);

        for (auto& body : bodies)
            body.update(bh.metric, dtSim);

        // Update ISCO test results if active
        if (activeScenario == ResearchScenario::ISCOTest)
            updateISCOResults();

        // Update pulsar orbital scenario if active
        if (activeScenario == ResearchScenario::PulsarOrbital)
            updatePulsar(dtSim);

        // Update tidal disruption scenario if active
        if (activeScenario == ResearchScenario::TidalDisruption)
            updateTidalDisruption(dtSim);
    }

    void rebuildPhotons(unsigned int windowHeight, bool highRes = false) {
        photons.clear();
        double halfHeightM = ((double)windowHeight * 0.5) / params.pixelsPerM;

        // 120 rays uniformly spaced in impact parameter b across the visible screen height.
        // uniform-in-b is NOT the ideal sampling near b_crit, where deflection changes
        // really fast and you'd want to cluster more rays there. but it looks good on screen
        // and changing it would mess up the visual spacing, so here we are.
        for (int i = 0; i < params.numRays; ++i) {
            double t = (params.numRays <= 1)
                     ? 0.5
                     : (double)i / (double)(params.numRays - 1);
            double b = (2.0 * t - 1.0) * halfHeightM;

            Photon p;
            p.impactParameter = b;
            p.computePath(bh.metric, params.rMaxIntegrate);
            photons.push_back(std::move(p));
        }

        // High-res lensing mode: add extra rays log-spaced near b_crit on both sides.
        // These show the strong-lensing regime in much finer detail without disturbing
        // the uniform background grid.
        if (highRes) {
            double b_crit = bh.metric.criticalImpact();
            constexpr int N_HIGHRES = 20; // rays per side
            for (int sign : {-1, 1}) {
                for (int i = 0; i < N_HIGHRES; ++i) {
                    double t = (double)i / (double)(N_HIGHRES - 1);
                    // epsilon log-spaced from 1e-3 to 0.2 (0.1% to 20% offset from b_crit)
                    double eps = std::exp(std::log(1e-3) + t * (std::log(0.2) - std::log(1e-3)));
                    double b = b_crit * (1.0 + sign * eps);
                    Photon p;
                    p.impactParameter = b;
                    p.computePath(bh.metric, params.rMaxIntegrate, 0.001); // finer angular step near b_crit
                    photons.push_back(std::move(p));
                }
            }
        }

        // Rebuild lensing analysis
        buildLensingData();
    }

    void reinitBodies() {
        double M = bh.metric.M;
        if (!bodies.empty()) {
            auto& b = bodies[0];
            b.initFromKeplerian(bh.metric, 10.0 * M, b.nominalEcc);
        }
        bodies.erase(
            std::remove_if(bodies.begin(), bodies.end(),
                [](const OrbitingBody& b){ return b.isGalaxyBody; }),
            bodies.end());
        if (galaxySystemActive && activePresetIdx >= 0)
            spawnGalaxySystem(activePresetIdx);
    }

    void spawnGalaxySystem(int presetIdx) {
        bodies.erase(
            std::remove_if(bodies.begin(), bodies.end(),
                [](const OrbitingBody& b){ return b.isGalaxyBody; }),
            bodies.end());

        if (presetIdx < 0 || presetIdx >= NUM_BH2D_PRESETS) return;
        const auto& preset = BH2D_PRESETS[presetIdx];
        if (!preset.isGalacticCenter || preset.numGalaxyBodies == 0) return;

        double M = bh.metric.M;
        for (int i = 0; i < preset.numGalaxyBodies; ++i) {
            const auto& gsb = preset.galaxyBodies[i];
            double aSim = gsb.semiMajorM * M;
            OrbitingBody ob(bh.metric, aSim, gsb.eccentricity);
            ob.label = gsb.label;
            ob.bodyType = gsb.type;
            ob.isGalaxyBody = true;
            ob.phi = (2.0 * M_PI * i) / preset.numGalaxyBodies; // stagger starting angles evenly so bodies don't all pile up at phi=0 which looks terrible
            ob.trail.clear();
            ob.trail.emplace_back(ob.worldX(), ob.worldY());
            bodies.push_back(std::move(ob));
        }
        activePresetIdx = presetIdx;
        galaxySystemActive = true;
    }

    void clearGalaxySystem() {
        bodies.erase(
            std::remove_if(bodies.begin(), bodies.end(),
                [](const OrbitingBody& b){ return b.isGalaxyBody; }),
            bodies.end());
        galaxySystemActive = false;
    }

    void reset() {
        bh.metric.M = 1.0;
        bodies.clear();
        bodies.emplace_back(bh.metric, 10.0 * bh.metric.M, 0.3);
        galaxySystemActive = false;
        activePresetIdx = -1;
        activeScenario = ResearchScenario::None;
        iscoResults.clear();
        photonSphereResults.clear();
        selectedBodyIdx = 0;
        // note: this does NOT reset the exportName. you're welcome to argue that it should,
        // but I've already accidentally lost data once by having reset() wipe a name I forgot to save.
        // so it stays. fight me.
    }

    /*--------- Lensing analysis ---------*/
    void buildLensingData() {
        lensingData.criticalImpactParam = bh.metric.criticalImpact();
        lensingData.deflectionTable.clear();
        lensingData.causticPoints.clear();

        for (const auto& p : photons) {
            PhotonDeflection d;
            d.impactParameter = p.impactParameter;
            d.captured = p.captured;
            d.deflectionAngle = p.deflectionAngle;
            d.deflectionDeg   = p.deflectionDeg;
            lensingData.deflectionTable.push_back(d);
        }

        // Caustic detection: find where adjacent non-captured rays cross
        // by checking if deflection angles cause path inversion
        for (size_t i = 1; i < lensingData.deflectionTable.size(); ++i) {
            const auto& prev = lensingData.deflectionTable[i-1];
            const auto& curr = lensingData.deflectionTable[i];
            if (prev.captured || curr.captured) continue;

            // Large deflection gradient → ray convergence (caustic region)
            double db = curr.impactParameter - prev.impactParameter;
            if (std::abs(db) < 1e-15) continue;
            double dDeflect = curr.deflectionAngle - prev.deflectionAngle;
            double gradient = std::abs(dDeflect / db);

            // threshold of 0.5 rad/M is completely empirical — I tuned it until the caustic markers
            // appeared in roughly the right places visually. I cannot rigorously justify this number.
            // if caustics are appearing in weird places, this is probably why.
            if (gradient > 0.5) {
                float avgB = (float)(0.5 * (prev.impactParameter + curr.impactParameter));
                lensingData.causticPoints.emplace_back(avgB, (float)gradient);
            }
        }
    }

    /*--------- ISCO validation test ---------*/
    void startISCOTest() {
        activeScenario = ResearchScenario::ISCOTest;
        // Remove non-galaxy bodies (except first default)
        bodies.clear();
        iscoResults.clear();

        double M = bh.metric.M;
        double testRadii[] = { 5.0 * M, 6.0 * M, 7.0 * M };
        const char* labels[] = { "5M (Unstable)", "6M (Critical)", "7M (Stable)" };

        for (int i = 0; i < 3; ++i) {
            OrbitingBody ob(bh.metric, testRadii[i], 0.0);
            ob.initCircularOrbit(bh.metric, testRadii[i]);
            ob.label = labels[i];
            // tiny radial kick to break the exact circular orbit so we can actually see stability play out.
            // 1e-6 is small enough not to visually disturb the orbit but large enough to diverge over time.
            // the sign matters: inward kick for the unstable case pushes it toward the horizon faster
            ob.vr = 1e-6 * (i == 0 ? -1.0 : 1.0);
            bodies.push_back(std::move(ob));

            ISCOTestResult res;
            res.testRadius_M = testRadii[i] / M;
            res.classification = bh.metric.stabilityClassification(testRadii[i]);
            iscoResults.push_back(res);
        }
        selectedBodyIdx = 0;
    }

    void updateISCOResults() {
        for (size_t i = 0; i < iscoResults.size() && i < bodies.size(); ++i) {
            double M = bh.metric.M;
            iscoResults[i].captured = bodies[i].captured;
            iscoResults[i].radiusDrift = (bodies[i].r - iscoResults[i].testRadius_M * M) / M;
            iscoResults[i].survivalTime = bodies[i].measurement.properTime;
        }
    }

    /*--------- Photon sphere test ---------*/
    void startPhotonSphereTest() {
        activeScenario = ResearchScenario::PhotonSphereTest;
        photonSphereResults.clear();

        double b_crit = bh.metric.criticalImpact();
        // Test photons near the critical impact parameter
        double epsilons[] = { 0.001, 0.01, 0.05, -0.001, -0.01, -0.05 };

        for (double eps : epsilons) {
            double b = b_crit * (1.0 + eps);

            Photon p;
            p.impactParameter = b;
            p.computePath(bh.metric, params.rMaxIntegrate, 0.001);

            PhotonSphereTestResult res;
            res.impactParameter = b;
            res.captured = p.captured;
            res.escaped  = !p.captured;

            if (!p.captured) {
                // Count orbits: total angle / (2π)
                res.stabilityAngle = p.deflectionAngle;
                res.orbitsBeforeDecay = std::abs(p.deflectionAngle) / (2.0 * M_PI);
            } else {
                // For captured photons, estimate from path length
                if (p.path.size() >= 2) {
                    double totalAngle = 0.0;
                    for (size_t i = 1; i < p.path.size(); ++i) {
                        double a1 = std::atan2(p.path[i-1].y, p.path[i-1].x);
                        double a2 = std::atan2(p.path[i].y, p.path[i].x);
                        double da = a2 - a1;
                        if (da > M_PI) da -= 2.0 * M_PI;
                        if (da < -M_PI) da += 2.0 * M_PI;
                        totalAngle += std::abs(da);
                    }
                    res.orbitsBeforeDecay = totalAngle / (2.0 * M_PI);
                    res.stabilityAngle = totalAngle;
                }
            }
            photonSphereResults.push_back(res);
        }
    }

    /*--------- Radial infall test ---------*/
    void startRadialInfall() {
        activeScenario = ResearchScenario::RadialInfall;
        bodies.clear();

        double M = bh.metric.M;
        double r0 = 20.0 * M; // start at 20M because anything closer and the thing hits the horizon
                               // before the user has time to read the data panel. tried 10M once. not great.

        OrbitingBody ob(bh.metric, r0, 0.0);
        ob.initRadialInfall(bh.metric, r0);
        ob.label = "Radial infall from 20M";
        bodies.push_back(std::move(ob));
        selectedBodyIdx = 0;
    }

    /*--------- Pulsar orbital simulation ---------*/
    void startPulsarOrbital() {
        activeScenario = ResearchScenario::PulsarOrbital;
        bodies.clear();

        double M   = bh.metric.M;
        double a0  = 20.0 * M;  // start at 20M — comfortably outside ISCO at 6M
        double e0  = 0.3;       // moderate eccentricity for visible variation

        OrbitingBody ob(bh.metric, a0, e0);
        ob.label = "Pulsar (1.4 Msun NS)";
        bodies.push_back(std::move(ob));
        selectedBodyIdx = 0;

        // Initialise pulsar physics state
        pulsarState.init(a0, e0);

        // Reset data struct and set normalisation references
        pulsarData = PulsarOrbitalData{};
        pulsarData.a0 = a0 / M;   // in units of M_BH for history normalisation

        double M_NS  = pulsarState.massGeom;
        double M_tot = M + M_NS;
        double T0_g  = pulsarOrbitalPeriodGeom(a0, M_tot);
        pulsarData.T0 = T0_g / units::c_SI;   // initial period in seconds

        double h0 = pulsarGWStrain(a0, e0, M, M_NS);
        pulsarData.h0 = (h0 > 1e-50) ? h0 : 1.0;
    }

    // Adiabatic GW-decay update called every sim frame when scenario is PulsarOrbital.
    void updatePulsar(double dtSim) {
        if (bodies.empty() || !pulsarState.active) return;

        auto& body = bodies[0];
        if (body.captured) { pulsarData.disrupted = true; return; }

        double M_BH  = bh.metric.M;
        double M_NS  = pulsarState.massGeom;
        double R_NS  = pulsarState.radiusM;
        double M_tot = M_BH + M_NS;

        // ── Adiabatic GW + magnetic orbital decay ────────────────────────────────
        // da/dt and de/dt are in geometric units (m per m of coordinate time).
        // min(1, M_BH/M_NS) suppresses GW decay for the unphysical M=1 default case.
        double mass_ratio_clamp = std::min(1.0, M_BH / std::max(M_NS, 1e-30));
        double vis = PulsarState::GW_VIS_SCALE * mass_ratio_clamp;

        double da_gw  = pulsarDadt(pulsarState.a, pulsarState.e, M_BH, M_NS) * dtSim * vis;
        double de_raw = pulsarDedt(pulsarState.a, pulsarState.e, M_BH, M_NS) * dtSim * vis;

        // Magnetic orbital decay (unipolar inductor, amplified for data-panel display).
        // Cap at 5 % of the GW contribution to avoid dominating the inspiral.
        double da_mag_real = pulsarMagDadt(pulsarState.a, M_BH, M_NS,
                                           pulsarState.magField, R_NS);
        double da_mag_vis  = da_mag_real * dtSim * PulsarState::MAG_VIS_SCALE;
        double da_mag_cap  = std::max(da_mag_vis, -std::abs(da_gw) * 0.05);
        double da_raw      = da_gw + da_mag_cap;

        // Safety clamp: limit to max 3 % of current a per step to prevent
        // numerical blow-up in the M=1 default-unit case.
        double max_da = -0.03 * pulsarState.a;
        double da     = std::max(da_raw, max_da);
        double de     = de_raw;  // de/dt is always negative (orbit circularises)

        pulsarState.a = std::max(pulsarState.a + da, bh.metric.isco() * 1.05);
        pulsarState.e = std::clamp(pulsarState.e + de, 0.0, 0.98);

        // Update body's conserved E/L to match the decayed (a, e).
        // The body continues from its current (r, phi, vr) but now evolves
        // on the new, slightly smaller Schwarzschild orbit.
        double r_peri = pulsarState.a * (1.0 - pulsarState.e);
        double r_apo  = pulsarState.a * (1.0 + pulsarState.e);
        r_peri = std::max(r_peri, bh.metric.isco() + 0.05 * M_BH);
        r_apo  = std::max(r_apo,  r_peri + 0.05 * M_BH);
        auto newEL  = bh.metric.boundOrbitEL(r_peri, r_apo);
        body.E       = newEL.E;
        body.L       = newEL.L;
        body.nominalA   = pulsarState.a;
        body.nominalEcc = pulsarState.e;

        // Merge detection
        if (pulsarState.a <= bh.metric.isco() * 1.06) {
            pulsarData.disrupted = true;
        }

        // ── Category 1: Pulsar Timing ──────────────────────────────────────────
        double r   = body.r;
        double phi = body.phi;

        pulsarData.shapiroDelay_us = pulsarShapiroDelayUs(phi, M_BH);
        pulsarData.shapiroMax_us   = pulsarShapiroMaxUs(M_BH);
        pulsarData.gravRedshift    = pulsarGravRedshift(r, M_BH);
        pulsarData.dopplerFactor   = pulsarDopplerFactor(r, phi, M_BH);

        // EMA baseline for timing residual (alpha = 0.015 → slow follower)
        const double ALPHA = 0.015;
        pulsarData.shapiro_ema_us = (1.0 - ALPHA) * pulsarData.shapiro_ema_us
                                  + ALPHA * pulsarData.shapiroDelay_us;
        pulsarData.timingResidual_us = pulsarData.shapiroDelay_us
                                     - pulsarData.shapiro_ema_us;

        // ── Category 2: Orbital Evolution ──────────────────────────────────────
        pulsarData.semiMajorAxis_M = pulsarState.a / M_BH;
        pulsarData.eccentricity    = pulsarState.e;
        double T_geom              = pulsarOrbitalPeriodGeom(pulsarState.a, M_tot);
        pulsarData.period_s        = T_geom / units::c_SI;
        pulsarData.dadt_real       = pulsarDadt(pulsarState.a, pulsarState.e, M_BH, M_NS);
        pulsarData.timeToMerger_s  = pulsarMergerTimeS(pulsarState.a, pulsarState.e,
                                                        M_BH, M_NS);

        // ── Category 3: Gravitational Waves ────────────────────────────────────
        pulsarData.gwStrain        = pulsarGWStrain(pulsarState.a, pulsarState.e,
                                                     M_BH, M_NS);
        pulsarData.gwFreq_Hz       = pulsarGWFreqHz(pulsarState.a, M_tot);
        pulsarData.gwPower_ergs    = pulsarGWPowerErgs(pulsarState.a, pulsarState.e,
                                                        M_BH, M_NS);
        pulsarData.chirpMass_solar = pulsarChirpMassSolar(M_BH, M_NS);

        // ── Category 4: Structural Integrity ────────────────────────────────────
        double r_roche             = pulsarRocheLimit(M_BH, M_NS, R_NS);
        pulsarData.rocheLimit_M    = r_roche / M_BH;
        pulsarData.currentRadius_M = r / M_BH;
        pulsarData.tidalForce_geom = 2.0 * M_BH / (r * r * r);
        pulsarData.bindingEnergy_J = pulsarBindingEnergyJ(M_NS, R_NS);
        pulsarData.rocheMargin     = (r_roche > 1e-30) ? r / r_roche : 1e9;
        pulsarData.rocheViolated   = (r < r_roche && !body.captured);
        if (pulsarData.rocheViolated) pulsarData.disrupted = true;

        // ── Category 5: Spin & Magnetosphere ──────────────────────────────────
        // Spin-down: target period to double in ~100 orbital periods, which gives a
        // visible slowdown of the jets over the same timescale as the GW inspiral.
        {
            double T_orb_vis = pulsarOrbitalPeriodGeom(pulsarState.a, M_tot);
            double frac      = 0.005 * (dtSim / std::max(T_orb_vis, 1e-30));
            pulsarState.spinOmega *= (1.0 - frac);
            double omega0          = 2.0 * M_PI / pulsarState.spinPeriod;
            pulsarState.spinOmega  = std::max(pulsarState.spinOmega, 0.02 * omega0);
            pulsarState.lightCylRadius_m = pulsarLightCylRadius_m(pulsarState.spinOmega);

            // Visual spin phase: ~1 rotation per orbit so jets sweep visibly but
            // don't blur into a smear. Slows naturally as spinOmega decays.
            double vis_rate = (2.0 * M_PI / std::max(T_orb_vis, 1e-30))
                            * (pulsarState.spinOmega / omega0);
            pulsarState.spinPhase += vis_rate * dtSim;

            // Geodetic precession (visually amplified to match inspiral timescale).
            double prec_geom = 1.5 * std::pow(M_BH, 1.5) / std::pow(pulsarState.a, 2.5);
            pulsarState.precPhase += prec_geom * dtSim * PulsarState::GW_VIS_SCALE;

            // Real (unscaled) observables for the data panel
            double Omega_real = 2.0 * M_PI / pulsarState.spinPeriod;
            double dO_dt_real = pulsarSpinDownRate(pulsarState.magField, R_NS, Omega_real);
            double R_LC_m     = pulsarState.lightCylRadius_m;

            pulsarData.spinPeriod_s     = 2.0 * M_PI / pulsarState.spinOmega;
            pulsarData.Pdot             = -(2.0 * M_PI / (Omega_real * Omega_real)) * dO_dt_real;
            pulsarData.spinDownLum_ergs = pulsarSpinDownLum_ergs(pulsarState.magField, R_NS,
                                                                   pulsarState.spinOmega);
            pulsarData.lightCylRadius_M = R_LC_m / std::max(M_BH, 1e-30);
            pulsarData.inLightCylinder  = (r < R_LC_m);
            pulsarData.geodeticRate_deg =
                pulsarGeodeticPrecRate_radps(pulsarState.a, M_BH)
                * (180.0 / M_PI) * (365.25 * 86400.0);
        }

        // ── Category 6: Magnetic Coupling (unipolar inductor) ─────────────────
        {
            double P_uni_W         = pulsarUnipolePower_W(r, M_BH, pulsarState.magField, R_NS);
            pulsarData.magPower_ergs = P_uni_W * 1.0e7;  // W → erg/s
            pulsarData.magDadt_real  = pulsarMagDadt(pulsarState.a, M_BH, M_NS,
                                                      pulsarState.magField, R_NS);
        }

        // ── History ─────────────────────────────────────────────────────────────
        pulsarData.coordTime += dtSim;
        pulsarData.pushHistory();
    }

    /*--------- Tidal disruption demo ---------*/
    void startTidalDisruption() {
        activeScenario  = ResearchScenario::TidalDisruption;
        tidalDisrupted  = false;
        bodies.clear();

        double M = bh.metric.M;

        // eccentricity 0.85 — this is deliberately extreme so the body actually reaches tidal disruption range.
        // if you set it lower the body just orbits happily and nothing dramatic happens, which defeats the point.
        // if it crosses ISCO it'll plunge in, which is also fine — that's kind of what tidal disruption events do.
        OrbitingBody ob(bh.metric, 8.0 * M, 0.85);
        ob.label = "Tidal test body";
        bodies.push_back(std::move(ob));
        selectedBodyIdx = 0;
    }

    void updateTidalDisruption(double /*dtSim*/) {
        if (bodies.empty() || tidalDisrupted) return;
        const auto& body = bodies[0];
        if (body.captured) {
            tidalDisrupted = true;
            return;
        }
        // Tidal disruption threshold: ~8 M is where tidal stress overwhelms
        // self-gravity for a solar-type star around a typical BH.  This is a
        // first-order approximation — real r_tidal depends on stellar density.
        double r_tidal = 8.0 * bh.metric.M;
        if (body.r < r_tidal) {
            tidalDisrupted = true;
            bodies[0].label = "!! TIDAL DISRUPTION !!";
        }
    }

    /*--------- Data panel info builder ---------*/
    DataPanelInfo buildDataPanel() const {
        DataPanelInfo info;
        if (bodies.empty() || selectedBodyIdx >= (int)bodies.size())
            return info;

        const auto& body = bodies[selectedBodyIdx];
        double M = bh.metric.M;

        info.radius_M = body.r / M;
        info.velocity_c = body.measurement.orbitalVelocity;
        info.timeDilation = body.measurement.timeDilation;
        info.redshift_z = body.measurement.redshift - 1.0;
        info.energy_E = body.E;
        info.angularMomentum_L = body.L;
        info.properTime = body.measurement.properTime;
        info.coordinateTime = body.measurement.coordinateTime;
        info.escapeVelocity_c = bh.metric.escapeVelocity(body.r);
        info.isBound = (body.E < 1.0);
        info.stabilityClass = bh.metric.stabilityClassification(body.r);

        // Tidal
        info.tidalForce = bh.metric.tidalForce(body.r);
        info.tidalStress1m = bh.metric.tidalStress(body.r, 1.0);

        // Conservation
        info.energyDrift = body.conservation.relativeDrift();
        info.maxEnergyDrift = body.conservation.maxAbsDrift;

        // Numerical error
        info.rk4LastError = body.errorTracker.lastError;
        info.rk4MaxError  = body.errorTracker.maxError;
        info.rk4AvgError  = body.errorTracker.avgError;

        // Precession
        info.precessionDeg = body.precession.lastPrecessionDeg();
        info.theoreticalPrecessionDeg =
            bh.metric.theoreticalPrecession(body.nominalA, body.nominalEcc) * 180.0 / M_PI;
        info.orbitsCompleted = body.precession.orbitsCompleted();

        // Bondi
        double cs = Schwarzschild::soundSpeedFromTemp(gasTemperatureK);
        info.gasTemperatureK = gasTemperatureK;
        info.soundSpeed_c = cs;
        info.bondiRadius_M = bh.metric.bondiRadius(cs) / M;

        return info;
    }

    /*--------- Format data panel as string ---------*/
    std::string formatDataPanel() const {
        auto info = buildDataPanel();
        auto sci = [](double v, int prec = 3) -> std::string {
            std::ostringstream os;
            if (std::fabs(v) >= 1e5 || (std::fabs(v) > 0 && std::fabs(v) < 1e-3))
                os << std::scientific << std::setprecision(prec) << v;
            else
                os << std::fixed << std::setprecision(prec + 1) << v;
            return os.str();
        };

        std::ostringstream ss;
        ss << "=== RESEARCH DATA ===\n";
        ss << "Body [" << selectedBodyIdx << "/"
           << (int)bodies.size()-1 << "]";
        if (selectedBodyIdx < (int)bodies.size() && !bodies[selectedBodyIdx].label.empty())
            ss << " " << bodies[selectedBodyIdx].label;
        ss << "\n";

        ss << "--- STATE ---\n";
        ss << "Radius:   " << sci(info.radius_M) << "M\n";
        ss << "Velocity: " << sci(info.velocity_c) << "c\n";
        ss << "Escape v: " << sci(info.escapeVelocity_c) << "c\n";
        ss << "Bound:    " << (info.isBound ? "YES" : "NO") << "\n";
        ss << "Stability:" << info.stabilityClass << "\n";

        ss << "--- RELATIVISTIC ---\n";
        ss << "dt/dtau:  " << sci(info.timeDilation) << "\n";
        ss << "Proper t: " << sci(info.properTime) << "\n";
        ss << "Coord t:  " << sci(info.coordinateTime) << "\n";
        ss << "z:        " << sci(info.redshift_z) << "\n";

        ss << "--- CONSERVED ---\n";
        ss << "E:        " << sci(info.energy_E) << "\n";
        ss << "L:        " << sci(info.angularMomentum_L) << "\n";
        ss << "E drift:  " << sci(info.energyDrift * 100.0) << "%\n";
        ss << "Max drift:" << sci(info.maxEnergyDrift * 100.0) << "%\n";

        ss << "--- PRECESSION ---\n";
        ss << "Orbits:   " << info.orbitsCompleted << "\n";
        ss << "Last dPhi:" << sci(info.precessionDeg) << " deg/orb\n";
        ss << "Theory:   " << sci(info.theoreticalPrecessionDeg) << " deg/orb\n";

        ss << "--- TIDAL ---\n";
        ss << "F_tidal:  " << sci(info.tidalForce) << " /M^2\n";
        ss << "Stress 1m:" << sci(info.tidalStress1m) << "\n";

        ss << "--- BONDI ---\n";
        ss << "T_gas:    " << sci(info.gasTemperatureK, 1) << " K\n";
        ss << "c_s:      " << sci(info.soundSpeed_c) << " c\n";
        ss << "r_Bondi:  " << sci(info.bondiRadius_M) << "M\n";

        ss << "--- NUMERICAL ---\n";
        ss << "RK4 last: " << sci(info.rk4LastError) << "\n";
        ss << "RK4 max:  " << sci(info.rk4MaxError) << "\n";
        ss << "RK4 avg:  " << sci(info.rk4AvgError) << "\n";

        // ISCO test results
        if (activeScenario == ResearchScenario::ISCOTest && !iscoResults.empty()) {
            ss << "--- ISCO TEST ---\n";
            for (const auto& r : iscoResults) {
                ss << sci(r.testRadius_M) << "M: "
                   << r.classification
                   << " drift=" << sci(r.radiusDrift)
                   << (r.captured ? " CAPTURED" : "") << "\n";
            }
        }

        // Photon sphere test
        if (activeScenario == ResearchScenario::PhotonSphereTest && !photonSphereResults.empty()) {
            ss << "--- PHOTON SPHERE ---\n";
            ss << "b_crit=" << sci(bh.metric.criticalImpact()) << "\n";
            for (const auto& r : photonSphereResults) {
                ss << "b=" << sci(r.impactParameter)
                   << " orbits=" << sci(r.orbitsBeforeDecay, 1)
                   << (r.escaped ? " ESC" : " CAP") << "\n";
            }
        }

        // Lensing
        if (!lensingData.deflectionTable.empty()) {
            ss << "--- LENSING ---\n";
            ss << "b_crit: " << sci(lensingData.criticalImpactParam) << "\n";
            ss << "Caustic pts: " << lensingData.causticPoints.size() << "\n";
        }

        // Pulsar orbital scenario
        if (activeScenario == ResearchScenario::PulsarOrbital) {
            const auto& pd = pulsarData;
            ss << "--- PULSAR ORBITAL ---\n";
            ss << "NS: " << std::fixed << std::setprecision(2)
               << pulsarState.massSolar << " Msun";
            if (pd.disrupted) ss << "  !! DISRUPTED !!";
            ss << "\n";

            ss << "--- 1. TIMING ---\n";
            ss << "Shapiro:  " << sci(pd.shapiroDelay_us) << " us\n";
            ss << "Max Shap: " << sci(pd.shapiroMax_us)   << " us\n";
            ss << "Residual: " << sci(pd.timingResidual_us) << " us\n";
            ss << "nu_obs/0: " << sci(pd.gravRedshift)    << "\n";
            ss << "Doppler:  " << sci(pd.dopplerFactor)   << "\n";

            ss << "--- 2. ORBIT DECAY ---\n";
            ss << "a/M_BH:  " << sci(pd.semiMajorAxis_M) << "\n";
            ss << "e:       " << sci(pd.eccentricity)     << "\n";
            ss << "Period:  " << sci(pd.period_s)         << " s\n";
            ss << "da/dt:   " << sci(pd.dadt_real)        << " m/m\n";

            // Merger time: display in years if large, seconds if small
            double t_merge = pd.timeToMerger_s;
            if (t_merge > 3.156e7)
                ss << "Merger:  ~" << sci(t_merge / 3.156e7) << " yr\n";
            else
                ss << "Merger:  ~" << sci(t_merge) << " s\n";

            ss << "--- 3. GRAV WAVES ---\n";
            ss << "h(1kpc): " << sci(pd.gwStrain)        << "\n";
            ss << "f_GW:    " << sci(pd.gwFreq_Hz)       << " Hz\n";
            ss << "P_GW:    " << sci(pd.gwPower_ergs)    << " erg/s\n";
            ss << "M_chirp: " << sci(pd.chirpMass_solar) << " Msun\n";

            ss << "--- 4. INTEGRITY ---\n";
            ss << "r_Roche: " << sci(pd.rocheLimit_M)    << "M\n";
            ss << "r_curr:  " << sci(pd.currentRadius_M) << "M\n";
            ss << "r/r_R:   " << sci(pd.rocheMargin)     << "\n";
            ss << "F_tidal: " << sci(pd.tidalForce_geom) << " /M^2\n";
            ss << "E_bind:  " << sci(pd.bindingEnergy_J) << " J\n";
            if (pd.rocheViolated) ss << "!! ROCHE LIMIT BREACHED !!\n";

            ss << "--- 5. SPIN & MAGNETOSPHERE ---\n";
            ss << "P_spin: " << sci(pd.spinPeriod_s)     << " s\n";
            ss << "P_dot:  " << sci(pd.Pdot)             << " s/s\n";
            ss << "L_sd:   " << sci(pd.spinDownLum_ergs) << " erg/s\n";
            ss << "R_LC:   " << sci(pd.lightCylRadius_M) << " M\n";
            ss << "In LC:  " << (pd.inLightCylinder ? "YES (magnetically coupled)" : "NO") << "\n";
            ss << "Prec:   " << sci(pd.geodeticRate_deg) << " deg/yr\n";

            ss << "--- 6. MAGNETIC COUPLING ---\n";
            ss << "P_uni:  " << sci(pd.magPower_ergs)  << " erg/s\n";
            ss << "da/dt_B:" << sci(pd.magDadt_real)   << " m/m\n";
            ss << "(BH unipolar inductor: field lines connect NS to BH)\n";
        }

        ss << "\nH: cycle body  X: export data";
        return ss.str();
    }

    /*--------- Helpers ---------*/

    // Strip characters that are invalid or problematic in directory names.
    static std::string sanitizeName(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            if (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ' ')
                out += static_cast<char>(c);
            else
                out += '_';
        }
        // Trim leading/trailing whitespace
        size_t start = out.find_first_not_of(' ');
        if (start == std::string::npos) return "untitled";
        size_t end = out.find_last_not_of(' ');
        return out.substr(start, end - start + 1);
    }

    // Returns the workspace-level export directory, or "" on failure.
    std::string makeExportDir() const {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        std::string safeName = sanitizeName(exportName.empty() ? "untitled_data" : exportName);
        std::string dir;
#ifdef __APPLE__
        dir = std::string(home) + "/Library/Application Support/Aetherion/exports/" + safeName + "/";
#elif defined(__linux__)
        dir = std::string(home) + "/.local/share/Aetherion/exports/" + safeName + "/";
#else
        dir = std::string(home) + "/Aetherion/exports/" + safeName + "/";
#endif
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return "";   // Signal failure to callers
        return dir;
    }

    // Returns a per-body subdirectory (body_<idx>[_<label>]/) so multiple
    // galaxy bodies never overwrite each other.  Returns "" on failure.
    std::string makeBodyExportDir() const {
        std::string base = makeExportDir();
        if (base.empty()) return "";

        std::string subdir = "body_" + std::to_string(selectedBodyIdx);
        if (selectedBodyIdx < (int)bodies.size() && !bodies[selectedBodyIdx].label.empty())
            subdir += "_" + sanitizeName(bodies[selectedBodyIdx].label);

        std::string dir = base + subdir + "/";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return "";
        return dir;
    }

    // Abbreviates the home directory as "~" so messages fit in the HUD.
    static std::string abbreviateHome(const std::string& path) {
        const char *home = getenv("HOME");
        if (home && path.substr(0, std::strlen(home)) == home)
            return "~" + path.substr(std::strlen(home));
        return path;
    }

    /*--------- CSV export ---------*/
    std::string exportAllCSV() const {
        std::string msg;
        std::string displayDir;

        if (!bodies.empty() && selectedBodyIdx < (int)bodies.size()) {
            if (bodies[selectedBodyIdx].trail.size() <= 1)
                return "No orbit data yet — let the simulation run first";
            std::string dir = makeBodyExportDir();
            if (dir.empty()) return "Export failed: could not create directory";
            displayDir = dir;
            const auto& body = bodies[selectedBodyIdx];
            if (CSVExport::exportOrbitData(dir + "orbit_data.csv", body.trail))
                msg += "orbit_data.csv ";
            if (CSVExport::exportConservationHistory(dir + "conservation.csv", body.conservation))
                msg += "conservation.csv ";
            if (CSVExport::exportPrecessionData(dir + "precession.csv", body.precession))
                msg += "precession.csv ";
        }
        if (!lensingData.deflectionTable.empty()) {
            std::string base = makeExportDir();
            if (!base.empty()) {
                if (CSVExport::exportDeflectionTable(base + "deflection.csv", lensingData.deflectionTable))
                    msg += "deflection.csv ";
                if (displayDir.empty()) displayDir = base;
            }
        }
        if (msg.empty()) return "No data to export";
        return "Saved: " + msg + "| " + abbreviateHome(displayDir);
    }

    /*--------- FITS export ---------*/
    std::string exportAllFITS() const {
        std::string msg;
        std::string displayDir;

        if (!bodies.empty() && selectedBodyIdx < (int)bodies.size()) {
            if (bodies[selectedBodyIdx].trail.size() <= 1)
                return "No orbit data yet — let the simulation run first";
            std::string dir = makeBodyExportDir();
            if (dir.empty()) return "Export failed: could not create directory";
            displayDir = dir;
            const auto& body = bodies[selectedBodyIdx];
            if (FITSExport::exportOrbitData(dir + "orbit_data.fits", body.trail))
                msg += "orbit_data.fits ";
            if (FITSExport::exportConservationHistory(dir + "conservation.fits", body.conservation))
                msg += "conservation.fits ";
            if (FITSExport::exportPrecessionData(dir + "precession.fits", body.precession))
                msg += "precession.fits ";
        }
        if (!lensingData.deflectionTable.empty()) {
            std::string base = makeExportDir();
            if (!base.empty()) {
                if (FITSExport::exportDeflectionTable(base + "deflection.fits", lensingData.deflectionTable))
                    msg += "deflection.fits ";
                if (displayDir.empty()) displayDir = base;
            }
        }
        if (msg.empty()) return "No data to export";
        return "Saved: " + msg + "| " + abbreviateHome(displayDir);
    }

    /*--------- Binary export ---------*/
    std::string exportAllBinary() const {
        std::string msg;
        std::string displayDir;

        if (!bodies.empty() && selectedBodyIdx < (int)bodies.size()) {
            if (bodies[selectedBodyIdx].trail.size() <= 1)
                return "No orbit data yet — let the simulation run first";
            std::string dir = makeBodyExportDir();
            if (dir.empty()) return "Export failed: could not create directory";
            displayDir = dir;
            const auto& body = bodies[selectedBodyIdx];
            // Orbit trail: count + (x,y) pairs as doubles
            {
                std::ofstream f(dir + "orbit_data.bin", std::ios::binary);
                if (f) {
                    uint64_t n = body.trail.size();
                    f.write(reinterpret_cast<const char*>(&n), sizeof(n));
                    for (const auto& [x, y] : body.trail) {
                        f.write(reinterpret_cast<const char*>(&x), sizeof(x));
                        f.write(reinterpret_cast<const char*>(&y), sizeof(y));
                    }
                    msg += "orbit_data.bin ";
                }
            }
            // Conservation drift: count + (time, drift) pairs
            {
                std::ofstream f(dir + "conservation.bin", std::ios::binary);
                if (f) {
                    uint64_t n = body.conservation.driftHistory.size();
                    f.write(reinterpret_cast<const char*>(&n), sizeof(n));
                    for (const auto& [t, d] : body.conservation.driftHistory) {
                        f.write(reinterpret_cast<const char*>(&t), sizeof(t));
                        f.write(reinterpret_cast<const char*>(&d), sizeof(d));
                    }
                    msg += "conservation.bin ";
                }
            }
            // Precession: count + precession_per_orbit doubles
            {
                std::ofstream f(dir + "precession.bin", std::ios::binary);
                if (f) {
                    uint64_t n = body.precession.precessionPerOrbit.size();
                    f.write(reinterpret_cast<const char*>(&n), sizeof(n));
                    for (double p : body.precession.precessionPerOrbit)
                        f.write(reinterpret_cast<const char*>(&p), sizeof(p));
                    msg += "precession.bin ";
                }
            }
        }
        if (!lensingData.deflectionTable.empty()) {
            std::string base = makeExportDir();
            if (!base.empty()) {
                std::ofstream f(base + "deflection.bin", std::ios::binary);
                if (f) {
                    uint64_t n = lensingData.deflectionTable.size();
                    f.write(reinterpret_cast<const char*>(&n), sizeof(n));
                    for (const auto& d : lensingData.deflectionTable) {
                        f.write(reinterpret_cast<const char*>(&d.impactParameter),  sizeof(double));
                        f.write(reinterpret_cast<const char*>(&d.deflectionAngle),  sizeof(double));
                        f.write(reinterpret_cast<const char*>(&d.deflectionDeg),    sizeof(double));
                        uint8_t cap = d.captured ? 1 : 0;
                        f.write(reinterpret_cast<const char*>(&cap), sizeof(cap));
                    }
                    msg += "deflection.bin ";
                    if (displayDir.empty()) displayDir = base;
                }
            }
        }
        if (msg.empty()) return "No data to export";
        return "Saved: " + msg + "| " + abbreviateHome(displayDir);
    }

    // Select next body for data panel
    void cycleSelectedBody() {
        if (bodies.empty()) return;
        selectedBodyIdx = (selectedBodyIdx + 1) % (int)bodies.size();
    }
};
