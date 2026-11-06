#pragma once

/// @file proto_unit.h
/// @brief Definições completas de ProtoUnit extraídas do proto.xmb / protox.xmb.
///        Captura todos os atributos relevantes de cada unidade/edifício do AoM.

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace aom {

/// Custo de recursos de uma unidade
struct ProtoUnitCost {
    float food = 0.0f;
    float wood = 0.0f;
    float gold = 0.0f;
    float favor = 0.0f;
};

/// Estatísticas de ataque
struct ProtoUnitAttack {
    float damage = 0.0f;
    float range = 0.0f;
    float reload_time = 0.0f;
    std::string damage_type;  ///< Hack, Slash, Pierce, Crush
};

/// Estatísticas completas de uma ProtoUnit
struct ProtoUnitData {
    std::string name;             ///< Nome interno (ex: "Hoplite", "Villager Greek")
    int dbid = 0;                 ///< ID no banco de dados
    int display_name_id = 0;      ///< ID da string de exibição
    int population_count = 1;     ///< Custo populacional

    // Estatísticas de combate
    float max_hitpoints = 100.0f;
    float los = 16.0f;            ///< Line of sight
    float max_velocity = 4.0f;    ///< Velocidade de movimento
    float acceleration = 0.0f;
    float deceleration = 0.0f;

    // Custo
    ProtoUnitCost cost;

    // Ataque
    ProtoUnitAttack attack;

    // Classificação
    bool worker = false;          ///< É um aldeão/trabalhador?
    bool military = false;
    bool building = false;
    bool ship = false;
    bool hero = false;
    bool myth_unit = false;

    // Flags de ação
    bool can_attack = false;
    bool can_move = false;
    bool can_build = false;
    bool can_gather = false;

    // Arquivos de modelo e ícone
    std::string brg_filename;     ///< Arquivo .brg do modelo
    std::string icon_filename;    ///< Ícone da UI
    std::string sound_def;        ///< Definição de som (*_snds.xml)
};

/// Registro expandido de ProtoUnits (base + expansão)
class ProtoUnitRegistry {
public:
    ProtoUnitRegistry() = default;
    ~ProtoUnitRegistry() = default;

    /// Carrega todas as unidades de um arquivo proto.xmb (XMB parseado)
    bool load_from_xmb(const XmbElement& root);

    /// Busca unidade por nome (case-insensitive)
    [[nodiscard]] const ProtoUnitData* find(const std::string& name) const;

    [[nodiscard]] size_t count() const { return units_.size(); }
    [[nodiscard]] const std::vector<ProtoUnitData>& all() const { return units_; }

    /// Busca unidades por tipo (ex: todas as ships)
    [[nodiscard]] std::vector<const ProtoUnitData*> find_by_type(bool ship) const;
    [[nodiscard]] std::vector<const ProtoUnitData*> find_workers() const;
    [[nodiscard]] std::vector<const ProtoUnitData*> find_military() const;

private:
    static ProtoUnitCost parse_cost(const XmbElement& cost_elem);
    static ProtoUnitAttack parse_attack(const XmbElement& attack_elem);

    std::vector<ProtoUnitData> units_;
    std::unordered_map<std::string, size_t> name_index_;
};

} // namespace aom
