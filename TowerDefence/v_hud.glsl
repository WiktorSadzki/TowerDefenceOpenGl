#version 330 core
// Simple vertex shader for HUD elements
layout(location = 0) in vec3 aPos;

// Uniform for the combined Model-View-Projection matrix
uniform mat4 MVP;
void main() {
    gl_Position = MVP * vec4(aPos, 1.0); // Transform the vertex position to clip space
}
