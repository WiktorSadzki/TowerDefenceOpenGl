#version 330 compatibility

in vec4 vColor;
in vec3 vNormal;
in vec4 vPos;
in vec3 vViewDir;
in vec2 vTexCoord;
in vec3 vViewDirTangent;
in vec3 vTangentEye;
in vec3 vWorldPos;

out vec4 pixelColor;

uniform mat4  V;
uniform vec3  lightDirGlobal;
uniform vec3  bulletPos;
uniform vec3  bulletColor;
uniform float bulletActive;
uniform float useWorldUV;

uniform vec3  trailPositions[8];
uniform float trailIntensities[8];
uniform int   activeTrailLights;

uniform vec3  rocketPositions[8];
uniform float rocketIntensities[8];
uniform int   activeRocketLights;

uniform sampler2D texBaseColor;
uniform sampler2D texNormal;
uniform sampler2D texMetallic;
uniform sampler2D texRoughness;
uniform sampler2D texAO;
uniform sampler2D texEmissive;
uniform sampler2D texHeight;
uniform float texBlendScale;

uniform float hasBaseColor;
uniform float hasNormal;
uniform float hasMetallic;
uniform float hasRoughness;
uniform float hasAO;
uniform float hasEmissive;
uniform float hasHeight;

uniform float useFixedTBN;
uniform vec3  fixedTangent;
uniform vec3  fixedBitangent;

uniform vec3  ghostColor;
uniform float renderMode;

const float PI = 3.14159265359;

vec2 parallax(vec3 v, vec2 t, float h, float s) {
    if (v.z <= 0.0) return t;

    vec2 parallax_offset = (v.xy / v.z) * h;
    vec2 tex_step = parallax_offset / s;
    float depth_step = 1.0 / s;

    vec2 current_tc = t;
    float current_depth = 1.0;
    float height_map_val = texture(texHeight, current_tc).r;

    int max_steps = int(s);
    while (current_depth > height_map_val && max_steps-- > 0) {
        current_tc -= tex_step;
        current_depth -= depth_step;
        height_map_val = texture(texHeight, current_tc).r;
    }
    return current_tc;
}

float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    float r  = roughness + 1.0;
    float k  = (r * r) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 cookTorrance(vec3 N, vec3 V, vec3 L, vec3 F0,
                  float roughness, float metallic, vec3 albedo) {
    vec3  H     = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(HdotV, F0);

    vec3 spec    = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD      = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + spec) * NdotL;
}

void main() {
    if (renderMode > 0.5 && renderMode < 1.5) {
        pixelColor = vec4(0.0, 0.0, 0.0, 0.5);
        return;
    }

    if (renderMode > 1.5) {
        pixelColor = vec4(ghostColor, 0.5);
        return;
    }

    vec3 N_geo = normalize(vNormal);
    vec3 T, B;
    vec2 uv;

    if (useWorldUV > 0.5) {
        vec3 T_world = vec3(1.0, 0.0, 0.0);
        vec3 B_world = vec3(0.0, 0.0, -1.0);
        
        vec3 T_eye = normalize(mat3(V) * T_world);
        T = normalize(T_eye - dot(T_eye, N_geo) * N_geo);
        B = cross(N_geo, T);
        
        uv = vec2(vWorldPos.x, -vWorldPos.z) * 0.12; 
    } else {
        T = normalize(vTangentEye);
        T = normalize(T - dot(T, N_geo) * N_geo);
        B = cross(N_geo, T);
        
        uv = vTexCoord;
    }

    vec3 V3 = normalize(vViewDir);

    mat3 tbn_matrix = transpose(mat3(T, B, N_geo));
    vec3 viewDirForParallax = tbn_matrix * V3;

    if (hasHeight > 0.5) {
        uv = parallax(viewDirForParallax, uv, 0.03, 64.0); 
    }

    vec3 albedo = (hasBaseColor > 0.5)
        ? pow(texture(texBaseColor, uv).rgb, vec3(2.2))
        : pow(vColor.rgb, vec3(2.2));

    float metallic  = (hasMetallic  > 0.5) ? texture(texMetallic,  uv).r : 0.0;
    float roughness = (hasRoughness > 0.5) ? texture(texRoughness, uv).r : 0.1;
    roughness = clamp(roughness, 0.04, 1.0);

    float ao = (hasAO > 0.5) ? texture(texAO, uv).r : 1.0;

    vec3 N = N_geo;
    if (hasNormal > 0.5) {
        vec3 nmSample = texture(texNormal, uv).rgb * 2.0 - 1.0;
        mat3 TBN = mat3(T, B, N_geo);
        N = normalize(TBN * nmSample);
    }

    vec3 V_local  = V3;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    {
        vec3 L        = normalize(lightDirGlobal);
        vec3 sunColor = vec3(1.0, 0.95, 0.85) * 6.0;
        Lo += cookTorrance(N, V_local, L, F0, roughness, metallic, albedo) * sunColor;
    }

    vec3 viewPosF = vPos.xyz;

    if (bulletActive > 0.5) {
        vec3  lightVec    = bulletPos - viewPosF;
        float dist        = length(lightVec);
        if (dist > 0.01) {
            vec3  Lb          = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.3 * dist + 0.15 * dist * dist);
            Lo += cookTorrance(N, V_local, Lb, F0, roughness, metallic, albedo)
                  * bulletColor * 8.0 * attenuation;
        }
    }

    vec3 fireColor = vec3(1.0, 0.38, 0.05);
    for (int i = 0; i < activeTrailLights; i++) {
        vec3  lightVec = trailPositions[i] - viewPosF;
        float dist     = length(lightVec);
        if (dist > 0.01) {
            vec3  Lt          = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.6 * dist + 1.5 * dist * dist);
            Lo += cookTorrance(N, V_local, Lt, F0, roughness, metallic, albedo)
                  * fireColor * trailIntensities[i] * attenuation;
        }
    }

    vec3 rocketExhaustColor = vec3(1.0, 0.45, 0.1);
    for (int i = 0; i < activeRocketLights; i++) {
        vec3  lightVec = rocketPositions[i] - viewPosF;
        float dist     = length(lightVec);
        if (dist > 0.01) {
            vec3  Lr          = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.4 * dist + 2.5 * dist * dist);
            Lo += cookTorrance(N, V_local, Lr, F0, roughness, metallic, albedo)
                  * rocketExhaustColor * rocketIntensities[i] * attenuation;
        }
    }

    vec3 ambient  = vec3(0.03) * albedo * ao;
    vec3 emissive = (hasEmissive > 0.5) ? texture(texEmissive, uv).rgb : vec3(0.0);

    vec3 total = ambient + Lo + emissive;

    if (any(isnan(total)) || any(isinf(total)))
        total = albedo * 0.03;

    total = total / (total + vec3(1.0));
    total = pow(total, vec3(1.0 / 2.2));

    float edgeMask = smoothstep(0.0, 0.15, vTexCoord.x) * smoothstep(1.0, 0.85, vTexCoord.x);
    
    pixelColor = vec4(total, vColor.a * edgeMask);
}