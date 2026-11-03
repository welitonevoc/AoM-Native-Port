#include "anim/animation_system.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace aom {
namespace anim {

// ----------------------------------------------------------------------------
// Helpers for binary BRG parsing
// ----------------------------------------------------------------------------
namespace {

inline uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline float read_f32(const uint8_t* p) {
    float val;
    std::memcpy(&val, p, sizeof(float));
    return val;
}

inline float half_to_float(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x00000001;
    uint32_t exp  = (h >> 10) & 0x0000001f;
    uint32_t mant = h & 0x000003ff;

    if (exp == 0) {
        if (mant == 0) {
            uint32_t res = sign << 31;
            float f;
            std::memcpy(&f, &res, sizeof(float));
            return f;
        }
        while (!(mant & 0x00000400)) {
            mant <<= 1;
            exp -= 1;
        }
        exp++;
        mant &= ~0x00000400;
    } else if (exp == 31) {
        uint32_t res = (sign << 31) | 0x7f800000 | (mant << 13);
        float f;
        std::memcpy(&f, &res, sizeof(float));
        return f;
    }

    exp = exp + (127 - 15);
    mant = mant << 13;
    uint32_t res = (sign << 31) | (exp << 23) | mant;
    float f;
    std::memcpy(&f, &res, sizeof(float));
    return f;
}

void compute_mesh_normals(const std::vector<glm::vec3>& positions,
                          const std::vector<uint32_t>& indices,
                          std::vector<glm::vec3>& out_normals) {
    out_normals.assign(positions.size(), glm::vec3(0.0f));
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if (i0 < positions.size() && i1 < positions.size() && i2 < positions.size()) {
            glm::vec3 p0 = positions[i0];
            glm::vec3 p1 = positions[i1];
            glm::vec3 p2 = positions[i2];
            glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
            out_normals[i0] += fn;
            out_normals[i1] += fn;
            out_normals[i2] += fn;
        }
    }
    for (auto& n : out_normals) {
        if (glm::length(n) > 1e-6f) {
            n = glm::normalize(n);
        } else {
            n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// AnimScriptParser
// ----------------------------------------------------------------------------
bool AnimScriptParser::parse_file(const std::string& filepath, AnimScript& out_script) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), out_script);
}

bool AnimScriptParser::parse(const std::string& script_text, AnimScript& out_script) {
    std::istringstream stream(script_text);
    std::string line;
    std::string current_block; // "define" or "anim"
    std::string current_name;
    AnimStateDef current_state_def;
    std::vector<AnimVisual> current_def_visuals;

    while (std::getline(stream, line)) {
        // Strip line comments
        auto comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);
        if (line.empty()) continue;

        std::istringstream ls(line);
        std::string token;
        ls >> token;

        if (token == "import") {
            std::string imp;
            ls >> imp;
            if (!imp.empty()) out_script.imports.push_back(imp);
        } else if (token == "define") {
            ls >> current_name;
            current_block = "define";
            current_def_visuals.clear();
        } else if (token == "anim") {
            ls >> current_name;
            current_block = "anim";
            current_state_def = AnimStateDef{};
            current_state_def.name = current_name;
        } else if (token == "Visual" || token == "VisualGranny" || token == "VisualParticle") {
            std::string vname;
            std::getline(ls, vname);
            size_t vf = vname.find_first_not_of(" \t\r\n");
            if (vf != std::string::npos) {
                size_t vl = vname.find_last_not_of(" \t\r\n");
                vname = vname.substr(vf, vl - vf + 1);
            }
            if (!vname.empty()) {
                AnimVisual vis{token, vname};
                if (current_block == "define") {
                    current_def_visuals.push_back(vis);
                    out_script.definitions[current_name] = current_def_visuals;
                } else if (current_block == "anim") {
                    current_state_def.visuals.push_back(vis);
                    std::string lower = current_name;
                    for (char& c : lower) c = static_cast<char>(::tolower(c));
                    out_script.states[lower] = current_state_def;
                }
            }
        } else if (token == "connect") {
            std::string hotspot, target;
            ls >> hotspot >> target;
            if (!hotspot.empty() && !target.empty() && current_block == "anim") {
                current_state_def.attachments.push_back({hotspot, target});
                std::string lower = current_name;
                for (char& c : lower) c = static_cast<char>(::tolower(c));
                out_script.states[lower] = current_state_def;
            }
        } else if (token == "tag") {
            std::string tag_name;
            float time_frac = 0.0f;
            std::string flag_str;
            ls >> tag_name >> time_frac >> flag_str;
            if (!tag_name.empty() && current_block == "anim") {
                bool flag = (flag_str == "true" || flag_str == "1");
                current_state_def.tags.push_back({tag_name, time_frac, flag});
                std::string lower = current_name;
                for (char& c : lower) c = static_cast<char>(::tolower(c));
                out_script.states[lower] = current_state_def;
            }
        }
    }
    return !out_script.states.empty();
}

