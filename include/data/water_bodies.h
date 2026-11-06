#pragma once

/// @file water_bodies.h
/// @brief Definições de corpos d'água (rios, oceanos, lagos) extraídas do data.bar
///        (waterbodies.xmb). Aplica cores, profundidade, texturas de beira e
///        clusters de objetos no renderizador de água.

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace aom {

/// Textura aplicada a uma distância específica da beira
struct TexturePlacement {
    float distance = 0.0f;   ///< Distância da beira em unidades de mundo
    std::string texture;     ///< Nome da textura (ex: "ShorelineMediterraneanB")
};

/// Cluster de objetos (rochas, árvores, etc.) colocados na água/beira
struct ObjectCluster {
    std::vector<std::string> protounits;  ///< Nomes das unidades de objeto
};

/// Definição de um rio
struct RiverDef {
    std::string name;
    std::string icon_name;
    std::string surface;      ///< Ex: "water"
    std::string bottom;       ///< Textura do fundo (ex: "RiverGrassyA")
    std::string bank;         ///< Textura da beira interna (ex: "RiverGrassyB")
    std::string outer_bank;   ///< Textura da beira externa (ex: "RiverGrassyC")
    glm::vec3 color{0, 50, 170};  ///< Cor da água (RGB 0-255)
    float depth = 3.0f;       ///< Profundidade em metros
    float slope = 0.0f;       ///< Inclinação da beira
    float curve_angle = 15.0f;
    float distance_between = 30.0f;
    float distance_variation = 10.0f;
    bool lock_to_water_edge = false;
    std::vector<TexturePlacement> texture_placements;
    std::vector<ObjectCluster> object_clusters;
};

/// Definição de um oceano
struct OceanDef {
    std::string name;
    std::string icon_name;
    std::string surface;
    std::string bottom;
    std::string bank;
    std::string outer_bank;
    glm::vec3 color{0, 50, 170};
    float depth = 4.0f;
    std::vector<TexturePlacement> texture_placements;
    std::vector<ObjectCluster> object_clusters;
};

/// Definição de um lago
struct LakeDef {
    std::string name;
    std::string icon_name;
    std::string surface;
    std::string bottom;
    std::string bank;
    std::string outer_bank;
    glm::vec3 color{0, 175, 235};
    float depth = 3.0f;
    float slope = 0.0f;
    float curve_angle = 15.0f;
    float distance_between = 40.0f;
    float distance_variation = 10.0f;
    bool lock_to_water_edge = false;
    std::vector<ObjectCluster> object_clusters;
};

/// Gerenciador de corpos d'água carregados do waterbodies.xmb
class WaterBodyManager {
public:
    WaterBodyManager() = default;
    ~WaterBodyManager() = default;

    /// Carrega definições de um arquivo waterbodies.xmb (via XMB parseado)
    bool load_from_xmb(const XmbElement& root);

    /// Busca rio por nome (case-insensitive)
    [[nodiscard]] const RiverDef* find_river(const std::string& name) const;

    /// Busca oceano por nome (case-insensitive)
    [[nodiscard]] const OceanDef* find_ocean(const std::string& name) const;

    /// Busca lago por nome (case-insensitive)
    [[nodiscard]] const LakeDef* find_lake(const std::string& name) const;

    [[nodiscard]] size_t river_count() const { return rivers_.size(); }
    [[nodiscard]] size_t ocean_count() const { return oceans_.size(); }
    [[nodiscard]] size_t lake_count() const { return lakes_.size(); }

    [[nodiscard]] const std::vector<RiverDef>& all_rivers() const { return rivers_; }
    [[nodiscard]] const std::vector<OceanDef>& all_oceans() const { return oceans_; }
    [[nodiscard]] const std::vector<LakeDef>& all_lakes() const { return lakes_; }

private:
    static glm::vec3 parse_color(const std::string& str);

    std::vector<RiverDef> rivers_;
    std::vector<OceanDef> oceans_;
    std::vector<LakeDef> lakes_;

    std::unordered_map<std::string, size_t> river_index_;
    std::unordered_map<std::string, size_t> ocean_index_;
    std::unordered_map<std::string, size_t> lake_index_;
};

} // namespace aom
