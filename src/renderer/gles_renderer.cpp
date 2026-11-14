#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "renderer/renderer.h"
#include "assets/ddt_texture.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/asset_manager.h>

#include <cstdio>
#include <unordered_map>
#include <vector>

#include "engine/logger.h"

#define LOG_TAG "AoM-GLES"
#define LOGI(...) AOM_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGW(...) AOM_LOGW(LOG_TAG, __VA_ARGS__)
#define LOGE(...) AOM_LOGE(LOG_TAG, __VA_ARGS__)

namespace aom {

namespace {

// 2D Quad UI Shaders
const char* kVertexSrc = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
uniform vec2 uScale;
uniform vec2 uOffset;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;
out vec2 vTex;
void main() {
    vTex = aTex * uUVScale + uUVOffset;
    gl_Position = vec4(aPos * uScale + uOffset, 0.0, 1.0);
}
)";

const char* kFragmentSrc = R"(#version 300 es
precision mediump float;
in vec2 vTex;
uniform sampler2D uTex;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vTex) * uColor;
}
)";

// 3D Line / Particle Streak Shaders
const char* kLineVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kLineFragmentSrc = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
}
)";

// Billboard (Particle) Shaders - camera-facing quads with texture support
const char* kBillboardVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;

uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform vec3 uBillboardPos;
uniform vec2 uBillboardSize;

out vec2 vTex;

void main() {
    // Billboard vertices are in local space [-0.5, 0.5] x [-0.5, 0.5]
    // Scale by billboard size
    vec3 scaledPos = aPos * vec3(uBillboardSize.x, uBillboardSize.y, 1.0);
    
    // Rotate to face camera (billboard)
    vec3 to_camera = normalize(uCameraPos - uBillboardPos);
    vec3 right = normalize(cross(to_camera, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, to_camera);
    
    mat3 billboard_rot = mat3(right, up, to_camera);
    vec3 world_pos = uBillboardPos + billboard_rot * scaledPos;
    
    mat4 model = mat4(
        vec4(right, 0.0),
        vec4(up, 0.0),
        vec4(to_camera, 0.0),
        vec4(uBillboardPos, 1.0)
    );
    
    vec4 worldPos = model * vec4(scaledPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vTex = aTex;
}
)";

const char* kBillboardFragmentSrc = R"(#version 300 es
precision mediump float;
in vec2 vTex;
uniform sampler2D uTex;
uniform int uHasTex;
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    vec4 texColor = (uHasTex != 0) ? texture(uTex, vTex) : vec4(1.0);
    fragColor = texColor * uColor;
}
)";

// 3D Mesh Shaders with Lighting and Vertex Colors
const char* kMeshVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aBoneIndices;
layout(location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;
uniform mat4 uBoneMatrices[64];

out vec3 vNormal;
out vec2 vTex;
out vec4 vColor;

void main() {
    // Skinning: compute world position using bone matrices
    vec4 skinnedPos = vec4(0.0);
    skinnedPos += aBoneWeights.x * uBoneMatrices[int(aBoneIndices.x)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.y * uBoneMatrices[int(aBoneIndices.y)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.z * uBoneMatrices[int(aBoneIndices.z)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.w * uBoneMatrices[int(aBoneIndices.w)] * vec4(aPos, 1.0);
    
    vec3 skinnedNormal = vec3(0.0);
    vec4 normalVec = vec4(aNormal, 0.0);
    skinnedNormal += aBoneWeights.x * mat3(uBoneMatrices[int(aBoneIndices.x)]) * aNormal;
    skinnedNormal += aBoneWeights.y * mat3(uBoneMatrices[int(aBoneIndices.y)]) * aNormal;
    skinnedNormal += aBoneWeights.z * mat3(uBoneMatrices[int(aBoneIndices.z)]) * aNormal;
    skinnedNormal += aBoneWeights.w * mat3(uBoneMatrices[int(aBoneIndices.w)]) * aNormal;
    skinnedNormal = normalize(skinnedNormal);

    vec4 worldPos = uModel * skinnedPos;
    vNormal = normalize(uNormalMat * skinnedNormal);
    vTex = aTex;
    vColor = aColor;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kMeshFragmentSrc = R"(#version 300 es
precision mediump float;

in vec3 vNormal;
in vec2 vTex;
in vec4 vColor;

uniform sampler2D uTex;
uniform int uHasTex;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 lighting = uAmbient + uLightColor * diff;

    vec4 baseColor = (uHasTex != 0) ? texture(uTex, vTex) : vColor;
    if (baseColor.a < 0.25) discard;
    fragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
)";

// 3D Terrain Shaders with Multi-Texturing, Slope Splatting and Sun Lighting
const char* kTerrainVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTex;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vTex = aTex;
    vColor = aColor;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kTerrainFragmentSrc = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTex;
in vec4 vColor;

uniform sampler2D uBaseTex;
uniform sampler2D uBlendTex;
uniform sampler2D uBankTex;
uniform sampler2D uOuterBankTex;
uniform int uHasBaseTex;
uniform int uHasBlendTex;
uniform int uHasBankTex;
uniform int uHasOuterBankTex;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 lighting = uAmbient + uLightColor * diff;

    // Slope splatting: flat ground has N.y ~ 1.0; steep cliff has N.y < 0.70.
    float slopeFactor = clamp((0.85 - N.y) / 0.25, 0.0, 1.0);
    float blendFactor = clamp(slopeFactor + (1.0 - vColor.a), 0.0, 1.0);

    vec4 baseColor = (uHasBaseTex != 0) ? texture(uBaseTex, vTex) : vec4(0.35, 0.58, 0.25, 1.0);
    vec4 cliffColor = (uHasBlendTex != 0) ? texture(uBlendTex, vTex * 2.5) : vec4(0.48, 0.44, 0.40, 1.0);

    // Water bank textures: near water level (y <= -0.35) use bank, slightly above use outer_bank.
    float water_level = -0.40f;
    float bankZone = smoothstep(water_level, water_level + 0.6f, vWorldPos.y);
    float outerZone = smoothstep(water_level + 0.6f, water_level + 1.4f, vWorldPos.y);

    vec4 bankColor = vec4(0.0);
    if (uHasBankTex != 0) {
        bankColor = texture(uBankTex, vTex * 3.0);
    }
    vec4 outerBankColor = vec4(0.0);
    if (uHasOuterBankTex != 0) {
        outerBankColor = texture(uOuterBankTex, vTex * 3.0);
    }

    // Camadas: base -> outer_bank (transicao) -> bank (agua) -> cliff (encosta)
    vec4 mixedTex = baseColor;
    if (uHasOuterBankTex != 0) {
        mixedTex = mix(mixedTex, outerBankColor, outerZone * (1.0 - blendFactor));
    }
    if (uHasBankTex != 0) {
        mixedTex = mix(mixedTex, bankColor, bankZone * (1.0 - blendFactor));
    }
    mixedTex = mix(mixedTex, cliffColor, blendFactor);

    vec3 finalRgb = mixedTex.rgb * vColor.rgb * lighting;
    fragColor = vec4(finalRgb, 1.0);
}
)";

// Water Plane Shaders with Gerstner Waves, Foam, Caustics, Foam at Shorelines
const char* kWaterVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform highp float uTime;
uniform float uWaterLevel;    // Nível da água (Y world)
uniform float uTerrainHeight; // Altura do terreno sob a água (para depth)

out vec3 vWorldPos;
out vec2 vTex;
out vec4 vColor;
out float vWaterDepth;

void main() {
    // Ondulação visível estilo The Titans (ondas amplas e perceptíveis)
    float wave = (sin(aPos.x * 0.35 + uTime * 1.8) * cos(aPos.z * 0.35 + uTime * 1.4) +
                  sin(aPos.x * 0.7 - uTime * 1.1)) * 0.07;
    vec3 animatedPos = aPos + vec3(0.0, wave, 0.0);
    vec4 worldPos = uModel * vec4(animatedPos, 1.0);
    vWorldPos = worldPos.xyz;
    vTex = aTex;
    vColor = aColor;

    // Gradiente de profundidade costeiro da ilha (costa oeste x < -0.4 rasa -> mar aberto leste x > -0.4 fundo)
    float coastDist = aPos.x - (-0.4);
    vWaterDepth = clamp((coastDist + 1.0) / 7.0, 0.0, 1.0);

    gl_Position = uViewProj * worldPos;
}
)";

const char* kWaterFragmentSrc = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec2 vTex;
in vec4 vColor;
in float vWaterDepth;

uniform sampler2D uNormalMap;
uniform int uHasNormalMap;
uniform sampler2D uWaterTex;
uniform int uHasWaterTex;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform highp float uTime;
uniform float uWaterLevel;

out vec4 fragColor;

void main() {
    // 1. Amostragem de normal map com coordenadas UV duplas em movimento (ondas)
    vec2 uvNorm1 = vWorldPos.xz * 0.08 + vec2(uTime * 0.03, uTime * 0.02);
    vec2 uvNorm2 = vWorldPos.xz * 0.12 - vec2(uTime * 0.02, uTime * 0.035);

    vec3 normal = vec3(0.0, 1.0, 0.0);
    if (uHasNormalMap != 0) {
        vec3 n1 = texture(uNormalMap, uvNorm1).rgb * 2.0 - 1.0;
        vec3 n2 = texture(uNormalMap, uvNorm2).rgb * 2.0 - 1.0;
        normal = normalize(vec3(n1.x + n2.x, 2.0, n1.y + n2.y));
    } else {
        float nx = sin(vWorldPos.x * 2.0 + uTime * 2.0) * 0.1;
        float nz = cos(vWorldPos.z * 2.0 + uTime * 1.5) * 0.1;
        normal = normalize(vec3(nx, 1.0, nz));
    }

    // 2. Iluminação do sol estilizada de AoM (sem brilho especular artificial moderno)
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    float spec = pow(max(dot(normal, H), 0.0), 16.0) * 0.35;
    vec3 specColor = uLightColor * spec;
    float fresnel = 0.15 + 0.35 * pow(clamp(1.0 - max(dot(normal, V), 0.0), 0.0, 1.0), 2.5);

    // 3. Cores clássicas do mar Mediterrâneo de The Titans: turquesa costeiro -> azul profundo
    vec3 shallowColor = vec3(0.12, 0.58, 0.72);
    vec3 deepColor    = vec3(0.04, 0.28, 0.58);
    vec3 waterColor = mix(shallowColor, deepColor, vWaterDepth);

    // Textura de ondulação e caustics original do AoM (water0000.ddt)
    if (uHasWaterTex != 0) {
        vec2 uvWater1 = vWorldPos.xz * 0.14 + vec2(uTime * 0.045, uTime * 0.025);
        vec2 uvWater2 = vWorldPos.xz * 0.10 - vec2(uTime * 0.030, uTime * 0.040);
        vec4 w1 = texture(uWaterTex, uvWater1);
        vec4 w2 = texture(uWaterTex, uvWater2);
        vec3 caustics = (w1.rgb + w2.rgb) * 0.5;
        waterColor = mix(waterColor, caustics * vec3(0.5, 0.9, 1.1), 0.35);
    }

    vec3 skyReflection = vec3(0.45, 0.65, 0.85);
    vec3 finalColor = mix(waterColor, skyReflection, fresnel * 0.40) + specColor;

    // 4. Espuma autêntica de rebentação e ondas quebrando na praia de Atlântida
    float shoreOffset = -0.4 + vWorldPos.z * 0.12 + sin(vWorldPos.z * 0.22) * 0.9;
    float distToCoast = vWorldPos.x - shoreOffset; // < 0 na praia, > 0 no mar

    // Trem de ondas se propagando em direção à praia
    float wavePhase = distToCoast * 3.2 - uTime * 2.4 + sin(vWorldPos.z * 1.6) * 0.7;
    float waveSurf = sin(wavePhase);

    // Faixa costeira onde a onda quebra (entre praia e água rasa)
    float surfZone = smoothstep(2.2, 0.1, distToCoast) * smoothstep(-0.8, -0.1, distToCoast);
    float crestFoam = smoothstep(0.35, 0.95, waveSurf) * surfZone;

    // Faixa de espuma na orla (marulhar da maré subindo e descendo na areia)
    float washWave = sin(uTime * 1.8 + vWorldPos.z * 0.8) * 0.35;
    float washFoam = smoothstep(0.45, 0.0, distToCoast - washWave) * smoothstep(-0.6, -0.1, distToCoast - washWave);

    float totalFoam = clamp(crestFoam * 1.3 + washFoam * 0.9, 0.0, 1.0);
    vec3 foamColor = vec3(0.96, 0.98, 1.0);
    finalColor = mix(finalColor, foamColor, totalFoam);

    float alpha = clamp(0.70 + vWaterDepth * 0.25 + totalFoam * 0.30, 0.0, 0.98);
    fragColor = vec4(finalColor, alpha);
}
)";

