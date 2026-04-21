#pragma once
// ============================================================
// presets.hpp — Black hole profile presets with full SimConfig
// ============================================================

#include "config.hpp"
#include <string>
#include <array>

// A complete black hole profile: config + metadata + default feature flags.
struct BlackHoleProfile {
    std::string       name;
    std::string       description;
    double            massSolar;      // Solar masses (informational)
    cfg::SimConfig    config;
    bool              defaultJets;    // Whether jets are visible by default
    bool              defaultBLR;     // Whether BLR is visible by default
    bool              defaultOrbBody; // Whether orbiting body is visible by default
    bool              defaultDoppler; // Whether Doppler beaming is on by default
    bool              defaultHostGalaxy; // Whether host galaxy is visible by default
    bool              defaultLAB;    // Whether Lyman-alpha Blob is visible by default
    bool              defaultCGM;    // Whether Circumgalactic Medium is visible by default
};

namespace profiles {

/*--------- TON 618 — Most massive known quasar ---------*/
inline BlackHoleProfile ton618() {
    BlackHoleProfile p;
    p.name        = "TON 618";
    p.description = "Most massive known quasar (~6.6e10 Msun)";
    p.massSolar   = 6.6e10;
    p.config      = cfg::defaultConfig();
    // TON 618: hot blue-white inner disk fading to warm orange outer
    p.config.disk.peakTemp           = 30000.0f;
    p.config.disk.displayTempInner   = 5500.0f;  // Warm white
    p.config.disk.displayTempOuter   = 2200.0f;  // Deep orange
    p.config.disk.saturationBoostInner = 2.0f;
    p.config.disk.saturationBoostOuter = 1.1f;
    // Orbiting body at larger distance for quasar scale
    p.config.orbital.semiMajor = 100.0f;  // Further out for quasar
    p.config.orbital.bodyRadius = 1.5f;   // Larger body for visibility
    p.defaultJets    = true;
    p.defaultBLR     = true;
    p.defaultOrbBody = true;
    p.defaultDoppler = true;
    p.defaultHostGalaxy = true;
    p.defaultLAB = true;
    p.defaultCGM = false;  // Diffuse fog off by default
    return p;
}

/*--------- Sagittarius A* — Milky Way center ---------*/
// Low-luminosity, radiatively inefficient accretion flow (RIAF).
// Weak/absent jets, no broad-line region, small dim disk.
inline BlackHoleProfile sgrAstar() {
    BlackHoleProfile p;
    p.name        = "Sgr A*";
    p.description = "Milky Way center (~4.3e6 Msun) — dim, quiet";
    p.massSolar   = 4.3e6;

    cfg::SimConfig c;
    // Moderate spin — current EHT constraints suggest ~0.5–0.9
    c.blackHole.spinParameter = 0.5f;
    c.blackHole.radius        = 1.0f;

    // Small, dim accretion flow — RIAF geometry
    c.disk.innerRadius   = 3.0f;   // Larger ISCO (lower spin)
    c.disk.outerRadius   = 10.0f;  // Compact accretion flow
    c.disk.halfThickness = 0.04f;  // Thicker, puffy RIAF

    // Weak jets (barely visible if toggled on)
    c.jet.radius = 0.15f;
    c.jet.length = 10.0f;
    c.jet.color  = glm::vec3(0.15f, 0.5f, 0.8f);  // Faint blue

    // No significant BLR
    c.blr.innerRadius = 8.0f;
    c.blr.outerRadius = 14.0f;
    c.blr.thickness   = 2.0f;

    // Camera closer — smaller object
    c.camera.initialPos = glm::vec3(0.0f, 4.0f, 18.0f);

    // Subtle bloom (dim source)
    c.bloom.threshold  = 1.5f;
    c.bloom.intensity  = 0.35f;
    c.bloom.exposure   = 0.9f;

    // Sgr A*: reddish-orange/yellow based on EHT images
    c.disk.peakTemp           = 8000.0f;   // Lower temp RIAF
    c.disk.displayTempInner   = 3200.0f;   // Reddish-orange inner
    c.disk.displayTempOuter   = 1600.0f;   // Deep red outer
    c.disk.saturationBoostInner = 2.8f;    // Strong warm saturation
    c.disk.saturationBoostOuter = 1.8f;    // Keep outer vivid

    p.config = c;
    p.defaultJets    = false;  // Sgr A* has no prominent jets
    p.defaultBLR     = false;  // No BLR in a low-luminosity AGN
    p.defaultOrbBody = false;  // No orbiting bodies for Sgr A*
    p.defaultDoppler = true;
    p.defaultHostGalaxy = false;
    p.defaultLAB = false;
    p.defaultCGM = false;
    return p;
}

/*--------- 3C 273 — First identified quasar ---------*/
// Powerful relativistic jet, bright accretion disk, active BLR.
inline BlackHoleProfile qso3c273() {
    BlackHoleProfile p;
    p.name        = "3C 273";
    p.description = "First quasar identified (~8.9e8 Msun) — bright jet";
    p.massSolar   = 8.86e8;

    cfg::SimConfig c;
    // High spin — jet-producing quasar
    c.blackHole.spinParameter = 0.9f;
    c.blackHole.radius        = 1.0f;

    // Bright, extended accretion disk
    c.disk.innerRadius   = 1.8f;   // Close to ISCO at high spin
    c.disk.outerRadius   = 22.0f;
    c.disk.halfThickness = 0.015f;

    // Powerful relativistic jet (3C 273 is famous for its jet)
    c.jet.radius = 0.45f;
    c.jet.length = 55.0f;  // Enormous jet
    c.jet.color  = glm::vec3(0.3f, 0.85f, 1.5f);  // Bright synchrotron blue

    // Active broad-line region
    c.blr.innerRadius = 16.0f;
    c.blr.outerRadius = 38.0f;
    c.blr.thickness   = 7.0f;

    c.camera.initialPos = glm::vec3(0.0f, 7.0f, 28.0f);

    // Bright bloom — luminous quasar
    c.bloom.threshold  = 1.0f;
    c.bloom.intensity  = 0.60f;
    c.bloom.exposure   = 1.1f;

    // 3C 273: bright blue-white quasar disk
    c.disk.peakTemp           = 35000.0f;
    c.disk.displayTempInner   = 6500.0f;   // Hot blue-white core
    c.disk.displayTempOuter   = 2500.0f;   // Warm yellow outer
    c.disk.saturationBoostInner = 1.8f;
    c.disk.saturationBoostOuter = 1.2f;

    p.config = c;
    p.defaultJets    = true;   // Famous jet
    p.defaultBLR     = true;
    p.defaultOrbBody = true;
    p.defaultDoppler = true;
    p.defaultHostGalaxy = true;
    p.defaultLAB = true;
    p.defaultCGM = false;
    return p;
}

/*--------- J0529-4351 — Most luminous quasar known ---------*/
// Extreme accretion rate, enormous disk, blazing luminosity.
inline BlackHoleProfile j0529() {
    BlackHoleProfile p;
    p.name        = "J0529-4351";
    p.description = "Most luminous quasar (~1.7e10 Msun) — extreme disk";
    p.massSolar   = 1.7e10;

    cfg::SimConfig c;
    // Very high spin — extreme accretion
    c.blackHole.spinParameter = 0.95f;
    c.blackHole.radius        = 1.0f;

    // Massive, bright accretion disk — largest known
    c.disk.innerRadius   = 1.5f;   // Very close ISCO at near-maximal spin
    c.disk.outerRadius   = 30.0f;  // Enormous disk
    c.disk.halfThickness = 0.01f;  // Thin, efficient disk

    // Strong jets
    c.jet.radius = 0.5f;
    c.jet.length = 50.0f;
    c.jet.color  = glm::vec3(0.35f, 0.95f, 1.6f);  // Intense synchrotron

    // Extensive BLR
    c.blr.innerRadius = 20.0f;
    c.blr.outerRadius = 42.0f;
    c.blr.thickness   = 8.0f;

    // Pull camera back — big object
    c.camera.initialPos = glm::vec3(0.0f, 8.0f, 32.0f);

    // Intense bloom — most luminous object in the universe
    c.bloom.threshold  = 0.8f;
    c.bloom.intensity  = 0.75f;
    c.bloom.exposure   = 1.2f;
    c.bloom.softKnee   = 0.7f;

    // J0529-4351: blazing white-blue disk (extreme luminosity)
    c.disk.peakTemp           = 40000.0f;
    c.disk.displayTempInner   = 7000.0f;   // Very hot blue-white
    c.disk.displayTempOuter   = 2800.0f;   // Yellow-orange fringe
    c.disk.saturationBoostInner = 1.6f;
    c.disk.saturationBoostOuter = 1.3f;

    p.config = c;
    p.defaultJets    = true;
    p.defaultBLR     = true;
    p.defaultOrbBody = true;
    p.defaultDoppler = true;
    p.defaultHostGalaxy = true;
    p.defaultLAB = true;
    p.defaultCGM = false;
    return p;
}

/*--------- All profiles in order ---------*/
inline std::array<BlackHoleProfile, 4> allProfiles() {
    return { ton618(), sgrAstar(), qso3c273(), j0529() };
}

constexpr int NUM_PROFILES = 4;

} // namespace profiles

