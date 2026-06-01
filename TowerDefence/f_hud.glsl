#version 330 core
// Simple fragment shader for HUD elements
uniform vec4 uColor;
// Output color of the fragment
out vec4 fragColor;
void main() {
    fragColor = uColor; // Set the fragment color to the uniform color
}
