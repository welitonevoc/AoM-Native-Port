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

#define LOG_TAG "AoM-GLES"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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

// 3D Mesh Shaders with Lighting and Vertex Colors
const char* kMeshVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;

out vec3 vNormal;
out vec2 vTex;
out vec4 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vNormal = normalize(uNormalMat * aNormal);
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

// Water Plane Shaders with Dual UV Waves, Fresnel Gradient and Specular Highlights
const char* kWaterVertexSrc = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform highp float uTime;

out vec3 vWorldPos;
out vec2 vTex;
out vec4 vColor;

void main() {
    float wave = sin(aPos.x * 0.75 + uTime * 1.8) * cos(aPos.z * 0.75 + uTime * 1.4) * 0.06;
    vec3 animatedPos = aPos + vec3(0.0, wave, 0.0);
    vec4 worldPos = uModel * vec4(animatedPos, 1.0);
    vWorldPos = worldPos.xyz;
    vTex = aTex;
    vColor = aColor;
    gl_Position = uViewProj * worldPos;
}
)";

const char* kWaterFragmentSrc = R"(#version 300 es
precision mediump float;

in vec3 vWorldPos;
in vec2 vTex;
in vec4 vColor;

uniform sampler2D uNormalMap;
uniform int uHasNormalMap;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform highp float uTime;

out vec4 fragColor;

const vec4 kShallowColor = vec4(0.08, 0.48, 0.58, 0.72);
const vec4 kDeepColor    = vec4(0.02, 0.14, 0.34, 0.90);

void main() {
    // Coordenadas UV de scrolling simplificadas e fundidas
    vec2 t_scroll = vec2(uTime * 0.02, uTime * 0.02);
    vec2 uv1 = vTex * 3.0 + t_scroll;
    vec2 uv2 = vTex * 4.5 + vec2(-t_scroll.x * 0.85, t_scroll.y * 1.35);

    vec3 waveNormal;
    if (uHasNormalMap != 0) {
        vec3 n1 = texture(uNormalMap, uv1).rgb * 2.0 - 1.0;
        vec3 n2 = texture(uNormalMap, uv2).rgb * 2.0 - 1.0;
        waveNormal = normalize(vec3(n1.xy + n2.xy, (n1.z + n2.z) * 0.5));
    } else {
        float nx = sin(vWorldPos.x * 1.8 + uTime * 2.2) * 0.12 + cos(vWorldPos.z * 2.4 + uTime * 1.6) * 0.08;
        float nz = cos(vWorldPos.x * 2.2 + uTime * 1.5) * 0.12 + sin(vWorldPos.z * 1.9 + uTime * 2.0) * 0.08;
        waveNormal = normalize(vec3(nx, 1.0, nz));
    }

    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    // Fresnel otimizado via multiplicacao direta (sem pow transcendental cara)
    float oneMinusNdotV = 1.0 - clamp(dot(waveNormal, V), 0.0, 1.0);
    float fresnel = oneMinusNdotV * oneMinusNdotV * oneMinusNdotV;

    vec4 waterBase = mix(kShallowColor, kDeepColor, clamp(0.5 + fresnel * 0.5, 0.0, 1.0));

    // Especular otimizado: pow(NdotH, 32.0) em potencia de 2 reduz ciclos de instrucao ALU
    float NdotH = max(dot(waveNormal, H), 0.0);
    float spec = pow(NdotH, 32.0);
    vec3 specular = uLightColor * (spec * 0.85);

    float diff = max(dot(waveNormal, L), 0.0);
    vec3 diffuse = uLightColor * (diff * 0.35);

    vec3 finalRgb = waterBase.rgb * (uAmbient + diffuse) + specular;
    fragColor = vec4(finalRgb, waterBase.a);
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

        if (!compile_program() || !compile_mesh_program() ||
            !compile_terrain_program() || !compile_water_program() ||
            !compile_line_program()) {
            shutdown();
            return false;
        }
        create_quad();

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
        if (terrain_program_) { glDeleteProgram(terrain_program_); terrain_program_ = 0; }
        if (water_program_) { glDeleteProgram(water_program_); water_program_ = 0; }
        if (water_vao_) { glDeleteVertexArrays(1, &water_vao_); water_vao_ = 0; }
        if (water_vbo_) { glDeleteBuffers(1, &water_vbo_); water_vbo_ = 0; }
        if (water_ebo_) { glDeleteBuffers(1, &water_ebo_); water_ebo_ = 0; }
        water_index_count_ = 0;

        if (line_program_) { glDeleteProgram(line_program_); line_program_ = 0; }
        if (line_vao_) { glDeleteVertexArrays(1, &line_vao_); line_vao_ = 0; }
        if (line_vbo_) { glDeleteBuffers(1, &line_vbo_); line_vbo_ = 0; }

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
    }

    void begin_frame() override {
        glClearColor(0.08f, 0.14f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void end_frame() override {
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
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

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

    void draw_water(const glm::vec4& water_plane, TextureHandle normal_map, float time) override {
        if (!water_program_) return;
        if (water_vao_ == 0) create_water_grid();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
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

        if (normal_map != INVALID_TEXTURE) {
            glUniform1i(water_uHasNormalMap_, 1);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(normal_map));
            glUniform1i(water_uNormalMap_, 0);
        } else {
            glUniform1i(water_uHasNormalMap_, 0);
        }

        glBindVertexArray(water_vao_);
        glDrawElements(GL_TRIANGLES, water_index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (normal_map != INVALID_TEXTURE) {
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

        glDisable(GL_DEPTH_TEST);
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
        if (color == 0xFFFFFFFF || color == 0) { cr = cg = cb = ca = 1.0f; }
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glUniform1i(tex_loc_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
        glBindVertexArray(quad_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
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
        if (color == 0xFFFFFFFF || color == 0) { cr = cg = cb = ca = 1.0f; }
        glUniform4f(color_loc_, cr, cg, cb, ca);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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

        for (char c : text) {
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
