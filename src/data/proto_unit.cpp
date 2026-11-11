#include "data/proto_unit.h"
#include "aom/protox_parser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#define LOG_TAG "AoM-ProtoUnit"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

/// Le um stat do prototipo.
///
/// No proto.xmb/protox.xmb do AoM os stats sao ELEMENTOS FILHOS do <unit>
/// (ex.: <maxhitpoints>115.0000</maxhitpoints>), e nao atributos. Lendo
/// apenas get_attribute() todos os stats caiam no valor default e o registro
/// ficava com HP/velocidade/populacao genericos. Procura primeiro no elemento
/// filho e usa o atributo como fallback.
float read_stat_float(const XmbElement& elem, const std::string& name, float def) {
    if (const auto* child = elem.find_child(name)) {
        if (!child->text.empty()) {
            try { return std::stof(child->text); } catch (...) {}
        }
        const std::string attr = child->get_attribute("value");
        if (!attr.empty()) {
            try { return std::stof(attr); } catch (...) {}
        }
    }
    const std::string attr = elem.get_attribute(name);
    if (!attr.empty()) {
        try { return std::stof(attr); } catch (...) {}
    }
    return def;
}

/// Versao inteira de read_stat_float().
int read_stat_int(const XmbElement& elem, const std::string& name, int def = 0) {
    if (const auto* child = elem.find_child(name)) {
        if (!child->text.empty()) {
            try { return std::stoi(child->text); } catch (...) {}
        }
        const std::string attr = child->get_attribute("value");
        if (!attr.empty()) {
            try { return std::stoi(attr); } catch (...) {}
        }
    }
    const std::string attr = elem.get_attribute(name);
    if (!attr.empty()) {
        try { return std::stoi(attr); } catch (...) {}
    }
    return def;
}

/// Versao textual de read_stat_float() (ex.: <icon>...</icon>).
std::string read_stat_string(const XmbElement& elem, const std::string& name) {
    if (const auto* child = elem.find_child(name)) {
        if (!child->text.empty()) return child->text;
        const std::string attr = child->get_attribute("value");
        if (!attr.empty()) return attr;
    }
    return elem.get_attribute(name);
}

} // anonymous namespace

ProtoUnitCost ProtoUnitRegistry::parse_cost(const XmbElement& cost_elem) {
    ProtoUnitCost c;
    auto rtype = cost_elem.get_attribute("resourcetype");
    float amount = cost_elem.text_as_float(0.0f);
    if (rtype == "Food") c.food = amount;
    else if (rtype == "Wood") c.wood = amount;
    else if (rtype == "Gold") c.gold = amount;
    else if (rtype == "Favor") c.favor = amount;
    return c;
}

ProtoUnitAttack ProtoUnitRegistry::parse_attack(const XmbElement& attack_elem) {
    ProtoUnitAttack a;
    // O elemento <action> dentro de <unit> contém parâmetros de ataque
    // Ex: <action name="Damage" value1="12.0"/>
    //     <action name="MaximumRange" value1="2.2"/>
    for (const auto& param : attack_elem.children) {
        if (!param) continue;
        auto pname = param->get_attribute("name");
        auto val = param->get_attribute("value1");
        if (val.empty()) continue;
        float fv = 0.0f;
        try { fv = std::stof(val); } catch (...) {}

        if (pname == "Damage") {
            a.damage = fv;
        } else if (pname == "MaximumRange") {
            a.range = (fv < 1.0f) ? 2.0f : fv;
        } else if (pname == "ReloadTime") {
            a.reload_time = fv;
        }
    }
    return a;
}

