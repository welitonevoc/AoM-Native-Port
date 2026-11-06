#include "data/water_bodies.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

/// Faz split de uma string de números separados por espaço (ex: "0 50 170")
std::vector<float> parse_floats(const std::string& str) {
    std::vector<float> out;
    std::istringstream iss(str);
    float v;
    while (iss >> v) {
        out.push_back(v);
    }
    return out;
}

} // anonymous namespace

glm::vec3 WaterBodyManager::parse_color(const std::string& str) {
    auto vals = parse_floats(str);
    if (vals.size() >= 3) {
        return glm::vec3(vals[0], vals[1], vals[2]);
    }
    return glm::vec3(0, 50, 170); // default: água escura
}

bool WaterBodyManager::load_from_xmb(const XmbElement& root) {
    bool ok = false;

    for (const auto& child : root.children) {
        if (!child) continue;

        if (child->name == "river") {
            RiverDef r;
            r.name = child->get_attribute("name");
            r.icon_name = child->get_attribute("iconname");
            r.surface = child->get_attribute("surface");
            r.bottom = child->get_attribute("bottom");
            r.bank = child->get_attribute("bank");
            r.outer_bank = child->get_attribute("outerbank");

            if (auto c = child->get_attribute("color"); !c.empty()) {
                r.color = parse_color(c);
            }
            r.depth = child->get_attribute_as_float("depth", 3.0f);
            r.slope = child->get_attribute_as_float("slope", 0.0f);

            // Leitura de sub-elementos
            for (const auto& sub : child->children) {
                if (!sub) continue;
                if (sub->name == "objectplacement") {
                    r.curve_angle = sub->get_attribute_as_float("curveangle", 15.0f);
                    r.distance_between = sub->get_attribute_as_float("distancebetween", 30.0f);
                    r.distance_variation = sub->get_attribute_as_float("distancevariation", 10.0f);
                    r.lock_to_water_edge = sub->get_attribute_as_bool("locktowateredge", false);
                } else if (sub->name == "textureplacement") {
                    for (const auto& tex : sub->children) {
                        if (!tex || tex->name != "texture") continue;
                        TexturePlacement tp;
                        tp.distance = tex->get_attribute_as_float("distance", 0.0f);
                        tp.texture = tex->text;
                        if (!tp.texture.empty()) {
                            r.texture_placements.push_back(std::move(tp));
                        }
                    }
                } else if (sub->name == "objectcluster") {
                    ObjectCluster oc;
                    for (const auto& pu : sub->children) {
                        if (!pu || pu->name != "protounit") continue;
                        oc.protounits.push_back(pu->get_attribute("name") );
                        if (oc.protounits.back().empty()) {
                            oc.protounits.back() = pu->text;
                        }
                    }
                    if (!oc.protounits.empty()) {
                        r.object_clusters.push_back(std::move(oc));
                    }
                }
            }

            if (!r.name.empty()) {
                const std::string key = to_lower(r.name);
                const auto it = river_index_.find(key);
                if (it != river_index_.end()) {
                    rivers_[it->second] = std::move(r);
                } else {
                    river_index_[key] = rivers_.size();
                    rivers_.push_back(std::move(r));
                }
                ok = true;
            }
        }
        else if (child->name == "ocean") {
            OceanDef o;
            o.name = child->get_attribute("name");
            o.icon_name = child->get_attribute("iconname");
            o.surface = child->get_attribute("surface");
            o.bottom = child->get_attribute("bottom");
            o.bank = child->get_attribute("bank");
            o.outer_bank = child->get_attribute("outerbank");

            if (auto c = child->get_attribute("color"); !c.empty()) {
                o.color = parse_color(c);
            }
            o.depth = child->get_attribute_as_float("depth", 4.0f);

            for (const auto& sub : child->children) {
                if (!sub) continue;
                if (sub->name == "textureplacement") {
                    for (const auto& tex : sub->children) {
                        if (!tex || tex->name != "texture") continue;
                        TexturePlacement tp;
                        tp.distance = tex->get_attribute_as_float("distance", 0.0f);
                        tp.texture = tex->text;
                        if (!tp.texture.empty()) {
                            o.texture_placements.push_back(std::move(tp));
                        }
                    }
                } else if (sub->name == "objectcluster") {
                    ObjectCluster oc;
                    for (const auto& pu : sub->children) {
                        if (!pu || pu->name != "protounit") continue;
                        oc.protounits.push_back(pu->get_attribute("name"));
                        if (oc.protounits.back().empty()) {
                            oc.protounits.back() = pu->text;
                        }
                    }
                    if (!oc.protounits.empty()) {
                        o.object_clusters.push_back(std::move(oc));
                    }
                }
            }

            if (!o.name.empty()) {
                const std::string key = to_lower(o.name);
                const auto it = ocean_index_.find(key);
                if (it != ocean_index_.end()) {
                    oceans_[it->second] = std::move(o);
                } else {
                    ocean_index_[key] = oceans_.size();
                    oceans_.push_back(std::move(o));
                }
                ok = true;
            }
        }
        else if (child->name == "lake") {
            LakeDef l;
            l.name = child->get_attribute("name");
            l.icon_name = child->get_attribute("iconname");
            l.surface = child->get_attribute("surface");
            l.bottom = child->get_attribute("bottom");
            l.bank = child->get_attribute("bank");
            l.outer_bank = child->get_attribute("outerbank");

            if (auto c = child->get_attribute("color"); !c.empty()) {
                l.color = parse_color(c);
            }
            l.depth = child->get_attribute_as_float("depth", 3.0f);
            l.slope = child->get_attribute_as_float("slope", 0.0f);

            for (const auto& sub : child->children) {
                if (!sub) continue;
                if (sub->name == "objectplacement") {
                    l.curve_angle = sub->get_attribute_as_float("curveangle", 15.0f);
                    l.distance_between = sub->get_attribute_as_float("distancebetween", 40.0f);
                    l.distance_variation = sub->get_attribute_as_float("distancevariation", 10.0f);
                    l.lock_to_water_edge = sub->get_attribute_as_bool("locktowateredge", false);
                } else if (sub->name == "objectcluster") {
                    ObjectCluster oc;
                    for (const auto& pu : sub->children) {
                        if (!pu || pu->name != "protounit") continue;
                        oc.protounits.push_back(pu->get_attribute("name"));
                        if (oc.protounits.back().empty()) {
                            oc.protounits.back() = pu->text;
                        }
                    }
                    if (!oc.protounits.empty()) {
                        l.object_clusters.push_back(std::move(oc));
                    }
                }
            }

            if (!l.name.empty()) {
                const std::string key = to_lower(l.name);
                const auto it = lake_index_.find(key);
                if (it != lake_index_.end()) {
                    lakes_[it->second] = std::move(l);
                } else {
                    lake_index_[key] = lakes_.size();
                    lakes_.push_back(std::move(l));
                }
                ok = true;
            }
        }
    }

    return ok;
}

const RiverDef* WaterBodyManager::find_river(const std::string& name) const {
    auto it = river_index_.find(to_lower(name));
    if (it != river_index_.end()) return &rivers_[it->second];
    return nullptr;
}

const OceanDef* WaterBodyManager::find_ocean(const std::string& name) const {
    auto it = ocean_index_.find(to_lower(name));
    if (it != ocean_index_.end()) return &oceans_[it->second];
    return nullptr;
}

const LakeDef* WaterBodyManager::find_lake(const std::string& name) const {
    auto it = lake_index_.find(to_lower(name));
    if (it != lake_index_.end()) return &lakes_[it->second];
    return nullptr;
}

} // namespace aom
