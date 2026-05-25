#version 330 compatibility

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec4 vColor;
out vec3 vNormal;
out vec4 vPos;
out vec3 vViewDir;
out vec2 vTexCoord;

void main() {
    vColor = gl_Color;
    vTexCoord = gl_MultiTexCoord0.xy;

    mat4 MV = V * M;
    vec4 eyeSpacePos = MV * gl_Vertex;
    vPos = eyeSpacePos;

    vec3 transformedNormal = mat3(MV) * gl_Normal;
    float lenTrans = length(transformedNormal);
    if (lenTrans > 0.0001) {
        vNormal = transformedNormal / lenTrans;
    } else {
        vNormal = vec3(0.0, 1.0, 0.0);
    }

    vViewDir = -eyeSpacePos.xyz;
    gl_Position = P * eyeSpacePos;
}