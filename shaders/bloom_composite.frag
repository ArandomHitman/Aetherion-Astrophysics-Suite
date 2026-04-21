// bloom_composite.frag - Combine scene with bloom layers + cinematic post-FX
#version 330 core

in vec2 fragUV;
out vec4 FragColor;

uniform sampler2D sceneTex;       // Original rendered scene
uniform sampler2D bloom1Tex;      // First bloom mip (fine detail)
uniform sampler2D bloom2Tex;      // Second bloom mip (medium glow)
uniform sampler2D bloom3Tex;      // Third bloom mip (wide glow)
uniform sampler2D bloom4Tex;      // Fourth bloom mip (very wide glow)

uniform float bloomIntensity;     // Overall bloom strength
uniform float bloom1Weight;       // Weight for each mip level
uniform float bloom2Weight;
uniform float bloom3Weight;
uniform float bloom4Weight;
uniform float exposure;
uniform int   cinematicMode;      // 0 = fast, 1 = cinematic
uniform float uTime;              // Time for animated effects

// ACES Filmic tonemapping
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ── Cinematic helpers ──────────────────────────────────────
// Integer-based hash — avoids sin() banding on some GPUs
float hash(vec2 p) {
    uvec2 q = uvec2(ivec2(p)) * uvec2(1597334673u, 3812015801u);
    uint n = (q.x ^ q.y) * 1597334673u;
    return float(n) * (1.0 / float(0xffffffffu));
}

// Subtle film grain
float filmGrain(vec2 uv, float t) {
    vec2 seed = uv * vec2(1920.0, 1080.0) + vec2(t * 173.17, t * 291.31);
    return (hash(seed) - 0.5) * 0.018;
}

// Chromatic aberration — subtle radial RGB offset at edges
vec3 chromaticAberration(sampler2D tex, vec2 uv, float strength) {
    vec2 center = uv - 0.5;
    float dist = length(center);
    vec2 dir = center * dist * strength;
    float r = texture(tex, uv + dir).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - dir).b;
    return vec3(r, g, b);
}

// Cinematic vignette — darkens edges for film framing
float vignette(vec2 uv) {
    vec2 d = uv - 0.5;
    return 1.0 - dot(d, d) * 1.4;
}

// Anamorphic horizontal bloom stretch
vec3 anamorphicStreak(sampler2D tex, vec2 uv, float spread) {
    vec3 streak = vec3(0.0);
    float totalWeight = 0.0;
    for (int i = -6; i <= 6; ++i) {
        float w = exp(-float(i * i) / 8.0);
        vec2 offset = vec2(float(i) * spread, 0.0);
        streak += texture(tex, uv + offset).rgb * w;
        totalWeight += w;
    }
    return streak / totalWeight;
}

void main() {
    vec3 scene;
    
    // Cinematic: apply chromatic aberration to scene sampling
    if (cinematicMode == 1) {
        scene = chromaticAberration(sceneTex, fragUV, 0.003);
    } else {
        scene = texture(sceneTex, fragUV).rgb;
    }
    
    // Sample all bloom mip levels
    vec3 b1 = texture(bloom1Tex, fragUV).rgb;
    vec3 b2 = texture(bloom2Tex, fragUV).rgb;
    vec3 b3 = texture(bloom3Tex, fragUV).rgb;
    vec3 b4 = texture(bloom4Tex, fragUV).rgb;
    
    // Weighted combination of bloom layers
    vec3 bloom = b1 * bloom1Weight 
               + b2 * bloom2Weight 
               + b3 * bloom3Weight 
               + b4 * bloom4Weight;
    
    // Warm bloom tint — richer in cinematic mode
    vec3 warmTint = (cinematicMode == 1) 
        ? vec3(1.0, 0.88, 0.62) 
        : vec3(1.0, 0.9, 0.75);
    bloom *= warmTint;
    
    // Anamorphic horizontal streak in cinematic mode
    if (cinematicMode == 1) {
        vec3 streak = anamorphicStreak(bloom1Tex, fragUV, 0.004) * bloom1Weight
                    + anamorphicStreak(bloom2Tex, fragUV, 0.006) * bloom2Weight;
        bloom += streak * 0.35 * warmTint;
    }
    
    // Combine scene + bloom
    vec3 color = scene + bloom * bloomIntensity;
    
    // Exposure and tonemap
    color *= exposure;
    color = ACESFilm(color);
    
    // Cinematic post-processing
    if (cinematicMode == 1) {
        // Color grading: subtle teal shadows, warm gold highlights
        vec3 shadowTint = vec3(0.03, 0.05, 0.07);   // Subtle blue-teal shadows
        vec3 highlightTint = vec3(1.02, 0.96, 0.88); // Warm gold highlights
        float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float shadowMask = 1.0 - smoothstep(0.0, 0.35, lum);
        float highlightMask = smoothstep(0.5, 1.0, lum);
        color += shadowTint * shadowMask * 0.2;
        color *= mix(vec3(1.0), highlightTint, highlightMask * 0.3);

        // Subtle contrast boost
        color = mix(vec3(lum), color, 1.12);

        // Vignette
        float vig = vignette(fragUV);
        color *= vig;
        
        // Film grain
        float grain = filmGrain(fragUV, uTime);
        color += grain;
    }
    
    // Gamma correction
    color = pow(max(color, 0.0), vec3(1.0 / 2.2));
    
    FragColor = vec4(color, 1.0);
}
