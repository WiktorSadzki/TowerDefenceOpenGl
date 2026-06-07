#version 330 compatibility

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec4 vColor;
out vec3 vNormal;
out vec4 vPos;
out vec3 vViewDir;
out vec2 vTexCoord;
out vec3 vViewDirTangent;
out vec3 vTangentEye;
out vec3 vWorldPos;

void main() {
    vColor = vec4(inColor, 1.0);
    vTexCoord = inTexCoord;

    // Compute and pass world position
    vec4 worldPos = M * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;

    mat4 MV = V * M;
    vec4 eyeSpacePos = MV * vec4(inPosition, 1.0);
    vPos = eyeSpacePos;

    vec3 transformedNormal = mat3(MV) * inNormal;
    float lenTrans = length(transformedNormal);
    if (lenTrans > 0.0001) {
        vNormal = transformedNormal / lenTrans;
    } else {
        vNormal = vec3(0.0, 1.0, 0.0);
    }

    vViewDir = -eyeSpacePos.xyz;

    vec3 N = normalize(mat3(MV) * inNormal);
    vec3 T = normalize(mat3(MV) * inTangent);
    T = normalize(T - dot(T, N) * N); 
    vTangentEye = T;
    vec3 B = cross(N, T);

    mat3 TBN_inv = transpose(mat3(T, B, N));
    vViewDirTangent = TBN_inv * normalize(-eyeSpacePos.xyz);

    gl_Position = P * eyeSpacePos;
}