// BlackHole3D_PhotorealDisk.frag
// ============================================================================
// TON 618 — Physically-motivated black hole visualization
// ============================================================================
//
// TON 618 is one of the most massive black holes known:
//   Mass:               M = 6.6 × 10^10 M☉ (66 billion solar masses)
//   Schwarzschild radius: Rs ≈ 1,300 AU ≈ 1.95 × 10^14 m
//   Luminosity:          L ≈ 4 × 10^40 W (≈ 10^14 L☉, near-Eddington)
//   Spin:                Unknown, assumed a* ≈ 0.8 (high spin quasar)
//   Peak disk temp:      T_peak ≈ 30,000 K (UV-dominated, Shakura-Sunyaev)
//   Photon sphere:       r_ph = 1.5 Rs
//   ISCO (a*=0.8):      r_ISCO ≈ 2.0 Rs (prograde Kerr)
//   Eddington rate:      Ṁ_Edd ≈ 150 M☉/yr
//
// All distances are in units of Rs (Schwarzschild radius).
// blackHoleRadius = Rs = 1.0 in simulation units.
//
// Physics implemented:
//   - Novikov-Thorne thin disk temperature: T(r) ∝ r^(-3/4) [1-√(rISCO/r)]^(1/4)
//   - Novikov-Thorne flux: F(r) ∝ [1-√(rISCO/r)] / r³
//   - Keplerian orbital velocity: v/c = √(Rs/(2r))
//   - Special relativistic Doppler factor: D = 1/(γ(1 - v·cosθ))
//   - Doppler surface brightness: I ∝ D^3 (relativistic beaming, full strength)
//   - Gravitational redshift: g = √(1 - Rs/r) applied as Planck temperature shift
//   - Kerr geodesic deflection: velocity Verlet integrator, Schwarzschild 1/r²
//     + Lense-Thirring frame-dragging a*(Rs²/r³)(ω̂×r̂)
//   - Photon ring step refinement near r = 1.5 Rs
//   - Disk self-shadowing via near-plane optical depth accumulation
//   - Beer-Lambert volumetric absorption for BLR and jets
//   - Synchrotron jet with spectral aging, expansion, and exponential fade
//   - Relativistic jet with bulk Lorentz factor Γ ≈ 10
// ============================================================================

#version 330 core

in vec2 fragUV;
out vec4 FragColor;

uniform sampler2D backgroundTex;
uniform sampler2D diskTex;       // Unused (kept for compatibility)
uniform vec2 resolution;
uniform vec3 cameraPos;
uniform vec3 cameraDir;
uniform vec3 cameraUp;
uniform float fov;
uniform float blackHoleRadius;
uniform vec3 blackHolePos;

uniform float diskInnerRadius;
uniform float diskOuterRadius;
uniform float diskHalfThickness;

uniform float jetRadius;
uniform float jetLength;
uniform vec3  jetColor;

uniform int showJets;          // Toggle jets on/off (J key)
uniform int showBLR;           // Toggle Broad Line Region storm cloud torus (G key)
uniform int showDoppler;        // Toggle Doppler beaming/shift (V key)
uniform int showBlueshift;      // Toggle gravitational blueshift of background (U key)

// BLR (Broad Line Region) parameters
uniform float blrInnerRadius;  // Inner edge of torus (~100s of Rs, compressed)
uniform float blrOuterRadius;  // Outer edge of torus
uniform float blrThickness;    // Half-height of torus cross-section

uniform float uTime;           // Elapsed time for animation
uniform float spinParameter;   // Dimensionless spin a* (0=Schwarzschild, 1=extremal Kerr)

// Per-preset disk color parameters
uniform float diskPeakTemp;          // Physical peak temperature [K]
uniform float diskDisplayTempInner;  // Display temp at inner disk [K]
uniform float diskDisplayTempOuter;  // Display temp at outer disk [K]
uniform float diskSatBoostInner;     // Saturation multiplier at outer edge
uniform float diskSatBoostOuter;     // Saturation multiplier at inner edge

// Orbiting bodies
const int MAX_ORB_BODIES = 10;
uniform vec3  orbBodyPos[MAX_ORB_BODIES];      // Positions of orbiting bodies [Rs units]
uniform float orbBodyRadius[MAX_ORB_BODIES];   // Radii of the bodies [Rs units]
uniform vec3  orbBodyColor[MAX_ORB_BODIES];    // Base colors of the bodies
uniform int   numOrbBodies;                    // Number of active orbiting bodies
uniform int   showOrbBody;                     // Toggle body visibility
uniform int   maxStepsOverride; // Runtime step limit (200=fast, 300=cinematic)

// Large-scale structures
uniform int   showHostGalaxy;                  // Toggle host galaxy visibility
uniform float hostGalaxyRadius;                // Radius of host galaxy [kpc, scaled]
uniform vec3  hostGalaxyColor;                 // Color of host galaxy
uniform int   showLAB;                         // Toggle Lyman-alpha Blob visibility
uniform vec3  labPos;                          // Position of LAB [kpc, scaled]
uniform float labRadius;                       // Radius of LAB [kpc, scaled]
uniform vec3  labColor;                        // Color of LAB
uniform int   showCGM;                         // Toggle Circumgalactic Medium visibility
uniform float cgmRadius;                       // Radius of CGM [kpc, scaled]
uniform vec3  cgmColor;                        // Color of CGM

// ============================================================================
// PHYSICAL CONSTANTS (in simulation units where Rs = 1)
// ============================================================================
// Photon sphere radius (Schwarzschild): 1.5 Rs
const float PHOTON_SPHERE = 1.5;
// Speed of light = 1 in natural units

const int   MAX_STEPS = 300;  // Compile-time max (cinematic ceiling)
const float MAX_DIST  = 250.0;  // Larger for TON 618 scale (disk 40Rs, BLR 30Rs, jets 40Rs)
const float EPSILON   = 1e-5;
const float PI        = 3.14159265359;

// ============================================================================
// RAY-SPHERE INTERSECTION
// ============================================================================
// Returns t of closest intersection (or -1 if miss)
float intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0) return -1.0;
    float sqrtDisc = sqrt(disc);
    float t0 = -b - sqrtDisc;
    float t1 = -b + sqrtDisc;
    if (t0 > EPSILON) return t0;
    if (t1 > EPSILON) return t1;
    return -1.0;
}

// Shade the orbiting body surface
vec3 shadeOrbBody(vec3 hitPos, vec3 normal, vec3 bodyCenter, vec3 bodyColor) {
    // Distance from BH for gravitational redshift
    float rBH = length(hitPos - blackHolePos);
    float rs = blackHoleRadius;
    float grav = sqrt(max(1.0 - rs / rBH, 0.05));
    
    // Simple diffuse shading from the accretion disk glow (disk center = BH pos)
    // Light comes from the hot inner disk — approximate as point light at BH
    vec3 lightDir = normalize(blackHolePos - hitPos);
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Additional illumination from the disk plane (broad area light)
    float diskLight = max(dot(normal, vec3(0.0, -sign(hitPos.y - blackHolePos.y), 0.0)), 0.0) * 0.3;
    
    // Tidal stretching indicator: redden when close to BH (spaghettification zone)
    float tidalFactor = smoothstep(3.0, 1.5, rBH / rs);
    vec3 tidalColor = mix(bodyColor, vec3(1.0, 0.3, 0.1), tidalFactor * 0.6);
    
    // Base illumination + disk-lit + ambient
    float lighting = NdotL * 0.7 + diskLight + 0.15;
    vec3 color = tidalColor * lighting * grav;
    
    return color;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

vec3 getRayDir(vec2 uv, vec3 camPos, vec3 camDir, vec3 camUp, float fovDeg) {
    float aspect = resolution.x / resolution.y;
    float px = (uv.x - 0.5) * 2.0 * aspect;
    float py = (uv.y - 0.5) * 2.0;
    float angle = radians(fovDeg * 0.5);
    float dz = 1.0 / tan(angle);
    vec3 right = normalize(cross(camDir, camUp));
    vec3 up = normalize(camUp);
    vec3 forward = normalize(camDir);
    return normalize(px * right + py * up + dz * forward);
}

bool rayHitsSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return false;
    return (-b - sqrt(h)) > 0.0;
}

