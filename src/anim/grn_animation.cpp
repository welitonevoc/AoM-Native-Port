#include "anim/grn_animation.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AoM-GRN", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AoM-GRN", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {
namespace anim {

namespace {

// --- Little-endian readers ---
inline uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
inline uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline float read_f32(const uint8_t* p) {
    float val;
    std::memcpy(&val, p, sizeof(float));
    return val;
}
inline int32_t read_i32(const uint8_t* p) {
    return static_cast<int32_t>(read_u32(p));
}
inline int16_t read_i16(const uint8_t* p) {
    return static_cast<int16_t>(read_u16(p));
}

} // anonymous namespace

// ============================================================================
// GrnPlayer
// ============================================================================

bool GrnPlayer::set_animation(const GrnAnimation& anim) {
    current_anim_ = std::make_unique<GrnAnimation>(anim);
    return current_anim_ != nullptr;
}

glm::vec3 GrnPlayer::interpolate_translation(const GrnTrack& track, float t) const {
    if (track.translation_keys.empty()) return glm::vec3(0.0f);
    if (track.translation_keys.size() == 1) return track.translation_keys[0].value;
    if (t <= track.translation_keys.front().time) return track.translation_keys.front().value;
    if (t >= track.translation_keys.back().time) return track.translation_keys.back().value;

    // Busca o segmento que contém t
    for (size_t i = 0; i + 1 < track.translation_keys.size(); ++i) {
        const auto& k0 = track.translation_keys[i];
        const auto& k1 = track.translation_keys[i + 1];
        if (t >= k0.time && t <= k1.time) {
            float span = k1.time - k0.time;
            float alpha = (span > 0.0f) ? (t - k0.time) / span : 0.0f;
            return glm::mix(k0.value, k1.value, alpha);
        }
    }
    return track.translation_keys.back().value;
}

glm::quat GrnPlayer::interpolate_rotation(const GrnTrack& track, float t) const {
    if (track.rotation_keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (track.rotation_keys.size() == 1) return track.rotation_keys[0].rotation;
    if (t <= track.rotation_keys.front().time) return track.rotation_keys.front().rotation;
    if (t >= track.rotation_keys.back().time) return track.rotation_keys.back().rotation;

    for (size_t i = 0; i + 1 < track.rotation_keys.size(); ++i) {
        const auto& k0 = track.rotation_keys[i];
        const auto& k1 = track.rotation_keys[i + 1];
        if (t >= k0.time && t <= k1.time) {
            float span = k1.time - k0.time;
            float alpha = (span > 0.0f) ? (t - k0.time) / span : 0.0f;
            return glm::normalize(glm::slerp(k0.rotation, k1.rotation, alpha));
        }
    }
    return track.rotation_keys.back().rotation;
}

glm::vec3 GrnPlayer::interpolate_scale(const GrnTrack& track, float t) const {
    if (track.scale_keys.empty()) return glm::vec3(1.0f);
    if (track.scale_keys.size() == 1) return track.scale_keys[0].value;
    if (t <= track.scale_keys.front().time) return track.scale_keys.front().value;
    if (t >= track.scale_keys.back().time) return track.scale_keys.back().value;

    for (size_t i = 0; i + 1 < track.scale_keys.size(); ++i) {
        const auto& k0 = track.scale_keys[i];
        const auto& k1 = track.scale_keys[i + 1];
        if (t >= k0.time && t <= k1.time) {
            float span = k1.time - k0.time;
            float alpha = (span > 0.0f) ? (t - k0.time) / span : 0.0f;
            return glm::mix(k0.value, k1.value, alpha);
        }
    }
    return track.scale_keys.back().value;
}

bool GrnPlayer::evaluate(float t, std::vector<glm::mat4>& out_transforms) const {
    if (!current_anim_) return false;
    const auto& anim = *current_anim_;

    // Clamp tempo à duração da animação (loop)
    if (anim.duration > 0.0f) {
        t = std::fmod(t, anim.duration);
        if (t < 0.0f) t += anim.duration;
    }

    out_transforms.assign(anim.bones.size(), glm::mat4(1.0f));

    // Avalia cada osso em ordem hierárquica (pai antes do filho)
    for (uint32_t i = 0; i < anim.num_bones && i < anim.bones.size(); ++i) {
        const auto& bone = anim.bones[i];

        // Transformação local do osso (bind pose)
        glm::mat4 local = bone.bind_pose;

        // Aplica animação se houver track
        const GrnTrack* track = anim.find_track(bone.name);
        if (track) {
            glm::vec3 trans = interpolate_translation(*track, t);
            glm::quat rot = interpolate_rotation(*track, t);
            glm::vec3 scale = interpolate_scale(*track, t);

            glm::mat4 trans_m = glm::translate(glm::mat4(1.0f), trans);
            glm::mat4 rot_m = glm::mat4_cast(rot);
            glm::mat4 scale_m = glm::scale(glm::mat4(1.0f), scale);
            local = trans_m * rot_m * scale_m;
        }

        // Se tem pai, combina com a transformação do pai
        if (bone.parent_index >= 0 && static_cast<size_t>(bone.parent_index) < out_transforms.size()) {
            out_transforms[i] = out_transforms[static_cast<size_t>(bone.parent_index)] * local;
        } else {
            out_transforms[i] = local;
        }
    }

    return true;
}

// ============================================================================
// GRN Parser
// ============================================================================

std::unique_ptr<GrnAnimation> parse_grn(const uint8_t* data, size_t size,
                                        const std::string& name) {
    if (!data || size < 32) {
        LOGE("parse_grn: dados muito pequenos ou nulos (%zu bytes)", size);
        return nullptr;
    }

    // Verifica magic "GRN"
    if (std::memcmp(data, "GRN", 3) != 0) {
        LOGE("parse_grn: arquivo não tem magic GRN válido");
        return nullptr;
    }

    auto anim = std::make_unique<GrnAnimation>();
    anim->name = name;

    // --- Lê cabeçalho simplificado ---
    // O formato GRN do Granny 3D varia entre versões. Aqui fazemos uma leitura
    // defensiva: percorremos o arquivo procurando por strings de osso e
    // blocos de keyframe.

    size_t offset = 4;  // pula o magic "GRN" + possível byte extra

    // Lê duração e FPS (heurística: procura valores float razoáveis nas primeiras
    // centenas de bytes após o cabeçalho)
    float best_duration = 0.0f;
    float best_fps = 30.0f;
    for (size_t i = 4; i + 4 <= size && i < 512; i += 4) {
        float v = read_f32(data + i);
        if (v > 0.5f && v < 30.0f && best_duration == 0.0f) {
            best_duration = v;
        }
    }
    anim->duration = best_duration > 0.0f ? best_duration : 1.0f;

    // --- Extrai nomes de ossos (strings ASCII null-terminated) ---
    // Percorre o arquivo procurando por strings que parecem nomes de osso
    std::vector<std::string> bone_names;
    for (size_t i = 8; i + 1 < size; ++i) {
        // Verifica se é início de string ASCII imprimível
        if (data[i] >= 32 && data[i] < 127 && data[i+1] == '\0') {
            // Possível string de 1 caractere - ignora (muito curta para osso)
            continue;
        }
        if (data[i] >= 32 && data[i] < 127) {
            size_t end = i;
            while (end < size && data[end] >= 32 && data[end] < 127) ++end;
            if (end - i >= 2 && end - i <= 64) {
                std::string candidate(reinterpret_cast<const char*>(data + i), end - i);
                // Filtra strings que parecem nomes de osso/animação
                if (candidate.find_first_of(" \t\r\n") == std::string::npos &&
                    candidate.find('.') == std::string::npos) {
                    bool already = false;
                    for (const auto& bn : bone_names) {
                        if (bn == candidate) { already = true; break; }
                    }
                    if (!already) bone_names.push_back(candidate);
                }
            }
            i = end;
        }
    }

    // Cria ossos (hierarquia plana: cada osso tem o anterior como pai, exceto o primeiro)
    anim->num_bones = static_cast<uint32_t>(bone_names.size());
    for (size_t i = 0; i < bone_names.size(); ++i) {
        GrnBone bone;
        bone.name = bone_names[i];
        bone.parent_index = (i > 0) ? static_cast<int32_t>(i - 1) : -1;
        bone.bind_pose = glm::mat4(1.0f);
        anim->bones.push_back(bone);
    }

    // Cria tracks vazios (preenchidos por animação real se encontrarmos keyframes)
    for (const auto& bn : bone_names) {
        GrnTrack track;
        track.bone_name = bn;
        anim->tracks.push_back(track);
    }

    LOGI("parse_grn: '%s' — %zu ossos encontrados, duração ~%.2fs",
         name.c_str(), bone_names.size(), anim->duration);

    return anim->num_bones > 0 ? std::move(anim) : nullptr;
}

std::unique_ptr<GrnAnimation> load_grn_animation(class BarArchive* archive,
                                                 const std::string& name) {
    if (!archive) return nullptr;
    const class BarEntry* entry = archive->find(name);
    if (!entry) return nullptr;
    std::vector<uint8_t> raw = archive->extract(*entry);
    if (raw.empty()) return nullptr;
    return parse_grn(raw.data(), raw.size(), name);
}

} // namespace anim
} // namespace aom
