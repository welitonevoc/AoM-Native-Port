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
    bar_ = &bar;
    bool ok = false;

    // proto.xmb — unidades do jogo base
    if (load_xmb("proto.xmb", &GameDataManager::load_proto_xmb)) ok = true;

    // protox.xmb — unidades da expansão The Titans (sobrescreve/estende proto.xmb)
    if (load_xmb("protox.xmb", &GameDataManager::load_proto_xmb)) ok = true;

    // majorgods.xmb — deuses maiores
    if (load_xmb("majorgods.xmb", &GameDataManager::load_majorgods_xmb)) ok = true;

    // minorgods.xml — deuses menores (pode estar no data2.bar)
    if (load_xmb("minorgods.xml", &GameDataManager::load_minorgods_xmb)) ok = true;

    // techtree.xmb — árvore tecnológica base
    if (load_xmb("techtree.xmb", &GameDataManager::load_techtree_xmb)) ok = true;

    // techtreex.xmb — árvore tecnológica da expansão
    if (load_xmb("techtreex.xmb", &GameDataManager::load_techtree_xmb)) ok = true;

    // waterbodies.xmb — corpos d'água
    if (load_xmb("waterbodies.xmb", &GameDataManager::load_waterbodies_xmb)) ok = true;

    // unittransform.xmb — transformações de unidades
    if (load_xmb("unittransform.xmb", &GameDataManager::load_unittransform_xmb)) ok = true;

    // soundsets.xml — conjuntos de som
    if (load_xmb("soundsets.xml", &GameDataManager::load_soundsets_xmb)) ok = true;

    return ok;
}

bool GameDataManager::load_proto_xmb(const XmbElement& root) {
    return proto_units_.load_from_xmb(root);
}

bool GameDataManager::load_majorgods_xmb(const XmbElement& root) {
    return gods_.load_majorgods_from_xmb(root);
}

bool GameDataManager::load_minorgods_xmb(const XmbElement& root) {
    return gods_.load_minorgods_from_xmb(root);
}

bool GameDataManager::load_techtree_xmb(const XmbElement& root) {
    bool ok = false;
    for (const auto& child : root.children) {
        if (!child || child->name != "tech") continue;

        TechData t;
        t.name = child->get_attribute("name");
        if (t.name.empty()) continue;

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
        auto it = tech_index_.find(key);
        if (it != tech_index_.end()) {
            techs_[it->second] = std::move(t);
        } else {
            tech_index_[key] = techs_.size();
            techs_.push_back(std::move(t));
        }
        ok = true;
    }
    return ok;
}

bool GameDataManager::load_waterbodies_xmb(const XmbElement& root) {
    return water_bodies_.load_from_xmb(root);
}

bool GameDataManager::load_unittransform_xmb(const XmbElement& root) {
    return unit_transforms_.load_from_xmb(root);
}

bool GameDataManager::load_soundsets_xmb(const XmbElement& root) {
    return sounds_.load_soundsets_from_xmb(root);
}

bool GameDataManager::load_protounit_sounds_xmb(const XmbElement& root) {
    return sounds_.load_protounit_sounds_from_xmb(root);
}

const ProtoUnitData* GameDataManager::find_unit(const std::string& name) const {
    return proto_units_.find(name);
}

const MajorGodData* GameDataManager::find_god(const std::string& tech_name) const {
    return gods_.find_major(tech_name);
}

const TechData* GameDataManager::find_tech(const std::string& name) const {
    auto it = tech_index_.find(to_lower(name));
    if (it != tech_index_.end()) return &techs_[it->second];
    return nullptr;
}

const MinorGodData* GameDataManager::find_minor_god(const std::string& tech_name) const {
    return gods_.find_minor(tech_name);
}

const SoundSetDef* GameDataManager::find_soundset(const std::string& name) const {
    return sounds_.find_soundset(name);
}

const ProtoUnitSoundDef* GameDataManager::find_protounit_sound(const std::string& unit_name) const {
    return sounds_.find_protounit_sound(unit_name);
}

} // namespace aom