// ----------------------------------------------------------------------------
// BrgAnimationClip
// ----------------------------------------------------------------------------
void BrgAnimationClip::evaluate(float time_sec,
                                std::vector<glm::vec3>& out_positions,
                                std::vector<glm::vec3>& out_normals) const {
    if (frames.empty() || num_vertices == 0) return;

    out_positions.resize(num_vertices);
    out_normals.resize(num_vertices);

    if (frames.size() == 1 || aset.duration <= 0.0f) {
        out_positions = frames[0].positions;
        out_normals = frames[0].normals;
        return;
    }

    // Wrap time inside clip duration
    float t = std::fmod(time_sec, aset.duration);
    if (t < 0.0f) t += aset.duration;

    float frame_exact = (t / aset.duration) * static_cast<float>(frames.size());
    size_t k = static_cast<size_t>(frame_exact) % frames.size();
    size_t k_next = (k + 1) % frames.size();
    float alpha = frame_exact - static_cast<float>(static_cast<size_t>(frame_exact));

    const auto& p0 = frames[k].positions;
    const auto& p1 = frames[k_next].positions;
    const auto& n0 = frames[k].normals;
    const auto& n1 = frames[k_next].normals;

    for (size_t v = 0; v < num_vertices; ++v) {
        out_positions[v] = glm::mix(p0[v], p1[v], alpha);
        out_normals[v] = glm::normalize(glm::mix(n0[v], n1[v], alpha));
    }
}

// ----------------------------------------------------------------------------
// UnitAnimationState
// ----------------------------------------------------------------------------
UnitAnimationState::UnitAnimationState() = default;

void UnitAnimationState::set_clip(const BrgAnimationClip* clip) {
    current_clip_ = clip;
    if (current_clip_ && current_clip_->duration() > 0.0f) {
        duration_ = current_clip_->duration();
    }
}

bool UnitAnimationState::play(const std::string& state_name, bool loop) {
    current_state_ = state_name;
    loop_ = loop;
    current_time_ = 0.0f;
    fired_tags_.clear();
    playing_ = true;

    if (script_) {
        current_state_def_ = script_->get_state(state_name);
    }

    if (current_clip_ && current_clip_->duration() > 0.0f) {
        duration_ = current_clip_->duration();
    } else {
        duration_ = 1.0f;
    }

    return true;
}

void UnitAnimationState::pause() { playing_ = false; }
void UnitAnimationState::resume() { playing_ = true; }
void UnitAnimationState::stop() {
    playing_ = false;
    current_time_ = 0.0f;
    fired_tags_.clear();
}

void UnitAnimationState::update(float dt, const std::function<void(const std::string& tag_name)>& on_tag) {
    if (!playing_ || duration_ <= 0.0f) return;

    float prev_time = current_time_;
    current_time_ += dt;

    // Check animation event tags
    if (current_state_def_) {
        for (const auto& tag : current_state_def_->tags) {
            float tag_time = tag.time_fraction * duration_;
            if (prev_time <= tag_time && current_time_ >= tag_time) {
                if (fired_tags_.find(tag.name) == fired_tags_.end()) {
                    fired_tags_.insert(tag.name);
                    if (on_tag) on_tag(tag.name);
                }
            }
        }
    }

    if (current_time_ >= duration_) {
        if (loop_) {
            current_time_ = std::fmod(current_time_, duration_);
            fired_tags_.clear();
        } else {
            current_time_ = duration_;
            playing_ = false;
        }
    }
}

// ----------------------------------------------------------------------------
// AnimationSystem
// ----------------------------------------------------------------------------
bool AnimationSystem::load_script_from_memory(const std::string& name, const std::string& text) {
    AnimScript script;
    script.name = name;
    if (AnimScriptParser::parse(text, script)) {
        scripts_[name] = std::move(script);
        return true;
    }
    return false;
}

bool AnimationSystem::load_brg_clip_from_memory(const std::string& name, const uint8_t* data, size_t size) {
    BrgAnimationClip clip;
    if (parse_brg_clip(data, size, name, clip)) {
        clips_[name] = std::move(clip);
        return true;
    }
    return false;
}