bool ProtoUnitRegistry::load_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child || child->name != "unit") continue;

        ProtoUnitData u;
        u.name = child->get_attribute("name");
        if (u.name.empty()) continue;

        u.dbid = read_stat_int(*child, "dbid");
        u.display_name_id = read_stat_int(*child, "displaynameid");
        u.population_count = read_stat_int(*child, "populationcount", 1);

        // Estatísticas básicas
        u.max_hitpoints = read_stat_float(*child, "maxhitpoints", 100.0f);
        u.los = read_stat_float(*child, "los", 16.0f);
        u.max_velocity = read_stat_float(*child, "maxvelocity", 4.0f);
        u.acceleration = read_stat_float(*child, "acceleration", 0.0f);
        u.deceleration = read_stat_float(*child, "deceleration", 0.0f);

        // Custos
        for (const auto& prop : child->children) {
            if (!prop) continue;
            if (prop->name == "cost") {
                auto cost = parse_cost(*prop);
                u.cost.food += cost.food;
                u.cost.wood += cost.wood;
                u.cost.gold += cost.gold;
                u.cost.favor += cost.favor;
            }
        }

        // Ataque (dentro de <action>)
        for (const auto& prop : child->children) {
            if (!prop || prop->name != "action") continue;
            auto attack = parse_attack(*prop);
            if (attack.damage > 0 || attack.range > 0) {
                u.attack = attack;
                u.can_attack = true;
            }
        }

        // Flags de classificação baseadas no nome e atributos
        auto lname = to_lower(u.name);
        u.worker = (lname.find("villager") != std::string::npos ||
                    lname.find("worker") != std::string::npos ||
                    lname.find("dwarf") != std::string::npos);
        u.ship = (lname.find("ship") != std::string::npos ||
                  lname.find("trireme") != std::string::npos ||
                  lname.find("longboat") != std::string::npos ||
                  lname.find("galley") != std::string::npos ||
                  lname.find("transport") != std::string::npos);
        u.building = (lname.find("town center") != std::string::npos ||
                      lname.find("house") != std::string::npos ||
                      lname.find("barracks") != std::string::npos ||
                      lname.find("tower") != std::string::npos ||
                      lname.find("wall") != std::string::npos ||
                      lname.find("gate") != std::string::npos ||
                      lname.find("fortress") != std::string::npos ||
                      lname.find("temple") != std::string::npos ||
                      lname.find("armory") != std::string::npos ||
                      lname.find("stable") != std::string::npos ||
                      lname.find("archery") != std::string::npos ||
                      lname.find("dock") != std::string::npos ||
                      lname.find("storehouse") != std::string::npos ||
                      lname.find("granary") != std::string::npos);
        u.military = (lname.find("hoplite") != std::string::npos ||
                      lname.find("toxotes") != std::string::npos ||
                      lname.find("minotaur") != std::string::npos ||
                      lname.find("ulfsark") != std::string::npos ||
                      lname.find("huskarl") != std::string::npos ||
                      lname.find("throwing axeman") != std::string::npos);
        u.hero = (lname.find("arkantos") != std::string::npos ||
                  lname.find("heroes") != std::string::npos ||
                  lname.find("heracle") != std::string::npos ||
                  lname.find("theseus") != std::string::npos ||
                  lname.find("jason") != std::string::npos ||
                  lname.find("odysseus") != std::string::npos);
        u.myth_unit = (lname.find("minotaur") != std::string::npos ||
                       lname.find("cyclops") != std::string::npos ||
                       lname.find("hydra") != std::string::npos ||
                       lname.find("medusa") != std::string::npos ||
                       lname.find("kraken") != std::string::npos);

        u.can_move = u.max_velocity > 0.1f && !u.building;

        // Arquivo .brg: atributo <unit brg="..."> quando presente, senao a
        // convencao <name>.brg
        u.brg_filename = read_stat_string(*child, "brg");
        if (u.brg_filename.empty()) {
            // Convenção: <name>.brg
            u.brg_filename = u.name + ".brg";
        }
        // Ícone da UI: no proto.xmb é o ELEMENTO <icon>...</icon>
        u.icon_filename = read_stat_string(*child, "icon");

        // Inserir/atualizar no registro (protox.xmb sobrescreve proto.xmb)
        const std::string key = to_lower(u.name);
        auto it = name_index_.find(key);
        if (it != name_index_.end()) {
            units_[it->second] = std::move(u);
        } else {
            name_index_[key] = units_.size();
            units_.push_back(std::move(u));
        }
        ok = true;
    }

    return ok;
}

const ProtoUnitData* ProtoUnitRegistry::find(const std::string& name) const {
    auto it = name_index_.find(to_lower(name));
    if (it != name_index_.end()) return &units_[it->second];
    return nullptr;
}

std::vector<const ProtoUnitData*> ProtoUnitRegistry::find_by_type(bool ship) const {
    std::vector<const ProtoUnitData*> result;
    for (const auto& u : units_) {
        if (u.ship == ship) result.push_back(&u);
    }
    return result;
}

std::vector<const ProtoUnitData*> ProtoUnitRegistry::find_workers() const {
    std::vector<const ProtoUnitData*> result;
    for (const auto& u : units_) {
        if (u.worker) result.push_back(&u);
    }
    return result;
}

std::vector<const ProtoUnitData*> ProtoUnitRegistry::find_military() const {
    std::vector<const ProtoUnitData*> result;
    for (const auto& u : units_) {
        if (u.military) result.push_back(&u);
    }
    return result;
}