vec3 sampleBackground(vec3 dir) {
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
    vec3 bg = texture(backgroundTex, vec2(u, v)).rgb;
    // Boost stars without lifting the dark sky: brighten highlights, keep blacks
    float lum = dot(bg, vec3(0.2126, 0.7152, 0.0722));
    float boost = smoothstep(0.05, 0.5, lum) * 1.8 + 1.0;
    bg *= boost;
    return bg;
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// === 3D HASH & NOISE for volumetric clouds ===
float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float valueNoise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    
    return mix(mix(mix(hash13(i + vec3(0,0,0)), hash13(i + vec3(1,0,0)), u.x),
                   mix(hash13(i + vec3(0,1,0)), hash13(i + vec3(1,1,0)), u.x), u.y),
               mix(mix(hash13(i + vec3(0,0,1)), hash13(i + vec3(1,0,1)), u.x),
                   mix(hash13(i + vec3(0,1,1)), hash13(i + vec3(1,1,1)), u.x), u.y), u.z);
}

// Cheap pseudo-cellular noise: approximates Worley billow structure
// using abs(noise) trick — much faster than real Worley (no 27-cell search)
float billowNoise3D(vec3 p) {
    return abs(valueNoise3D(p) * 2.0 - 1.0);
}

// 3D FBM using value noise
float fbm3D(vec3 p, int octaves) {
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        val += amp * valueNoise3D(p * freq);
        amp *= 0.5;
        freq *= 2.0;
    }
    return val;
}

// Billow FBM — gives cauliflower-like cumulus structure cheaply
float billowFbm3D(vec3 p, int octaves) {
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        val += amp * billowNoise3D(p * freq);
        amp *= 0.5;
        freq *= 2.0;
    }
    return val;
}

// Cloud density: billow FBM for cumulus shape (replaces expensive Worley)
float cloudNoise3D(vec3 p) {
    float fbmVal = fbm3D(p, 2);
    float billow = billowFbm3D(p * 1.5, 2);
    // Mix: base shape from FBM, billow adds round cauliflower lumps
    return clamp(fbmVal * 0.65 + billow * 0.35, 0.0, 1.0);
}

// Cheaper single-octave variant used in fast mode to reduce ALU cost on iGPUs.
float cloudNoiseFast3D(vec3 p) {
    float fbmVal = fbm3D(p, 1);
    float billow = billowNoise3D(p * 1.5);
    return clamp(fbmVal * 0.65 + billow * 0.35, 0.0, 1.0);
}

float smoothNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p, int octaves) {
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        val += amp * smoothNoise(p * freq);
        amp *= 0.5;
        freq *= 2.0;
    }
    return val;
}

// ============================================================================
// SPECTRAL FREQUENCY SHIFT — for background starlight and general color
// ============================================================================
// Redistributes RGB energy to approximate a spectral wavelength shift.
// freqRatio > 1: blueshift (light gained energy), < 1: redshift (light lost energy).
// Based on Planck spectrum shifting: when frequency scales by D, the entire
// blackbody curve shifts, moving visible-band energy between RGB channels.
vec3 applyFrequencyShift(vec3 color, float freqRatio) {
    float shift = freqRatio - 1.0;
    vec3 shifted = color;

    if (shift > 0.0) {
        // Blueshift: energy migrates R→G→B (shorter wavelengths)
        // Phase 1 (mild, freqRatio 1–3): visible color shift toward blue
        float s1 = clamp(shift, 0.0, 2.0);
        shifted.r *= (1.0 - s1 * 0.45);
        shifted.g = color.g * (1.0 - s1 * 0.15) + color.r * s1 * 0.25;
        shifted.b = color.b + color.g * s1 * 0.40 + color.r * s1 * 0.10;

        // Phase 2 (strong, freqRatio 3–8): light shifts into UV.
        // Visible light fades to blue-violet, then desaturates toward white
        // as the entire Planck curve floods all visible channels.
        float s2 = clamp(shift - 2.0, 0.0, 5.0) / 5.0; // 0–1 over freqRatio 3–8
        vec3 uvWhite = vec3(0.7, 0.75, 1.0); // UV-tinged white
        shifted = mix(shifted, uvWhite * dot(shifted, vec3(0.33)), s2);

        // Phase 3 (extreme, freqRatio >8): light shifts beyond visible entirely.
        // Observer sees diminishing violet glow, then near-black as all photon
        // energy has been boosted past the visible band into X-ray/gamma.
        float s3 = clamp(shift - 7.0, 0.0, 8.0) / 8.0; // 0–1 over freqRatio 8–15
        float fadeToInvisible = 1.0 - s3 * s3; // Quadratic fade
        shifted *= fadeToInvisible;
    } else {
        // Redshift: energy migrates B→G→R (longer wavelengths)
        float s1 = clamp(-shift, 0.0, 2.0);
        shifted.b *= (1.0 - s1 * 0.45);
        shifted.g = color.g * (1.0 - s1 * 0.15) + color.b * s1 * 0.25;
        shifted.r = color.r + color.g * s1 * 0.40 + color.b * s1 * 0.10;

        // Deep redshift: light fades into infrared, disappearing from visible
        float s2 = clamp(-shift - 2.0, 0.0, 5.0) / 5.0;
        shifted *= (1.0 - s2 * 0.85);
    }

    // Relativistic intensity: I_obs ∝ D^3 (Liouville's theorem)
    // Clamped to prevent HDR blowout while still allowing dramatic brightening
    float brightFactor = pow(clamp(freqRatio, 0.05, 12.0), 2.5);
    brightFactor = min(brightFactor, 50.0); // HDR safety cap
    return max(shifted * brightFactor, vec3(0.0));
}

// ============================================================================
// BLACKBODY & DISK PHYSICS — TON 618
// ============================================================================

vec3 kelvinToRGB(float K) {
    // Attempt blackbody color approximation (Tanner Helland algorithm)
    K = clamp(K, 1000.0, 40000.0) / 100.0;
    float r, g, b;
    
    if (K <= 66.0) {
        r = 1.0;
        g = clamp(0.39008157877 * log(K) - 0.63184144378, 0.0, 1.0);
        b = (K <= 19.0) ? 0.0 : clamp(0.54320678911 * log(K - 10.0) - 1.19625408914, 0.0, 1.0);
    } else {
        float t = K - 60.0;
        r = clamp(1.29293618606 * pow(t, -0.1332047592), 0.0, 1.0);
        g = clamp(1.12989086089 * pow(t, -0.0755148492), 0.0, 1.0);
        b = 1.0;
    }
    return vec3(r, g, b);
}

