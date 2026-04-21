// luminosity_highpass.frag - Extract bright areas for bloom
#version 330 core

in vec2 fragUV;
out vec4 FragColor;

uniform sampler2D sceneTex;
uniform float threshold;      // Luminosity threshold (e.g., 0.8)
uniform float softKnee;       // Soft knee width (e.g., 0.5)

void main() {
    vec3 color = texture(sceneTex, fragUV).rgb;
    
    // Calculate luminosity
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Soft knee threshold curve for smoother bloom
    float knee = threshold * softKnee;
    float soft = lum - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    
    float contribution = max(soft, lum - threshold) / max(lum, 0.0001);
    contribution = clamp(contribution, 0.0, 1.0);
    
    // Output bright parts only
    FragColor = vec4(color * contribution, 1.0);
}