bool AnimationSystem::parse_brg_clip(const uint8_t* data, size_t size, const std::string& name, BrgAnimationClip& out_clip) {
    if (!data || size < 28) return false;
    if (std::memcmp(data, "BANG", 4) != 0) return false;

    out_clip.name = name;
    out_clip.frames.clear();
    out_clip.indices.clear();
    out_clip.uvs.clear();

    size_t pos = 28;

    while (pos + 4 <= size) {
        const uint8_t* tag = data + pos;
        pos += 4;

        if (std::memcmp(tag, "ASET", 4) == 0) {
            if (pos + 28 <= size) {
                out_clip.aset.num_frames = *reinterpret_cast<const uint32_t*>(data + pos + 0);
                out_clip.aset.frame_time = *reinterpret_cast<const float*>(data + pos + 4);
                out_clip.aset.anim_speed = *reinterpret_cast<const float*>(data + pos + 8);
                out_clip.aset.duration = *reinterpret_cast<const float*>(data + pos + 12);
                out_clip.aset.step_distance = *reinterpret_cast<const float*>(data + pos + 16);
                out_clip.aset.fps = *reinterpret_cast<const float*>(data + pos + 20);
                pos += 28;
            }
        } else if (std::memcmp(tag, "MESI", 4) == 0) {
            if (pos + 80 > size) break;
            const uint8_t* hdr = data + pos;
            pos += 80;

            uint16_t vert_stride = read_u16(hdr + 2);
            uint16_t num_verts = read_u16(hdr + 4);
            uint16_t num_faces = read_u16(hdr + 6);

            if (out_clip.num_vertices == 0) {
                out_clip.num_vertices = num_verts;
                out_clip.num_faces = num_faces;
            }

            BrgFrame frame;
            frame.positions.resize(num_verts);

            // Read vertex positions
            if (vert_stride == 12 && pos + num_verts * 12 <= size) {
                for (uint16_t v = 0; v < num_verts; ++v) {
                    frame.positions[v].x = read_f32(data + pos + v * 12 + 0);
                    frame.positions[v].y = read_f32(data + pos + v * 12 + 4);
                    frame.positions[v].z = read_f32(data + pos + v * 12 + 8);
                }
                pos += num_verts * 12;
            } else if (pos + num_verts * 6 <= size) {
                for (uint16_t v = 0; v < num_verts; ++v) {
                    uint16_t hx = read_u16(data + pos + v * 6 + 0);
                    uint16_t hy = read_u16(data + pos + v * 6 + 2);
                    uint16_t hz = read_u16(data + pos + v * 6 + 4);
                    frame.positions[v] = glm::vec3(half_to_float(hx), half_to_float(hy), half_to_float(hz));
                }
                pos += num_verts * 6;
            }

            // Frame 0 topology & UVs
            if (out_clip.frames.empty()) {
                if (pos + num_verts * 4 <= size) {
                    out_clip.uvs.resize(num_verts);
                    for (uint16_t v = 0; v < num_verts; ++v) {
                        uint16_t hu = read_u16(data + pos + v * 4 + 0);
                        uint16_t hv = read_u16(data + pos + v * 4 + 2);
                        out_clip.uvs[v] = glm::vec2(half_to_float(hu), half_to_float(hv));
                    }
                    pos += num_verts * 4;
                }

                // Skip face materials
                if (pos + num_faces * 2 <= size) pos += num_faces * 2;

                // Indices
                if (pos + num_faces * 6 <= size) {
                    out_clip.indices.resize(num_faces * 3);
                    for (uint32_t f = 0; f < num_faces * 3; ++f) {
                        out_clip.indices[f] = read_u16(data + pos + f * 2);
                    }
                    pos += num_faces * 6;
                }
            }

            compute_mesh_normals(frame.positions, out_clip.indices, frame.normals);
            out_clip.frames.push_back(std::move(frame));

            // Scan forward to next chunk
            size_t next_chunk = size;
            for (const char* nt : {"MESI", "MTRL", "ASET", "PROP"}) {
                for (size_t s = pos; s + 4 <= size; ++s) {
                    if (std::memcmp(data + s, nt, 4) == 0) {
                        if (s < next_chunk) next_chunk = s;
                        break;
                    }
                }
            }
            pos = next_chunk;
        } else if (std::memcmp(tag, "MTRL", 4) == 0) {
            // Find texture name
            size_t next_tag = size;
            for (const char* next : {"MESI", "ASET", "MTRL"}) {
                for (size_t s = pos; s + 4 <= size; ++s) {
                    if (std::memcmp(data + s, next, 4) == 0) {
                        if (s < next_tag) next_tag = s;
                        break;
                    }
                }
            }
            for (size_t i = pos; i + 3 < next_tag; ++i) {
                if (data[i] >= 32 && data[i] < 127) {
                    size_t len = 0;
                    while (i + len < next_tag && data[i + len] >= 32 && data[i + len] < 127) len++;
                    if (len >= 4) {
                        out_clip.texture_name = std::string(reinterpret_cast<const char*>(data + i), len);
                        break;
                    }
                }
            }
            pos = next_tag;
        }
    }

    return out_clip.is_valid();
}

const AnimScript* AnimationSystem::find_script(const std::string& name) const {
    auto it = scripts_.find(name);
    return (it != scripts_.end()) ? &it->second : nullptr;
}

const BrgAnimationClip* AnimationSystem::find_clip(const std::string& name) const {
    auto it = clips_.find(name);
    return (it != clips_.end()) ? &it->second : nullptr;
}

} // namespace anim
} // namespace aom