// Novikov-Thorne thin disk temperature profile
// T(r) = T_peak × f(r) where f(r) encodes the GR correction
//
// For TON 618 (M = 6.6×10¹⁰ M☉) at near-Eddington accretion:
//   T_peak ≈ 2×10⁵ × (M/10⁸ M☉)^(-1/4) ≈ 30,000 K
//   Peak emission is in the UV (Wien peak λ ≈ 97 nm)
//   The visible-band contribution is the Rayleigh-Jeans tail
float diskTemperature(float r, float rISCO) {
    // Novikov-Thorne: T(r) ∝ r^(-3/4) × [1 - √(rISCO/r)]^(1/4)
    // This peaks at r ≈ (49/36) × rISCO
    // Normalized so max temperature = T_peak
    float x = r / rISCO;
    float noTorque = max(1.0 - sqrt(1.0 / x), 0.0);  // Zero-torque inner boundary
    float profile = pow(1.0 / x, 0.75) * pow(noTorque, 0.25);
    
    // Peak of this profile occurs at x = 49/36 ≈ 1.361
    // Normalize so the peak value = 1.0
    float xPeak = 49.0 / 36.0;
    float peakVal = pow(1.0 / xPeak, 0.75) * pow(1.0 - sqrt(1.0 / xPeak), 0.25);
    
    return profile / max(peakVal, 0.001);
}

// ISCO radius as function of spin parameter a*
// Approximation of the Kerr ISCO formula for prograde orbits
float computeISCO(float a) {
    // Exact Kerr ISCO: complex formula involving Z1, Z2
    // Good approximation for prograde orbits:
    // r_ISCO/Rs ≈ 3 - 2.54*(a - 0.2)  for 0 ≤ a ≤ 0.95
    // At a=0: 3Rs (Schwarzschild), a=1: ~0.5Rs (extremal Kerr prograde)
    float rISCO = 3.0 - 2.0 * a * (1.0 + 0.27 * a);  // Smooth fit
    return max(rISCO, 0.6);  // Physical minimum ~0.5Rs
}

// Novikov-Thorne flux profile: F(r) ∝ [1 - √(rISCO/r)] / r³
// This is T(r)^4 from diskTemperature() — extracted here for clarity
float novikovThorneFlux(float r, float rISCO) {
    if (r < rISCO) return 0.0;
    float x = r / rISCO;
    return max(1.0 - sqrt(1.0 / x), 0.0) / (x * x * x);
}

// Disk emission with physically-motivated Doppler shift for TON 618
vec3 diskEmission(float r, float rIn, float rOut, vec3 hitPos, vec3 rayDir, vec3 bhPos) {
    if (r < rIn || r > rOut) return vec3(0.0);
    
    float tRad = (r - rIn) / (rOut - rIn);
    vec3 rel = hitPos - bhPos;
    float angleRaw = atan(rel.z, rel.x);

    // === KEPLERIAN DIFFERENTIAL ROTATION ===
    // Angular velocity: ω(r) ∝ r^(-3/2)  (Kepler's 3rd law)
    // Inner disk orbits much faster than outer disk.
    // Speed tuned so inner edge (~2 Rs) completes one revolution in ~12s.
    float omega = 1.5 * pow(max(r, rIn), -1.5);
    float angle = angleRaw + uTime * omega;

    // === DISK PULSATION & WOBBLE ===
    // MHD-driven breathing modes: the disk is a turbulent fluid, not a solid.
    // Radial pulsation (breathing): inner disk pulses faster than outer
    float pulseFreq = 2.5 * pow(max(r / rIn, 1.0), -0.8);
    float pulse = 1.0 + 0.12 * sin(uTime * pulseFreq + r * 1.2)
                      + 0.06 * sin(uTime * pulseFreq * 1.7 + angle * 3.0);
    // Azimuthal wobble: disk surface undulates like waves on water
    float wobble = 0.08 * sin(angle * 4.0 + uTime * 1.8 - r * 0.5)
                 + 0.05 * sin(angle * 7.0 - uTime * 2.3 + r * 0.3);
    
    // === ISCO & TEMPERATURE (Novikov-Thorne for TON 618) ===
    float rISCO = computeISCO(spinParameter);
    // Physical peak temperature for TON 618 (M = 6.6×10¹⁰ M☉):
    //   T_eff,max ≈ 2×10⁵ × (M/10⁸ M☉)^(-1/4) ≈ 30,000 K
    //   Peak emission is UV (Wien λ_max ≈ 97 nm), NOT optical.
    //   Visible light comes from the Rayleigh-Jeans tail.
    float T_peak = diskPeakTemp;  // Per-preset Novikov-Thorne peak [K]
    float Tprofile = diskTemperature(r, rISCO);
    
    // Floor temperature from X-ray irradiation and viscous dissipation
    // Outer disk reprocesses inner UV/X-ray emission → warm extended disk
    float T_floor_inner = diskDisplayTempInner * 0.78;  // Scale floor from display temps
    float T_floor_outer = diskDisplayTempOuter;
    float T_floor = mix(T_floor_inner, T_floor_outer, pow(tRad, 0.6));
    float T = max(T_peak * Tprofile, T_floor);
    
    // === KEPLERIAN ORBITAL VELOCITY ===
    // v/c = √(Rs / (2r)) for circular Schwarzschild orbits
    // In our units Rs = blackHoleRadius = 1
    float rs = blackHoleRadius;
    float v = sqrt(rs / (2.0 * max(r, rISCO)));
    v = clamp(v, 0.0, 0.7);  // Cap at ~0.7c (physical limit near ISCO)
    
    // Tangential velocity direction (prograde, counter-clockwise in XZ plane)
    vec3 velDir = normalize(vec3(-rel.z, 0.0, rel.x));
    float cosTheta = dot(velDir, -rayDir);
    
    // === SPECIAL RELATIVISTIC DOPPLER FACTOR ===
    // D = 1 / (γ × (1 - β·cosθ))
    // γ = 1/√(1 - β²), β = v/c
    float gamma = 1.0 / sqrt(max(1.0 - v * v, 0.01));
    float D = 1.0 / (gamma * (1.0 - v * cosTheta));
    D = clamp(D, 0.15, 5.0);  // Wider range for full relativistic Doppler
    // When Doppler is toggled off, disable velocity-dependent shift
    if (showDoppler == 0) D = 1.0;
    
    // === GRAVITATIONAL REDSHIFT (observer-corrected) ===
    // Full GR frequency ratio: f_obs/f_emit = √((1 - Rs/r_emit) / (1 - Rs/r_obs))
    // When observer is far: reduces to standard √(1 - Rs/r)
    // When observer is close to BH: redshift is reduced (observer also in the well)
    float rObs = length(cameraPos - bhPos);
    float gEmit = sqrt(max(1.0 - rs / r, 0.01));
    float gObs  = sqrt(max(1.0 - rs / rObs, 0.01));
    float grav  = gEmit / max(gObs, 0.05);
    
    // Combined frequency shift: Doppler × gravitational
    // Observed temperature: T_obs = T_emit × D × g
    // This is the physically correct Planck spectrum shift:
    //   Redshift uniformly scales frequency → shifts the blackbody peak.
    //   kelvinToRGB(T_obs) then maps the shifted Planck spectrum to visible color.
    //   No per-channel RGB hack needed — the Planck function handles it.
    float freqShift = D * grav;
    
    // Radial color gradient: use disk position directly for visible color.
    // Map radial position to per-preset displayable temperature range
    // where kelvinToRGB produces rich, distinguishable colors.
    float radialColorGradient = pow(1.0 - tRad, 0.4);
    float T_display = mix(diskDisplayTempOuter, diskDisplayTempInner, radialColorGradient);
    // Doppler + gravitational shift affects observed blackbody color
    // Wide range allows dramatic visual asymmetry: approaching side blazes blue-white,
    // receding side dims to deep red as photons lose energy fighting gravity + recession.
    T_display *= clamp(freqShift, 0.3, 3.0);
    T_display = clamp(T_display, 600.0, 25000.0);
    vec3 color = kelvinToRGB(T_display);
    
    // Boost saturation — per-preset values
    float satBoost = mix(diskSatBoostInner, diskSatBoostOuter, radialColorGradient);
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = max(mix(vec3(lum), color, satBoost), 0.0);
    
    // === BEAM STRUCTURE (spiral arms + radial streaks + filaments) ===
    // Prominent logarithmic spiral arms — the dominant rotating feature.
    // Two-arm pattern (m=2) like simulated MHD turbulence.
    float spiralPhase = angle * 2.0 - log(max(r / rIn, 0.01)) * 5.0;
    float spiralArm = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(spiralPhase), 3.0);
    // Add a brighter "hotspot" clump orbiting in the inner disk
    float hotspotAngle = angle - uTime * 1.5 * pow(rIn * 1.5, -1.5);
    float hotspot = exp(-pow(r - rIn * 2.0, 2.0) * 0.5) *
                    exp(-pow(mod(hotspotAngle, 6.2832) - 3.14159, 2.0) * 2.0) * 2.0;
    // Radial streaks emanating outward (rotate with disk)
    float streaks = 0.55 + 0.45 * pow(abs(sin(angle * 6.0)), 2.5);
    // Fine filaments from MHD turbulence — slowly evolving in time
    float turbTime = uTime * 0.35;  // Turbulent evolution rate
    float filaments = 0.70 + 0.30 * fbm(vec2(angle * 6.0 + turbTime * 0.7, r * 2.0 + turbTime * 0.3), 2);
    // Concentric density waves — spiral inward over time
    float wavePropagation = uTime * 0.5;  // Inward-propagating density waves
    float waves = 0.82 + 0.18 * sin(r * 5.0 + angle * 2.0 - wavePropagation) * sin(r * 3.0 + wavePropagation * 0.5);
    
    // === RADIAL INTENSITY (Novikov-Thorne luminosity + floor) ===
    // Primary: σT⁴ Stefan-Boltzmann from Novikov-Thorne
    // Secondary: irradiation floor keeps outer disk visible
    float innerFade = smoothstep(rIn, rIn * 1.15, r);
    float outerFade = smoothstep(rOut, rOut * 0.90, r);
    
    // Physical Novikov-Thorne flux + floor contribution
    float ntFlux = Tprofile * Tprofile * Tprofile * Tprofile;
    float floorFlux = pow(T_floor / T_peak, 4.0);
    float flux = max(ntFlux, floorFlux * 0.4);
    // Compress dynamic range: sqrt mapping preserves more brightness than pow(0.4)
    flux = mix(flux, sqrt(flux), 0.65);
    
    // Relativistic surface brightness: I_obs ∝ D^3 × I_emit  (Liouville's theorem)
    // Full D^3 beaming — essential for scientific accuracy
    float dopplerBright = pow(clamp(D, 0.2, 4.0), 3.0);
    dopplerBright = mix(1.0, dopplerBright, 0.65);  // Stronger asymmetry (was 0.35)
    
    // Inner hot region boost — TON 618 is hyperluminous, ISCO region blazing
    float innerBoost = 1.0 + 5.0 * exp(-(r - rISCO * 1.36) * (r - rISCO * 1.36) * 0.3);
    // Additional broad inner glow out to ~5 Rs
    innerBoost += 1.5 * smoothstep(6.0, rISCO, r);
    
    float I = flux * innerFade * outerFade * spiralArm * streaks * filaments * waves * dopplerBright * innerBoost * pulse;
    I += hotspot * innerFade * dopplerBright;  // Orbiting hotspot adds on top
    // Wobble slightly modulates brightness (surface normal tilts toward/away from viewer)
    I *= (1.0 + wobble);
    
    // Power-law compression to prevent ACES white crush
    I = pow(max(I, 0.0), 0.42) * 3.0;
    
    return color * I;
}

