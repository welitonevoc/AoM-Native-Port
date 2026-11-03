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

    // Load proto.xmb
    auto proto_bytes = bar.extract("proto.xmb");
    if (!proto_bytes.empty()) {
        auto proto_root = XmbParser::parse(proto_bytes);
        if (proto_root && load_proto_xmb(*proto_root)) {
            ok = true;
        }
    }

    // Load majorgods.xmb
    auto gods_bytes = bar.extract("majorgods.xmb");
    if (!gods_bytes.empty()) {
        auto gods_root = XmbParser::parse(gods_bytes);
        if (gods_root) {
            load_majorgods_xmb(*gods_root);
        }
    }

    // Load techtree.xmb
    auto tech_bytes = bar.extract("techtree.xmb");
    if (!tech_bytes.empty()) {
        auto tech_root = XmbParser::parse(tech_bytes);
        if (tech_root) {
            load_techtree_xmb(*tech_root);
        }
    }

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

        unit_index_[to_lower(u.name)] = units_.size();
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
            god_index_[to_lower(g.tech_name)] = gods_.size();
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

        tech_index_[to_lower(t.name)] = techs_.size();
        techs_.push_back(std::move(t));
    }
    return !techs_.empty();
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
