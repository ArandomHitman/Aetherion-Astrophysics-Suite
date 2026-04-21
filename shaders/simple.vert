// simple.vert - Full-screen quad vertex shader
#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 fragUV;

void main() {
    fragUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