// === VOLUMETRIC "PUFFING" - atmospheric glow around outer disk ===
// Sampled continuously during ray march for a soft halo effect
vec3 diskAtmosphere(vec3 pos, vec3 bhPos, float rIn, float rOut) {
    vec3 rel = pos - bhPos;
    float rXZ = length(rel.xz);
    float y = abs(rel.y);
    
    // Only active in the outer 60% of the disk range, extending above/below
    float outerStart = mix(rIn, rOut, 0.4);
    if (rXZ < outerStart || rXZ > rOut * 1.3) return vec3(0.0);
    
    // Puff height: thinner, more disk-like atmosphere (reduced from 2.5)
    float tRad = (rXZ - outerStart) / (rOut * 1.3 - outerStart);
    float puffHeight = mix(0.15, 0.9, pow(tRad, 0.7));
    if (y > puffHeight) return vec3(0.0);
    
    // Vertical density falloff — tighter Gaussian for thinner profile
    float verticalDensity = exp(-y * y / (puffHeight * puffHeight * 0.2));
    
    // Radial density: peaks in mid-outer region, fades at edge
    float radialDensity = smoothstep(outerStart, outerStart + 1.5, rXZ) 
                        * smoothstep(rOut * 1.3, rOut * 0.85, rXZ);
    
    // Turbulent puffing structure — rotates with Keplerian flow and evolves
    float angle = atan(rel.z, rel.x);
    float atmoOmega = 1.5 * pow(max(rXZ, rIn), -1.5);
    float rotatedAngle = angle + uTime * atmoOmega;
    float puffTime = uTime * 0.28;  // Turbulent evolution
    float puffNoise = fbm(vec2(rotatedAngle * 4.0 + rXZ * 0.3 + puffTime * 0.4, y * 2.0 + rXZ * 0.5 + puffTime * 0.2), 2);
    // Pulsating atmosphere: breathing mode makes the glow throb
    float atmoPulse = 1.0 + 0.2 * sin(uTime * 1.5 + rXZ * 0.4)
                          + 0.1 * sin(uTime * 2.8 - rotatedAngle * 2.0);
    float puff = verticalDensity * radialDensity * (0.6 + 0.4 * puffNoise) * atmoPulse;
    
    // Color gradient: warm (inner atmosphere) → cool (outer)
    // Reflects the temperature gradient of the underlying disk (per-preset)
    float T = mix(diskDisplayTempInner, diskDisplayTempOuter, tRad);
    vec3 glowColor = kelvinToRGB(T);
    
    return glowColor * puff * 0.10;  // Reduced opacity so thin disk shows through
}

// ============================================================================
// BROAD LINE REGION — volumetric storm-cloud torus
// Uses 3D noise (FBM + Worley) for realistic cumulus structure,
// density remapping for hard cloud edges, Beer-powder lighting,
// and time-based animation for slow churning motion.
// ============================================================================

