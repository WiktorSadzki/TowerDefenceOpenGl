#version 330 compatibility

in vec4 vColor;
in vec3 vNormal;
in vec4 vPos;
in vec3 vViewDir;
in vec2 vTexCoord;

out vec4 pixelColor;

uniform vec3  lightDirGlobal;
uniform vec3  bulletPos;
uniform vec3  bulletColor;
uniform float bulletActive;

uniform sampler2D texBaseColor;
uniform sampler2D texEmissive;
uniform float hasBaseColor;
uniform float hasEmissive;

void main() {
    if (vColor.a < 0.9) {
        pixelColor = vColor;
        return;
    }

    vec3 albedo = vColor.rgb;
    if (hasBaseColor > 0.5) {
        albedo = texture(texBaseColor, vTexCoord).rgb;
    }

    float lenN = length(vNormal);
    vec3 N = (lenN > 0.0001) ? vNormal / lenN : vec3(0.0, 1.0, 0.0);

    float lenV = length(vViewDir);
    vec3 V = (lenV > 0.0001) ? vViewDir / lenV : vec3(0.0, 0.0, 1.0);

    float lenL = length(lightDirGlobal);
    vec3 L = (lenL > 0.0001) ? lightDirGlobal / lenL : vec3(0.0, 1.0, 0.0);

    float nlGlobal = max(dot(N, L), 0.0);

    float ambientStrength = 0.3; 
    vec3 sunColor = vec3(1.0, 0.95, 0.85);
    
    vec3 diffuse = nlGlobal * sunColor * 1.0; 
    vec3 ambient = sunColor * ambientStrength; 

    vec3 H = L + V;
    float lenH = length(H);
    vec3 H_norm = (lenH > 0.0001) ? H / lenH : vec3(0.0);
    float spec = pow(max(dot(N, H_norm), 0.0), 32.0);
    vec3 specular = spec * vec3(0.3);

    vec3 bulletLighting = vec3(0.0);
    if (bulletActive > 0.5) {
        vec3 lightVec = bulletPos - vPos.xyz;
        float dist = length(lightVec);
        if (dist > 0.01) {
            vec3 Lb = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.1 * dist + 0.05 * dist * dist);
            float diffB = max(dot(N, Lb), 0.0);
            bulletLighting = bulletColor * diffB * attenuation * 1.5;
        }
    }

    vec3 emissive = vec3(0.0);
    if (hasEmissive > 0.5) emissive = texture(texEmissive, vTexCoord).rgb;

    vec3 total = (albedo * (ambient + diffuse + bulletLighting)) + specular + emissive;

    if (isnan(total.r) || isnan(total.g) || isnan(total.b) || isinf(total.r) || isinf(total.g) || isinf(total.b)) {
        total = albedo * ambient;
    }

    total = total / (total + vec3(0.5));
    pixelColor = vec4(total, vColor.a);
}