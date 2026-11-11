#include "data/proto_unit.h"

#include <algorithm>
#include <cctype>

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

} // namespace aom