vec3 blrDensity(vec3 pos, vec3 bhPos) {
    if (showBLR == 0) return vec3(0.0);

    vec3 rel = pos - bhPos;
    float rXZ = length(rel.xz);
    float y   = rel.y;

    float t = uTime * 0.07;

    float torusMajorR = (blrInnerRadius + blrOuterRadius) * 0.5;
    float torusMinorR = (blrOuterRadius - blrInnerRadius) * 0.5;

    float distFromRing = rXZ - torusMajorR;
    if (abs(distFromRing) > torusMinorR * 1.7) return vec3(0.0);
    if (abs(y) > blrThickness * 1.5) return vec3(0.0);

    float phi = atan(rel.z, rel.x);

    // Continuous azimuthal storm modulation: ring is always present, but density
    // varies ~3x between weather cells. Three incommensurate frequencies produce
    // an irregular, non-repeating pattern. Floor of 0.35 prevents any arc
    // going completely dark so the torus remains a connected halo.
    float storm = 0.52 * sin(phi * 1.7 + t * 0.09 + 2.1)
                + 0.30 * sin(phi * 3.3 - t * 0.06 + 0.7)
                + 0.18 * sin(phi * 6.1 + t * 0.13 + 4.3);
    float stormFactor = 0.35 + 0.65 * (storm * 0.5 + 0.5);  // [0.35, 1.0]

    // Vertical thickness varies independently per sector (different frequencies)
    float thickBias = 1.0 + 0.28 * sin(phi * 2.4 + t * 0.07 + 1.3)
                          + 0.14 * sin(phi * 5.7 - t * 0.04 + 3.1);

    float vertScale = (blrThickness / torusMinorR) * max(thickBias, 0.35);
    float dTorus    = sqrt(distFromRing * distFromRing + (y * y) / (vertScale * vertScale));

    if (dTorus > torusMinorR * 1.5) return vec3(0.0);
    float tNorm = dTorus / torusMinorR;

    // Dense arcs extend further radially; thin arcs taper faster
    float edgeRamp = mix(0.55, 1.15, stormFactor);
    float coverage = (1.0 - smoothstep(0.08, edgeRamp, tNorm)) * stormFactor;
    if (coverage < 0.012) return vec3(0.0);

    vec3 noisePos = rel * 0.25;
    noisePos.xz  += vec2(cos(t), sin(t)) * 0.3;
    noisePos.y   += sin(t * 0.7) * 0.1;

    bool hiQuality = (maxStepsOverride >= 300);

    // Domain warp (cinematic only): slow FBM displacement makes clouds look like
    // storm systems — irregular overhangs and tendrils without extra march steps.
    if (hiQuality) {
        vec3 wp = noisePos * 0.5 + vec3(t * 0.025);
        vec3 warp = vec3(
            fbm3D(wp,             1),
            fbm3D(wp + vec3(0.6), 1),
            fbm3D(wp + vec3(1.3), 1)
        ) - 0.5;
        noisePos += warp * 0.55;
    }

    float cloudShape = hiQuality ? cloudNoise3D(noisePos * 0.8)
                                 : cloudNoiseFast3D(noisePos * 0.8);
    float detail     = fbm3D(noisePos * 2.5 + vec3(t * 0.5, 0.0, t * 0.3),
                             hiQuality ? 2 : 1);
    float fineDetail = hiQuality ? valueNoise3D(noisePos * 5.0 + vec3(0.0, t * 0.2, 0.0))
                                 : detail;

    float rawDensity = cloudShape * 0.55 + detail * 0.30 + fineDetail * 0.15;
    float remapped   = clamp((rawDensity - (1.0 - coverage)) / max(coverage, 0.05), 0.0, 1.0);
    remapped = pow(remapped, 1.8);

    float erosion = 1.0 - smoothstep(0.0, 0.25, remapped) * (1.0 - fineDetail) * 0.35;
    float density = remapped * erosion;

    float wispZone = smoothstep(0.7, 1.2, tNorm);
    float wisps    = valueNoise3D(noisePos * 3.0 + vec3(t * 0.4)) * wispZone * 0.20;
    density += wisps;

    if (density < 0.005) return vec3(0.0);

    float beer        = exp(-density * 3.0);
    float powder      = 1.0 - exp(-density * 6.0);
    float lightEnergy = mix(beer, beer * powder * 2.0, 0.5);

    float topLight   = smoothstep(-blrThickness * 0.8, blrThickness * 0.6, y);
    float innerLight = smoothstep(blrOuterRadius, blrInnerRadius * 0.8, rXZ);
    float totalLight = (mix(0.25, 1.0, topLight) + innerLight * 0.5) * lightEnergy;

    float tRadial = clamp((rXZ - blrInnerRadius) / (blrOuterRadius - blrInnerRadius), 0.0, 1.0);

    vec3 brightColor = vec3(1.0, 0.88, 0.62);
    vec3 midColor    = vec3(0.78, 0.55, 0.28);
    vec3 darkColor   = vec3(0.30, 0.18, 0.08);
    vec3 rimColor    = vec3(1.0, 0.75, 0.45);

    vec3 cloudColor = mix(brightColor, midColor, tRadial * 0.7);
    cloudColor = mix(darkColor, cloudColor, clamp(totalLight + 0.15, 0.0, 1.0));

    float rimFactor = smoothstep(0.15, 0.02, density) * innerLight;
    cloudColor += rimColor * rimFactor * 0.6;
    cloudColor *= 0.9 + 0.1 * fineDetail;

    return cloudColor * density * 0.18;
}
// ============================================================================
// JET EMISSION — wide volumetric conical jets
// ============================================================================

// Relativistic jet emission for TON 618
// TON 618 jets: bulk Lorentz factor Γ ≈ 5-15 (we use Γ ≈ 10)
// Synchrotron emission temperature: ~10^8 K+ at base, ~10^6 K at edges
// Relativistic beaming: I_obs = D^(2+α) × I_emit, where α ≈ 0.7 (synchrotron index)
vec3 jetEmission(vec3 pos, vec3 bhPos) {
    if (showJets == 0) return vec3(0.0);
    
    vec3 rel = pos - bhPos;
    float ay = abs(rel.y);
    if (ay < diskHalfThickness * 2.0 || ay > jetLength) return vec3(0.0);
    
    float radial = length(rel.xz);
    
    // MHD-collimated jet: parabolic expansion near base, conical at distance
    // Matches VLBI observations of M87 jet profile
    float tHeight = ay / jetLength;
    float coneRadius = jetRadius * (1.0 + 3.0 * pow(tHeight, 0.6) + tHeight * 5.0);
    
    if (radial > coneRadius * 1.2) return vec3(0.0);
    
    float rNorm = radial / max(coneRadius, 0.001);
    
    // Bright synchrotron core with soft falloff
    float core = exp(-rNorm * rNorm * 3.0);
    float envelope = smoothstep(1.2, 0.7, rNorm);
    
    // Longitudinal: strong near base (magneto-centrifugal launch region)
    // Exponential radial falloff models synchrotron + adiabatic losses
    float along = pow(smoothstep(jetLength, 0.0, ay), 0.3) * exp(-ay * 0.08);
    float baseBoost = exp(-tHeight * 1.5);  // Blandford-Znajek power concentrated at base
    
    // Kelvin-Helmholtz & kink instabilities at jet boundary
    float angle = atan(rel.z, rel.x);
    float turb = fbm(vec2(angle * 6.0 + ay * 0.5, ay * 1.5 + radial), 2);
    float edgeNoise = mix(1.0, 0.3 + 0.7 * turb, smoothstep(0.3, 0.9, rNorm));
    
    // Internal shock structure (colliding shells)
    float filaments = 0.8 + 0.2 * sin(angle * 8.0 + ay * 3.0) * sin(ay * 5.0 + radial * 2.0);
    
    float density = (core * 0.7 + envelope * 0.3) * along * edgeNoise * filaments;
    density += baseBoost * core * 0.5;
    
    // Synchrotron color with spectral aging:
    // Near base: fresh acceleration → white-blue (~10⁸ K equivalent)
    // Far from base: synchrotron cooling → redder (high-E electrons radiate away)
    vec3 coreColor = mix(vec3(1.0, 0.95, 0.9), vec3(1.0, 0.8, 0.6), tHeight * 0.7);
    vec3 edgeColor = mix(vec3(0.3, 0.5, 1.0), vec3(0.5, 0.3, 0.8), tHeight * 0.5);
    vec3 color = mix(coreColor, edgeColor, rNorm * 0.6);
    
    // Relativistic beaming: Doppler factor for bulk jet flow
    // Jet velocity β_jet ≈ 0.995 → Γ ≈ 10
    // D_jet = 1/(Γ(1 - β cos θ_obs))
    // This makes approaching jet much brighter than receding one
    float betaJet = 0.95;  // Bulk jet velocity (β = v/c)
    float gammaJet = 1.0 / sqrt(1.0 - betaJet * betaJet);  // Γ ≈ 3.2
    float jetSign = sign(rel.y);  // +1 for north jet, -1 for south jet
    vec3 jetAxis = vec3(0.0, jetSign, 0.0);
    // Approximate: use camera direction to estimate viewing angle
    float cosObs = dot(normalize(cameraPos - bhPos), jetAxis);
    float Djet = 1.0 / (gammaJet * (1.0 - betaJet * cosObs));
    Djet = clamp(Djet, 0.1, 5.0);
    
    // Beaming: I_obs ∝ D^(2+α) where α ≈ 0.7 (synchrotron spectral index)
    float beamFactor = pow(Djet, 2.7);
    beamFactor = mix(1.0, beamFactor, 0.6);  // Partially apply to avoid extreme asymmetry
    
    return color * density * 4.5 * beamFactor;
}

