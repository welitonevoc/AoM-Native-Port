#pragma once

/// @file sound_def.h
/// @brief Definições de som extraídas do sounds.bar (soundsets.xml + *_snds.xml).
///        Mapeia soundsets e tipos de som de protounits para arquivos .wav.

#include <string>
#include <vector>
#include <unordered_map>
#include "assets/xmb_parser.h"

namespace aom {

/// Um som individual dentro de um soundset
struct SoundEntry {
    std::string filename;   ///< Ex: "Woodchop1.wav"
    float volume = 1.0f;    ///< Volume (0.0 - 1.0)
};

/// Um conjunto de sons (soundset) com múltiplas variações
struct SoundSetDef {
    std::string name;       ///< Ex: "GatherChop", "MilitaryGreekSelect"
    float volume = 1.0f;    ///< Volume global do set
    int max_num = 1;        ///< Número máximo de instâncias simultâneas
    std::vector<SoundEntry> sounds;  ///< Lista de arquivos de som
};

/// Tipo de som de uma protounit (Select, Grunt, Hit, Death, etc.)
struct ProtoUnitSoundType {
    std::string type_name;                    ///< Ex: "Select", "Hit", "Death"
    std::vector<std::string> sound_sets;      ///< Soundsets associados a este tipo
    /// Para Hit com dano tipificado:
    std::unordered_map<std::string, std::string> damage_type_to_set;  ///< Hack->MetalSlice, etc.
    /// Para Acknowledge com alvo:
    std::unordered_map<std::string, std::string> target_to_set;       ///< default->..., enemy->...
};

/// Definição completa de som de uma protounit
struct ProtoUnitSoundDef {
    std::string unit_name;    ///< Nome da protounit (ex: "Hoplite")
    std::vector<ProtoUnitSoundType> sound_types;
};

/// Gerenciador de definições de som
class SoundDefManager {
public:
    SoundDefManager() = default;
    ~SoundDefManager() = default;

    /// Carrega soundsets.xml (XMB parseado)
    bool load_soundsets_from_xmb(const XmbElement& root);

    /// Carrega *_snds.xml (XMB parseado) - definições por protounit
    bool load_protounit_sounds_from_xmb(const XmbElement& root);

    /// Busca um soundset por nome
    [[nodiscard]] const SoundSetDef* find_soundset(const std::string& name) const;

    /// Busca definição de som de uma protounit
    [[nodiscard]] const ProtoUnitSoundDef* find_protounit_sound(const std::string& unit_name) const;

    /// Escolhe um som aleatório de um soundset (por índice determinístico)
    [[nodiscard]] const std::string* pick_sound(const SoundSetDef& set, uint32_t seed) const;

    [[nodiscard]] size_t soundset_count() const { return soundsets_.size(); }
    [[nodiscard]] size_t protounit_sound_count() const { return protounit_sounds_.size(); }

    [[nodiscard]] const std::vector<SoundSetDef>& all_soundsets() const { return soundsets_; }

private:
    std::vector<SoundSetDef> soundsets_;
    std::vector<ProtoUnitSoundDef> protounit_sounds_;
    std::unordered_map<std::string, size_t> soundset_index_;
    std::unordered_map<std::string, size_t> protounit_index_;
};

} // namespace aom
