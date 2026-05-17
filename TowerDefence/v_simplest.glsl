#version 130

out vec4 vColor;
out vec3 vNormal;
out vec4 vPos;

void main() {
    vColor = gl_Color;
    
    vNormal = gl_NormalMatrix * gl_Normal; 
    
    vPos = gl_ModelViewMatrix * gl_Vertex; 
    
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}