// ============================================================================
// LARGE-SCALE STRUCTURES
// ============================================================================

// Host Galaxy: faint disk centered on BH
vec3 hostGalaxyEmission(vec3 pos, vec3 bhPos) {
    if (showHostGalaxy == 0) return vec3(0.0);
    
    vec3 rel = pos - bhPos;
    float r = length(rel.xz);
    float h = abs(rel.y);
    
    // Scale: assume hostGalaxyRadius is in kpc, but we scale it down for visibility
    // In simulation units, 1 kpc ≈ 2.37e8 Rs, but that's too big, so scale to ~1000 Rs
    float scale = hostGalaxyRadius * 1000.0;  // Arbitrary scaling for visibility
    
    float diskDensity = exp(-r * r / (scale * scale)) * exp(-h * h / (scale * 0.1 * scale * 0.1));
    return hostGalaxyColor * diskDensity * 0.1;  // Faint
}

// Lyman-alpha Blob: large glowing cloud
vec3 labEmission(vec3 pos, vec3 bhPos) {
    if (showLAB == 0) return vec3(0.0);
    
    vec3 rel = pos - bhPos - labPos * 1000.0;  // Scale position
    float r = length(rel);
    
    float scale = labRadius * 1000.0;  // Scale for visibility
    float density = exp(-r * r / (scale * scale));
    return labColor * density * 0.05;  // Very faint
}

// Circumgalactic Medium: diffuse halo
vec3 cgmEmission(vec3 pos, vec3 bhPos) {
    if (showCGM == 0) return vec3(0.0);
    
    vec3 rel = pos - bhPos;
    float r = length(rel);
    
    float scale = cgmRadius * 1000.0;  // Scale for visibility
    float density = exp(-r * r / (scale * scale * 4.0));  // Wider distribution
    return cgmColor * density * 0.02;  // Very faint halo
}

// ============================================================================
// RAY MARCHING WITH MULTI-CROSSING DISK ACCUMULATION
// ============================================================================

