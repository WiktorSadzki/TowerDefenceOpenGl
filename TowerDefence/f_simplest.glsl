#version 330 compatibility

in vec4 vColor;
in vec3 vNormal;
in vec4 vPos;
in vec3 vViewDir;
in vec2 vTexCoord;
in vec3 vViewDirTangent;

out vec4 pixelColor;

uniform vec3  lightDirGlobal;
uniform vec3  bulletPos;
uniform vec3  bulletColor;
uniform float bulletActive;

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

// Marches a ray through the height map to fake surface depth.
// v is the view direction in tangent space, t is the starting UV,
// h is max displacement and s is how many steps to take.
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

// How sharply microfacets are aligned to the half-vector. Rougher = wider highlight.
float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}

// Accounts for microfacets blocking each other's light and view.
float G_Smith(float NdotV, float NdotL, float roughness) {
    float r  = roughness + 1.0;
    float k  = (r * r) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

// At grazing angles even non-metals get reflective, this models that.
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Combines diffuse and specular for one light. The diffuse part is just Lambert
// (albedo * NdotL) made energy-conserving, and the specular is the D*G*F microfacet lobe.
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
    vec3 kD      = (1.0 - F) * (1.0 - metallic); // metals skip diffuse entirely
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

    if (useFixedTBN > 0.5) {
        T = normalize(fixedTangent);
        B = normalize(fixedBitangent);
    } else {
        vec3 dp1  = dFdx(vPos.xyz);  vec3 dp2  = dFdy(vPos.xyz);
        vec2 duv1 = dFdx(vTexCoord); vec2 duv2 = dFdy(vTexCoord);
        float det = duv1.x * duv2.y - duv1.y * duv2.x;
        if (abs(det) < 0.00001) {
            // UVs are degenerate here, just pick something reasonable
            T = normalize(cross(N_geo, vec3(0.0, 0.0, 1.0)));
            B = cross(N_geo, T);
        } else {
            T = normalize( duv2.y * dp1 - duv1.y * dp2);
            B = -normalize(-duv2.x * dp1 + duv1.x * dp2);
        }
    }

    // Parallax needs the view direction in tangent space, not world space
    vec3 V3 = normalize(vViewDir);
    
    // Build a unified Tangent Space matrix using the dynamic T and B derived above
    mat3 tbn_matrix = transpose(mat3(T, B, N_geo));
    vec3 viewDirForParallax = tbn_matrix * normalize(vViewDir);

    vec2 uv = vTexCoord;
    if (hasHeight > 0.5) {
        // Lowered height scale to 0.04 to prevent massive texture tearing
        uv = parallax(viewDirForParallax, vTexCoord, 0.05, 32.0);
    }

    vec3 albedo = (hasBaseColor > 0.5)
        ? pow(texture(texBaseColor, uv).rgb, vec3(2.2))
        : pow(vColor.rgb, vec3(2.2));

    float metallic  = (hasMetallic  > 0.5) ? texture(texMetallic,  uv).r : 0.0;
    float roughness = (hasRoughness > 0.5) ? texture(texRoughness, uv).r : 0.1;
    roughness = clamp(roughness, 0.04, 1.0); // zero roughness causes a division singularity

    float ao = (hasAO > 0.5) ? texture(texAO, uv).r : 1.0;

    vec3 N = N_geo;
    if (hasNormal > 0.5) {
        vec3 nmSample = texture(texNormal, uv).rgb * 2.0 - 1.0;
        mat3 TBN = mat3(T, B, N_geo);
        N = normalize(TBN * nmSample);
    }

    vec3 V  = V3;
    vec3 F0 = mix(vec3(0.04), albedo, metallic); // 0.04 is a typical dielectric reflectance
    vec3 Lo = vec3(0.0);

    {
        vec3 L        = normalize(lightDirGlobal);
        vec3 sunColor = vec3(1.0, 0.95, 0.85) * 6.0;
        Lo += cookTorrance(N, V, L, F0, roughness, metallic, albedo) * sunColor;
    }

    if (bulletActive > 0.5) {
        vec3  lightVec    = bulletPos - vPos.xyz;
        float dist        = length(lightVec);
        if (dist > 0.01) {
            vec3  Lb          = lightVec / dist;
            float attenuation = 1.0 / (1.0 + 0.3 * dist + 0.15 * dist * dist);
            Lo += cookTorrance(N, V, Lb, F0, roughness, metallic, albedo)
                  * bulletColor * 8.0 * attenuation;
        }
    }

    vec3 ambient  = vec3(0.03) * albedo * ao;
    vec3 emissive = (hasEmissive > 0.5) ? texture(texEmissive, uv).rgb : vec3(0.0);

    vec3 total = ambient + Lo + emissive;

    if (any(isnan(total)) || any(isinf(total)))
        total = albedo * 0.03;

    total = total / (total + vec3(1.0)); // Reinhard tone mapping
    total = pow(total, vec3(1.0 / 2.2)); // gamma correction

    pixelColor = vec4(total, vColor.a);
}