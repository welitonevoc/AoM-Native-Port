#pragma once

/// @file major_god.h
/// @brief Definições de deuses maiores e menores extraídas do majorgods.xmb / minorgods.xml.

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace aom {

/// Deuse menor (Minor God)
struct MinorGodData {
    int dbid = 0;
    int name_id = 0;
    std::string tech_name;      ///< Ex: "Age 2 Athena"
    std::string portrait;       ///< Ex: "god minor portrait greek Athena"
    int age = 0;                ///< Era (2, 3, 4)
    int position = 0;           ///< Posição no painel de seleção
    std::string civilization;   ///< greek, egypt, norse, atlantis
};

/// Deuse maior (Major God) com seus deuses menores
struct MajorGodData {
    int name_id = 0;
    std::string tech_name;      ///< Ex: "Age 1 Zeus"
    std::string lg_portrait;    ///< Retrato grande 256x256
    std::string sm_portrait;    ///< Retrato pequeno 32x32
    std::string md_portrait;    ///< Retrato médio 64x64
    std::string lg2_portrait;   ///< Retrato 128x128
    int position = 0;
    std::vector<MinorGodData> minor_gods;
    std::string civilization;
};

/// Registro de deuses (base + expansão The Titans)
class GodRegistry {
public:
    GodRegistry() = default;
    ~GodRegistry() = default;

    /// Carrega deuses maiores de um arquivo majorgods.xmb (XMB parseado)
    bool load_majorgods_from_xmb(const XmbElement& root);

    /// Carrega deuses menores de um arquivo minorgods.xml (XMB parseado)
    bool load_minorgods_from_xmb(const XmbElement& root);

    /// Busca deuse maior por tech_name (case-insensitive)
    [[nodiscard]] const MajorGodData* find_major(const std::string& tech_name) const;

    /// Busca deuse menor por tech_name (case-insensitive)
    [[nodiscard]] const MinorGodData* find_minor(const std::string& tech_name) const;

    /// Lista todos os deuses menores de uma civilização
    [[nodiscard]] std::vector<const MinorGodData*> find_minors_by_civ(const std::string& civ) const;

    [[nodiscard]] size_t major_count() const { return major_gods_.size(); }
    [[nodiscard]] size_t minor_count() const { return minor_gods_.size(); }

    [[nodiscard]] const std::vector<MajorGodData>& all_majors() const { return major_gods_; }
    [[nodiscard]] const std::vector<MinorGodData>& all_minors() const { return minor_gods_; }

private:
    std::vector<MajorGodData> major_gods_;
    std::vector<MinorGodData> minor_gods_;
    std::unordered_map<std::string, size_t> major_index_;
    std::unordered_map<std::string, size_t> minor_index_;
};

} // namespace aom
