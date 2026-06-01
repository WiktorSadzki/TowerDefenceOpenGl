// Physically Based Rendering (PBR) model.
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

uniform vec3 ghostColor; // color of the ghost
uniform float renderMode; // 0=Default, 1=Shadow, 2=Ghost


const float PI = 3.14159265359;

// Cook-Torrance microfacet distribution (GGX)

// Normal distribution function (NDF) for GGX
float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

// Geometric shadowing function (Smith's method)
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
    vec3  H      = normalize(L + V); // Half-vector
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 0.001);
    float NdotH  = max(dot(N, H), 0.0);
    float HdotV  = max(dot(H, V), 0.0);

    // BRDF components
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

    vec3 dp1  = dFdx(eyePos); // Rate of change of position in x direction
    vec3 dp2  = dFdy(eyePos); // Rate of change of position in y direction
    vec2 duv1 = dFdx(uv); // Rate of change in texture coordinates in x direction
    vec2 duv2 = dFdy(uv); // Rate of change in texture coordinates in y direction

    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 0.00001) return N;

    vec3 T = normalize( duv2.y * dp1 - duv1.y * dp2);
    vec3 B = normalize(-duv2.x * dp1 + duv1.x * dp2);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nmSample);
}

void main() {
    // Shadow
    if (renderMode > 0.5 && renderMode < 1.5) {
        pixelColor = vec4(0.0, 0.0, 0.0, 0.5); // Black + 50% Alpha
        return;
    }

    // Ghost
    if (renderMode > 1.5) {
        pixelColor = vec4(ghostColor, 0.5); 
        return;
    }

    // Base color
    vec3 albedo = (hasBaseColor > 0.5)
        ? pow(texture(texBaseColor, vTexCoord).rgb, vec3(2.2))
        : pow(vColor.rgb, vec3(2.2));

    float metallic  = (hasMetallic  > 0.5) ? texture(texMetallic,  vTexCoord).r : 0.0;
    float roughness = (hasRoughness > 0.5) ? texture(texRoughness, vTexCoord).r : 0.1;
    roughness = clamp(roughness, 0.04, 1.0);   // avoid zero-roughness singularity

    float ao = (hasAO > 0.5) ? texture(texAO, vTexCoord).r : 1.0; // Ambient occlusion

    // Normal
    vec3 N = normalize(vNormal);
    if (hasNormal > 0.5) {
        vec3 nmSample = texture(texNormal, vTexCoord).rgb * 2.0 - 1.0;
        vec3 T, B; // Tangent and bitangent

        // Optionally use fixed TBN for better stability on low-poly models
        if (useFixedTBN > 0.5) {
            T = normalize(fixedTangent);
            B = normalize(fixedBitangent);
        } else {
            // Compute TBN from derivatives for proper normal mapping on low-poly models
            vec3 dp1 = dFdx(vPos.xyz); vec3 dp2 = dFdy(vPos.xyz);
            vec2 duv1 = dFdx(vTexCoord); vec2 duv2 = dFdy(vTexCoord);
            float det = duv1.x * duv2.y - duv1.y * duv2.x;
            // If the determinant is near zero, the UV mapping is degenerate and we can't compute a valid TBN matrix.
            if (abs(det) < 0.00001) { /* zostaw N */ }
            else {
                T = normalize( duv2.y * dp1 - duv1.y * dp2);
                B = normalize(-duv2.x * dp1 + duv1.x * dp2);
            }
        }
        mat3 TBN = mat3(T, B, N); // Tangent-Bitangent-Normal matrix
        N = normalize(TBN * nmSample); // Apply normal map perturbation
    }

    vec3 V = normalize(vViewDir);

    // F0: 0.04 for non-metals, albedo for metals
    vec3 F0 = mix(vec3(0.04), albedo, metallic); // Interpolate between dielectric and metallic reflectance

    vec3 Lo = vec3(0.0); // Outgoing radiance (light contribution)

    // Sun
    {
        vec3  L = normalize(lightDirGlobal); // Directional light from the sun
        vec3  sunColor = vec3(1.0, 0.95, 0.85) * 6.0;   // Sunlight color and intensity
        Lo += cookTorrance(N, V, L, F0, roughness, metallic, albedo) * sunColor; // Add sun contribution
    }

    // Bullet / point light
    if (bulletActive > 0.5) {
        // Calculate vector from surface point to bullet light position
        vec3  lightVec = bulletPos - vPos.xyz; /
        float dist     = length(lightVec);
        // Avoid singularity and excessive brightness at very close distances
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

    // Handle potential NaN or Inf values from extreme lighting conditions
    if (any(isnan(total)) || any(isinf(total)))
        total = albedo * 0.03;

    // Tone mapping (simple Reinhard) and gamma correction
    total = total / (total + vec3(1.0));
    total = pow(total, vec3(1.0 / 2.2));

    // Output final color with original alpha
    pixelColor = vec4(total, vColor.a);
}
