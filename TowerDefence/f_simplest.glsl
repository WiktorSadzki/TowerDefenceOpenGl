#version 330

in vec4 vColor;
in vec4 vNormal;
in vec4 vPos;
in vec3 vViewDir;

out vec4 pixelColor;

uniform vec3 lightDirGlobal; 

uniform vec3 bulletPos;    
uniform vec3 bulletColor;   
uniform float bulletActive; 

void main() {
    vec3 n = normalize(vNormal.xyz);
    vec3 view = normalize(vViewDir);
    
    float shininess = 32.0;
    vec3 specularColor = vec3(1.0, 1.0, 1.0);
    
    vec3 lGlobal = normalize(lightDirGlobal);
    float nlGlobal = clamp(dot(n, lGlobal), 0.0, 1.0);
    vec3 colorGlobal = vColor.rgb * nlGlobal;

    vec3 specGlobal = vec3(0.0);
    if (nlGlobal > 0.0) {
        vec3 reflectGlobal = reflect(-lGlobal, n);
        float specFactorGlobal = max(dot(reflectGlobal, view), 0.0);
        specGlobal = specularColor * pow(specFactorGlobal, shininess);
    }
    
    vec3 colorBullet = vec3(0.0);
    vec3 specBullet = vec3(0.0);
    if (bulletActive > 0.5) {
        vec3 lBullet = normalize(bulletPos - vPos.xyz);
        float dist = length(bulletPos - vPos.xyz);
        float attenuation = 1.0 / (1.0 + 0.1 * dist + 0.05 * dist * dist);
        
        float nlBullet = clamp(dot(n, lBullet), 0.0, 1.0);
        colorBullet = bulletColor * nlBullet * attenuation;
        
        if (nlBullet > 0.0) {
            vec3 reflectBullet = reflect(-lBullet, n);
            float specFactorBullet = max(dot(reflectBullet, view), 0.0);
            specBullet = bulletColor * pow(specFactorBullet, shininess) * attenuation;
        }
    }
    
    vec3 ambient = vColor.rgb * 0.2; 

    pixelColor = vec4(ambient + colorGlobal + specGlobal + colorBullet + specBullet, vColor.a);
}