/*--------- Legacy compatibility for BlackHole2D ---------*/
// The 2D simulator uses a flat name/mass/description struct.
struct BlackHolePreset {
    std::string name;
    double massSolar;
    std::string description;
};

const inline BlackHolePreset BH_PRESETS[] = {
    {"TON 618",          6.6e10, "Most massive known quasar BH (~6.6e10 Msun)"},
    {"Sgr A*",           4.3e6,  "Milky Way center (~4.3e6 Msun)"},
    {"3C 273",           8.86e8, "First quasar identified (~8.9e8 Msun)"},
    {"J0529-4351",       1.7e10, "Most luminous quasar (~1.7e10 Msun)"},
    {"M87",              6.5e9,  "M87 galaxy BH (~6.5e9 Msun, EHT image)"},
    {"Cygnus X-1",       21.2,   "Famous stellar-mass black hole (~21 Msun)"},
    {"LIGO GW150914",    62.0,   "First gravitational wave merger remnant (~62 Msun)"},
    {"Intermediate-mass", 1e4,   "Hypothetical intermediate-mass BH (~1e4 Msun)"},
    {"Primordial",        1e-5,  "Tiny primordial black hole (~1e-5 Msun)"}
};
constexpr int NUM_BH_PRESETS = sizeof(BH_PRESETS) / sizeof(BH_PRESETS[0]);
