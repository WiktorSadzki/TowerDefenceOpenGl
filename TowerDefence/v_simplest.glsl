#version 130

out vec4 vColor;
out vec3 vNormal;
out vec4 vPos;
out vec3 vViewDir;

void main() {
    vColor = gl_Color;

    vNormal = normalize(gl_NormalMatrix * gl_Normal); 

    vPos = gl_ModelViewMatrix * gl_Vertex; 
    
    vViewDir = normalize(-vPos.xyz);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}