// ── Skybox Atmospheric Scattering Shaders ───────────────────────────────────
// Sky procedural com Rayleigh/Mie scattering simplificado + sol + nuvens
const char* kSkyboxVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;

uniform mat4 uViewProj;   // view sem translação * projection
out vec3 vDir;

void main() {
    vDir = aPos;  // direção do vértice (esfera invertida)
    vec4 pos = uViewProj * vec4(aPos, 1.0);
    // Forçar z = w para que o skybox fique sempre no fundo (depth = 1.0)
    gl_Position = pos.xyww;
}
)";

const char* kSkyboxFragmentSrc = R"(#version 300 es
precision mediump float;

in vec3 vDir;
uniform vec3 uSunDir;       // direção do sol (normalizada)
uniform vec3 uSunColor;     // cor do sol
uniform vec3 uZenithColor;  // cor do zênite (fallback procedural)
uniform vec3 uHorizonColor; // cor do horizonte (fallback procedural)
uniform highp float uTime;
uniform samplerCube uCubemap;  // cubemap do skybox (0 = usa fallback procedural)
uniform int uHasCubemap;       // 1 = usa cubemap, 0 = procedural

out vec4 fragColor;

// Hash simples para nuvens procedurais
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// Noise suave para nuvens
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// Sky procedural (fallback quando não há cubemap)
vec3 sky_procedural(vec3 dir, vec3 sunDir, vec3 sunColor,
                    vec3 zenithColor, vec3 horizonColor, float time) {
    float h = clamp(dir.y, 0.0, 1.0);
    float t = pow(h, 0.4);
    vec3 sky = mix(horizonColor, zenithColor, t);

    float sunAmount = max(dot(dir, normalize(sunDir)), 0.0);
    vec3 sunGlow = sunColor * pow(sunAmount, 8.0) * 0.6;
    vec3 sunDisk  = sunColor * pow(sunAmount, 3500.0) * 3.0;

    float clouds = 0.0;
    if (dir.y > 0.02) {
        vec2 uv = dir.xz / max(dir.y, 0.02) * 0.35;
        uv += vec2(time * 0.004, time * 0.002);
        float c = fbm(uv * 2.0);
        float coverage = smoothstep(0.45, 0.75, c);
        float fade = smoothstep(0.02, 0.15, dir.y);
        clouds = coverage * fade;
    }

    vec3 cloudColor = mix(vec3(0.85, 0.87, 0.92), sunColor, pow(sunAmount, 3.0) * 0.5);
    cloudColor = mix(cloudColor, vec3(1.0), 0.15);

    vec3 finalRgb = sky + sunGlow + sunDisk;
    finalRgb = mix(finalRgb, cloudColor, clouds * 0.85);
    finalRgb = finalRgb / (finalRgb + vec3(0.6)) * 1.55;
    return finalRgb;
}

void main() {
    vec3 dir = normalize(vDir);
    vec3 skyColor;

    if (uHasCubemap == 1) {
        // Sample cubemap - a direção vDir já está no espaço de visão
        // O cubemap deve estar orientado corretamente
        skyColor = texture(uCubemap, dir).rgb;

        // Adicionar sol procedural sobre o cubemap
        float sunAmount = max(dot(dir, normalize(uSunDir)), 0.0);
        vec3 sunGlow = uSunColor * pow(sunAmount, 8.0) * 0.4;
        vec3 sunDisk  = uSunColor * pow(sunAmount, 3500.0) * 2.0;
        skyColor += sunGlow + sunDisk;

        // Nuvens procedurais leves sobre o cubemap (opcional)
        if (dir.y > 0.02) {
            vec2 uv = dir.xz / max(dir.y, 0.02) * 0.35;
            uv += vec2(uTime * 0.004, uTime * 0.002);
            float c = fbm(uv * 2.0);
            float coverage = smoothstep(0.45, 0.75, c);
            float fade = smoothstep(0.02, 0.15, dir.y);
            float clouds = coverage * fade;
            vec3 cloudColor = mix(vec3(0.85, 0.87, 0.92), uSunColor, pow(sunAmount, 3.0) * 0.3);
            skyColor = mix(skyColor, cloudColor, clouds * 0.5);
        }
    } else {
        // Fallback procedural completo
        skyColor = sky_procedural(dir, uSunDir, uSunColor,
                                  uZenithColor, uHorizonColor, uTime);
    }

    fragColor = vec4(skyColor, 1.0);
}
)";

// ── Full-screen Post-Processing (Tone Mapping + Vignette + Color Grade) ─────
const char* kPostVertexSrc = R"(#version 300 es
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* kPostFragmentSrc = R"(#version 300 es
precision mediump float;

in vec2 vUV;
uniform sampler2D uScene;
uniform float uExposure;      // exposição HDR
uniform float uVignette;      // intensidade da vinheta (0 = off)
uniform float uSaturation;    // saturação de cor (1.0 = normal)
uniform float uWarmth;        // mudança de temperatura de cor (-1 frio, +1 quente)

out vec4 fragColor;

void main() {
    vec3 color = texture(uScene, vUV).rgb;

    // Exposure
    color *= uExposure;

    // ACES aproximação de tone mapping (barato, sem curvas LUT)
    vec3 x = color * (2.51 * color + 0.03);
    vec3 y = color * (2.43 * color + 0.59) + 0.14;
    color = clamp(x / y, 0.0, 1.0);

    // Saturação
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, uSaturation);

    // Temperatura de cor (warm = +R/-B, cool = -R/+B)
    color.r = clamp(color.r + uWarmth * 0.08, 0.0, 1.0);
    color.b = clamp(color.b - uWarmth * 0.08, 0.0, 1.0);

    // Vinheta
    if (uVignette > 0.0) {
        vec2 d = vUV - 0.5;
        float vig = 1.0 - dot(d, d) * uVignette;
        color *= clamp(vig, 0.0, 1.0);
    }

    fragColor = vec4(color, 1.0);
}
)";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        LOGE("Shader compile failed: %s", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // anonymous namespace

class GLESRenderer : public IRenderer {
public:
    ~GLESRenderer() override { shutdown(); }

    bool initialize(void* native_window, int width, int height) override {
        shutdown();

        if (!native_window) {
            LOGE("initialize: native window is null");
            return false;
        }

        egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (egl_display_ == EGL_NO_DISPLAY) {
            LOGE("eglGetDisplay failed");
            return false;
        }

        EGLint major = 0, minor = 0;
        if (!eglInitialize(egl_display_, &major, &minor)) {
            LOGE("eglInitialize failed: 0x%x", eglGetError());
            egl_display_ = EGL_NO_DISPLAY;
            return false;
        }
        LOGI("EGL %d.%d initialized", major, minor);

        const EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE,
        };

