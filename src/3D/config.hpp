#pragma once
// ============================================================
// config.hpp — Centralized simulation parameters and magic numbers
// ============================================================

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <string>


// ah yes, NEATNESS. I love organization, when it works (hours wasted trying to fix what was actually just a semicolon typo and a missing header: 2)

/*--------- Window defaults ---------*/
namespace cfg {

constexpr unsigned int DEFAULT_WIDTH  = 1280;
constexpr unsigned int DEFAULT_HEIGHT = 720;

/*--------- Camera ---------*/
struct CameraConfig {
    glm::vec3 initialPos     = glm::vec3(0.0f, 6.0f, 25.0f);
    float     initialYaw     = 0.0f;
    float     initialPitch   = -0.235f;
    float     orbitPitch0    = 0.245f;
    float     orbitPitchMax  = 1.35f;
    float     minOrbitRadius = 1.6f;
    float     moveSpeed      = 10.0f;
    float     fastMultiplier = 3.0f;     // Shift multiplier
    float     rotateSpeed    = 0.9f;
    float     zoomSpeed      = 10.0f;
    float     mouseSensitivity = 0.0025f;
    float     freelookPitchLimit = 1.5607f;  // Nearly π/2
    float     fov            = 60.0f;
};

/*--------- Accretion disk ---------*/
struct DiskConfig {
    float innerRadius    = 2.0f;   // ≈ ISCO for a*=0.8
    float outerRadius    = 20.0f;  // Hot visible disk
    float halfThickness  = 0.02f;
    // Per-preset disk color temperatures (Kelvin)
    float peakTemp       = 30000.0f; // Novikov-Thorne peak temp
    float displayTempInner = 4500.0f; // Display color temp at inner disk
    float displayTempOuter = 1800.0f; // Display color temp at outer disk
    float saturationBoostInner = 2.2f; // Saturation boost at outer edge
    float saturationBoostOuter = 1.1f; // Saturation boost at inner edge
};

/*--------- Black hole ---------*/
struct BlackHoleConfig {
    float radius         = 1.0f;   // Schwarzschild radius (simulation unit)
    float spinParameter  = 0.8f;   // Dimensionless Kerr spin: 0–1
    glm::vec3 position   = glm::vec3(0.0f);
};

/*--------- Relativistic jets ---------*/
struct JetConfig {
    float     radius  = 0.35f;
    float     length  = 40.0f;   // Extended for quasar jet
    glm::vec3 color   = glm::vec3(0.25f, 0.9f, 1.45f); // Synchrotron blue
};

/*--------- Broad Line Region ---------*/
struct BLRConfig {
    float innerRadius = 18.0f;  // Clear gap from disk edge
    float outerRadius = 35.0f;  // Compressed BLR scale
    float thickness   = 6.0f;   // Thick torus geometry
};

/*--------- Orbiting body ---------*/
struct OrbitalConfig {
    float     semiMajor   = 50.0f;    // Semi-major axis [Rs]
    float     eccentricity = 0.15f;
    float     inclination  = 0.35f;   // Radians (~20°)
    float     bodyRadius   = 0.8f;    // Visual radius [Rs]
    glm::vec3 bodyColor    = glm::vec3(0.85f, 0.65f, 0.45f);
    float     timeScale    = 0.5f;    // Normal speed
    float     fastTimeScale = 15.0f;  // Toggled fast speed
    int       keplerIterMax = 8;
    float     keplerTolerance = 1e-6f;
};

/*--------- Bloom / post-processing ---------*/
struct BloomConfig {
    float threshold     = 1.2f;
    float softKnee      = 0.6f;
    float intensity     = 0.50f;
    float mipWeights[4] = { 0.25f, 0.40f, 0.55f, 0.65f };
    float exposure      = 1.0f;
};

/*--------- Master configuration ---------*/
struct SimConfig {
    CameraConfig   camera; 
    DiskConfig     disk;
    BlackHoleConfig blackHole;
    JetConfig      jet;
    BLRConfig      blr;
    OrbitalConfig  orbital;
    BloomConfig    bloom;
};

// Cinematic bloom: lower threshold, richer glow, warmer, higher exposure. 
inline BloomConfig cinematicBloom() {
    BloomConfig bc;
    bc.threshold  = 0.55f;
    bc.softKnee   = 0.8f;
    bc.intensity   = 0.80f;
    bc.mipWeights[0] = 0.30f;
    bc.mipWeights[1] = 0.50f;
    bc.mipWeights[2] = 0.70f;
    bc.mipWeights[3] = 0.85f;
    bc.exposure    = 1.15f;
    return bc; // houston we have a problem: returning a struct by value causes a copy, which is bad for performance. 
    // but if I return by reference I have to make it static or something, which is gross and also not thread safe. 
    // so here we are, returning by value and hoping for NRVO to kick in and elide the copy. if it doesn't, well, 
    // it's just a few floats, so maybe it's not the end of the world. but still, this is pretty unideal
}

// Default configuration (TON 618 scenario)
inline SimConfig defaultConfig() { return SimConfig{}; }

} // namespace cfg
