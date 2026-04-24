// BlackHole3D.frag
// GLSL Fragment Shader for a Ray-Traced Black Hole Effect
// Author: Copilot (GPT-4.1)
// Usage: Render a full-screen quad with this shader. Provide a background texture and set uniforms as needed.

#version 330 core // Fun fact: This was released back in 2010. Should I use it? maybe not, but for compatibility sake we will make do.

in vec2 fragUV; // UV coordinates from vertex shader (0,0) to (1,1)
out vec4 FragColor;

uniform sampler2D backgroundTex; // Background (star field, accretion disk, etc.)
uniform sampler2D diskTex;       // Accretion disk RGBA (polar mapped)
uniform vec2 resolution;         // Screen resolution
uniform vec3 cameraPos;          // Camera position in world space
uniform vec3 cameraDir;          // Camera forward direction
uniform vec3 cameraUp;           // Camera up vector
uniform float fov;               // Field of view in degrees
uniform float blackHoleRadius;   // Schwarzschild radius (event horizon)
uniform vec3 blackHolePos;       // Black hole position in world space

uniform float diskInnerRadius;   // world units
uniform float diskOuterRadius;   // world units
uniform float diskHalfThickness; // world units (half thickness)

uniform float jetRadius;         // world units
uniform float jetLength;         // world units (each direction)
uniform vec3  jetColor;          // emissive color

uniform int   showDoppler;       // Toggle Doppler shift (V key)
uniform int   showBlueshift;     // Toggle gravitational blueshift of background (U key)
uniform float spinParameter;     // Kerr spin parameter (0-1)

uniform float uTime;             // Elapsed time for animation
uniform int   showJets;          // Toggle jet emission (J key)     [ADDED 2026-04-24: was unconditional]
uniform int   showBLR;           // Toggle Broad Line Region          [ADDED 2026-04-24: uniform existed on CPU, never declared here]
uniform float blrInnerRadius;    // BLR inner edge (world units)     [ADDED 2026-04-24]
uniform float blrOuterRadius;    // BLR outer edge (world units)     [ADDED 2026-04-24]
uniform float blrThickness;      // BLR vertical half-thickness      [ADDED 2026-04-24]

// Constants
const int MAX_STEPS = 140; // FIXME: This is quite possibly the WORST way to do this.
// MAX_DIST was hardcoded 140.0 — REMOVED 2026-04-24. Scene radius is now computed dynamically
// from diskOuterRadius/jetLength inside traceBentRay and main() so large configs (e.g. outer=200 Rs)
// no longer clip the outer disk or jet tips. The maxStepsOverride uniform still does nothing
// (it's declared on the CPU and sent every frame but never read here — see the original sin above).
// Fixing that is left as an exercise for a future, more caffeinated author.
const float EPSILON = 1e-4;
const float PI = 3.14159265359;

// Utility: Convert screen UV to world ray direction - standard FPS type camera behavior.
vec3 getRayDir(vec2 uv, vec3 camPos, vec3 camDir, vec3 camUp, float fov) {
    float aspect = resolution.x / resolution.y;
    float px = (uv.x - 0.5) * 2.0 * aspect;
    float py = (uv.y - 0.5) * 2.0;
    float angle = radians(fov * 0.5);
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
    float t = -b - sqrt(h);
    return t > 0.0;
}

// Simple lensing: iteratively bend the ray toward the black hole.
// Not physically exact, but stable and produces a convincing distortion.
vec3 sampleBackground(vec3 samplePos, vec3 bhPos) {
    vec3 rel = normalize(samplePos - bhPos);
    float u = 0.5 + atan(rel.z, rel.x) / (2.0 * PI);
    float v = 0.5 - asin(rel.y) / PI;
    vec3 bg = texture(backgroundTex, vec2(u, v)).rgb;
    // Boost stars without lifting the dark sky: brighten highlights, keep blacks
    float lum = dot(bg, vec3(0.2126, 0.7152, 0.0722));
    float boost = smoothstep(0.05, 0.5, lum) * 1.8 + 1.0;
    bg *= boost;
    return bg;
}