        EGLint num_configs = 0;
        if (!eglChooseConfig(egl_display_, config_attribs, &egl_config_, 1,
                             &num_configs) || num_configs < 1) {
            LOGW("eglChooseConfig with ES3 bit failed, falling back to ES2 bit");
            const EGLint fallback_attribs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 16,
                EGL_NONE,
            };
            if (!eglChooseConfig(egl_display_, fallback_attribs, &egl_config_, 1,
                                 &num_configs) || num_configs < 1) {
                LOGE("eglChooseConfig failed: 0x%x", eglGetError());
                shutdown();
                return false;
            }
        }

        // Set native window buffer geometry matching EGL visual ID
        EGLint visual_id = 0;
        if (eglGetConfigAttrib(egl_display_, egl_config_, EGL_NATIVE_VISUAL_ID, &visual_id) && visual_id > 0) {
            ANativeWindow_setBuffersGeometry(
                static_cast<ANativeWindow*>(native_window), 0, 0, visual_id);
        }

        if (!eglBindAPI(EGL_OPENGL_ES_API)) {
            LOGE("eglBindAPI failed: 0x%x", eglGetError());
            shutdown();
            return false;
        }

        const EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE,
        };
        egl_context_ = eglCreateContext(egl_display_, egl_config_,
                                        EGL_NO_CONTEXT, context_attribs);
        if (egl_context_ == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed: 0x%x", eglGetError());
            shutdown();
            return false;
        }

        egl_surface_ = eglCreateWindowSurface(
            egl_display_, egl_config_,
            static_cast<EGLNativeWindowType>(native_window), nullptr);
        if (egl_surface_ == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
            shutdown();
            return false;
        }

        if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_,
                            egl_context_)) {
            LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
            shutdown();
            return false;
        }

        EGLint surf_w = 0, surf_h = 0;
        eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &surf_w);
        eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &surf_h);
        if (surf_w > 0 && surf_h > 0) {
            width_ = surf_w;
            height_ = surf_h;
        } else {
            width_ = (width > 0) ? width : 1280;
            height_ = (height > 0) ? height : 720;
        }
        glViewport(0, 0, width_, height_);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        if (!compile_program() || !compile_mesh_program() || !compile_skinned_mesh_program() ||
            !compile_terrain_program() || !compile_water_program() ||
            !compile_line_program() || !compile_billboard_program()) {
            shutdown();
            return false;
        }
        // Skybox e post-processing são opcionais (falha não bloqueia o render)
        if (!compile_skybox_program()) {
            LOGW("Skybox program failed to compile - procedural sky disabled");
        }
        if (!compile_post_program()) {
            LOGW("Post-processing program failed to compile - effects disabled");
        }
        create_quad();
        create_post_quad();

        // Default camera and light
        camera_.aspect = (height_ > 0) ? (static_cast<float>(width_) / height_) : (16.0f / 9.0f);
        camera_.position = glm::vec3(0.0f, 2.5f, 4.5f);
        camera_.target = glm::vec3(0.0f, 0.0f, 0.0f);
        camera_.up = glm::vec3(0.0f, 1.0f, 0.0f);

        light_.direction = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));
        light_.color = glm::vec3(1.0f, 0.95f, 0.85f);
        light_.ambient = glm::vec3(0.25f, 0.28f, 0.35f);

        LOGI("GLES renderer ready: %s", glGetString(GL_VERSION));
        return true;
    }

    void shutdown() override {
        for (auto& pair : meshes_) {
            glDeleteVertexArrays(1, &pair.second.vao);
            glDeleteBuffers(1, &pair.second.vbo);
            glDeleteBuffers(1, &pair.second.ebo);
        }
        meshes_.clear();

        if (quad_vao_) { glDeleteVertexArrays(1, &quad_vao_); quad_vao_ = 0; }
        if (quad_vbo_) { glDeleteBuffers(1, &quad_vbo_); quad_vbo_ = 0; }
        if (program_) { glDeleteProgram(program_); program_ = 0; }
        if (mesh_program_) { glDeleteProgram(mesh_program_); mesh_program_ = 0; }
        if (skinned_mesh_program_) { glDeleteProgram(skinned_mesh_program_); skinned_mesh_program_ = 0; }
        if (terrain_program_) { glDeleteProgram(terrain_program_); terrain_program_ = 0; }
        if (water_program_) { glDeleteProgram(water_program_); water_program_ = 0; }
        if (water_vao_) { glDeleteVertexArrays(1, &water_vao_); water_vao_ = 0; }
        if (water_vbo_) { glDeleteBuffers(1, &water_vbo_); water_vbo_ = 0; }
        if (water_ebo_) { glDeleteBuffers(1, &water_ebo_); water_ebo_ = 0; }
        water_index_count_ = 0;

        if (line_program_) { glDeleteProgram(line_program_); line_program_ = 0; }
        if (line_vao_) { glDeleteVertexArrays(1, &line_vao_); line_vao_ = 0; }
        if (line_vbo_) { glDeleteBuffers(1, &line_vbo_); line_vbo_ = 0; }

        // Billboard
        if (billboard_program_) { glDeleteProgram(billboard_program_); billboard_program_ = 0; }
        if (billboard_vao_) { glDeleteVertexArrays(1, &billboard_vao_); billboard_vao_ = 0; }
        if (billboard_vbo_) { glDeleteBuffers(1, &billboard_vbo_); billboard_vbo_ = 0; }

        // Skybox
        if (skybox_program_) { glDeleteProgram(skybox_program_); skybox_program_ = 0; }
        if (skybox_vao_) { glDeleteVertexArrays(1, &skybox_vao_); skybox_vao_ = 0; }
        if (skybox_vbo_) { glDeleteBuffers(1, &skybox_vbo_); skybox_vbo_ = 0; }
        if (skybox_ebo_) { glDeleteBuffers(1, &skybox_ebo_); skybox_ebo_ = 0; }
        skybox_index_count_ = 0;

        // Post-processing
        if (post_program_) { glDeleteProgram(post_program_); post_program_ = 0; }
        if (post_vao_) { glDeleteVertexArrays(1, &post_vao_); post_vao_ = 0; }
        if (post_vbo_) { glDeleteBuffers(1, &post_vbo_); post_vbo_ = 0; }
        destroy_post_fbo();

        if (skybox_program_) { glDeleteProgram(skybox_program_); skybox_program_ = 0; }

        if (font_atlas_tex_ != INVALID_TEXTURE) {
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&font_atlas_tex_));
            font_atlas_tex_ = INVALID_TEXTURE;
        }
        font_atlas_loaded_ = false;

        if (white_tex_ != INVALID_TEXTURE) {
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&white_tex_));
            white_tex_ = INVALID_TEXTURE;
        }

        if (egl_display_ == EGL_NO_DISPLAY) return;
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_display_, egl_context_);
            egl_context_ = EGL_NO_CONTEXT;
        }
        if (egl_surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(egl_display_, egl_surface_);
            egl_surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(egl_display_);
        egl_display_ = EGL_NO_DISPLAY;
    }

    void resize(int width, int height) override {
        if (width <= 0 || height <= 0) return;
        width_ = width;
        height_ = height;
        if (egl_display_ != EGL_NO_DISPLAY) {
            glViewport(0, 0, width_, height_);
        }
        camera_.aspect = (height_ > 0) ? (static_cast<float>(width_) / height_) : (16.0f / 9.0f);
        // Recriar FBO do post-processing com nova resolução
        if (post_fbo_ != 0) {
            destroy_post_fbo();
            create_post_fbo();
        }
    }

    void begin_frame() override {
        frame_time_ += 1.0f / 60.0f;  // incrementa tempo do skybox

        // Render para FBO se post-processing habilitado
        if (post_enabled_ && post_program_ != 0) {
            if (post_fbo_ == 0) create_post_fbo();
            if (post_fbo_ != 0) {
                glBindFramebuffer(GL_FRAMEBUFFER, post_fbo_);
            }
        }

        glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void end_frame() override {
        // Aplicar post-processing (render FBO → tela)
        if (post_enabled_ && post_program_ != 0 && post_fbo_ != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width_, height_);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            glUseProgram(post_program_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, post_scene_tex_);
            glUniform1i(post_uScene_, 0);
            glUniform1f(post_uExposure_, post_exposure_);
            glUniform1f(post_uVignette_, post_vignette_);
            glUniform1f(post_uSaturation_, post_saturation_);
            glUniform1f(post_uWarmth_, post_warmth_);

            glBindVertexArray(post_vao_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        if (egl_display_ != EGL_NO_DISPLAY && egl_surface_ != EGL_NO_SURFACE) {
            if (!eglSwapBuffers(egl_display_, egl_surface_)) {
                LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
            }
        }
    }

    TextureHandle create_texture(const DdtTexture& ddt) override {
        std::vector<uint8_t> rgba = ddt.decode_rgba();
        if (rgba.empty()) {
            LOGE("create_texture: unsupported DDT format %d", static_cast<int>(ddt.format));
            return INVALID_TEXTURE;
        }
        return create_texture(ddt.width, ddt.height, rgba.data());
    }

    TextureHandle create_texture(uint32_t width, uint32_t height, const uint8_t* rgba_data) override {
        if (!rgba_data || width == 0 || height == 0) return INVALID_TEXTURE;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);

        if (width > 1 && height > 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    void update_texture(TextureHandle handle, uint32_t width, uint32_t height,
                        const uint8_t* rgba_data) override {
        GLuint tex = static_cast<GLuint>(handle);
        if (!tex || !rgba_data || width == 0 || height == 0) return;
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy_texture(TextureHandle handle) override {
        GLuint tex = static_cast<GLuint>(handle);
        if (tex) glDeleteTextures(1, &tex);
    }

    MeshHandle create_mesh(const Vertex* vertices, uint32_t num_vertices,
                           const uint32_t* indices, uint32_t num_indices) override {
        if (!vertices || num_vertices == 0 || !indices || num_indices == 0) {
            return INVALID_MESH;
        }

        MeshData md;
        md.index_count = num_indices;

        glGenVertexArrays(1, &md.vao);
        glGenBuffers(1, &md.vbo);
        glGenBuffers(1, &md.ebo);

        glBindVertexArray(md.vao);

        glBindBuffer(GL_ARRAY_BUFFER, md.vbo);
        glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(Vertex), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, md.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(uint32_t), indices, GL_STATIC_DRAW);

        // Location 0: Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, position)));

        // Location 1: Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, normal)));

        // Location 2: Texcoord
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

        // Location 3: Color
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, color)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        MeshHandle handle = next_mesh_id_++;
        meshes_[handle] = md;
        LOGI("Created 3D mesh (handle %u): %u verts, %u indices", handle, num_vertices, num_indices);
        return handle;
    }

    MeshHandle create_terrain_mesh(const TerrainVertex* vertices, uint32_t num_vertices,
                                   const uint32_t* indices, uint32_t num_indices) override {
        if (!vertices || num_vertices == 0 || !indices || num_indices == 0) {
            return INVALID_MESH;
        }

        MeshData md;
        md.index_count = num_indices;
        md.vertex_count = num_vertices;
        md.dynamic = false;

        glGenVertexArrays(1, &md.vao);
        glGenBuffers(1, &md.vbo);
        glGenBuffers(1, &md.ebo);

        glBindVertexArray(md.vao);

        glBindBuffer(GL_ARRAY_BUFFER, md.vbo);
        glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(TerrainVertex), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, md.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * sizeof(uint32_t), indices, GL_STATIC_DRAW);

        // Location 0: Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, position)));

        // Location 1: Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, normal)));

        // Location 2: Texcoord
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, texcoord)));

        // Location 3: Color
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, color)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        MeshHandle handle = next_mesh_id_++;
        meshes_[handle] = md;
        LOGI("Created terrain mesh (handle %u): %u verts, %u indices", handle, num_vertices, num_indices);
        return handle;
    }

    void destroy_mesh(MeshHandle handle) override {
        auto it = meshes_.find(handle);
        if (it != meshes_.end()) {
            glDeleteVertexArrays(1, &it->second.vao);
            glDeleteBuffers(1, &it->second.vbo);
            glDeleteBuffers(1, &it->second.ebo);
            meshes_.erase(it);
        }
    }

    MeshHandle create_dynamic_mesh(const Vertex* vertices, uint32_t num_vertices,
                                   const uint32_t* indices, uint32_t num_indices) override {
        if (!vertices || num_vertices == 0 || !indices || num_indices == 0)
            return INVALID_MESH;

        MeshData md;
        md.index_count  = num_indices;
        md.vertex_count = num_vertices;
        md.dynamic      = true;

        glGenVertexArrays(1, &md.vao);
        glGenBuffers(1, &md.vbo);
        glGenBuffers(1, &md.ebo);

        glBindVertexArray(md.vao);

        // VBO: GL_DYNAMIC_DRAW — positions/normals updated every frame
        glBindBuffer(GL_ARRAY_BUFFER, md.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     num_vertices * sizeof(Vertex),
                     vertices,
                     GL_DYNAMIC_DRAW);

        // EBO: topology never changes, stays static
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, md.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     num_indices * sizeof(uint32_t),
                     indices,
                     GL_STATIC_DRAW);

        // Attribute layout matches the 3D mesh shader:
        // loc 0 = position, loc 1 = normal, loc 2 = texcoord, loc 3 = color
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, color)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        MeshHandle handle = next_mesh_id_++;
        meshes_[handle] = md;
        LOGI("Created dynamic mesh (handle %u): %u verts, %u indices", handle, num_vertices, num_indices);
        return handle;
    }

    /// Upload morphed positions (and optionally normals) into the dynamic VBO.
    /// We use glBufferSubData on individual struct fields to avoid rebuilding
    /// the full Vertex array; this only touches position and normal bytes.
    void update_mesh_vertices(MeshHandle handle,
                              const glm::vec3* positions,
                              const glm::vec3* normals,
                              uint32_t num_vertices) override {
        auto it = meshes_.find(handle);
        if (it == meshes_.end() || !it->second.dynamic) return;
        if (!positions || num_vertices == 0) return;

        const uint32_t n = std::min(num_vertices, it->second.vertex_count);

        glBindBuffer(GL_ARRAY_BUFFER, it->second.vbo);

        // Update only position and normal fields inside each Vertex struct.
        // Offsets within Vertex: position @ 0, normal @ 12, texcoord @ 24, color @ 32.
        static_assert(offsetof(Vertex, position) == 0,  "Vertex layout changed");
        static_assert(offsetof(Vertex, normal)   == 12, "Vertex layout changed");

        for (uint32_t i = 0; i < n; ++i) {
            const GLintptr base = static_cast<GLintptr>(i * sizeof(Vertex));
            // position (12 bytes)
            glBufferSubData(GL_ARRAY_BUFFER,
                            base + static_cast<GLintptr>(offsetof(Vertex, position)),
                            sizeof(glm::vec3),
                            &positions[i]);
            // normal (12 bytes) — only if caller provided them
            if (normals) {
                glBufferSubData(GL_ARRAY_BUFFER,
                                base + static_cast<GLintptr>(offsetof(Vertex, normal)),
                                sizeof(glm::vec3),
                                &normals[i]);
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void set_camera(const Camera& camera) override {
        camera_ = camera;
    }

    void set_light(const DirectionalLight& light) override {
        light_ = light;
    }

    void draw_mesh(MeshHandle mesh, const glm::mat4& transform, TextureHandle texture,
                   BlendMode blend) override {
        auto it = meshes_.find(mesh);
        if (it == meshes_.end() || !mesh_program_) return;

        (void)blend;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);

        glUseProgram(mesh_program_);

        const glm::mat4 view_proj = camera_.view_projection();
        const glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));

        glUniformMatrix4fv(mesh_uModel_, 1, GL_FALSE, &transform[0][0]);
        glUniformMatrix4fv(mesh_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniformMatrix3fv(mesh_uNormalMat_, 1, GL_FALSE, &normal_mat[0][0]);

        glUniform3fv(mesh_uLightDir_, 1, &light_.direction[0]);
        glUniform3fv(mesh_uLightColor_, 1, &light_.color[0]);
        glUniform3fv(mesh_uAmbient_, 1, &light_.ambient[0]);

        if (texture != INVALID_TEXTURE) {
            glUniform1i(mesh_uHasTex_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
            glUniform1i(mesh_uTex_, 0);
        } else {
            glUniform1i(mesh_uHasTex_, 0);
        }

        glBindVertexArray(it->second.vao);
        glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (texture != INVALID_TEXTURE) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glUseProgram(0);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    void draw_mesh_skinned(MeshHandle mesh, const glm::mat4& transform,
                           TextureHandle texture,
                           const std::vector<glm::mat4>& bone_matrices,
                           BlendMode blend) override {
        auto it = meshes_.find(mesh);
        if (it == meshes_.end() || !skinned_mesh_program_) return;

        (void)blend;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);

        glUseProgram(skinned_mesh_program_);

        const glm::mat4 view_proj = camera_.view_projection();
        const glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(transform)));

        glUniformMatrix4fv(skinned_mesh_uModel_, 1, GL_FALSE, &transform[0][0]);
        glUniformMatrix4fv(skinned_mesh_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniformMatrix3fv(skinned_mesh_uNormalMat_, 1, GL_FALSE, &normal_mat[0][0]);

        // Upload bone matrices
        if (!bone_matrices.empty()) {
            const size_t count = std::min<size_t>(bone_matrices.size(), 64);
            glUniformMatrix4fv(skinned_mesh_uBoneMatrices_, static_cast<GLsizei>(count), GL_FALSE, &bone_matrices[0][0][0]);
        }

        glUniform3fv(skinned_mesh_uLightDir_, 1, &light_.direction[0]);
        glUniform3fv(skinned_mesh_uLightColor_, 1, &light_.color[0]);
        glUniform3fv(skinned_mesh_uAmbient_, 1, &light_.ambient[0]);

        if (texture != INVALID_TEXTURE) {
            glUniform1i(skinned_mesh_uHasTex_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
            glUniform1i(skinned_mesh_uTex_, 0);
        } else {
            glUniform1i(skinned_mesh_uHasTex_, 0);
        }

        glBindVertexArray(it->second.vao);
        glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (texture != INVALID_TEXTURE) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glUseProgram(0);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    void draw_terrain(MeshHandle mesh, TextureHandle base_texture,
                      TextureHandle blend_texture,
                      TextureHandle bank_texture,
                      TextureHandle outer_bank_texture) override {
        auto it = meshes_.find(mesh);
        if (it == meshes_.end() || !terrain_program_) return;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glUseProgram(terrain_program_);

        const glm::mat4 model = glm::mat4(1.0f);
        const glm::mat4 view_proj = camera_.view_projection();
        const glm::mat3 normal_mat = glm::mat3(1.0f);

        glUniformMatrix4fv(terrain_uModel_, 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(terrain_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniformMatrix3fv(terrain_uNormalMat_, 1, GL_FALSE, &normal_mat[0][0]);

        glUniform3fv(terrain_uLightDir_, 1, &light_.direction[0]);
        glUniform3fv(terrain_uLightColor_, 1, &light_.color[0]);
        glUniform3fv(terrain_uAmbient_, 1, &light_.ambient[0]);

        if (base_texture != INVALID_TEXTURE) {
            glUniform1i(terrain_uHasBaseTex_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(base_texture));
            glUniform1i(terrain_uBaseTex_, 0);
        } else {
            glUniform1i(terrain_uHasBaseTex_, 0);
        }

        if (blend_texture != INVALID_TEXTURE) {
            glUniform1i(terrain_uHasBlendTex_, 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(blend_texture));
            glUniform1i(terrain_uBlendTex_, 1);
        } else {
            glUniform1i(terrain_uHasBlendTex_, 0);
        }

        if (bank_texture != INVALID_TEXTURE) {
            glUniform1i(terrain_uHasBankTex_, 1);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bank_texture));
            glUniform1i(terrain_uBankTex_, 2);
        } else {
            glUniform1i(terrain_uHasBankTex_, 0);
        }

        if (outer_bank_texture != INVALID_TEXTURE) {
            glUniform1i(terrain_uHasOuterBankTex_, 1);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(outer_bank_texture));
            glUniform1i(terrain_uOuterBankTex_, 3);
        } else {
            glUniform1i(terrain_uHasOuterBankTex_, 0);
        }

        glBindVertexArray(it->second.vao);
        glDrawElements(GL_TRIANGLES, it->second.index_count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (base_texture != INVALID_TEXTURE) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (blend_texture != INVALID_TEXTURE) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (bank_texture != INVALID_TEXTURE) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (outer_bank_texture != INVALID_TEXTURE) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);

        glUseProgram(0);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
    }

    void draw_billboard(const glm::vec3& position, const glm::vec2& size,
                         TextureHandle texture, uint32_t color) override {
        if (!billboard_program_) return;
        if (billboard_vao_ == 0) {
            // Should have been created in compile_billboard_program
            return;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);  // Billboards should be visible from both sides
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(billboard_program_);

        const glm::mat4 view_proj = camera_.view_projection();

        glUniformMatrix4fv(billboard_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniform3fv(billboard_uCameraPos_, 1, &camera_.position[0]);
        glUniform3fv(billboard_uBillboardPos_, 1, &position[0]);
        glUniform2fv(billboard_uBillboardSize_, 1, &size[0]);

        float r = ((color >> 24) & 0xFF) / 255.0f;
        float g = ((color >> 16) & 0xFF) / 255.0f;
        float b = ((color >> 8) & 0xFF) / 255.0f;
        float a = (color & 0xFF) / 255.0f;
        glUniform4f(billboard_uColor_, r, g, b, a);

        if (texture != INVALID_TEXTURE) {
            glUniform1i(billboard_uHasTex_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
            glUniform1i(billboard_uTex_, 0);
        } else {
            glUniform1i(billboard_uHasTex_, 0);
        }

        glBindVertexArray(billboard_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        if (texture != INVALID_TEXTURE) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glUseProgram(0);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void create_water_grid() {
        if (water_vao_ != 0) return;
        const int GRID = 32;
        const float SIZE = 64.0f;
        const float STEP = SIZE / static_cast<float>(GRID);
        const float HALF = SIZE * 0.5f;

        std::vector<TerrainVertex> verts;
        verts.reserve((GRID + 1) * (GRID + 1));
        std::vector<uint32_t> indices;
        indices.reserve(GRID * GRID * 6);

        for (int z = 0; z <= GRID; ++z) {
            for (int x = 0; x <= GRID; ++x) {
                float wx = -HALF + x * STEP;
                float wz = -HALF + z * STEP;
                TerrainVertex v;
                v.position = glm::vec3(wx, 0.0f, wz);
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                v.texcoord = glm::vec2(static_cast<float>(x) / GRID * 4.0f, static_cast<float>(z) / GRID * 4.0f);
                v.color = glm::vec4(1.0f);
                verts.push_back(v);
            }
        }

        for (int z = 0; z < GRID; ++z) {
            for (int x = 0; x < GRID; ++x) {
                uint32_t tl = z * (GRID + 1) + x;
                uint32_t tr = tl + 1;
                uint32_t bl = (z + 1) * (GRID + 1) + x;
                uint32_t br = bl + 1;

                indices.push_back(tl);
                indices.push_back(bl);
                indices.push_back(tr);

                indices.push_back(tr);
                indices.push_back(bl);
                indices.push_back(br);
            }
        }

        water_index_count_ = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &water_vao_);
        glGenBuffers(1, &water_vbo_);
        glGenBuffers(1, &water_ebo_);

        glBindVertexArray(water_vao_);

        glBindBuffer(GL_ARRAY_BUFFER, water_vbo_);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TerrainVertex), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, water_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, texcoord)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                              reinterpret_cast<void*>(offsetof(TerrainVertex, color)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void draw_water(const glm::vec4& water_plane, TextureHandle normal_map, float time,
                    const glm::vec3& water_color = glm::vec3(0, 50, 170),
                    float water_level = -0.4f,
                    float terrain_height = -0.4f,
                    TextureHandle water_tex = INVALID_TEXTURE) override {
        if (!water_program_) return;
        if (water_vao_ == 0) create_water_grid();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE); // Água transparente não bloqueia depth buffer de objetos
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(water_program_);

        const float water_y = -water_plane.w;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(camera_.target.x, water_y, camera_.target.z));
        const glm::mat4 view_proj = camera_.view_projection();

        glUniformMatrix4fv(water_uModel_, 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(water_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniform3fv(water_uCameraPos_, 1, &camera_.position[0]);
        glUniform3fv(water_uLightDir_, 1, &light_.direction[0]);
        glUniform3fv(water_uLightColor_, 1, &light_.color[0]);
        glUniform3fv(water_uAmbient_, 1, &light_.ambient[0]);
        glUniform1f(water_uTime_, time);
        glUniform3fv(water_uWaterColor_, 1, &water_color[0]);
        glUniform1f(water_uWaterLevel_, water_level);
        glUniform1f(water_uTerrainHeight_, terrain_height);

        if (normal_map != INVALID_TEXTURE) {
            glUniform1i(water_uHasNormalMap_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(normal_map));
            glUniform1i(water_uNormalMap_, 0);
        } else {
            glUniform1i(water_uHasNormalMap_, 0);
        }

        if (water_tex != INVALID_TEXTURE) {
            glUniform1i(water_uHasWaterTex_, 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(water_tex));
            glUniform1i(water_uWaterTex_, 1);
        } else {
            glUniform1i(water_uHasWaterTex_, 0);
        }

        glBindVertexArray(water_vao_);
        glDrawElements(GL_TRIANGLES, water_index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE); // Restaura escrita normal de profundidade

        if (normal_map != INVALID_TEXTURE) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (water_tex != INVALID_TEXTURE) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glUseProgram(0);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }

    void begin_ui() override {}

        void draw_ui_quad(const glm::vec2& pos, const glm::vec2& size,
                       TextureHandle tex, uint32_t color) override {
        if (!program_) return;

        if (tex == INVALID_TEXTURE) {
            if (white_tex_ == INVALID_TEXTURE) {
                const uint32_t white_pixel = 0xFFFFFFFF;
                white_tex_ = create_texture(1, 1, reinterpret_cast<const uint8_t*>(&white_pixel));
            }
            tex = white_tex_;
        }
        if (tex == INVALID_TEXTURE) return;

        // UI sempre por cima: desabilita depth test E depth write
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float vw = width_ > 0 ? static_cast<float>(width_) : 1.0f;
        const float vh = height_ > 0 ? static_cast<float>(height_) : 1.0f;
        const float sx = size.x / vw;
        const float sy = size.y / vh;
        const float ox = (2.0f * pos.x + size.x) / vw - 1.0f;
        const float oy = 1.0f - (2.0f * pos.y + size.y) / vh;

        glUseProgram(program_);
        glUniform2f(scale_loc_, sx, sy);
        glUniform2f(offset_loc_, ox, oy);
        glUniform2f(uv_scale_loc_, 1.0f, 1.0f);
        glUniform2f(uv_offset_loc_, 0.0f, 0.0f);

        // UI colors use the Android/C++ convention 0xAARRGGBB.
        float ca = ((color >> 24) & 0xff) / 255.0f;
        float cr = ((color >> 16) & 0xff) / 255.0f;
        float cg = ((color >> 8) & 0xff) / 255.0f;
        float cb = (color & 0xff) / 255.0f;
        // color == 0 é alpha 0 (transparente total) — não tratar como branco opaco.
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glUniform1i(tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
        glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // Restaurar depth test para não afetar render 3D subsequente
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void draw_ui_quad_uv(const glm::vec2& pos, const glm::vec2& size,
                         TextureHandle tex,
                         const glm::vec2& uv_min, const glm::vec2& uv_max,
                         uint32_t color) override {
        if (!program_) return;

        if (tex == INVALID_TEXTURE) {
            if (white_tex_ == INVALID_TEXTURE) {
                const uint32_t white_pixel = 0xFFFFFFFF;
                white_tex_ = create_texture(1, 1, reinterpret_cast<const uint8_t*>(&white_pixel));
            }
            tex = white_tex_;
        }
        if (tex == INVALID_TEXTURE) return;

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float vw = width_ > 0 ? static_cast<float>(width_) : 1.0f;
        const float vh = height_ > 0 ? static_cast<float>(height_) : 1.0f;
        const float sx = size.x / vw;
        const float sy = size.y / vh;
        const float ox = (2.0f * pos.x + size.x) / vw - 1.0f;
        const float oy = 1.0f - (2.0f * pos.y + size.y) / vh;

        glUseProgram(program_);
        glUniform2f(scale_loc_, sx, sy);
        glUniform2f(offset_loc_, ox, oy);
        glUniform2f(uv_scale_loc_, uv_max.x - uv_min.x, uv_max.y - uv_min.y);
        glUniform2f(uv_offset_loc_, uv_min.x, uv_min.y);

        float ca = ((color >> 24) & 0xff) / 255.0f;
        float cr = ((color >> 16) & 0xff) / 255.0f;
        float cg = ((color >> 8) & 0xff) / 255.0f;
        float cb = (color & 0xff) / 255.0f;
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glUniform1i(tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
        glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void draw_ui_quad_nineslice(const glm::vec2& pos, const glm::vec2& size,
                                 TextureHandle tex,
                                 uint32_t tex_w, uint32_t tex_h,
                                 float border_left, float border_right,
                                 float border_top, float border_bottom,
                                 uint32_t color) override {
        if (!program_) return;

        if (tex == INVALID_TEXTURE) {
            if (white_tex_ == INVALID_TEXTURE) {
                const uint32_t white_pixel = 0xFFFFFFFF;
                white_tex_ = create_texture(1, 1, reinterpret_cast<const uint8_t*>(&white_pixel));
            }
            tex = white_tex_;
            tex_w = tex_h = 1;
        }
        if (tex == INVALID_TEXTURE) return;

        // Se não há info de textura ou não há bordas, fazer full-stretch simples
        if (tex_w == 0 || tex_h == 0 ||
            (border_left == 0 && border_right == 0 && border_top == 0 && border_bottom == 0)) {
            draw_ui_quad(pos, size, tex, color);
            return;
        }

        // Limitar bordas ao tamanho da textura
        float bl = std::min(border_left, static_cast<float>(tex_w) * 0.5f);
        float br = std::min(border_right, static_cast<float>(tex_w) * 0.5f);
        float bt = std::min(border_top, static_cast<float>(tex_h) * 0.5f);
        float bb = std::min(border_bottom, static_cast<float>(tex_h) * 0.5f);

        // Garantir que bordas não excedam o tamanho do quad
        float max_w_blend = size.x * 0.5f;
        float max_h_blend = size.y * 0.5f;
        if (bl + br > max_w_blend) {
            float scale = max_w_blend / (bl + br);
            bl *= scale; br *= scale;
        }
        if (bt + bb > max_h_blend) {
            float scale = max_h_blend / (bt + bb);
            bt *= scale; bb *= scale;
        }

        // UVs das bordas (em coordenadas normalizadas 0-1)
        float uv_l = bl / static_cast<float>(tex_w);
        float uv_r = (static_cast<float>(tex_w) - br) / static_cast<float>(tex_w);
        float uv_t = bt / static_cast<float>(tex_h);
        float uv_b = (static_cast<float>(tex_h) - bb) / static_cast<float>(tex_h);

        // Posições dos 9 slices
        float x0 = pos.x;
        float x1 = pos.x + bl;
        float x2 = pos.x + size.x - br;
        float x3 = pos.x + size.x;

        float y0 = pos.y;
        float y1 = pos.y + bt;
        float y2 = pos.y + size.y - bb;
        float y3 = pos.y + size.y;

        // UI sempre por cima: desabilita depth test E depth write
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float vw = width_ > 0 ? static_cast<float>(width_) : 1.0f;
        const float vh = height_ > 0 ? static_cast<float>(height_) : 1.0f;

        // Struct auxiliar para um slice
        struct Slice {
            float px0, py0, px1, py1;  // posição em tela
            float uv0, uv1, uv2, uv3;  // UVs (s0, t0, s1, t1)
        };

        Slice slices[9] = {
            // Top-left
            {x0, y0, x1, y1, 0.0f, 0.0f, uv_l, uv_t},
            // Top
            {x1, y0, x2, y1, uv_l, 0.0f, uv_r, uv_t},
            // Top-right
            {x2, y0, x3, y1, uv_r, 0.0f, 1.0f, uv_t},
            // Left
            {x0, y1, x1, y2, 0.0f, uv_t, uv_l, uv_b},
            // Center
            {x1, y1, x2, y2, uv_l, uv_t, uv_r, uv_b},
            // Right
            {x2, y1, x3, y2, uv_r, uv_t, 1.0f, uv_b},
            // Bottom-left
            {x0, y2, x1, y3, 0.0f, uv_b, uv_l, 1.0f},
            // Bottom
            {x1, y2, x2, y3, uv_l, uv_b, uv_r, 1.0f},
            // Bottom-right
            {x2, y2, x3, y3, uv_r, uv_b, 1.0f, 1.0f},
        };

        glUseProgram(program_);
        glUniform1i(tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));

        // UI colors use the Android/C++ convention 0xAARRGGBB.
        float ca = ((color >> 24) & 0xff) / 255.0f;
        float cr = ((color >> 16) & 0xff) / 255.0f;
        float cg = ((color >> 8) & 0xff) / 255.0f;
        float cb = (color & 0xff) / 255.0f;
        // color == 0 é alpha 0 (transparente total) — não tratar como branco opaco.
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glBindVertexArray(quad_vao_);

        for (const auto& s : slices) {
            // Converter posição de tela para NDC (-1 a 1)
            const float sx = (s.px1 - s.px0) / vw;
            const float sy = (s.py1 - s.py0) / vh;
            const float ox = (2.0f * s.px0 + (s.px1 - s.px0)) / vw - 1.0f;
            const float oy = 1.0f - (2.0f * s.py0 + (s.py1 - s.py0)) / vh;

            glUniform2f(scale_loc_, sx, sy);
            glUniform2f(offset_loc_, ox, oy);
            glUniform2f(uv_scale_loc_, s.uv2 - s.uv0, s.uv3 - s.uv1);
            glUniform2f(uv_offset_loc_, s.uv0, s.uv1);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // Restaurar depth test para não afetar render 3D subsequente
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    bool load_font_atlas() {
        if (font_atlas_loaded_) return true;

        std::vector<uint8_t> font_buf;

        if (!font_memory_buf_.empty()) {
            font_buf = font_memory_buf_;
            LOGI("Loading font from memory buffer (%zu bytes)", font_buf.size());
        } else if (asset_mgr_) {
            AAsset* asset = AAssetManager_open(asset_mgr_, "font.ttf", AASSET_MODE_BUFFER);
            if (!asset) {
                asset = AAssetManager_open(asset_mgr_, "assets/font.ttf", AASSET_MODE_BUFFER);
            }
            if (asset) {
                off_t len = AAsset_getLength(asset);
                font_buf.resize(static_cast<size_t>(len));
                AAsset_read(asset, font_buf.data(), len);
                AAsset_close(asset);
                LOGI("Loaded font directly from APK assets: font.ttf (%zu bytes)", font_buf.size());
            }
        }

        if (font_buf.empty()) {
            const char* paths[] = {
                "assets/font.ttf",
                "/storage/emulated/0/Documents/AoM-Native-Port/assets/font.ttf",
                "/system/fonts/Roboto-Regular.ttf",
                "/system/fonts/DroidSans.ttf",
                "/system/fonts/NotoSansCJK-Regular.ttc"
            };

            for (const char* p : paths) {
                FILE* f = fopen(p, "rb");
                if (f) {
                    LOGI("Loading font from filesystem: %s", p);
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    font_buf.resize(static_cast<size_t>(sz));
                    fread(font_buf.data(), 1, sz, f);
                    fclose(f);
                    break;
                }
            }
        }

        const int atlas_w = 512;
        const int atlas_h = 512;

        if (font_buf.empty()) {
            LOGW("Font TTF not found in assets or system, generating raster fallback font atlas");
            std::vector<uint8_t> rgba(atlas_w * atlas_h * 4, 0);
            for (int ch = 32; ch < 127; ++ch) {
                int col = (ch - 32) % 16;
                int row = (ch - 32) / 16;
                int gx = col * 32 + 4;
                int gy = row * 32 + 4;
                stbtt_bakedchar& bc = baked_chars_[ch - 32];
                bc.x0 = static_cast<unsigned short>(gx);
                bc.y0 = static_cast<unsigned short>(gy);
                bc.x1 = static_cast<unsigned short>(gx + 16);
                bc.y1 = static_cast<unsigned short>(gy + 24);
                bc.xoff = 0.0f;
                bc.yoff = -22.0f;
                bc.xadvance = 16.0f;
                for (int y = 0; y < 22; ++y) {
                    for (int x = 0; x < 14; ++x) {
                        bool on = false;
                        if (x == 0 || x == 13 || y == 0 || y == 21) on = true;
                        if (ch >= 65 && ch <= 90 && (x == 6 || x == 7)) on = true;
                        if (ch >= 97 && ch <= 122 && (y == 10 || y == 11)) on = true;
                        if (ch >= 48 && ch <= 57 && (x == y / 2)) on = true;
                        if (on) {
                            int p = ((gy + y) * atlas_w + (gx + x)) * 4;
                            rgba[p + 0] = 255;
                            rgba[p + 1] = 255;
                            rgba[p + 2] = 255;
                            rgba[p + 3] = 255;
                        }
                    }
                }
            }
            font_atlas_tex_ = create_texture(atlas_w, atlas_h, rgba.data());
            font_atlas_loaded_ = (font_atlas_tex_ != INVALID_TEXTURE);
            return font_atlas_loaded_;
        }

        std::vector<uint8_t> mono(atlas_w * atlas_h, 0);
        int res = stbtt_BakeFontBitmap(font_buf.data(), 0, 28.0f,
                                       mono.data(), atlas_w, atlas_h,
                                       32, 96, baked_chars_);
        if (res <= 0) {
            LOGE("stbtt_BakeFontBitmap failed: %d", res);
            return false;
        }

        std::vector<uint8_t> rgba(atlas_w * atlas_h * 4);
        for (int i = 0; i < atlas_w * atlas_h; ++i) {
            uint8_t a = mono[i];
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = a;
        }

        font_atlas_tex_ = create_texture(atlas_w, atlas_h, rgba.data());
        if (font_atlas_tex_ == INVALID_TEXTURE) {
            LOGE("Failed to create font atlas texture");
            return false;
        }

        font_atlas_loaded_ = true;
        LOGI("Font atlas loaded successfully (%dx%d)", atlas_w, atlas_h);
        return true;
    }

    void draw_ui_glyph(const stbtt_aligned_quad& q, uint32_t color) {
        if (!program_ || font_atlas_tex_ == INVALID_TEXTURE) return;

        float gw = q.x1 - q.x0;
        float gh = q.y1 - q.y0;
        if (gw <= 0.0f || gh <= 0.0f) return;

        const float vw = width_ > 0 ? static_cast<float>(width_) : 1.0f;
        const float vh = height_ > 0 ? static_cast<float>(height_) : 1.0f;
        const float sx = gw / vw;
        const float sy = gh / vh;
        const float ox = (2.0f * q.x0 + gw) / vw - 1.0f;
        const float oy = 1.0f - (2.0f * q.y0 + gh) / vh;

        glUniform2f(scale_loc_, sx, sy);
        glUniform2f(offset_loc_, ox, oy);
        glUniform2f(uv_scale_loc_, q.s1 - q.s0, q.t1 - q.t0);
        glUniform2f(uv_offset_loc_, q.s0, q.t0);

        // UI colors use the Android/C++ convention 0xAARRGGBB.
        float ca = ((color >> 24) & 0xff) / 255.0f;
        float cr = ((color >> 16) & 0xff) / 255.0f;
        float cg = ((color >> 8) & 0xff) / 255.0f;
        float cb = (color & 0xff) / 255.0f;
        // color == 0 é alpha 0 (transparente total) — não tratar como branco opaco.
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    static char decode_utf8_char(const std::string& text, size_t& i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            return static_cast<char>(c);
        }
        if (c == 0xC3 && i + 1 < text.size()) {
            unsigned char c2 = static_cast<unsigned char>(text[++i]);
            uint32_t cp = 0xC0 | (c2 & 0x3F);
            if (cp >= 0xC0 && cp <= 0xC5) return 'A';
            if (cp == 0xC7) return 'C';
            if (cp >= 0xC8 && cp <= 0xCB) return 'E';
            if (cp >= 0xCC && cp <= 0xCF) return 'I';
            if (cp >= 0xD2 && cp <= 0xD6) return 'O';
            if (cp >= 0xD9 && cp <= 0xDC) return 'U';
            if (cp >= 0xE0 && cp <= 0xE5) return 'a';
            if (cp == 0xE7) return 'c';
            if (cp >= 0xE8 && cp <= 0xEB) return 'e';
            if (cp >= 0xEC && cp <= 0xEF) return 'i';
            if (cp >= 0xF2 && cp <= 0xF6) return 'o';
            if (cp >= 0xF9 && cp <= 0xFC) return 'u';
            return '?';
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            i++;
            return '?';
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            i += 2;
            return '?';
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            i += 3;
            return '?';
        }
        return '?';
    }

    void draw_ui_text(const std::string& text, const glm::vec2& pos, float size, uint32_t color) override {
        if (text.empty()) return;
        if (!font_atlas_loaded_) {
            if (!load_font_atlas()) return;
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(program_);
        glUniform1i(tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(font_atlas_tex_));
        glBindVertexArray(quad_vao_);

        const float target_size = (size > 0.0f) ? size : 20.0f;
        const float font_scale = target_size / 28.0f;
        const float line_h = target_size * 1.25f;

        float cur_x = pos.x;
        float cur_y = pos.y + target_size;
        const float start_x = cur_x;

        for (size_t i = 0; i < text.size(); ++i) {
            char c = decode_utf8_char(text, i);
            if (c == '\n') {
                cur_x = start_x;
                cur_y += line_h;
                continue;
            }
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 32 && uc <= 126) {
                float dummy_x = 0.0f;
                float dummy_y = 0.0f;
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(baked_chars_, 512, 512, uc - 32, &dummy_x, &dummy_y, &q, 1);

                stbtt_aligned_quad scaled_q;
                scaled_q.x0 = cur_x + q.x0 * font_scale;
                scaled_q.x1 = cur_x + q.x1 * font_scale;
                scaled_q.y0 = cur_y + q.y0 * font_scale;
                scaled_q.y1 = cur_y + q.y1 * font_scale;
                scaled_q.s0 = q.s0;
                scaled_q.s1 = q.s1;
                scaled_q.t0 = q.t0;
                scaled_q.t1 = q.t1;

                draw_ui_glyph(scaled_q, color);
                cur_x += dummy_x * font_scale;
            }
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    float measure_ui_text(const std::string& text, float size) override {
        if (text.empty()) return 0.0f;
        if (!font_atlas_loaded_) {
            if (!load_font_atlas()) return text.size() * size * 0.55f;
        }
        const float target_size = (size > 0.0f) ? size : 20.0f;
        const float font_scale = target_size / 28.0f;
        float width = 0.0f;
        float max_width = 0.0f;

        for (size_t i = 0; i < text.size(); ++i) {
            char c = decode_utf8_char(text, i);
            if (c == '\n') {
                if (width > max_width) max_width = width;
                width = 0.0f;
                continue;
            }
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 32 && uc <= 126) {
                float dummy_x = 0.0f;
                float dummy_y = 0.0f;
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(baked_chars_, 512, 512, uc - 32, &dummy_x, &dummy_y, &q, 1);
                width += dummy_x * font_scale;
            }
        }
        return (width > max_width) ? width : max_width;
    }

    void end_ui() override {}
    void draw_debug_line(const glm::vec3& from, const glm::vec3& to, uint32_t color) override {
        if (!line_program_ || line_vao_ == 0) return;
        glUseProgram(line_program_);
        glm::mat4 vp = camera_.view_projection();
        glUniformMatrix4fv(line_uViewProj_, 1, GL_FALSE, &vp[0][0]);

        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = (color & 0xFF) / 255.0f;
        float a = ((color >> 24) & 0xFF) / 255.0f;
        if (a <= 0.0f) a = 1.0f;
        glUniform4f(line_uColor_, r, g, b, a);

        glm::vec3 pts[2] = {from, to};
        glBindVertexArray(line_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 2 * sizeof(glm::vec3), pts);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        if (a < 1.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glDrawArrays(GL_LINES, 0, 2);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
        if (a < 1.0f) glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }
    float get_fps() const override { return fps_; }

    bool frustum_test_sphere(const glm::vec3& center, float radius) const override {
        return camera_.frustum_test_sphere(center, radius);
    }
    const char* get_name() const override { return "OpenGL ES 3 Renderer"; }

    void set_asset_manager(void* asset_mgr) override {
        asset_mgr_ = static_cast<AAssetManager*>(asset_mgr);
    }

    bool set_font_data(const uint8_t* data, size_t size) override {
        if (!data || size == 0) return false;
        font_memory_buf_.assign(data, data + size);
        font_atlas_loaded_ = false;
        return true;
    }

private:
    bool compile_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        program_ = glCreateProgram();
        glAttachShader(program_, vert);
        glAttachShader(program_, frag);
        glLinkProgram(program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(program_, len, nullptr, log.data());
            LOGE("UI Program link failed: %s", log.data());
            glDeleteProgram(program_);
            program_ = 0;
            return false;
        }

        scale_loc_ = glGetUniformLocation(program_, "uScale");
        offset_loc_ = glGetUniformLocation(program_, "uOffset");
        uv_scale_loc_ = glGetUniformLocation(program_, "uUVScale");
        uv_offset_loc_ = glGetUniformLocation(program_, "uUVOffset");
        color_loc_ = glGetUniformLocation(program_, "uColor");
        tex_loc_ = glGetUniformLocation(program_, "uTex");
        return true;
    }

    bool compile_mesh_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kMeshVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kMeshFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        mesh_program_ = glCreateProgram();
        glAttachShader(mesh_program_, vert);
        glAttachShader(mesh_program_, frag);
        glLinkProgram(mesh_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(mesh_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(mesh_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(mesh_program_, len, nullptr, log.data());
            LOGE("Mesh Program link failed: %s", log.data());
            glDeleteProgram(mesh_program_);
            mesh_program_ = 0;
            return false;
        }

        mesh_uModel_ = glGetUniformLocation(mesh_program_, "uModel");
        mesh_uViewProj_ = glGetUniformLocation(mesh_program_, "uViewProj");
        mesh_uNormalMat_ = glGetUniformLocation(mesh_program_, "uNormalMat");
        mesh_uLightDir_ = glGetUniformLocation(mesh_program_, "uLightDir");
        mesh_uLightColor_ = glGetUniformLocation(mesh_program_, "uLightColor");
        mesh_uAmbient_ = glGetUniformLocation(mesh_program_, "uAmbient");
        mesh_uHasTex_ = glGetUniformLocation(mesh_program_, "uHasTex");
        mesh_uTex_ = glGetUniformLocation(mesh_program_, "uTex");
        return true;
    }

    bool compile_skinned_mesh_program() {
        const char* kSkinnedMeshVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aBoneIndices;
layout(location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;
uniform mat4 uBoneMatrices[64];

out vec3 vNormal;
out vec2 vTex;
out vec4 vColor;

void main() {
    // Skinning: compute world position using bone matrices
    vec4 skinnedPos = vec4(0.0);
    skinnedPos += aBoneWeights.x * uBoneMatrices[int(aBoneIndices.x)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.y * uBoneMatrices[int(aBoneIndices.y)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.z * uBoneMatrices[int(aBoneIndices.z)] * vec4(aPos, 1.0);
    skinnedPos += aBoneWeights.w * uBoneMatrices[int(aBoneIndices.w)] * vec4(aPos, 1.0);

    vec3 skinnedNormal = vec3(0.0);
    skinnedNormal += aBoneWeights.x * mat3(uBoneMatrices[int(aBoneIndices.x)]) * aNormal;
    skinnedNormal += aBoneWeights.y * mat3(uBoneMatrices[int(aBoneIndices.y)]) * aNormal;
    skinnedNormal += aBoneWeights.z * mat3(uBoneMatrices[int(aBoneIndices.z)]) * aNormal;
    skinnedNormal += aBoneWeights.w * mat3(uBoneMatrices[int(aBoneIndices.w)]) * aNormal;
    skinnedNormal = normalize(skinnedNormal);

    vec4 worldPos = uModel * skinnedPos;
    vNormal = normalize(uNormalMat * skinnedNormal);
    vTex = aTex;
    vColor = aColor;
    gl_Position = uViewProj * worldPos;
}
)";

        GLuint vert = compile_shader(GL_VERTEX_SHADER, kSkinnedMeshVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kMeshFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        skinned_mesh_program_ = glCreateProgram();
        glAttachShader(skinned_mesh_program_, vert);
        glAttachShader(skinned_mesh_program_, frag);
        glLinkProgram(skinned_mesh_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(skinned_mesh_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(skinned_mesh_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(skinned_mesh_program_, len, nullptr, log.data());
            LOGE("Skinned Mesh Program link failed: %s", log.data());
            glDeleteProgram(skinned_mesh_program_);
            skinned_mesh_program_ = 0;
            return false;
        }

        skinned_mesh_uModel_ = glGetUniformLocation(skinned_mesh_program_, "uModel");
        skinned_mesh_uViewProj_ = glGetUniformLocation(skinned_mesh_program_, "uViewProj");
        skinned_mesh_uNormalMat_ = glGetUniformLocation(skinned_mesh_program_, "uNormalMat");
        skinned_mesh_uBoneMatrices_ = glGetUniformLocation(skinned_mesh_program_, "uBoneMatrices");
        skinned_mesh_uLightDir_ = glGetUniformLocation(skinned_mesh_program_, "uLightDir");
        skinned_mesh_uLightColor_ = glGetUniformLocation(skinned_mesh_program_, "uLightColor");
        skinned_mesh_uAmbient_ = glGetUniformLocation(skinned_mesh_program_, "uAmbient");
        skinned_mesh_uHasTex_ = glGetUniformLocation(skinned_mesh_program_, "uHasTex");
        skinned_mesh_uTex_ = glGetUniformLocation(skinned_mesh_program_, "uTex");
        return true;
    }

    bool compile_terrain_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kTerrainVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kTerrainFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        terrain_program_ = glCreateProgram();
        glAttachShader(terrain_program_, vert);
        glAttachShader(terrain_program_, frag);
        glLinkProgram(terrain_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(terrain_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(terrain_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(terrain_program_, len, nullptr, log.data());
            LOGE("Terrain Program link failed: %s", log.data());
            glDeleteProgram(terrain_program_);
            terrain_program_ = 0;
            return false;
        }

        terrain_uModel_ = glGetUniformLocation(terrain_program_, "uModel");
        terrain_uViewProj_ = glGetUniformLocation(terrain_program_, "uViewProj");
        terrain_uNormalMat_ = glGetUniformLocation(terrain_program_, "uNormalMat");
        terrain_uLightDir_ = glGetUniformLocation(terrain_program_, "uLightDir");
        terrain_uLightColor_ = glGetUniformLocation(terrain_program_, "uLightColor");
        terrain_uAmbient_ = glGetUniformLocation(terrain_program_, "uAmbient");
        terrain_uHasBaseTex_ = glGetUniformLocation(terrain_program_, "uHasBaseTex");
        terrain_uHasBlendTex_ = glGetUniformLocation(terrain_program_, "uHasBlendTex");
        terrain_uHasBankTex_ = glGetUniformLocation(terrain_program_, "uHasBankTex");
        terrain_uHasOuterBankTex_ = glGetUniformLocation(terrain_program_, "uHasOuterBankTex");
        terrain_uBaseTex_ = glGetUniformLocation(terrain_program_, "uBaseTex");
        terrain_uBlendTex_ = glGetUniformLocation(terrain_program_, "uBlendTex");
        terrain_uBankTex_ = glGetUniformLocation(terrain_program_, "uBankTex");
        terrain_uOuterBankTex_ = glGetUniformLocation(terrain_program_, "uOuterBankTex");
        return true;
    }

    bool compile_water_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kWaterVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kWaterFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        water_program_ = glCreateProgram();
        glAttachShader(water_program_, vert);
        glAttachShader(water_program_, frag);
        glLinkProgram(water_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(water_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(water_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(water_program_, len, nullptr, log.data());
            LOGE("Water Program link failed: %s", log.data());
            glDeleteProgram(water_program_);
            water_program_ = 0;
            return false;
        }

        water_uModel_ = glGetUniformLocation(water_program_, "uModel");
        water_uViewProj_ = glGetUniformLocation(water_program_, "uViewProj");
        water_uCameraPos_ = glGetUniformLocation(water_program_, "uCameraPos");
        water_uLightDir_ = glGetUniformLocation(water_program_, "uLightDir");
        water_uLightColor_ = glGetUniformLocation(water_program_, "uLightColor");
        water_uAmbient_ = glGetUniformLocation(water_program_, "uAmbient");
        water_uTime_ = glGetUniformLocation(water_program_, "uTime");
        water_uHasNormalMap_ = glGetUniformLocation(water_program_, "uHasNormalMap");
        water_uNormalMap_ = glGetUniformLocation(water_program_, "uNormalMap");
        water_uWaterColor_ = glGetUniformLocation(water_program_, "uWaterColor");
        water_uWaterLevel_ = glGetUniformLocation(water_program_, "uWaterLevel");
        water_uTerrainHeight_ = glGetUniformLocation(water_program_, "uTerrainHeight");
        water_uHasWaterTex_ = glGetUniformLocation(water_program_, "uHasWaterTex");
        water_uWaterTex_ = glGetUniformLocation(water_program_, "uWaterTex");
        return true;
    }

    bool compile_line_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kLineVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kLineFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        line_program_ = glCreateProgram();
        glAttachShader(line_program_, vert);
        glAttachShader(line_program_, frag);
        glLinkProgram(line_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(line_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            glDeleteProgram(line_program_);
            line_program_ = 0;
            return false;
        }

        line_uViewProj_ = glGetUniformLocation(line_program_, "uViewProj");
        line_uColor_ = glGetUniformLocation(line_program_, "uColor");

        glGenVertexArrays(1, &line_vao_);
        glGenBuffers(1, &line_vbo_);
        glBindVertexArray(line_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }

    bool compile_billboard_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kBillboardVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kBillboardFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        billboard_program_ = glCreateProgram();
        glAttachShader(billboard_program_, vert);
        glAttachShader(billboard_program_, frag);
        glLinkProgram(billboard_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(billboard_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(billboard_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(billboard_program_, len, nullptr, log.data());
            LOGE("Billboard Program link failed: %s", log.data());
            glDeleteProgram(billboard_program_);
            billboard_program_ = 0;
            return false;
        }

        billboard_uViewProj_ = glGetUniformLocation(billboard_program_, "uViewProj");
        billboard_uCameraPos_ = glGetUniformLocation(billboard_program_, "uCameraPos");
        billboard_uBillboardPos_ = glGetUniformLocation(billboard_program_, "uBillboardPos");
        billboard_uBillboardSize_ = glGetUniformLocation(billboard_program_, "uBillboardSize");
        billboard_uHasTex_ = glGetUniformLocation(billboard_program_, "uHasTex");
        billboard_uTex_ = glGetUniformLocation(billboard_program_, "uTex");
        billboard_uColor_ = glGetUniformLocation(billboard_program_, "uColor");

        // Create billboard geometry (quad centered at origin)
        glGenVertexArrays(1, &billboard_vao_);
        glGenBuffers(1, &billboard_vbo_);

        // Quad vertices: position (x, y, 0) + texcoord (u, v)
        // Vertices: bottom-left, bottom-right, top-left, top-right (triangle strip)
        const float billboard_verts[] = {
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom-left
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // bottom-right
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,  // top-left
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f   // top-right
        };

        glGenVertexArrays(1, &billboard_vao_);
        glGenBuffers(1, &billboard_vbo_);
        glBindVertexArray(billboard_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, billboard_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(billboard_verts), billboard_verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return true;
    }


    // ── Skybox Program (cubemap + procedural fallback) ────────────────────────────
    bool compile_skybox_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kSkyboxVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kSkyboxFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        skybox_program_ = glCreateProgram();
        glAttachShader(skybox_program_, vert);
        glAttachShader(skybox_program_, frag);
        glLinkProgram(skybox_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(skybox_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(skybox_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(skybox_program_, len, nullptr, log.data());
            LOGE("Skybox Program link failed: %s", log.data());
            glDeleteProgram(skybox_program_);
            skybox_program_ = 0;
            return false;
        }

        skybox_uViewProj_ = glGetUniformLocation(skybox_program_, "uViewProj");
        skybox_uSunDir_ = glGetUniformLocation(skybox_program_, "uSunDir");
        skybox_uSunColor_ = glGetUniformLocation(skybox_program_, "uSunColor");
        skybox_uZenithColor_ = glGetUniformLocation(skybox_program_, "uZenithColor");
        skybox_uHorizonColor_ = glGetUniformLocation(skybox_program_, "uHorizonColor");
        skybox_uTime_ = glGetUniformLocation(skybox_program_, "uTime");
        skybox_uCubemap_ = glGetUniformLocation(skybox_program_, "uCubemap");
        skybox_uHasCubemap_ = glGetUniformLocation(skybox_program_, "uHasCubemap");
        return true;
    }

    // ── Post-Processing Program (tone mapping + vignette + color grade) ─────
    bool compile_post_program() {
        GLuint vert = compile_shader(GL_VERTEX_SHADER, kPostVertexSrc);
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, kPostFragmentSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        post_program_ = glCreateProgram();
        glAttachShader(post_program_, vert);
        glAttachShader(post_program_, frag);
        glLinkProgram(post_program_);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(post_program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(post_program_, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
            glGetProgramInfoLog(post_program_, len, nullptr, log.data());
            LOGE("Post Program link failed: %s", log.data());
            glDeleteProgram(post_program_);
            post_program_ = 0;
            return false;
        }

        post_uScene_ = glGetUniformLocation(post_program_, "uScene");
        post_uExposure_ = glGetUniformLocation(post_program_, "uExposure");
        post_uVignette_ = glGetUniformLocation(post_program_, "uVignette");
        post_uSaturation_ = glGetUniformLocation(post_program_, "uSaturation");
        post_uWarmth_ = glGetUniformLocation(post_program_, "uWarmth");
        return true;
    }

    // ── Cria FBO para render-to-texture (post-processing) ───────────────────
    bool create_post_fbo() {
        if (post_fbo_ != 0) return true;

        glGenFramebuffers(1, &post_fbo_);
        glGenTextures(1, &post_scene_tex_);
        glGenRenderbuffers(1, &post_depth_rbo_);

        glBindTexture(GL_TEXTURE_2D, post_scene_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindRenderbuffer(GL_RENDERBUFFER, post_depth_rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width_, height_);

        glBindFramebuffer(GL_FRAMEBUFFER, post_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, post_scene_tex_, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, post_depth_rbo_);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("Post FBO incomplete: 0x%x", status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &post_fbo_); post_fbo_ = 0;
            glDeleteTextures(1, &post_scene_tex_); post_scene_tex_ = 0;
            glDeleteRenderbuffers(1, &post_depth_rbo_); post_depth_rbo_ = 0;
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOGI("Post-processing FBO created (%dx%d)", width_, height_);
        return true;
    }

    void destroy_post_fbo() {
        if (post_fbo_) { glDeleteFramebuffers(1, &post_fbo_); post_fbo_ = 0; }
        if (post_scene_tex_) { glDeleteTextures(1, &post_scene_tex_); post_scene_tex_ = 0; }
        if (post_depth_rbo_) { glDeleteRenderbuffers(1, &post_depth_rbo_); post_depth_rbo_ = 0; }
    }

    // ── Cria geometria do skybox (cubo invertido) ───────────────────────────
    void create_skybox_geometry() {
        if (skybox_vao_ != 0) return;

        const float R = 1.0f;
        struct Face { glm::vec3 v[4]; };
        const Face faces[6] = {
            {{{-R, R,-R},{ R, R,-R},{ R, R, R},{-R, R, R}}}, // UP
            {{{-R,-R, R},{ R,-R, R},{ R,-R,-R},{-R,-R,-R}}}, // DOWN
            {{{-R,-R, R},{ R,-R, R},{ R, R, R},{-R, R, R}}}, // FRONT (+Z)
            {{{ R,-R,-R},{-R,-R,-R},{-R, R,-R},{ R, R,-R}}}, // BACK (-Z)
            {{{-R,-R,-R},{-R,-R, R},{-R, R, R},{-R, R,-R}}}, // LEFT (-X)
            {{{ R,-R, R},{ R,-R,-R},{ R, R,-R},{ R, R, R}}}, // RIGHT (+X)
        };

        std::vector<glm::vec3> verts;
        std::vector<uint32_t> indices;
        verts.reserve(24);
        indices.reserve(36);

        for (const auto& f : faces) {
            uint32_t base = static_cast<uint32_t>(verts.size());
            for (int i = 0; i < 4; ++i) verts.push_back(f.v[i]);
            indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
            indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
        }

        skybox_index_count_ = static_cast<uint32_t>(indices.size());

        glGenVertexArrays(1, &skybox_vao_);
        glGenBuffers(1, &skybox_vbo_);
        glGenBuffers(1, &skybox_ebo_);

        glBindVertexArray(skybox_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skybox_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // ── Post-processing quad (fullscreen triangle strip) ─────────────────────
    void create_post_quad() {
        if (post_vao_ != 0) return;
        glGenVertexArrays(1, &post_vao_);
        glGenBuffers(1, &post_vbo_);
        glBindVertexArray(post_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, post_vbo_);
        const float quad[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void draw_skybox(const TextureHandle faces[6], float radius) override {
        (void)faces; (void)radius;
        // Fallback: usa cubemap interno se já foi criado, senão sky procedural
        if (skybox_cubemap_ != 0) {
            draw_skybox_cubemap(static_cast<TextureHandle>(skybox_cubemap_), radius);
        } else if (skybox_program_ != 0) {
            // Sky procedural (sem cubemap)
            if (skybox_vao_ == 0) create_skybox_geometry();

            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glUseProgram(skybox_program_);

            glm::mat4 view_no_translate = glm::mat4(glm::mat3(camera_.view_matrix()));
            glm::mat4 view_proj = camera_.projection_matrix() * view_no_translate;

            glUniformMatrix4fv(skybox_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
            glUniform3fv(skybox_uSunDir_, 1, &light_.direction[0]);
            glUniform3fv(skybox_uSunColor_, 1, &light_.color[0]);

            glm::vec3 zenith(0.18f, 0.38f, 0.72f);
            glm::vec3 horizon(0.72f, 0.80f, 0.90f);
            glUniform3fv(skybox_uZenithColor_, 1, &zenith[0]);
            glUniform3fv(skybox_uHorizonColor_, 1, &horizon[0]);
            glUniform1f(skybox_uTime_, frame_time_);
            glUniform1i(skybox_uHasCubemap_, 0);

            glBindVertexArray(skybox_vao_);
            glDrawElements(GL_TRIANGLES, skybox_index_count_, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);

            glUseProgram(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_CULL_FACE);
        }
    }

    void draw_skybox_cubemap(TextureHandle cubemap, float radius) override {
        (void)radius;
        if (skybox_program_ == 0) return;
        if (skybox_vao_ == 0) create_skybox_geometry();

        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glUseProgram(skybox_program_);

        glm::mat4 view_no_translate = glm::mat4(glm::mat3(camera_.view_matrix()));
        glm::mat4 view_proj = camera_.projection_matrix() * view_no_translate;

        glUniformMatrix4fv(skybox_uViewProj_, 1, GL_FALSE, &view_proj[0][0]);
        glUniform3fv(skybox_uSunDir_, 1, &light_.direction[0]);
        glUniform3fv(skybox_uSunColor_, 1, &light_.color[0]);

        glm::vec3 zenith(0.18f, 0.38f, 0.72f);
        glm::vec3 horizon(0.72f, 0.80f, 0.90f);
        glUniform3fv(skybox_uZenithColor_, 1, &zenith[0]);
        glUniform3fv(skybox_uHorizonColor_, 1, &horizon[0]);
        glUniform1f(skybox_uTime_, frame_time_);

        // Bind cubemap
        GLuint cubemap_gl = static_cast<GLuint>(cubemap);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_gl);
        glUniform1i(skybox_uCubemap_, 0);
        glUniform1i(skybox_uHasCubemap_, 1);

        glBindVertexArray(skybox_vao_);
        glDrawElements(GL_TRIANGLES, skybox_index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glUseProgram(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
    }

    // Configura parâmetros de post-processing
    void set_post_params(float exposure, float vignette, float saturation, float warmth) {
        post_exposure_ = exposure;
        post_vignette_ = vignette;
        post_saturation_ = saturation;
        post_warmth_ = warmth;
        post_enabled_ = (vignette > 0.0f || saturation != 1.0f || warmth != 0.0f || exposure != 1.0f);
    }

    void create_quad() {
        const float quad[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &quad_vao_);
        glGenBuffers(1, &quad_vbo_);
        glBindVertexArray(quad_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    TextureHandle create_cubemap_from_rgba(
        const std::array<std::vector<uint8_t>, 6>& faces_rgba,
        uint32_t width, uint32_t height) override
    {
        if (width == 0 || height == 0) return INVALID_TEXTURE;

        GLuint cubemap = 0;
        glGenTextures(1, &cubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

        // Ordem das faces: UP, DOWN, FRONT, BACK, LEFT, RIGHT
        // OpenGL: POSITIVE_Y=UP, NEGATIVE_Y=DOWN, POSITIVE_Z=FRONT, NEGATIVE_Z=BACK, NEGATIVE_X=LEFT, POSITIVE_X=RIGHT
        static const GLenum cubemap_targets[6] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,   // 0: UP
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,   // 1: DOWN
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,   // 2: FRONT
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,   // 3: BACK
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,   // 4: LEFT
            GL_TEXTURE_CUBE_MAP_POSITIVE_X    // 5: RIGHT
        };

        for (int i = 0; i < 6; ++i) {
            const auto& rgba = faces_rgba[i];
            if (rgba.empty() || rgba.size() != width * height * 4) {
                glDeleteTextures(1, &cubemap);
                return INVALID_TEXTURE;
            }
            glTexImage2D(cubemap_targets[i], 0, GL_RGBA,
                         width, height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        LOGI("Created cubemap texture from 6 RGBA faces (%ux%u)", width, height);
        return static_cast<TextureHandle>(cubemap);
    }

    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    EGLConfig egl_config_ = nullptr;

    // 2D Quad
    GLuint program_ = 0;
    GLuint quad_vao_ = 0;
    GLuint quad_vbo_ = 0;
    GLint scale_loc_ = -1;
    GLint offset_loc_ = -1;
    GLint uv_scale_loc_ = -1;
    GLint uv_offset_loc_ = -1;
    GLint color_loc_ = -1;
    GLint tex_loc_ = -1;

    // Font Atlas & Solid Color
    TextureHandle font_atlas_tex_ = INVALID_TEXTURE;
    TextureHandle white_tex_ = INVALID_TEXTURE;
    stbtt_bakedchar baked_chars_[96];
    bool font_atlas_loaded_ = false;
    AAssetManager* asset_mgr_ = nullptr;
    std::vector<uint8_t> font_memory_buf_;

    // 3D Mesh Program & Uniforms
    GLuint mesh_program_ = 0;
    GLint mesh_uModel_ = -1;
    GLint mesh_uViewProj_ = -1;
    GLint mesh_uNormalMat_ = -1;
    GLint mesh_uLightDir_ = -1;
    GLint mesh_uLightColor_ = -1;
    GLint mesh_uAmbient_ = -1;
    GLint mesh_uHasTex_ = -1;
    GLint mesh_uTex_ = -1;

    // Skinned Mesh Program & Uniforms
    GLuint skinned_mesh_program_ = 0;
    GLint skinned_mesh_uModel_ = -1;
    GLint skinned_mesh_uViewProj_ = -1;
    GLint skinned_mesh_uNormalMat_ = -1;
    GLint skinned_mesh_uBoneMatrices_ = -1;
    GLint skinned_mesh_uLightDir_ = -1;
    GLint skinned_mesh_uLightColor_ = -1;
    GLint skinned_mesh_uAmbient_ = -1;
    GLint skinned_mesh_uHasTex_ = -1;
    GLint skinned_mesh_uTex_ = -1;

    // 3D Terrain Program & Uniforms
    GLuint terrain_program_ = 0;
    GLint terrain_uModel_ = -1;
    GLint terrain_uViewProj_ = -1;
    GLint terrain_uNormalMat_ = -1;
    GLint terrain_uLightDir_ = -1;
    GLint terrain_uLightColor_ = -1;
    GLint terrain_uAmbient_ = -1;
    GLint terrain_uHasBaseTex_ = -1;
    GLint terrain_uHasBlendTex_ = -1;
    GLint terrain_uHasBankTex_ = -1;
    GLint terrain_uHasOuterBankTex_ = -1;
    GLint terrain_uBaseTex_ = -1;
    GLint terrain_uBlendTex_ = -1;
    GLint terrain_uBankTex_ = -1;
    GLint terrain_uOuterBankTex_ = -1;

    // Water Plane Program & Uniforms
    GLuint water_program_ = 0;
    GLint water_uModel_ = -1;
    GLint water_uViewProj_ = -1;
    GLint water_uCameraPos_ = -1;
    GLint water_uLightDir_ = -1;
    GLint water_uLightColor_ = -1;
    GLint water_uAmbient_ = -1;
    GLint water_uTime_ = -1;
    GLint water_uHasNormalMap_ = -1;
    GLint water_uNormalMap_ = -1;
    GLint water_uWaterColor_ = -1;
    GLint water_uWaterLevel_ = -1;
    GLint water_uTerrainHeight_ = -1;
    GLint water_uHasWaterTex_ = -1;
    GLint water_uWaterTex_ = -1;
    GLuint water_vao_ = 0;
    GLuint water_vbo_ = 0;
    GLuint water_ebo_ = 0;
    uint32_t water_index_count_ = 0;

    // 3D Line Program & Buffers
    GLuint line_program_ = 0;
    GLint line_uViewProj_ = -1;
    GLint line_uColor_ = -1;
    GLuint line_vao_ = 0;
    GLuint line_vbo_ = 0;

    // Billboard (Particle) Program & Buffers
    GLuint billboard_program_ = 0;
    GLint billboard_uViewProj_ = -1;
    GLint billboard_uCameraPos_ = -1;
    GLint billboard_uBillboardPos_ = -1;
    GLint billboard_uBillboardSize_ = -1;
    GLint billboard_uHasTex_ = -1;
    GLint billboard_uTex_ = -1;
    GLint billboard_uColor_ = -1;
    GLuint billboard_vao_ = 0;
    GLuint billboard_vbo_ = 0;

    // Skybox Program, Uniforms & Geometry
    GLuint skybox_program_ = 0;
    GLint skybox_uViewProj_ = -1;
    GLint skybox_uSunDir_ = -1;
    GLint skybox_uSunColor_ = -1;
    GLint skybox_uZenithColor_ = -1;
    GLint skybox_uHorizonColor_ = -1;
    GLint skybox_uTime_ = -1;
    GLint skybox_uCubemap_ = -1;
    GLint skybox_uHasCubemap_ = -1;
    GLuint skybox_vao_ = 0;
    GLuint skybox_vbo_ = 0;
    GLuint skybox_ebo_ = 0;
    uint32_t skybox_index_count_ = 0;
    GLuint skybox_cubemap_ = 0;  // GL texture ID for cubemap

    // Post-processing Program, Uniforms, FBO & Geometry
    GLuint post_program_ = 0;
    GLint post_uScene_ = -1;
    GLint post_uExposure_ = -1;
    GLint post_uVignette_ = -1;
    GLint post_uSaturation_ = -1;
    GLint post_uWarmth_ = -1;
    GLuint post_vao_ = 0;
    GLuint post_vbo_ = 0;
    GLuint post_fbo_ = 0;
    GLuint post_scene_tex_ = 0;
    GLuint post_depth_rbo_ = 0;
    bool post_enabled_ = true;
    float post_exposure_ = 1.0f;
    float post_vignette_ = 0.15f;
    float post_saturation_ = 1.1f;
    float post_warmth_ = 0.05f;

    // Skybox animation time
    float frame_time_ = 0.0f;

    Camera camera_;
    DirectionalLight light_;

    struct MeshData {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        uint32_t index_count = 0;
        uint32_t vertex_count = 0;  ///< Needed for partial VBO updates
        bool dynamic = false;        ///< true = GL_DYNAMIC_DRAW (morph targets)
    };
    std::unordered_map<MeshHandle, MeshData> meshes_;
    MeshHandle next_mesh_id_ = 1;

    int width_ = 0;
    int height_ = 0;
    float fps_ = 0.0f;
};

std::unique_ptr<IRenderer> create_gles_renderer() {
    return std::make_unique<GLESRenderer>();
}

} // namespace aom
