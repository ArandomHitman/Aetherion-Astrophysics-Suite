#pragma once

// ============================================================
// Embedded GLSL shader source strings
// ============================================================

// Vertex shader for full-screen quad (with UV attribute)
inline const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 position;
out vec2 fragUV;
void main() {
    fragUV = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

// Simple alpha-textured passthrough (for HUD overlay, Y-flipped for SFML→GL)
inline const char* hudOverlaySrc = R"(
#version 330 core
in vec2 fragUV;
out vec4 FragColor;
uniform sampler2D hudTex;
void main() {
    FragColor = texture(hudTex, vec2(fragUV.x, 1.0 - fragUV.y));
}
)";

// Simple luminosity highpass shader (embedded fallback)
inline const char* luminosityHighpassSrc = R"(
#version 330 core
in vec2 fragUV;
out vec4 FragColor;
uniform sampler2D sceneTex;
uniform float threshold;
uniform float softKnee;
void main() {
    vec3 color = texture(sceneTex, fragUV).rgb;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = threshold * softKnee;
    float soft = lum - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contribution = max(soft, lum - threshold) / max(lum, 0.0001);
    contribution = clamp(contribution, 0.0, 1.0);
    FragColor = vec4(color * contribution, 1.0);
}
)";

// Gaussian blur shader (embedded fallback)
inline const char* blurShaderSrc = R"(
#version 330 core
in vec2 fragUV;
out vec4 FragColor;
uniform sampler2D inputTex;
uniform vec2 direction;
uniform vec2 resolution;
const float weights[9] = float[](
    0.0162162162, 0.0540540541, 0.1216216216, 0.1945945946, 0.2270270270,
    0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162
);
void main() {
    vec2 texelSize = 1.0 / resolution;
    vec2 offset = direction * texelSize;
    vec3 result = vec3(0.0);
    for (int i = -4; i <= 4; ++i) {
        vec2 sampleUV = fragUV + offset * float(i);
        result += texture(inputTex, sampleUV).rgb * weights[i + 4];
    }
    FragColor = vec4(result, 1.0);
}
)";

// Bloom composite shader (embedded fallback)
inline const char* compositeShaderSrc = R"(
#version 330 core
in vec2 fragUV;
out vec4 FragColor;
uniform sampler2D sceneTex;
uniform sampler2D bloom1Tex;
uniform sampler2D bloom2Tex;
uniform sampler2D bloom3Tex;
uniform sampler2D bloom4Tex;
uniform float bloomIntensity;
uniform float bloom1Weight;
uniform float bloom2Weight;
uniform float bloom3Weight;
uniform float bloom4Weight;
uniform float exposure;
uniform int   cinematicMode;
uniform float uTime;
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash(vec2 p) {
    uvec2 q = uvec2(ivec2(p)) * uvec2(1597334673u, 3812015801u);
    uint n = (q.x ^ q.y) * 1597334673u;
    return float(n) * (1.0 / float(0xffffffffu));
}
float filmGrain(vec2 uv, float t) {
    vec2 seed = uv * vec2(1920.0, 1080.0) + vec2(t * 173.17, t * 291.31);
    return (hash(seed) - 0.5) * 0.018;
}
vec3 chromaticAberration(sampler2D tex, vec2 uv, float strength) {
    vec2 center = uv - 0.5;
    float dist = length(center);
    vec2 dir = center * dist * strength;
    float r = texture(tex, uv + dir).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - dir).b;
    return vec3(r, g, b);
}
float vignette(vec2 uv) {
    vec2 d = uv - 0.5;
    return 1.0 - dot(d, d) * 1.4;
}
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
    if (cinematicMode == 1) {
        scene = chromaticAberration(sceneTex, fragUV, 0.003);
    } else {
        scene = texture(sceneTex, fragUV).rgb;
    }
    vec3 b1 = texture(bloom1Tex, fragUV).rgb;
    vec3 b2 = texture(bloom2Tex, fragUV).rgb;
    vec3 b3 = texture(bloom3Tex, fragUV).rgb;
    vec3 b4 = texture(bloom4Tex, fragUV).rgb;
    vec3 bloom = b1 * bloom1Weight + b2 * bloom2Weight + b3 * bloom3Weight + b4 * bloom4Weight;
    vec3 warmTint = (cinematicMode == 1) ? vec3(1.0, 0.88, 0.62) : vec3(1.0, 0.85, 0.65);
    bloom *= warmTint;
    if (cinematicMode == 1) {
        vec3 streak = anamorphicStreak(bloom1Tex, fragUV, 0.004) * bloom1Weight
                    + anamorphicStreak(bloom2Tex, fragUV, 0.006) * bloom2Weight;
        bloom += streak * 0.35 * warmTint;
    }
    vec3 color = scene + bloom * bloomIntensity;
    color *= exposure;
    color = ACESFilm(color);
    if (cinematicMode == 1) {
        vec3 shadowTint = vec3(0.03, 0.05, 0.07);
        vec3 highlightTint = vec3(1.02, 0.96, 0.88);
        float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
        float shadowMask = 1.0 - smoothstep(0.0, 0.35, lum);
        float highlightMask = smoothstep(0.5, 1.0, lum);
        color += shadowTint * shadowMask * 0.2;
        color *= mix(vec3(1.0), highlightTint, highlightMask * 0.3);
        color = mix(vec3(lum), color, 1.12);
        float vig = vignette(fragUV);
        color *= vig;
        float grain = filmGrain(fragUV, uTime);
        color += grain;
    }
    color = pow(max(color, 0.0), vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
)";