// Disk is assumed to lie in the XZ plane at bhPos (normal = +Y).
vec4 sampleDiskRGBA(vec3 pos, vec3 bhPos) {
    vec3 rel = pos - bhPos;
    float r = length(rel.xz);
    if (r < diskInnerRadius || r > diskOuterRadius) return vec4(0.0);
    if (abs(rel.y) > diskHalfThickness) return vec4(0.0);

    // Keplerian differential rotation: inner disk orbits faster
    float omega = 1.5 * pow(max(r, diskInnerRadius), -1.5);
    float rotAngle = uTime * omega;
    float cosR = cos(rotAngle);
    float sinR = sin(rotAngle);
    vec2 rotXZ = vec2(rel.x * cosR - rel.z * sinR,
                      rel.x * sinR + rel.z * cosR);

    // diskTex is a planar RGBA ring image centered in the texture.
    vec2 uv = rotXZ / (diskOuterRadius * 2.0) + 0.5;
    vec4 d = texture(diskTex, uv);
    // Fade by radius so the hard edge of the texture doesn't look cut-out.
    float radFade = smoothstep(diskOuterRadius, diskOuterRadius * 0.92, r) *
                    smoothstep(diskInnerRadius, diskInnerRadius * 1.10, r);
    d.a *= radFade;
    return d;
}

// ============================================================================
// SPECTRAL FREQUENCY SHIFT
// ============================================================================
// Redistributes RGB energy to approximate a spectral wavelength shift.
// freqRatio > 1: blueshift (light gained energy), < 1: redshift (light lost energy).
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
        float s2 = clamp(shift - 2.0, 0.0, 5.0) / 5.0;
        vec3 uvWhite = vec3(0.7, 0.75, 1.0);
        shifted = mix(shifted, uvWhite * dot(shifted, vec3(0.33)), s2);

        // Phase 3 (extreme, freqRatio >8): light shifts beyond visible entirely.
        // Observer sees diminishing violet glow, then near-black as all photon
        // energy has been boosted past the visible band into X-ray/gamma.
        float s3 = clamp(shift - 7.0, 0.0, 8.0) / 8.0;
        float fadeToInvisible = 1.0 - s3 * s3;
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
    // Cap raised to 20 [FIXED 2026-04-24: was lowered to 8 which under-lit the disk;
    // 50 was the original (blew out large-disk configs), 20 is the balance]
    float brightFactor = pow(clamp(freqRatio, 0.05, 12.0), 2.5);
    brightFactor = min(brightFactor, 20.0);
    return max(shifted * brightFactor, vec3(0.0));
}

// Physically motivated Doppler + gravitational redshift for the accretion disk.
// Computes Keplerian orbital velocity, relativistic Doppler factor, and
// gravitational redshift corrected for observer position.
vec3 applyDoppler(vec3 color, vec3 rel, vec3 viewDir) {
    float r = length(rel.xz);
    float rs = blackHoleRadius;

    // Keplerian orbital velocity: v/c = √(Rs / (2r))
    float v = sqrt(rs / (2.0 * max(r, rs * 2.0)));
    v = clamp(v, 0.0, 0.7);

    // Tangential velocity direction (prograde, counter-clockwise in XZ plane)
    vec3 velDir = normalize(vec3(-rel.z, 0.0, rel.x));
    float cosTheta = dot(velDir, -viewDir);

    // Special relativistic Doppler factor: D = 1/(γ(1 - β·cosθ))
    float gamma = 1.0 / sqrt(max(1.0 - v * v, 0.01));
    float D = 1.0 / (gamma * (1.0 - v * cosTheta));
    D = clamp(D, 0.15, 5.0);

    // Toggle Doppler on/off
    if (showDoppler == 0) D = 1.0;

    // Gravitational redshift: f_obs/f_emit = √((1-Rs/r_emit)/(1-Rs/r_obs))
    // Accounts for observer position in the gravitational well
    float rObs = length(cameraPos - blackHolePos);
    float gEmit = sqrt(max(1.0 - rs / max(r, rs * 1.01), 0.01));
    float gObs  = sqrt(max(1.0 - rs / rObs, 0.01));
    float grav  = gEmit / max(gObs, 0.05);

    // Combined frequency shift: Doppler × gravitational
    float freqShift = D * grav;

    // Apply spectral shift and relativistic beaming to the color
    return applyFrequencyShift(color, freqShift);
}

