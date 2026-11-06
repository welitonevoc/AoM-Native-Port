#pragma once
/// @file grn_animation.h
/// @brief Parser e player de animações .grn (Granny 3D) para AoM Native Port
///
/// Os arquivos .grn contêm animação esquelética (hierarquia de ossos + keyframes
/// de translação, rotação e escala). Este módulo faz parse do formato binário
/// e expõe os dados para o sistema de animação existente.

#include "anim/animation_system.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace aom {
namespace anim {

/// Keyframe de um canal de animação (translação, rotação ou escala)
struct GrnKeyframe {
    float time = 0.0f;        ///< Tempo em segundos
    glm::vec3 value{0.0f};    ///< Valor (translação, rotação Euler ou escala)
    glm::quat rotation{};     ///< Rotação como quaternion (para canais de rotação)
    bool is_rotation = false; ///< Se true, usa rotation em vez de value
};

/// Canal de animação de um osso
struct GrnTrack {
    std::string bone_name;
    std::vector<GrnKeyframe> translation_keys;
    std::vector<GrnKeyframe> rotation_keys;
    std::vector<GrnKeyframe> scale_keys;
};

/// Hierarquia de ossos
struct GrnBone {
    std::string name;
    int32_t parent_index = -1;
    glm::mat4 bind_pose = glm::mat4(1.0f);  ///< Pose de ligação (inverse bind matrix)
};

/// Animação GRN completa
struct GrnAnimation {
    std::string name;
    float duration = 0.0f;          ///< Duração total em segundos
    float fps = 30.0f;              ///< Frames por segundo
    uint32_t num_bones = 0;
    std::vector<GrnBone> bones;
    std::vector<GrnTrack> tracks;   ///< Um track por osso animado

    /// Busca o índice do osso pelo nome
    int32_t find_bone(const std::string& name) const {
        for (uint32_t i = 0; i < bones.size(); ++i) {
            if (bones[i].name == name) return static_cast<int32_t>(i);
        }
        return -1;
    }

    /// Busca o track de animação de um osso
    const GrnTrack* find_track(const std::string& bone_name) const {
        for (const auto& track : tracks) {
            if (track.bone_name == bone_name) return &track;
        }
        return nullptr;
    }
};

/// Player de animação GRN — avalia a pose em um dado tempo
class GrnPlayer {
public:
    GrnPlayer() = default;

    /// Carrega uma animação GRN
    bool set_animation(const GrnAnimation& anim);

    /// Avalia a animação no tempo t (segundos), escrevendo matrizes de transformação
    /// por osso em out_transforms (tamanho = num_bones)
    bool evaluate(float t, std::vector<glm::mat4>& out_transforms) const;

    const GrnAnimation* animation() const { return current_anim_.get(); }
    float duration() const { return current_anim_ ? current_anim_->duration : 0.0f; }

private:
    std::unique_ptr<GrnAnimation> current_anim_;

    /// Interpola linearmente entre keyframes de um canal
    glm::vec3 interpolate_translation(const GrnTrack& track, float t) const;
    glm::quat interpolate_rotation(const GrnTrack& track, float t) const;
    glm::vec3 interpolate_scale(const GrnTrack& track, float t) const;
};

/// Faz parse de um arquivo .grn a partir de dados binários
/// Retorna nullptr se o formato for inválido ou não suportado
std::unique_ptr<GrnAnimation> parse_grn(const uint8_t* data, size_t size,
                                        const std::string& name = "");

/// Carrega uma animação .grn de um BarArchive
std::unique_ptr<GrnAnimation> load_grn_animation(class BarArchive* archive,
                                                 const std::string& name);

} // namespace anim
} // namespace aom
