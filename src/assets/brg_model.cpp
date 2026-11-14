#include "assets/brg_model.h"
#include "assets/bar_archive.h"
#include "renderer/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#define LOG_TAG "AoM-BRG"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

inline float half_to_float(uint16_t h) {
    uint32_t s = (h & 0x8000) << 16;
    int32_t e = (h >> 10) & 0x1f;
    uint32_t m = h & 0x03ff;
    if (e == 0) {
        if (m == 0) return (s ? -0.0f : 0.0f);
        while (!(m & 0x0400)) { m <<= 1; e--; }
        e++;
        m &= ~0x0400;
    } else if (e == 31) {
        e = 255;
    } else {
        e += (127 - 15);
    }
    uint32_t val = s | (e << 23) | (m << 13);
    float f = 0.0f;
    std::memcpy(&f, &val, 4);
    return f;
}

uint16_t read_u16(const uint8_t* p) {
    uint16_t v = 0;
    std::memcpy(&v, p, 2);
    return v;
}

uint32_t read_u32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

float read_f32(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

void compute_normals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (auto& v : vertices) {
        v.normal = glm::vec3(0.0f);
    }
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if (i0 < vertices.size() && i1 < vertices.size() && i2 < vertices.size()) {
            glm::vec3 p0 = vertices[i0].position;
            glm::vec3 p1 = vertices[i1].position;
            glm::vec3 p2 = vertices[i2].position;
            glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
            if (glm::length(fn) > 1e-6f) {
                fn = glm::normalize(fn);
                vertices[i0].normal += fn;
                vertices[i1].normal += fn;
                vertices[i2].normal += fn;
            }
        }
    }
    for (auto& v : vertices) {
        if (glm::length(v.normal) > 1e-6f) {
            v.normal = glm::normalize(v.normal);
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

} // anonymous namespace

std::unique_ptr<BrgModel> parse_brg(const uint8_t* data, size_t size, const std::string& name) {
    if (!data || size < 28) {
        LOGE("BRG: File too small for BANG header\n");
        return nullptr;
    }

    if (std::memcmp(data, "BANG", 4) != 0) {
        LOGE("BRG: Invalid magic, expected BANG\n");
        return nullptr;
    }

    auto model = std::make_unique<BrgModel>();
    model->name = name;

    size_t pos = 28; // Skip BANG header
    uint32_t mesh_index = 0;

    while (pos + 4 <= size) {
        const uint8_t* tag = data + pos;
        pos += 4;

        if (std::memcmp(tag, "ASET", 4) == 0) {
            if (pos + 28 <= size) {
                pos += 28; // Skip ASET animation set header
            }
        } else if (std::memcmp(tag, "MESI", 4) == 0) {
            if (pos + 80 > size) break;
            const uint8_t* hdr = data + pos;
            pos += 80;

            uint16_t version = read_u16(hdr + 0);
            uint16_t vert_stride = read_u16(hdr + 2);
            uint16_t num_verts = read_u16(hdr + 4);
            uint16_t num_faces = read_u16(hdr + 6);
            uint16_t flags = read_u16(hdr + 54);

            (void)version;
            (void)flags;

            if (num_verts > 0 && num_faces > 0) {
                BrgMesh mesh;
                mesh.name = name + "_mesh_" + std::to_string(mesh_index++);
                mesh.vertices.resize(num_verts);

                // 1. Read vertex positions (AoM BRG models use 12-byte float32 x,y,z)
                if (pos + num_verts * 12 <= size) {
                    for (uint16_t v = 0; v < num_verts; ++v) {
                        mesh.vertices[v].position.x = read_f32(data + pos + v * 12 + 0);
                        mesh.vertices[v].position.y = read_f32(data + pos + v * 12 + 4);
                        mesh.vertices[v].position.z = read_f32(data + pos + v * 12 + 8);
                        mesh.vertices[v].color = glm::vec4(1.0f);
                    }
                    pos += num_verts * 12;
                } else if (pos + num_verts * 6 <= size) {
                    for (uint16_t v = 0; v < num_verts; ++v) {
                        uint16_t hx = read_u16(data + pos + v * 6 + 0);
                        uint16_t hy = read_u16(data + pos + v * 6 + 2);
                        uint16_t hz = read_u16(data + pos + v * 6 + 4);
                        mesh.vertices[v].position = glm::vec3(
                            half_to_float(hx), half_to_float(hy), half_to_float(hz));
                        mesh.vertices[v].color = glm::vec4(1.0f);
                    }
                    pos += num_verts * 6;
                }

                // 2. Read texture coordinates (UVs)
                if (pos + num_verts * 4 <= size) {
                    for (uint16_t v = 0; v < num_verts; ++v) {
                        uint16_t hu = read_u16(data + pos + v * 4 + 0);
                        uint16_t hv = read_u16(data + pos + v * 4 + 2);
                        mesh.vertices[v].texcoord = glm::vec2(
                            half_to_float(hu), half_to_float(hv));
                    }
                    pos += num_verts * 4;
                }

                // 3. Skip face materials
                if (pos + num_faces * 2 <= size) {
                    pos += num_faces * 2;
                }

                // 4. Read face triangle indices
                if (pos + num_faces * 6 <= size) {
                    mesh.indices.resize(num_faces * 3);
                    for (uint32_t f = 0; f < num_faces * 3; ++f) {
                        mesh.indices[f] = read_u16(data + pos + f * 2);
                    }
                    pos += num_faces * 6;
                }

                // 5. Generate smooth normals
                compute_normals(mesh.vertices, mesh.indices);

                model->total_vertices += num_verts;
                model->total_faces += num_faces;
                model->meshes.push_back(std::move(mesh));

                // Assign pending texture name from earlier MTRL chunk to new mesh
                if (!model->pending_texture_name.empty() &&
                    model->meshes.back().texture_name.empty()) {
                    model->meshes.back().texture_name = model->pending_texture_name;
                    // Keep pending for any additional meshes (MTRL applies to all)
                }
            }
        } else if (std::memcmp(tag, "MTRL", 4) == 0) {
            size_t next_tag = size;
            for (const char* next : {"MESI", "ASET", "MTRL", "ATTR"}) {
                for (size_t s = pos; s + 4 <= size; ++s) {
                    if (std::memcmp(data + s, next, 4) == 0) {
                        if (s < next_tag) next_tag = s;
                        break;
                    }
                }
            }

            // Look for ASCII texture name
            std::string tex_name;
            for (size_t i = pos; i + 3 < next_tag; ++i) {
                if (data[i] >= 32 && data[i] < 127) {
                    size_t len = 0;
                    while (i + len < next_tag && data[i + len] >= 32 && data[i + len] < 127) {
                        len++;
                    }
                    if (len >= 4) {
                        tex_name = std::string(reinterpret_cast<const char*>(data + i), len);
                        break;
                    }
                }
            }
            if (!tex_name.empty()) {
                model->textures.push_back(tex_name);
                // Assign to ALL existing meshes that don't have texture yet
                bool assigned = false;
                for (auto& mesh : model->meshes) {
                    if (mesh.texture_name.empty()) {
                        mesh.texture_name = tex_name;
                        assigned = true;
                    }
                }
                // Also store for any future meshes
                if (!assigned) {
                    model->pending_texture_name = tex_name;
                }
            }
            pos = next_tag;
        } else {
            // Unknown chunk, advance until next valid tag
            size_t next_tag = size;
            for (const char* next : {"MESI", "ASET", "MTRL", "ATTR"}) {
                for (size_t s = pos; s + 4 <= size; ++s) {
                    if (std::memcmp(data + s, next, 4) == 0) {
                        if (s < next_tag) next_tag = s;
                        break;
                    }
                }
            }
            pos = next_tag;
        }
    }

    LOGI("BRG: Loaded model '%s' with %zu meshes, %u vertices, %u faces",
         name.c_str(), model->meshes.size(), model->total_vertices, model->total_faces);
    for (size_t i = 0; i < model->meshes.size(); ++i) {
        const auto& m = model->meshes[i];
        LOGI("  Mesh[%zu]: '%s' verts=%u faces=%u texture='%s'",
             i, m.name.c_str(), m.vertices.size(), m.indices.size() / 3, m.texture_name.c_str());
    }
    return model;
}

std::unique_ptr<BrgModel> load_brg_model_data(BarArchive* archive, const std::string& name) {
    if (!archive) return nullptr;
    const BarEntry* entry = archive->find(name);
    if (!entry) {
        LOGE("BRG: Entry '%s' not found in archive\n", name.c_str());
        return nullptr;
    }
    std::vector<uint8_t> raw = archive->extract(*entry);
    if (raw.empty()) {
        LOGE("BRG: Failed to extract '%s'\n", name.c_str());
        return nullptr;
    }
    return parse_brg(raw.data(), raw.size(), name);
}

MeshHandle load_brg_model(BarArchive* archive, const std::string& name, IRenderer* renderer) {
    if (!renderer) return INVALID_MESH;
    auto model = load_brg_model_data(archive, name);
    if (!model || model->meshes.empty()) {
        LOGE("BRG: Failed to parse '%s', falling back to procedural 3D model\n", name.c_str());
        auto fallback = create_procedural_brg();
        if (fallback && !fallback->meshes.empty()) {
            const auto& m = fallback->meshes[0];
            return renderer->create_mesh(
                m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                m.indices.data(), static_cast<uint32_t>(m.indices.size()));
        }
        return INVALID_MESH;
    }

    const auto& m = model->meshes[0];
    return renderer->create_mesh(
        m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
        m.indices.data(), static_cast<uint32_t>(m.indices.size()));
}

std::unique_ptr<BrgModel> create_procedural_brg() {
    auto model = std::make_unique<BrgModel>();
    model->name = "Greek_Pyramid_Procedural";

    BrgMesh mesh;
    mesh.name = "Pyramid_Mesh";

    // 3D Pyramid geometry: Apex and 4 base corners
    const glm::vec3 apex(0.0f, 1.4f, 0.0f);
    const glm::vec3 b0(-1.0f, -0.6f,  1.0f);
    const glm::vec3 b1( 1.0f, -0.6f,  1.0f);
    const glm::vec3 b2( 1.0f, -0.6f, -1.0f);
    const glm::vec3 b3(-1.0f, -0.6f, -1.0f);

    const glm::vec4 gold(0.83f, 0.68f, 0.21f, 1.0f);
    const glm::vec4 royal_blue(0.12f, 0.28f, 0.55f, 1.0f);

    // Front face (apex, b0, b1)
    mesh.vertices.push_back({apex, glm::vec3(0,0,0), glm::vec2(0.5f, 1.0f), gold});
    mesh.vertices.push_back({b0,   glm::vec3(0,0,0), glm::vec2(0.0f, 0.0f), royal_blue});
    mesh.vertices.push_back({b1,   glm::vec3(0,0,0), glm::vec2(1.0f, 0.0f), royal_blue});
    mesh.indices.push_back(0); mesh.indices.push_back(1); mesh.indices.push_back(2);

    // Right face (apex, b1, b2)
    mesh.vertices.push_back({apex, glm::vec3(0,0,0), glm::vec2(0.5f, 1.0f), gold});
    mesh.vertices.push_back({b1,   glm::vec3(0,0,0), glm::vec2(0.0f, 0.0f), royal_blue});
    mesh.vertices.push_back({b2,   glm::vec3(0,0,0), glm::vec2(1.0f, 0.0f), royal_blue});
    mesh.indices.push_back(3); mesh.indices.push_back(4); mesh.indices.push_back(5);

    // Back face (apex, b2, b3)
    mesh.vertices.push_back({apex, glm::vec3(0,0,0), glm::vec2(0.5f, 1.0f), gold});
    mesh.vertices.push_back({b2,   glm::vec3(0,0,0), glm::vec2(0.0f, 0.0f), royal_blue});
    mesh.vertices.push_back({b3,   glm::vec3(0,0,0), glm::vec2(1.0f, 0.0f), royal_blue});
    mesh.indices.push_back(6); mesh.indices.push_back(7); mesh.indices.push_back(8);

    // Left face (apex, b3, b0)
    mesh.vertices.push_back({apex, glm::vec3(0,0,0), glm::vec2(0.5f, 1.0f), gold});
    mesh.vertices.push_back({b3,   glm::vec3(0,0,0), glm::vec2(0.0f, 0.0f), royal_blue});
    mesh.vertices.push_back({b0,   glm::vec3(0,0,0), glm::vec2(1.0f, 0.0f), royal_blue});
    mesh.indices.push_back(9); mesh.indices.push_back(10); mesh.indices.push_back(11);

    // Base face (b0, b2, b1 and b0, b3, b2)
    mesh.vertices.push_back({b0, glm::vec3(0,0,0), glm::vec2(0,0), gold});
    mesh.vertices.push_back({b2, glm::vec3(0,0,0), glm::vec2(1,1), gold});
    mesh.vertices.push_back({b1, glm::vec3(0,0,0), glm::vec2(1,0), gold});
    mesh.indices.push_back(12); mesh.indices.push_back(13); mesh.indices.push_back(14);

    mesh.vertices.push_back({b0, glm::vec3(0,0,0), glm::vec2(0,0), gold});
    mesh.vertices.push_back({b3, glm::vec3(0,0,0), glm::vec2(0,1), gold});
    mesh.vertices.push_back({b2, glm::vec3(0,0,0), glm::vec2(1,1), gold});
    mesh.indices.push_back(15); mesh.indices.push_back(16); mesh.indices.push_back(17);

    compute_normals(mesh.vertices, mesh.indices);

    model->total_vertices = static_cast<uint32_t>(mesh.vertices.size());
    model->total_faces = static_cast<uint32_t>(mesh.indices.size() / 3);
    model->meshes.push_back(std::move(mesh));

    return model;
}

} // namespace aom
