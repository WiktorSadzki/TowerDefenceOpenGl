#version 330 compatibility

// Vertex shader
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

// Uniform matrices for transformations
uniform mat4 P; // Projection matrix
uniform mat4 V; // View matrix
uniform mat4 M; // Model matrix

// Output variables to the fragment shader
out vec4 vColor;
out vec3 vNormal;
out vec4 vPos;
out vec3 vViewDir;
out vec2 vTexCoord;

void main() {
    vColor = vec4(inColor, 1.0);
    vTexCoord = inTexCoord;

    // Model-View transformation
    mat4 MV = V * M;

    // Transform the vertex position to eye space
    vec4 eyeSpacePos = MV * vec4(inPosition, 1.0);
    vPos = eyeSpacePos;

    // Transform the normal vector and normalize it
    vec3 transformedNormal = mat3(MV) * inNormal;
    float lenTrans = length(transformedNormal);
    // Avoid division by zero when normalizing
    if (lenTrans > 0.0001) {
        vNormal = transformedNormal / lenTrans;
    } else {
        vNormal = vec3(0.0, 1.0, 0.0);
    }

    // Calculate the view direction in eye space
    vViewDir = -eyeSpacePos.xyz;

    // Final transformation to clip space
    gl_Position = P * eyeSpacePos;
}