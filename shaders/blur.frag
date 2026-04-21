// blur.frag - Gaussian blur (configurable direction)
#version 330 core

in vec2 fragUV;
out vec4 FragColor;

uniform sampler2D inputTex;
uniform vec2 direction;      // (1,0) for horizontal, (0,1) for vertical
uniform vec2 resolution;

// 9-tap Gaussian weights for sigma ~2.5
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