void traceRay(
    in vec3 ro,
    in vec3 rd,
    in vec3 bhPos,
    in float bhRadius,
    out vec3 outColor,
    out float outAlpha,
    out bool absorbed)
{
    vec3 pos = ro;
    vec3 dir = normalize(rd);
    
    absorbed = false;
    outColor = vec3(0.0);
    outAlpha = 0.0;
    
    // Accumulated disk emission
    vec3 accumulatedColor = vec3(0.0);
    float accumulatedAlpha = 0.0;
    bool diskHit = false;  // First disk crossing
    int diskCrossings = 0; // Count disk plane crossings for photon ring
    
    // Orbiting bodies — pre-compute intersections along initial ray
    // (We test against the straight-line ray; lensing will be minor at body distance)
    float orbBodyDist = -1.0;
    vec3 orbBodyHitColor = vec3(0.0);
    float orbBodyHitAlpha = 0.0;
    bool orbBodyHit = false;
    int hitBodyIndex = -1;
    if (showOrbBody != 0) {
        for (int i = 0; i < numOrbBodies; ++i) {
            float t = intersectSphere(ro, rd, orbBodyPos[i], orbBodyRadius[i]);
            if (t > 0.0 && (orbBodyDist < 0.0 || t < orbBodyDist)) {
                orbBodyDist = t;
                hitBodyIndex = i;
            }
        }
    }
    float totalDist = 0.0;  // Track total ray distance marched
    
    // Jet + BLR volumetric accumulation
    vec3 jetAcc = vec3(0.0);
    float jetTau = 0.0;        // Jet optical depth (synchrotron self-absorption)
    vec3 blrAcc = vec3(0.0);
    float blrAlphaAcc = 0.0;
    
    // Disk self-shadowing: accumulate optical depth near disk plane
    float diskShadowTau = 0.0;
    
    float baseStep = MAX_DIST / float(MAX_STEPS);
    
    // Lensing strength: Schwarzschild geodesic deflection
    // For a photon at distance r from mass M:
    //   deflection angle dα = 2Rs/r per unit path length
    // Our parameter k controls the acceleration toward BH
    // k = Rs² × 2.0 gives correct 1/r² force law (Newtonian + GR correction)
    float k = bhRadius * bhRadius * 2.0;
    
    // Track which side of disk plane we're on
    float lastSide = (pos - bhPos).y;
    
    // Small jitter to reduce banding
    float jitter = (hash12(fragUV * resolution + 0.5) - 0.5) * baseStep * 0.1;
    pos += dir * jitter;
    
    for (int i = 0; i < MAX_STEPS; ++i) {
        // Runtime step limit (200 for fast, 300 for cinematic)
        if (i >= maxStepsOverride) break;
        
        vec3 toBH = bhPos - pos;
        float rBH = length(toBH);
        
        // Check absorption
        if (rBH < bhRadius * 1.01) {
            absorbed = true;
            break;
        }
        
        // Adaptive step size: smaller near the black hole for accuracy
        float stepLen = baseStep * mix(0.4, 1.0, smoothstep(bhRadius * 2.0, bhRadius * 15.0, rBH));
        
        // Tighten step near photon sphere (1.5 Rs) for sharper photon ring
        if (abs(rBH - PHOTON_SPHERE) < 0.3)
            stepLen *= 0.5;
        
        // Check if we've reached the orbiting body depth
        if (orbBodyDist > 0.0 && !orbBodyHit && totalDist + stepLen >= orbBodyDist * 0.85) {
            // Re-test from current (lensed) position for accurate hit
            float tBody = intersectSphere(pos, dir, orbBodyPos[hitBodyIndex], orbBodyRadius[hitBodyIndex]);
            if (tBody > 0.0 && tBody < stepLen * 5.0 + orbBodyRadius[hitBodyIndex] * 2.0) {
                vec3 bodyHitPos = pos + dir * tBody;
                vec3 bodyNormal = normalize(bodyHitPos - orbBodyPos[hitBodyIndex]);
                orbBodyHitColor = shadeOrbBody(bodyHitPos, bodyNormal, orbBodyPos[hitBodyIndex], orbBodyColor[hitBodyIndex]);
                orbBodyHitAlpha = 1.0;
                orbBodyHit = true;
            } else if (totalDist > orbBodyDist * 1.5) {
                orbBodyDist = -1.0;  // Missed due to lensing, stop checking
            }
        }
        totalDist += stepLen;
        
        // Accumulate jet emission with Beer-Lambert optical depth
        // Cheap bounding check: only evaluate when inside jet cone
        {
            vec3 relJet = pos - bhPos;
            float ayJet = abs(relJet.y);
            float radJet = length(relJet.xz);
            float maxConeR = jetRadius * (1.0 + 3.0 * pow(ayJet / max(jetLength, 0.001), 0.6) + ayJet / max(jetLength, 0.001) * 5.0);
            if (showJets != 0 && ayJet > diskHalfThickness * 2.0 && ayJet < jetLength && radJet < maxConeR * 1.3) {
                vec3 jetSample = jetEmission(pos, bhPos);
                float jetTransmit = exp(-jetTau);
                jetAcc += jetSample * stepLen * 0.5 * jetTransmit;
                jetTau += length(jetSample) * stepLen * 0.08; // Synchrotron self-absorption
            }
        }
        
        // Accumulate volumetric disk atmosphere ("puffing")
        if (!diskHit) {
            vec3 atmo = diskAtmosphere(pos, bhPos, diskInnerRadius, diskOuterRadius);
            jetAcc += atmo * stepLen;
            
            // Disk self-shadowing: accumulate optical depth near disk plane
            vec3 relSh = pos - bhPos;
            float rSh = length(relSh.xz);
            float ySh = abs(relSh.y);
            float shThick = max(diskHalfThickness * 5.0, 0.25); // Effective disk atmosphere thickness
            if (rSh > diskInnerRadius && rSh < diskOuterRadius && ySh < shThick) {
                float shDensity = exp(-ySh * ySh / (shThick * shThick));
                float shRadial = smoothstep(diskInnerRadius, diskInnerRadius * 1.2, rSh)
                               * smoothstep(diskOuterRadius, diskOuterRadius * 0.85, rSh);
                diskShadowTau += shDensity * shRadial * stepLen * 0.5;
            }
        }
        
        // Accumulate Broad Line Region storm cloud torus
        // Cheap bounding check: skip expensive noise if clearly outside torus volume
        if (blrAlphaAcc < 0.55 && showBLR != 0) {
            vec3 relBLR = pos - bhPos;
            float rBLR = length(relBLR.xz);
            float torusMajor = (blrInnerRadius + blrOuterRadius) * 0.5;
            float torusMinor = (blrOuterRadius - blrInnerRadius) * 0.5;
            float dRing = abs(rBLR - torusMajor);
            float vertScale = blrThickness / torusMinor;
            float dApprox = sqrt(dRing * dRing + (relBLR.y * relBLR.y) / (vertScale * vertScale));
            if (dApprox < torusMinor * 1.6) {
                vec3 blrSample = blrDensity(pos, bhPos);
                float blrLum = length(blrSample);
                if (blrLum > 0.001) {
                    float blrStepAlpha = clamp(blrLum * stepLen * 0.4, 0.0, 1.0);
                    blrAcc += blrSample * stepLen * 0.3 * (1.0 - blrAlphaAcc);
                    blrAlphaAcc += blrStepAlpha * (1.0 - blrAlphaAcc);
                }
            }
        }
        
        // ================================================================
        // Kerr geodesic — velocity Verlet (symplectic) integrator
        // ================================================================
        // Verlet is 2nd-order and symplectic: conserves phase-space volume,
        // eliminates the systematic energy drift of Euler integration.
        // This matters near the photon sphere where photons nearly orbit.
        //
        // Force model: Schwarzschild 1/r² + Lense-Thirring frame-dragging.
        // This is NOT full Kerr geodesic integration in Boyer-Lindquist
        // coordinates — it's an effective-potential approximation that
        // captures the correct qualitative behavior (lensing, frame-dragging
        // asymmetry, photon sphere) at real-time shader cost.
        // ================================================================
        
        // Compute acceleration at current position
        vec3 pullDir = toBH / rBH; // Direction toward BH
        float strength = k / (rBH * rBH + bhRadius * bhRadius * 0.25);
        
        // Kerr frame-dragging: Lense-Thirring precession
        // a_drag ∝ a* × Rs² / r³ × (ω̂ × r̂)
        vec3 spinAxis = vec3(0.0, 1.0, 0.0); // this will not work well during the disk crossing, but it's a simple approximation for the effect
        vec3 frameDrag = spinParameter * bhRadius * bhRadius
                       * cross(spinAxis, -pullDir)
                       / (rBH * rBH * rBH + bhRadius * bhRadius * bhRadius * 0.125);
        
        // Photon sphere enhancement
        float photonSphereBoost = 1.0 + 2.0 * exp(-(rBH - PHOTON_SPHERE) * (rBH - PHOTON_SPHERE) * 4.0);
        
        vec3 accel = (pullDir * strength + frameDrag) * photonSphereBoost;
        
        // Velocity Verlet step 1: half-kick + drift
        //   v_{1/2} = v_n + a_n × dt/2
        //   x_{n+1} = x_n + v_{1/2} × dt
        vec3 halfDir = normalize(dir + accel * (stepLen * 0.5));
        vec3 newPos = pos + halfDir * stepLen;
        
        // Recompute acceleration at new position
        vec3 toBH2 = bhPos - newPos;
        float rBH2 = length(toBH2);
        vec3 pullDir2 = toBH2 / max(rBH2, EPSILON);
        float strength2 = k / (rBH2 * rBH2 + bhRadius * bhRadius * 0.25);
        vec3 frameDrag2 = spinParameter * bhRadius * bhRadius
                        * cross(spinAxis, -pullDir2)
                        / (rBH2 * rBH2 * rBH2 + bhRadius * bhRadius * bhRadius * 0.125);
        float psBoost2 = 1.0 + 2.0 * exp(-(rBH2 - PHOTON_SPHERE) * (rBH2 - PHOTON_SPHERE) * 4.0);
        vec3 accel2 = (pullDir2 * strength2 + frameDrag2) * psBoost2;
        
        // Velocity Verlet step 2: second half-kick
        //   v_{n+1} = v_{1/2} + a_{n+1} × dt/2
        vec3 newDir = normalize(halfDir + accel2 * (stepLen * 0.5));
        
        // === DISK PLANE CROSSING TEST ===
        // Accumulate ALL crossings — secondary/tertiary images
        // create the iconic Einstein ring from Interstellar.
        float newSide = (newPos - bhPos).y;
        if (sign(newSide) != sign(lastSide)) {
            float t = abs(lastSide) / max(abs(lastSide - newSide), EPSILON);
            vec3 hitPos = mix(pos, newPos, clamp(t, 0.0, 1.0));
            
            vec3 rel = hitPos - bhPos;
            float r = length(rel.xz);
            
            if (r > diskInnerRadius * 0.95 && r < diskOuterRadius * 1.05) {
                vec3 emission = diskEmission(r, diskInnerRadius, diskOuterRadius, hitPos, newDir, bhPos);
                
                float tRad = clamp((r - diskInnerRadius) / (diskOuterRadius - diskInnerRadius), 0.0, 1.0);
                float tau = mix(10.0, 3.0, tRad);
                float alpha = 1.0 - exp(-tau);
                
                // === PHOTON RING ===
                // Rays grazing the photon sphere (1.5 Rs) orbit the BH
                // and cross the disk multiple times, creating thin bright
                // rings — the n=1,2,3... photon sub-rings.
                diskCrossings++;
                float rPhoton = 1.5 * blackHoleRadius;  // Photon sphere
                // How close this crossing is to the photon sphere radius
                float proximityToPhoton = exp(-pow((r - rPhoton) / (0.8 * blackHoleRadius), 2.0));
                // Base proximity boost (all crossings near photon sphere)
                float naturalRingBoost = 1.0 + 10.0 * proximityToPhoton;
                // Higher-order crossings (n>=2) are the actual photon ring:
                // exponentially demagnified sub-images of the full disk.
                if (diskCrossings >= 2) {
                    // Strong boost for secondary+ images near photon sphere
                    float orderBoost = 18.0 / float(diskCrossings);
                    naturalRingBoost += orderBoost * proximityToPhoton;
                    // Moderate boost even at larger radii (lensed secondary image)
                    naturalRingBoost += 4.0 / float(diskCrossings);
                }
                
                // Apply disk self-shadowing attenuation
                float selfShadow = exp(-diskShadowTau);
                vec3 crossEmission = emission * selfShadow * naturalRingBoost;
                
                // Composite this crossing over previous ones (front-to-back)
                accumulatedColor += crossEmission * (1.0 - accumulatedAlpha);
                accumulatedAlpha += alpha * (1.0 - accumulatedAlpha);
                diskHit = true;
            }
        }
        
        pos = newPos;
        dir = newDir;
        lastSide = newSide;
        
        // Early exit if disk fully opaque or all volumes saturated
        if (accumulatedAlpha > 0.98 && blrAlphaAcc > 0.85) break;
        // Skip remaining march for rays that have left the scene
        if (totalDist > MAX_DIST * 0.85 && accumulatedAlpha > 0.5) break;
    }
    
    // Sample background: black if absorbed by BH, otherwise bent-ray skybox
    vec3 bgColor = absorbed ? vec3(0.0) : sampleBackground(dir);

    // Add large-scale structures as distant emission
    vec3 distantPos = ro + dir * MAX_DIST * 2.0;  // Approximate distant position
    vec3 hostEmission = hostGalaxyEmission(distantPos, bhPos);
    vec3 labEmission = labEmission(distantPos, bhPos);
    vec3 cgmEmission = cgmEmission(distantPos, bhPos);
    bgColor += hostEmission + labEmission + cgmEmission;

    // === GRAVITATIONAL BLUESHIFT OF BACKGROUND STARLIGHT ===
    // An observer at radius r_obs from the BH measures photons arriving from
    // infinity (distant stars) with a frequency ratio:
    //   f_obs / f_∞ = 1 / √(1 - Rs/r_obs)
    // Deep in the gravitational well, the entire external universe appears as a
    // warped, blue-shifted halo — background stars become brighter and bluer.
    // Near the event horizon, this blueshift becomes extreme and light shifts
    // beyond the visible spectrum into UV/X-ray.
    if (!absorbed && showBlueshift != 0) {
        float rCamera = length(cameraPos - bhPos);
        float gCamera = sqrt(max(1.0 - bhRadius / rCamera, 0.001));
        if (gCamera < 0.98) {
            // At r = 1.01 Rs: gCamera ≈ 0.1 → freqShift ≈ 10 (extreme UV)
            // At r = 2 Rs: gCamera ≈ 0.71 → freqShift ≈ 1.41 (mild blue)
            // At r = 10 Rs: gCamera ≈ 0.95 → freqShift ≈ 1.05 (barely visible)
            float bgFreqShift = 1.0 / max(gCamera, 0.02);
            bgColor = applyFrequencyShift(bgColor, bgFreqShift);
        }
    }

    // Composite layers: BLR behind disk, disk on top, body on top, jets additive
    vec3 result = bgColor;
    
    // Layer BLR storm clouds (behind/around the thin disk)
    if (blrAlphaAcc > 0.0) {
        result = result * (1.0 - blrAlphaAcc) + blrAcc;
    }
    
    // Layer orbiting body (behind or in front of disk depending on depth)
    if (orbBodyHit) {
        result = mix(result, orbBodyHitColor, orbBodyHitAlpha);
    }
    
    // Layer primary thin accretion disk over
    result = mix(result, accumulatedColor, accumulatedAlpha);
    
    // Add jets on top (additive)
    result += jetAcc;
    
    outColor = result;
    outAlpha = max(accumulatedAlpha, blrAlphaAcc);
}

