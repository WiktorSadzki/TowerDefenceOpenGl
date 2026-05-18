#version 330

in vec4 vColor;
in vec3 vNormal;
in vec4 vPos;
in vec3 vViewDir;

out vec4 pixelColor;

uniform vec3 lightDirGlobal; 
uniform vec3 bulletPos;    
uniform vec3 bulletColor;   
uniform float bulletActive; 

void main() {
    vec3 n = normalize(vNormal);
    vec3 view = normalize(vViewDir);
    
    float shininess = 28.0;
    float specularStrength = 0.5;
    vec3 specularColor = vec3(specularStrength);
    
    vec3 lGlobalSpace = normalize((gl_ModelViewMatrix * vec4(lightDirGlobal, 0.0)).xyz);
    
    float nlGlobal = clamp(dot(n, lGlobalSpace), 0.0, 1.0);
    vec3 colorGlobal = vColor.rgb * nlGlobal;

    vec3 specGlobal = vec3(0.0);
    if (nlGlobal > 0.0) {
        vec3 reflectGlobal = reflect(-lGlobalSpace, n);
        float specFactorGlobal = max(dot(reflectGlobal, view), 0.0);
        specGlobal = specularColor * pow(specFactorGlobal, shininess);
    }
    
    vec3 colorBullet = vec3(0.0);
    vec3 specBullet = vec3(0.0);
    
    if (bulletActive > 0.5) {
        vec3 bulletPosSpace = (gl_ModelViewMatrix * vec4(bulletPos, 1.0)).xyz;
        
        vec3 lBullet = normalize(bulletPosSpace - vPos.xyz);
        float dist = length(bulletPosSpace - vPos.xyz);
        float attenuation = 1.0 / (1.0 + 0.1 * dist + 0.05 * dist * dist);
        
        float nlBullet = clamp(dot(n, lBullet), 0.0, 1.0);
        colorBullet = bulletColor * nlBullet * attenuation;
        
        if (nlBullet > 0.0) {
            vec3 reflectBullet = reflect(-lBullet, n);
            float specFactorBullet = max(dot(reflectBullet, view), 0.0);
            specBullet = bulletColor * pow(specFactorBullet, shininess) * attenuation;
        }
    }
    
    vec3 ambient = vColor.rgb * 0.08;

    vec3 coolFill = vColor.rgb * vec3(0.05, 0.07, 0.12);

    vec3 total = ambient + coolFill + colorGlobal + specGlobal + colorBullet + specBullet;
    pixelColor = vec4(min(total, vec3(1.0)), vColor.a);
}