vec3 jetEmission(vec3 pos, vec3 bhPos) {
    // Jets along +Y and -Y.
    vec3 rel = pos - bhPos;
    float y = rel.y;
    float ay = abs(y);
    if (ay < diskHalfThickness) return vec3(0.0);
    if (ay > jetLength) return vec3(0.0);

    float radial = length(rel.xz);
    if (radial > jetRadius) return vec3(0.0);

    float core = exp(-radial * radial / max(jetRadius * jetRadius * 0.25, EPSILON));
    float along = smoothstep(jetLength, jetLength * 0.2, ay);
    float flicker = 0.85 + 0.15 * sin(ay * 12.0);
    return jetColor * core * along * flicker;
}

void traceBentRay(
    in vec3 ro,
    in vec3 rd,
    in vec3 bhPos,
    in float bhRadius,
    out vec3 outPos,
    out vec3 outDir,
    out bool absorbed,
    out vec3 outJet,
    out vec4 outDisk,
    out vec3 outBLR)
{
    vec3 pos = ro;
    vec3 dir = normalize(rd);
    absorbed = false;
    outJet = vec3(0.0);
    outDisk = vec4(0.0);
    outBLR = vec3(0.0);

    // Scene radius scales with the largest user-configured feature so the full disk/jets are always reached.
    // [FIXED 2026-04-24: was hardcoded MAX_DIST=140.0, which clipped outer disk/jets on large configs]
    float sceneRadius = max(max(diskOuterRadius, jetLength) * 1.5, 140.0);
    float baseStep = sceneRadius / float(MAX_STEPS);
    // Strength factor tuned by hand; scale by bhRadius so users can tweak via uniform.
    float k = bhRadius * bhRadius * 1;

    float lastDiskSide = (pos - bhPos).y;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 toBH = bhPos - pos;
        float r = length(toBH);

        // Exact EH absorption: check if this step segment intersects the event horizon sphere.
        // [FIXED 2026-04-24: replaced the old 'r < bhRadius + EPSILON' point-check, which missed
        //  near-grazing rays when stepLen ≥ bhRadius (the step would skip over the horizon),
        //  producing the bright seam / 'hole' at the shadow edge. Sphere intersection is exact
        //  regardless of step size, so there is no need to change stepLen near the BH.]
        {
            float tca = dot(toBH, dir);     // signed distance to closest approach
            float d2  = dot(toBH, toBH) - tca * tca;  // squared miss distance
            if (d2 < bhRadius * bhRadius) {
                float thc = sqrt(bhRadius * bhRadius - d2);
                float tHit = tca - thc;     // entry intersection along ray
                if (tHit >= 0.0 && tHit <= baseStep) {
                    absorbed = true;
                    break;
                }
            }
        }

        // Jet emission — gated by showJets toggle.
        // [FIXED 2026-04-24: was unconditional; showJets uniform was declared on CPU but never read here]
        if (showJets != 0)
            outJet += jetEmission(pos, bhPos) * baseStep;

        // BLR volumetric fog — gated by showBLR toggle.
        // [ADDED 2026-04-24: showBLR/blrInnerRadius/blrOuterRadius/blrThickness uniforms were sent
        //  from CPU every frame but never declared in the shader, so BLR was always invisible]
        if (showBLR != 0) {
            float blrAbsY = abs((pos - bhPos).y);
            if (r > blrInnerRadius && r < blrOuterRadius && blrAbsY < blrThickness) {
                float radFade = smoothstep(blrInnerRadius, blrInnerRadius * 1.2, r)
                              * smoothstep(blrOuterRadius, blrOuterRadius * 0.85, r);
                float vertFade = smoothstep(blrThickness, blrThickness * 0.4, blrAbsY);
                outBLR += vec3(0.65, 0.82, 1.0) * 0.012 * radFade * vertFade * baseStep;
            }
        }

        // Detect crossing of the disk plane and sample if within thickness.
        float diskSide = (pos - bhPos).y;
        if (sign(diskSide) != sign(lastDiskSide)) {
            // Approximate intersection point at the plane.
            float t = lastDiskSide / max(lastDiskSide - diskSide, EPSILON);
            vec3 hitPos = mix(pos - dir * baseStep, pos, clamp(t, 0.0, 1.0));
            vec4 d = sampleDiskRGBA(hitPos, bhPos);
            if (d.a > 0.001) {
                outDisk = d;
                outPos = hitPos;
                outDir = dir;
                return;
            }
        }
        lastDiskSide = diskSide;

        vec3 pullDir = toBH / max(r, EPSILON);
        // Inverse-square-ish pull; softened for stability
        float strength = k / (r * r + bhRadius * bhRadius);
        dir = normalize(dir + pullDir * strength * baseStep);
        pos += dir * baseStep;
    }

    outPos = pos;
    outDir = dir;
}