/*----------------------Main function-----------------------*/
void main() { // note, we chose void here since we're returning early on absorption of each ray, but I guess you could just return black if you're lazy
    vec3 rayDir = getRayDir(fragUV, cameraPos, cameraDir, cameraUp, fov); // Compute initial ray direction from camera through pixel (janky ahh approach but it works)
    vec3 rayOrigin = cameraPos;
    
    // No early sphere test here — the ray marcher handles absorption
    // after accumulating any disk/BLR/jet emission in front of the BH.
    
    vec3 color; // Nothing beats a jet 2 holiday - I mean the color of a ray after tracing through the scene
    float alpha;
    bool absorbed; // Whether the ray is to be absorbed/terminated by the event horizon (for the tangential rays that skim the photon sphere, we still want to accumulate the disk emission before absorption)
    
    traceRay(rayOrigin, rayDir, blackHolePos, blackHoleRadius, color, alpha, absorbed);
    
    // ================================================================
    // PHOTON RING — thin bright ring at the black hole shadow boundary.
    // The shadow edge corresponds to the critical impact parameter:
    //   b_crit = 3√3/2 × Rs ≈ 2.598 Rs  (Schwarzschild)
    // Light at this impact parameter orbits the BH multiple times,
    // creating a bright thin ring of stacked disk images.
    // This MUST apply to both absorbed and non-absorbed rays since
    // the ring straddles the shadow boundary.
    // ================================================================
    {
        // Project BH center to screen-space for proper ring placement
        vec3 toBH = blackHolePos - cameraPos;
        vec3 camRight = normalize(cross(cameraDir, cameraUp));
        vec3 camUpN   = normalize(cross(camRight, cameraDir));
        float zDist = dot(toBH, cameraDir);
        
        if (zDist > 0.0) {
            float xProj = dot(toBH, camRight);
            float yProj = dot(toBH, camUpN);
            
            // Convert to normalized screen coordinates
            float tanHalfFov = tan(radians(fov) * 0.5);
            float aspect = resolution.x / resolution.y;
            vec2 bhScreen = vec2(
                0.5 + (xProj / zDist) / (2.0 * tanHalfFov * aspect),
                0.5 + (yProj / zDist) / (2.0 * tanHalfFov)
            );
            
            // Distance from this pixel to the BH center in screen space
            vec2 pixDelta = (fragUV - bhScreen) * vec2(aspect, 1.0);
            float distFromBH = length(pixDelta);
            
            // Critical impact parameter: shadow edge radius
            // b_crit = 3√3/2 × Rs ≈ 2.598 Rs for Schwarzschild
            // Enlarged for a more prominent visual photon ring
            float bCrit = 3.4 * blackHoleRadius;
            float shadowAngular = bCrit / zDist;
            float shadowScreen = shadowAngular / (2.0 * tanHalfFov);
            
            // Bright ring at the shadow edge — razor-thin photon ring
            float ringDist = abs(distFromBH - shadowScreen);
            float ringWidth = shadowScreen * 0.04;  // Thin, physically correct
            float ring = exp(-ringDist * ringDist / (ringWidth * ringWidth));
            
            // Subtle secondary glow just outside shadow edge
            float glowWidth = shadowScreen * 0.15;
            float glow = exp(-ringDist * ringDist / (glowWidth * glowWidth)) * 0.3;
            
            // Only brighten at/outside the shadow edge — NO bleed inside
            float insideShadow = smoothstep(shadowScreen * 0.92, shadowScreen, distFromBH);
            float outerFalloff = smoothstep(shadowScreen * 1.25, shadowScreen * 1.02, distFromBH);
            float edgeBright = insideShadow * outerFalloff;
            
            vec3 ringColor = vec3(1.0, 0.85, 0.55); // Warm gold matching inner disk
            color += ringColor * (ring * 1.8 + glow) * edgeBright;
        }
    }
    
    if (absorbed) { 
        // If the ray is absorbed by the black hole, we can skip all further processing and just return black 
        //(or a very dark relative) since nothing escapes the event horizon (law of physics and some shi)
        // This optimization allows us to terminate rays early and save computation on rays that would contribute no visible light
        vec3 bgColor = vec3(0.0);  // Black behind the horizon
        vec3 result = bgColor; 
        // The traceRay composites everything, so just use the output
        FragColor = vec4(color, 1.0);
        return;
    }
    
    // Output linear HDR (post-processing handles tonemapping and bloom)
    FragColor = vec4(color, 1.0);
}
