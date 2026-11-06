#pragma once

/// @file game_data.h
/// @brief Native registry and database for Age of Mythology proto units, major gods, and tech tree

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"
#include "data/water_bodies.h"
#include "data/unit_transform.h"

namespace aom {

/// Represents gameplay statistics extracted from proto.xmb
struct ProtoUnitData {
    std::string name;
    int dbid = 0;
    int display_name_id = 0;
    float max_hitpoints = 100.0f;
    float los = 16.0f;
    float max_velocity = 4.0f;
    float cost_food = 0.0f;
    float cost_wood = 0.0f;
    float cost_gold = 0.0f;
    float cost_favor = 0.0f;
};

/// Represents Major God configuration from majorgods.xmb
struct MajorGodData {
    std::string tech_name;
    int name_id = 0;
    std::vector<std::string> minor_gods;
};

/// Represents technology tree nodes from techtree.xmb
struct TechData {
    std::string name;
    int dbid = 0;
    int display_name_id = 0;
    float research_points = 0.0f;
};

/// Central manager and database for Age of Mythology game rules and entities
class GameDataManager {
public:
    GameDataManager() = default;
    ~GameDataManager() = default;

    /// Extract and load all core definitions from a data.bar archive
    bool load_from_bar(const BarArchive& bar);

    /// Parse proto.xmb root element
    bool load_proto_xmb(const XmbElement& root);

    /// Parse majorgods.xmb root element
    bool load_majorgods_xmb(const XmbElement& root);

    /// Parse techtree.xmb root element
    bool load_techtree_xmb(const XmbElement& root);

    /// Parse waterbodies.xmb root element
    bool load_waterbodies_xmb(const XmbElement& root);

    /// Parse unittransform.xmb root element
    bool load_unittransform_xmb(const XmbElement& root);

    /// Find unit definition by case-insensitive name
    [[nodiscard]] const ProtoUnitData* find_unit(const std::string& name) const;

    /// Find major god by tech name
    [[nodiscard]] const MajorGodData* find_god(const std::string& tech_name) const;

    /// Find technology by name
    [[nodiscard]] const TechData* find_tech(const std::string& name) const;

    [[nodiscard]] size_t unit_count() const { return units_.size(); }
    [[nodiscard]] size_t god_count() const { return gods_.size(); }
    [[nodiscard]] size_t tech_count() const { return techs_.size(); }

    /// Water bodies access
    [[nodiscard]] const WaterBodyManager& water_bodies() const { return water_bodies_; }
    [[nodiscard]] WaterBodyManager& water_bodies() { return water_bodies_; }

    /// Unit transforms access
    [[nodiscard]] const UnitTransformManager& unit_transforms() const { return unit_transforms_; }
    [[nodiscard]] UnitTransformManager& unit_transforms() { return unit_transforms_; }

    [[nodiscard]] const std::vector<ProtoUnitData>& all_units() const { return units_; }
    [[nodiscard]] const std::vector<MajorGodData>& all_gods() const { return gods_; }

private:
    std::vector<ProtoUnitData> units_;
    std::unordered_map<std::string, size_t> unit_index_;

    std::vector<MajorGodData> gods_;
    std::unordered_map<std::string, size_t> god_index_;

    std::vector<TechData> techs_;
    std::unordered_map<std::string, size_t> tech_index_;

    WaterBodyManager water_bodies_;
    UnitTransformManager unit_transforms_;
};

} // namespace aom