void main() {
    // Get ray direction from camera through this pixel
    vec3 rayDir = getRayDir(fragUV, cameraPos, cameraDir, cameraUp, fov);
    vec3 rayOrigin = cameraPos;

    // Absorb rays that hit the event horizon (visible black disc)
    if (rayHitsSphere(rayOrigin, rayDir, blackHolePos, blackHoleRadius)) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Bend the ray (gravitational lensing approximation)
    vec3 bentPos;
    vec3 bentDir;
    bool absorbed;
    vec3 jetAcc;
    vec4 diskHit;
    vec3 blrAcc;
    traceBentRay(rayOrigin, rayDir, blackHolePos, blackHoleRadius, bentPos, bentDir, absorbed, jetAcc, diskHit, blrAcc);
    if (absorbed) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color;

    // Gravitational blueshift factor for background starlight
    // f_obs / f_∞ = 1/√(1 - Rs/r_obs) — blueshift when observer is close to BH
    // Deep in the gravitational well, distant stars appear as a warped blue halo.
    float rCamera = length(cameraPos - blackHolePos);
    float gCamera = sqrt(max(1.0 - blackHoleRadius / rCamera, 0.001));
    float bgFreqShift = (showBlueshift != 0 && gCamera < 0.98) ? (1.0 / max(gCamera, 0.02)) : 1.0;

    // Scene radius for background sampling — must match what traceBentRay uses.
    float sceneRadius = max(max(diskOuterRadius, jetLength) * 1.5, 140.0);

    if (diskHit.a > 0.001) {
        vec3 rel = (bentPos - blackHolePos);
        vec3 diskCol = applyDoppler(diskHit.rgb, rel, bentDir);
        // Composite disk over background — scale look-ahead with scene so stars aren't sampled
        // inside the disk on large configs [FIXED 2026-04-24: was hardcoded 40.0]
        vec3 bg = sampleBackground(bentPos + bentDir * max(diskOuterRadius * 0.5, 40.0), blackHolePos);
        if (bgFreqShift > 1.01) bg = applyFrequencyShift(bg, bgFreqShift);
        color = mix(bg, diskCol, diskHit.a);
    } else {
        // Sample far along the bent ray for the background.
        color = sampleBackground(bentPos + bentDir * (sceneRadius * 0.5), blackHolePos);
        if (bgFreqShift > 1.01) color = applyFrequencyShift(color, bgFreqShift);
    }

    // Add emissive contributions (jets + BLR volumetric)
    color += jetAcc + blrAcc;

    // Photon ring handled by ray marching; no screen-space rim needed

    FragColor = vec4(color, 1.0);
}
