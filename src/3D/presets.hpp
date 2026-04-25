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

/*--------- Gaia BH1 — Nearest known dormant black hole ---------*/
// Discovered via astrometric wobble of its G-dwarf companion (Gaia DR3).
// Dormant: NO detectable accretion disk. Disk params represent theoretical
// Bondi-Hoyle capture of stellar wind at ~10^-8 L_Edd — far below any
// observable threshold. The companion star is the entire visual story here.
inline BlackHoleProfile gaiaBH1() {
    BlackHoleProfile p;
    p.name        = "Gaia BH1";
    p.description = "Nearest dormant BH (~9.6 Msun) — discovered via astrometry, no disk";
    p.massSolar   = 9.62;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.3f;
    c.blackHole.radius        = 1.0f;
    // No accretion disk — dormant black hole, discovered by astrometry only.
    // outerRadius = 0 makes the shader condition (r < diskOuterRadius) always
    // false, suppressing all disk rendering passes.
    c.disk.innerRadius   = 3.0f;
    c.disk.outerRadius   = 0.0f;    // Zero — disables disk entirely in shader
    c.disk.halfThickness = 0.0f;
    c.disk.peakTemp             = 1000.0f;
    c.disk.displayTempInner     = 1000.0f;
    c.disk.displayTempOuter     = 1000.0f;
    c.disk.saturationBoostInner = 0.0f;
    c.disk.saturationBoostOuter = 0.0f;
    c.jet.radius = 0.08f; c.jet.length = 4.0f;
    c.jet.color  = glm::vec3(0.1f, 0.3f, 0.6f);
    c.orbital.semiMajor  = 28.0f;
    c.orbital.bodyRadius = 1.2f;    // G-dwarf companion — the visual focus
    c.camera.initialPos  = glm::vec3(0.0f, 3.5f, 15.0f);
    c.bloom.threshold    = 4.0f;    // Nothing blooms at this luminosity
    c.bloom.intensity    = 0.04f;
    c.bloom.exposure     = 0.40f;
    p.config = c;
    p.defaultJets        = false;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- Gaia BH2 — Dormant BH with red giant companion ---------*/
// Discovered via astrometric wobble of a red giant companion ~1.16 kpc away.
// Dormant: NO detectable accretion disk. The red giant overfills its Roche
// lobe slightly, but the resulting RIAF luminosity is still undetectable.
// The oversized companion body is the sole visual identifier for this system.
inline BlackHoleProfile gaiaBH2() {
    BlackHoleProfile p;
    p.name        = "Gaia BH2";
    p.description = "Dormant BH (~8.9 Msun) — discovered via astrometry, red giant companion";
    p.massSolar   = 8.94;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.2f;
    c.blackHole.radius        = 1.0f;
    // No accretion disk — dormant black hole, discovered by astrometry only.
    // outerRadius = 0 makes the shader condition (r < diskOuterRadius) always
    // false, suppressing all disk rendering passes.
    c.disk.innerRadius   = 3.0f;
    c.disk.outerRadius   = 0.0f;    // Zero — disables disk entirely in shader
    c.disk.halfThickness = 0.0f;
    c.disk.peakTemp             = 1000.0f;
    c.disk.displayTempInner     = 1000.0f;
    c.disk.displayTempOuter     = 1000.0f;
    c.disk.saturationBoostInner = 0.0f;
    c.disk.saturationBoostOuter = 0.0f;
    c.jet.radius = 0.08f; c.jet.length = 4.0f;
    c.jet.color  = glm::vec3(0.1f, 0.3f, 0.6f);
    c.orbital.semiMajor  = 30.0f;
    c.orbital.bodyRadius = 2.4f;    // Red giant is noticeably larger — the visual focus
    c.camera.initialPos  = glm::vec3(0.0f, 3.5f, 15.0f);
    c.bloom.threshold    = 4.0f;
    c.bloom.intensity    = 0.04f;
    c.bloom.exposure     = 0.40f;
    p.config = c;
    p.defaultJets        = false;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- Gaia BH3 — Most massive nearby dormant BH ---------*/
// Discovered via astrometric wobble of a metal-poor giant companion.
// ~33 Msun — well above typical stellar BH mass, yet completely dormant.
// No detectable X-ray or optical emission from any accretion disk.
// Disk params: theoretical RIAF trace only; renders invisible by design.
inline BlackHoleProfile gaiaBH3() {
    BlackHoleProfile p;
    p.name        = "Gaia BH3";
    p.description = "Most massive nearby dormant BH (~33 Msun) — discovered via astrometry, no disk";
    p.massSolar   = 32.7;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.45f;
    c.blackHole.radius        = 1.0f;
    // No accretion disk — dormant black hole, discovered by astrometry only.
    // outerRadius = 0 makes the shader condition (r < diskOuterRadius) always
    // false, suppressing all disk rendering passes.
    c.disk.innerRadius   = 2.8f;
    c.disk.outerRadius   = 0.0f;    // Zero — disables disk entirely in shader
    c.disk.halfThickness = 0.0f;
    c.disk.peakTemp             = 1000.0f;
    c.disk.displayTempInner     = 1000.0f;
    c.disk.displayTempOuter     = 1000.0f;
    c.disk.saturationBoostInner = 0.0f;
    c.disk.saturationBoostOuter = 0.0f;
    c.jet.radius = 0.1f; c.jet.length = 5.0f;
    c.jet.color  = glm::vec3(0.1f, 0.35f, 0.7f);
    c.orbital.semiMajor  = 22.0f;
    c.orbital.bodyRadius = 1.6f;    // Metal-poor giant companion
    c.camera.initialPos  = glm::vec3(0.0f, 3.5f, 15.0f);
    c.bloom.threshold    = 4.0f;
    c.bloom.intensity    = 0.04f;
    c.bloom.exposure     = 0.40f;
    p.config = c;
    p.defaultJets        = false;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- V404 Cygni — X-ray nova microquasar ---------*/
// Represents the 2015 outburst: brightest stellar BH transient in decades.
// Hot bright disk. kelvinToRGB(7500) → near-white with yellow tinge.
// Well-distinguished from quiescent Gaia BHs by temperature and brightness.
inline BlackHoleProfile v404Cyg() {
    BlackHoleProfile p;
    p.name        = "V404 Cygni";
    p.description = "X-ray nova microquasar (~9 Msun) — outburst state, K-giant";
    p.massSolar   = 9.0;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.5f;
    c.blackHole.radius        = 1.0f;
    c.disk.innerRadius   = 2.5f;
    c.disk.outerRadius   = 12.0f;
    c.disk.halfThickness = 0.03f;   // Thin, efficient accretion (outburst)
    // kelvinToRGB(7500) → bright pale-yellow/almost-white inner core.
    // kelvinToRGB(3800) → orange middle ring — classic X-ray nova gradient.
    c.disk.peakTemp             = 30000.0f;
    c.disk.displayTempInner     = 7500.0f;   // Hot pale-yellow inner ring
    c.disk.displayTempOuter     = 3800.0f;   // Orange outer disk
    c.disk.saturationBoostInner = 2.0f;
    c.disk.saturationBoostOuter = 3.0f;      // Punchy orange outer
    c.jet.radius = 0.20f;
    c.jet.length = 14.0f;
    c.jet.color  = glm::vec3(0.2f, 0.65f, 1.3f);
    c.orbital.semiMajor  = 26.0f;
    c.orbital.bodyRadius = 1.4f;
    c.camera.initialPos  = glm::vec3(0.0f, 3.5f, 16.0f);
    c.bloom.threshold    = 1.35f;
    c.bloom.intensity    = 0.45f;
    c.bloom.exposure     = 1.0f;
    p.config = c;
    p.defaultJets        = true;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- A0620-00 — First confirmed stellar BH ---------*/
// Quiescent LMXB — archetypal dormant system; K-dwarf donor, very low accretion.
// Faintest disk of any preset. kelvinToRGB(3000) → deep red-orange ember glow.
// Distinctive as the "dimmest" — users can see it's barely accreting.
inline BlackHoleProfile a062000() {
    BlackHoleProfile p;
    p.name        = "A0620-00";
    p.description = "First confirmed stellar BH (~6.6 Msun) — quiescent K-dwarf";
    p.massSolar   = 6.61;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.12f;  // Low spin — lowest Novikov-Thorne flux
    c.blackHole.radius        = 1.0f;
    c.disk.innerRadius   = 3.2f;
    c.disk.outerRadius   = 8.0f;
    c.disk.halfThickness = 0.065f;  // Puffy ADAF
    // kelvinToRGB(3000) → vivid deep red-orange (the ember look).
    // kelvinToRGB(2100) → smouldering red edge.
    c.disk.peakTemp             = 8000.0f;
    c.disk.displayTempInner     = 3000.0f;   // Deep red-orange ember
    c.disk.displayTempOuter     = 2100.0f;   // Smouldering red outer
    c.disk.saturationBoostInner = 3.6f;      // Max saturation to make the ember glow pop
    c.disk.saturationBoostOuter = 2.8f;
    c.jet.radius = 0.08f; c.jet.length = 4.0f;
    c.jet.color  = glm::vec3(0.1f, 0.25f, 0.5f);
    c.orbital.semiMajor  = 24.0f;
    c.orbital.bodyRadius = 1.1f;
    c.camera.initialPos  = glm::vec3(0.0f, 3.0f, 13.0f);
    c.bloom.threshold    = 1.95f;
    c.bloom.intensity    = 0.22f;
    c.bloom.exposure     = 0.72f;
    p.config = c;
    p.defaultJets        = false;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- GRO J1655-40 — Microquasar with relativistic jets ---------*/
// Best-measured spin in the stellar-mass regime (a* ~ 0.7).
// Hot, efficient thin disk during outburst.
// kelvinToRGB(9500) → white with a perceptible blue-violet tinge — the hottest
// stellar BH in the set. The jet color reinforces the blue identity.
inline BlackHoleProfile groJ165540() {
    BlackHoleProfile p;
    p.name        = "GRO J1655-40";
    p.description = "Microquasar with relativistic jets (~6.3 Msun) — F-star companion";
    p.massSolar   = 6.3;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.7f;
    c.blackHole.radius        = 1.0f;
    c.disk.innerRadius   = 1.9f;    // Smaller ISCO at a*=0.7
    c.disk.outerRadius   = 10.0f;
    c.disk.halfThickness = 0.025f;  // Very thin, efficient disk
    // kelvinToRGB(9500) → cold white with faint blue cast — clearly hotter than V404.
    // kelvinToRGB(4500) → warm yellow outer; keeps the gradient interesting.
    c.disk.peakTemp             = 40000.0f;
    c.disk.displayTempInner     = 9500.0f;   // Near-white with blue cast
    c.disk.displayTempOuter     = 4500.0f;   // Yellow-orange middle/outer
    c.disk.saturationBoostInner = 1.5f;      // Low — blue-white desaturates naturally
    c.disk.saturationBoostOuter = 2.6f;      // Orange outer pops against the white core
    c.jet.radius = 0.24f;
    c.jet.length = 18.0f;
    c.jet.color  = glm::vec3(0.3f, 0.8f, 1.6f);   // Vivid synchrotron blue
    c.orbital.semiMajor  = 22.0f;
    c.orbital.bodyRadius = 1.3f;
    c.camera.initialPos  = glm::vec3(0.0f, 3.5f, 16.0f);
    c.bloom.threshold    = 1.25f;
    c.bloom.intensity    = 0.50f;
    c.bloom.exposure     = 1.05f;
    p.config = c;
    p.defaultJets        = true;
    p.defaultBLR         = false;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = false;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- NGC 1277 — Overmassive SMBH in compact elliptical ---------*/
// ~14% of host-galaxy bulge mass — 10× above the M-σ relation.
// AGN-class accretion; should visually resemble a compact 3C 273 but with
// a hotter, more saturated disk to distinguish it.
// kelvinToRGB(6800) → warm-white with slight gold tinge.
inline BlackHoleProfile ngc1277() {
    BlackHoleProfile p;
    p.name        = "NGC 1277";
    p.description = "Overmassive SMBH in compact elliptical (~1.7e10 Msun)";
    p.massSolar   = 1.7e10;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.65f;
    c.blackHole.radius        = 1.0f;
    c.disk.innerRadius   = 1.9f;
    c.disk.outerRadius   = 20.0f;
    c.disk.halfThickness = 0.014f;
    // kelvinToRGB(6800) → gold-white inner. kelvinToRGB(3000) → orange outer.
    // The gold-to-orange gradient makes it feel distinctly different from
    // the bluer OJ 287 or the warm-white quasars.
    c.disk.peakTemp             = 28000.0f;
    c.disk.displayTempInner     = 6800.0f;   // Gold-white inner AGN disk
    c.disk.displayTempOuter     = 3000.0f;   // Orange outer — vivid contrast
    c.disk.saturationBoostInner = 2.2f;
    c.disk.saturationBoostOuter = 3.2f;      // Make the orange outer edge striking
    c.jet.radius = 0.30f;
    c.jet.length = 28.0f;
    c.jet.color  = glm::vec3(0.22f, 0.70f, 1.35f);
    c.blr.innerRadius = 13.0f;
    c.blr.outerRadius = 28.0f;
    c.blr.thickness   = 5.0f;
    c.orbital.semiMajor  = 80.0f;
    c.orbital.bodyRadius = 1.5f;
    c.camera.initialPos  = glm::vec3(0.0f, 6.0f, 24.0f);
    c.bloom.threshold    = 1.05f;
    c.bloom.intensity    = 0.58f;
    c.bloom.exposure     = 1.10f;
    p.config = c;
    p.defaultJets        = false;
    p.defaultBLR         = true;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = true;
    p.defaultLAB         = false;
    p.defaultCGM         = false;
    return p;
}

/*--------- OJ 287 — SMBH binary with optical outbursts ---------*/
// Primary BH ~1.8×10¹⁰ Msun; secondary (~1.5×10⁸ Msun) punches through the
// disk every ~12 years, triggering UV/optical flares.
// Blazar-class: jet pointed near line of sight → Doppler-boosted bright jet.
// kelvinToRGB(11000) → distinctly icy-blue-white; hottest and brightest of the set.
inline BlackHoleProfile oj287() {
    BlackHoleProfile p;
    p.name        = "OJ 287";
    p.description = "SMBH binary system (~1.8e10 Msun primary) — blazar, optical outbursts";
    p.massSolar   = 1.8e10;

    cfg::SimConfig c;
    c.blackHole.spinParameter = 0.82f;
    c.blackHole.radius        = 1.0f;
    c.disk.innerRadius   = 1.75f;   // Very close ISCO at a*=0.82
    c.disk.outerRadius   = 26.0f;
    c.disk.halfThickness = 0.011f;  // Extremely thin Shakura-Sunyaev disk
    // kelvinToRGB(11000) → icy blue-white — immediately distinguishable from all others.
    // kelvinToRGB(4200) → warm yellow-orange outer — strong thermal gradient.
    c.disk.peakTemp             = 38000.0f;
    c.disk.displayTempInner     = 11000.0f;  // Icy blue-white blazar inner disk
    c.disk.displayTempOuter     = 4200.0f;   // Yellow-orange outer — broad gradient
    c.disk.saturationBoostInner = 1.4f;      // Cool — blue-white is naturally pale
    c.disk.saturationBoostOuter = 2.8f;      // Pop the yellow outer ring
    c.jet.radius = 0.45f;
    c.jet.length = 48.0f;
    c.jet.color  = glm::vec3(0.35f, 0.85f, 1.65f);   // Intense electric-blue jet
    c.blr.innerRadius = 17.0f;
    c.blr.outerRadius = 36.0f;
    c.blr.thickness   = 7.0f;
    c.orbital.semiMajor  = 14.0f;    // Secondary SMBH in tight orbit
    c.orbital.bodyRadius = 2.2f;
    c.camera.initialPos  = glm::vec3(0.0f, 7.0f, 27.0f);
    c.bloom.threshold    = 0.88f;
    c.bloom.intensity    = 0.68f;
    c.bloom.exposure     = 1.15f;
    c.bloom.softKnee     = 0.62f;
    p.config = c;
    p.defaultJets        = true;
    p.defaultBLR         = true;
    p.defaultOrbBody     = true;
    p.defaultDoppler     = true;
    p.defaultHostGalaxy  = true;
    p.defaultLAB         = true;
    p.defaultCGM         = false;
    return p;
}

/*--------- All profiles in order ---------*/
inline std::array<BlackHoleProfile, 12> allProfiles() {
    return { ton618(), sgrAstar(), qso3c273(), j0529(),
             gaiaBH1(), gaiaBH2(), gaiaBH3(),
             v404Cyg(), a062000(), groJ165540(),
             ngc1277(), oj287() };
}

constexpr int NUM_PROFILES = 12;

} // namespace profiles

/*--------- Legacy compatibility for BlackHole2D ---------*/
// The 2D simulator uses a flat name/mass/description struct.
struct BlackHolePreset {
    std::string name;
    double massSolar;
    std::string description;
};

const inline BlackHolePreset BH_PRESETS[] = {
    {"TON 618",           6.6e10, "Most massive known quasar BH (~6.6e10 Msun)"},
    {"Sgr A*",            4.3e6,  "Milky Way center (~4.3e6 Msun)"},
    {"3C 273",            8.86e8, "First quasar identified (~8.9e8 Msun)"},
    {"J0529-4351",        1.7e10, "Most luminous quasar (~1.7e10 Msun)"},
    {"M87",               6.5e9,  "M87 galaxy BH (~6.5e9 Msun, EHT image)"},
    {"Cygnus X-1",        21.2,   "Famous stellar-mass HMXB black hole (~21 Msun)"},
    {"LIGO GW150914",     62.0,   "First gravitational wave merger remnant (~62 Msun)"},
    {"Intermediate-mass", 1e4,    "Hypothetical intermediate-mass BH (~1e4 Msun)"},
    {"Primordial",        1e-5,   "Tiny primordial black hole (~1e-5 Msun)"},
    {"Gaia BH1",          9.62,   "Nearest dormant BH, G-dwarf companion (~9.6 Msun)"},
    {"Gaia BH2",          8.94,   "Dormant BH with red giant companion (~8.9 Msun)"},
    {"Gaia BH3",          32.7,   "Most massive nearby dormant BH (~33 Msun)"},
    {"V404 Cygni",        9.0,    "X-ray nova microquasar, K-giant companion (~9 Msun)"},
    {"A0620-00",          6.61,   "First confirmed stellar BH, quiescent K-dwarf (~6.6 Msun)"},
    {"GRO J1655-40",      6.3,    "Microquasar with relativistic jets (~6.3 Msun)"},
    {"NGC 1277",          1.7e10, "Overmassive SMBH in compact elliptical (~1.7e10 Msun)"},
    {"OJ 287",            1.8e10, "SMBH binary, optical outburst source (~1.8e10 Msun)"}
};
constexpr int NUM_BH_PRESETS = sizeof(BH_PRESETS) / sizeof(BH_PRESETS[0]);
