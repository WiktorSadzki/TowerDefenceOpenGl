#version 330 compatibility

in vec4 vColor;
in vec3 vNormal;
in vec4 vPos;
in vec3 vViewDir;
in vec2 vTexCoord;

out vec4 pixelColor;

// Lights
uniform vec3  lightDirGlobal;
uniform vec3  bulletPos;
uniform vec3  bulletColor;
uniform float bulletActive;

// Textures
uniform sampler2D texBaseColor;
uniform sampler2D texNormal;
uniform sampler2D texMetallic;
uniform sampler2D texRoughness;
uniform sampler2D texAO;
uniform sampler2D texEmissive;
uniform float texBlendScale;

// Presence flags
uniform float hasBaseColor;
uniform float hasNormal;
uniform float hasMetallic;
uniform float hasRoughness;
uniform float hasAO;
uniform float hasEmissive;

uniform float useFixedTBN;
uniform vec3  fixedTangent;
uniform vec3  fixedBitangent;


const float PI = 3.14159265359;

// GGX / Trowbridge-Reitz NDF
float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

// Smith's height-correlated visibility / geometry term
float G_Smith(float NdotV, float NdotL, float roughness) {
    float r  = roughness + 1.0;
    float k  = (r * r) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

// Fresnel-Schlick approximation
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Full Cook-Torrance lobe for one light direction L
vec3 cookTorrance(vec3 N, vec3 V, vec3 L, vec3 F0,
                  float roughness, float metallic, vec3 albedo) {
    vec3  H      = normalize(L + V);
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 0.001);
    float NdotH  = max(dot(N, H), 0.0);
    float HdotV  = max(dot(H, V), 0.0);

    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(HdotV, F0);

    // Specular lobe
    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    // Diffuse: energy-conserving, zero for metals
    vec3 kD      = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + spec) * NdotL;
}

// Normal-map
vec3 applyNormalMap(vec3 N, vec3 eyePos, vec2 uv) {
    vec3 nmSample = texture(texNormal, uv).rgb * 2.0 - 1.0;

    vec3 dp1  = dFdx(eyePos);
    vec3 dp2  = dFdy(eyePos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 0.00001) return N;

    vec3 T = normalize( duv2.y * dp1 - duv1.y * dp2);
    vec3 B = normalize(-duv2.x * dp1 + duv1.x * dp2);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nmSample);
}

void main() {
    // Shadow / ghost pass — skip PBR, draw as-is
    if (vColor.a < 0.9) {
        pixelColor = vColor;
        return;
    }

    // Base color
    vec3 albedo = (hasBaseColor > 0.5)
        ? pow(texture(texBaseColor, vTexCoord).rgb, vec3(2.2))
        : pow(vColor.rgb, vec3(2.2));

    float metallic  = (hasMetallic  > 0.5) ? texture(texMetallic,  vTexCoord).r : 0.0;
    float roughness = (hasRoughness > 0.5) ? texture(texRoughness, vTexCoord).r : 0.1;
    roughness = clamp(roughness, 0.04, 1.0);   // avoid zero-roughness singularity

    float ao = (hasAO > 0.5) ? texture(texAO, vTexCoord).r : 1.0;

    vec3 N = normalize(vNormal);
    if (hasNormal > 0.5) {
        vec3 nmSample = texture(texNormal, vTexCoord).rgb * 2.0 - 1.0;
        vec3 T, B;
        if (useFixedTBN > 0.5) {
            T = normalize(fixedTangent);
            B = normalize(fixedBitangent);
        } else {
            vec3 dp1 = dFdx(vPos.xyz); vec3 dp2 = dFdy(vPos.xyz);
            vec2 duv1 = dFdx(vTexCoord); vec2 duv2 = dFdy(vTexCoord);
            float det = duv1.x * duv2.y - duv1.y * duv2.x;
            if (abs(det) < 0.00001) { /* zostaw N */ }
            else {
                T = normalize( duv2.y * dp1 - duv1.y * dp2);
                B = normalize(-duv2.x * dp1 + duv1.x * dp2);
            }
        }
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * nmSample);
    }

    vec3 V = normalize(vViewDir);

    // F0: 0.04 for non-metals, albedo for metals
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Sun
    {
        vec3  L        = normalize(lightDirGlobal);
        vec3  sunColor = vec3(1.0, 0.95, 0.85) * 6.0;   // HDR sun radiance
        Lo += cookTorrance(N, V, L, F0, roughness, metallic, albedo) * sunColor;
    }

    // Bullet / point light
    if (bulletActive > 0.5) {
        vec3  lightVec = bulletPos - vPos.xyz;
        float dist     = length(lightVec);
        if (dist > 0.01) {
            vec3  Lb          = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.3 * dist + 0.15 * dist * dist);
            Lo += cookTorrance(N, V, Lb, F0, roughness, metallic, albedo)
                  * bulletColor * 8.0 * attenuation;
        }
    }

    // Ambient
    vec3 ambient = vec3(0.03) * albedo * ao;

    // Emissive
    vec3 emissive = (hasEmissive > 0.5)
        ? texture(texEmissive, vTexCoord).rgb
        : vec3(0.0);

    // Combine 
    vec3 total = ambient + Lo + emissive;

    if (any(isnan(total)) || any(isinf(total)))
        total = albedo * 0.03;

    total = total / (total + vec3(1.0));
    total = pow(total, vec3(1.0 / 2.2));

    pixelColor = vec4(total, vColor.a);
}
