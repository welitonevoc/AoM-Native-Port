#pragma once

/// @file proto_unit.h
/// @brief Definições completas de ProtoUnit extraídas do proto.xmb / protox.xmb.
///        Captura todos os atributos relevantes de cada unidade/edifício do AoM.

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "aom/unit_stats.hpp"
#include "assets/xmb_parser.h"

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
    float accuracy = 1.0f;    ///< Probabilidade de acerto (0.0-1.0)
    float max_spread = 0.0f;  ///< Spread máximo no alvo (world units)
    float turn_rate = 18.0f;  ///< Turn rate em graus/seg
    float spread_factor = 0.0f;  ///< RangedAttack SpreadFactor
    float track_rating = 0.0f;   ///< RangedAttack TrackRating
    std::string projectile_proto; ///< <projectileprotounit>

    // Damage bonuses (vs specific unit types)
    struct DamageBonus {
        std::string target_type;
        float multiplier = 1.0f;
    };
    std::vector<DamageBonus> damage_bonuses;
};

/// Estatísticas completas de uma ProtoUnit
struct ProtoUnitData {
    std::string name;             ///< Nome interno (ex: "Hoplite", "Villager Greek")
    int dbid = 0;                 ///< ID no banco de dados
    int display_name_id = 0;      ///< ID da string de exibição
    int population_count = 1;     ///< Custo populacional

    // Estatísticas de combate
    float max_hitpoints = 100.0f;
    float initial_hitpoints = 100.0f;  ///< <initialhitpoints>
    float los = 16.0f;            ///< Line of sight
    float max_velocity = 4.0f;    ///< Velocidade de movimento
    float acceleration = 0.0f;
    float deceleration = 0.0f;

    // Armor (porcentagem de redução por tipo de dano, percentflag="1" no protox.xml)
    float armor_hack = 0.0f;      ///< Redução contra Hack (melee)
    float armor_pierce = 0.0f;    ///< Redução contra Pierce (ranged)
    float armor_crush = 0.0f;     ///< Redução contra Crush (siege)

    // Custo
    ProtoUnitCost cost;

    // Economy
    float train_points = 0.0f;    ///< <trainpoints>
    int allowed_age = 0;          ///< <allowedage>

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
    std::string projectile_proto; ///< <projectileprotounit>

    // Source tracking
    bool from_protox_xml = false; ///< Se veio do protox.xml (prioridade alta)
};

/// Registro expandido de ProtoUnits (base + expansão)
class ProtoUnitRegistry {
public:
    ProtoUnitRegistry() = default;
    ~ProtoUnitRegistry() = default;

    /// Carrega todas as unidades de um arquivo proto.xmb (XMB parseado)
    bool load_from_xmb(const XmbElement& root);

    /// Carrega stats de gameplay do protox.xml legível (via SAF no Android)
    /// Este arquivo contém os valores REAIS de gameplay (o XMB binário é só estrutura)
    /// protox.xml tem precedência sobre XMB para: max_hitpoints, max_velocity, armor, attack, turn_rate
    bool load_from_protox_xml(const std::string& xml_path);

    /// Carrega stats de gameplay do protox.xml a partir de string (já lido via SAF)
    bool load_from_protox_xml_string(std::string_view xml_content);

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

    // Merge protox.xml stats into existing unit (or create new)
    void merge_protox_stats(const aom::ProtoxUnitStats& stats);

    std::vector<ProtoUnitData> units_;
    std::unordered_map<std::string, size_t> name_index_;
};

} // namespace aom
