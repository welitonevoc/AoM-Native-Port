#include "data/game_data.h"

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

} // anonymous namespace

bool GameDataManager::load_from_bar(const BarArchive& bar) {
    bool ok = false;

    // Carrega um XMB do archive, se existir. `find()` é usado antes de
    // `extract()` para não gerar log de erro quando o arquivo não existe
    // (ex.: protox.xmb só está presente na expansão The Titans).
    auto load_xmb = [&](const char* filename,
                        bool (GameDataManager::*loader)(const XmbElement&)) -> bool {
        const BarEntry* entry = bar.find(filename);
        if (!entry) return false;
        const std::vector<uint8_t> bytes = bar.extract(*entry);
        if (bytes.empty()) return false;
        auto root = XmbParser::parse(bytes);
        if (!root) return false;
        return (this->*loader)(*root);
    };

    // proto.xmb — unidades e construções do jogo base
    if (load_xmb("proto.xmb", &GameDataManager::load_proto_xmb)) {
        ok = true;
    }

    // protox.xmb — unidades e construções da expansão The Titans
    if (load_xmb("protox.xmb", &GameDataManager::load_proto_xmb)) {
        ok = true;
    }

    // majorgods.xmb — deuses maiores
    load_xmb("majorgods.xmb", &GameDataManager::load_majorgods_xmb);

    // techtree.xmb — árvore tecnológica do jogo base
    load_xmb("techtree.xmb", &GameDataManager::load_techtree_xmb);

    // techtreex.xmb — árvore tecnológica da expansão The Titans
    load_xmb("techtreex.xmb", &GameDataManager::load_techtree_xmb);

    // waterbodies.xmb — corpos d'água (rios, oceanos, lagos)
    load_xmb("waterbodies.xmb", &GameDataManager::load_waterbodies_xmb);

    // unittransform.xmb — transformações de unidades
    load_xmb("unittransform.xmb", &GameDataManager::load_unittransform_xmb);

    return ok;
}

bool GameDataManager::load_proto_xmb(const XmbElement& root) {
    for (const auto& child : root.children) {
        if (!child || child->name != "unit") {
            continue;
        }

        ProtoUnitData u;
        u.name = child->get_attribute("name");
        if (u.name.empty()) {
            continue;
        }

        for (const auto& prop : child->children) {
            if (!prop) continue;
            if (prop->name == "dbid") {
                u.dbid = prop->text_as_int();
            } else if (prop->name == "displaynameid") {
                u.display_name_id = prop->text_as_int();
            } else if (prop->name == "maxhitpoints") {
                u.max_hitpoints = prop->text_as_float();
            } else if (prop->name == "los") {
                u.los = prop->text_as_float();
            } else if (prop->name == "maxvelocity") {
                u.max_velocity = prop->text_as_float();
            } else if (prop->name == "cost") {
                std::string rtype = prop->get_attribute("resourcetype");
                float amount = prop->text_as_float();
                if (rtype == "Food") u.cost_food = amount;
                else if (rtype == "Wood") u.cost_wood = amount;
                else if (rtype == "Gold") u.cost_gold = amount;
                else if (rtype == "Favor") u.cost_favor = amount;
            }
        }

        // Substitui definições repetidas: protox.xmb (expansão) é um
        // super-set de proto.xmb (base) e traz a versão atualizada.
        const std::string key = to_lower(u.name);
        const auto it = unit_index_.find(key);
        if (it != unit_index_.end()) {
            units_[it->second] = std::move(u);
            continue;
        }

        unit_index_[key] = units_.size();
        units_.push_back(std::move(u));
    }
    return !units_.empty();
}

bool GameDataManager::load_majorgods_xmb(const XmbElement& root) {
    for (const auto& child : root.children) {
        if (!child || child->name != "god") {
            continue;
        }

        MajorGodData g;
        g.tech_name = child->get_attribute("techname");
        std::string nid = child->get_attribute("nameid");
        if (!nid.empty()) {
            g.name_id = std::stoi(nid);
        }

        for (const auto& mg : child->children) {
            if (mg && mg->name == "minorgod") {
                std::string mg_name = mg->get_attribute("techname");
                if (mg_name.empty()) {
                    mg_name = mg->get_attribute("nameid");
                }
                if (!mg_name.empty()) {
                    g.minor_gods.push_back(mg_name);
                }
            }
        }

        if (!g.tech_name.empty()) {
            const std::string key = to_lower(g.tech_name);
            const auto it = god_index_.find(key);
            if (it != god_index_.end()) {
                gods_[it->second] = std::move(g);
                continue;
            }
            god_index_[key] = gods_.size();
        }
        gods_.push_back(std::move(g));
    }
    return !gods_.empty();
}

bool GameDataManager::load_techtree_xmb(const XmbElement& root) {
    for (const auto& child : root.children) {
        if (!child || child->name != "tech") {
            continue;
        }

        TechData t;
        t.name = child->get_attribute("name");
        if (t.name.empty()) {
            continue;
        }

        for (const auto& prop : child->children) {
            if (!prop) continue;
            if (prop->name == "dbid") {
                t.dbid = prop->text_as_int();
            } else if (prop->name == "displaynameid") {
                t.display_name_id = prop->text_as_int();
            } else if (prop->name == "researchpoints") {
                t.research_points = prop->text_as_float();
            }
        }

        const std::string key = to_lower(t.name);
        const auto it = tech_index_.find(key);
        if (it != tech_index_.end()) {
            techs_[it->second] = std::move(t);
            continue;
        }
        tech_index_[key] = techs_.size();
        techs_.push_back(std::move(t));
    }
    return !techs_.empty();
}

bool GameDataManager::load_waterbodies_xmb(const XmbElement& root) {
    return water_bodies_.load_from_xmb(root);
}

bool GameDataManager::load_unittransform_xmb(const XmbElement& root) {
    return unit_transforms_.load_from_xmb(root);
}

const ProtoUnitData* GameDataManager::find_unit(const std::string& name) const {
    auto it = unit_index_.find(to_lower(name));
    if (it != unit_index_.end()) {
        return &units_[it->second];
    }
    return nullptr;
}

const MajorGodData* GameDataManager::find_god(const std::string& tech_name) const {
    auto it = god_index_.find(to_lower(tech_name));
    if (it != god_index_.end()) {
        return &gods_[it->second];
    }
    return nullptr;
}

const TechData* GameDataManager::find_tech(const std::string& name) const {
    auto it = tech_index_.find(to_lower(name));
    if (it != tech_index_.end()) {
        return &techs_[it->second];
    }
    return nullptr;
}

} // namespace aom