bool ProtoUnitRegistry::load_from_protox_xml(const std::string& xml_path) {
    std::ifstream file(xml_path);
    if (!file) {
        LOGW("load_from_protox_xml: não foi possível abrir %s", xml_path.c_str());
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_protox_xml_string(buffer.str());
}

bool ProtoUnitRegistry::load_from_protox_xml_string(std::string_view xml_content) {
    aom::ProtoxParser parser;
    auto result = parser.ParseString(xml_content);

    if (result.units.empty()) {
        LOGW("load_from_protox_xml_string: nenhuma unidade parseada do protox.xml");
        for (const auto& w : result.warnings) {
            LOGW("  aviso: %s", w.c_str());
        }
        for (const auto& e : result.errors) {
            LOGE("  erro: %s", e.c_str());
        }
        return false;
    }

    LOGI("load_from_protox_xml_string: %zu unidades parseadas do protox.xml", result.units.size());

    for (const auto& [proto_id, stats] : result.units) {
        merge_protox_stats(stats);
    }

    return true;
}

void ProtoUnitRegistry::merge_protox_stats(const aom::ProtoxUnitStats& stats) {
    // Find existing unit by name (case-insensitive)
    std::string key = to_lower(std::string(stats.name));
    auto it = name_index_.find(key);

    ProtoUnitData* unit = nullptr;
    if (it != name_index_.end()) {
        unit = &units_[it->second];
    } else {
        // Create new unit if not found in XMB
        ProtoUnitData new_unit;
        new_unit.name = std::string(stats.name);
        new_unit.dbid = static_cast<int>(stats.proto_id);
        new_unit.from_protox_xml = true;

        name_index_[key] = units_.size();
        units_.push_back(std::move(new_unit));
        unit = &units_.back();
    }

    // Override with protox.xml values (these are the REAL gameplay values)
    unit->max_hitpoints = stats.max_hp;
    unit->initial_hitpoints = stats.initial_hp;
    unit->max_velocity = stats.move_speed;
    unit->acceleration = stats.acceleration;
    unit->deceleration = stats.deceleration;
    unit->los = stats.los;
    unit->population_count = stats.pop_cost;

    // Cost
    unit->cost.food = static_cast<float>(stats.cost_food);
    unit->cost.wood = static_cast<float>(stats.cost_wood);
    unit->cost.gold = static_cast<float>(stats.cost_gold);
    unit->cost.favor = static_cast<float>(stats.cost_favor);

    // Economy
    unit->train_points = stats.train_points;
    unit->allowed_age = stats.allowed_age;

    // Attack
    unit->attack.damage = stats.damage;
    unit->attack.damage_type = aom::DamageTypeName(stats.damage_type);
    unit->attack.range = stats.attack_range;
    unit->attack.reload_time = stats.attack_cooldown;
    unit->attack.accuracy = stats.accuracy;
    unit->attack.max_spread = stats.max_spread;
    unit->attack.turn_rate = stats.turn_rate_deg_s;
    unit->attack.spread_factor = stats.spread_factor;
    unit->attack.track_rating = stats.track_rating;
    unit->attack.projectile_proto = stats.projectile_proto;

    // Damage bonuses
    unit->attack.damage_bonuses.clear();
    for (const auto& bonus : stats.damage_bonuses) {
        ProtoUnitAttack::DamageBonus bonus_copy;
        bonus_copy.target_type = bonus.target_type;
        bonus_copy.multiplier = bonus.multiplier;
        unit->attack.damage_bonuses.push_back(std::move(bonus_copy));
    }

    // Armor
    unit->armor_hack = stats.armor[static_cast<size_t>(aom::DamageType::Hack)];
    unit->armor_pierce = stats.armor[static_cast<size_t>(aom::DamageType::Pierce)];
    unit->armor_crush = stats.armor[static_cast<size_t>(aom::DamageType::Crush)];

    // Projectile proto
    unit->projectile_proto = stats.projectile_proto;

    unit->can_attack = (stats.damage > 0.0f);
    unit->can_move = (stats.move_speed > 0.1f);
    unit->from_protox_xml = true;

    // Re-classify based on protox.xml stats
    auto lname = to_lower(unit->name);
    unit->military = (lname.find("hoplite") != std::string::npos ||
                      lname.find("toxotes") != std::string::npos ||
                      lname.find("minotaur") != std::string::npos ||
                      lname.find("ulfsark") != std::string::npos ||
                      lname.find("huskarl") != std::string::npos ||
                      lname.find("throwing axeman") != std::string::npos ||
                      stats.damage > 0.0f);
}

} // namespace aom
