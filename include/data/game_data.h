#pragma once

/// @file game_data.h
/// @brief Native registry and database for all Age of Mythology game data:
///        proto units, tech tree, major/minor gods, water bodies, unit transforms,
///        and sound definitions. Consumes data.bar + data2.bar (The Titans expansion).

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"
#include "data/water_bodies.h"
#include "data/unit_transform.h"
#include "data/proto_unit.h"
#include "data/major_god.h"
#include "data/sound_def.h"

namespace aom {

/// Technology data from techtree.xmb / techtreex.xmb
struct TechData {
    std::string name;
    int dbid = 0;
    int display_name_id = 0;
    float research_points = 0.0f;
};

/// Central manager and database for all Age of Mythology game rules and entities.
/// Loads data from data.bar (base game) and data2.bar (The Titans expansion)
/// via BarArchive mounts (overlay semantics: last mount wins).
class GameDataManager {
public:
    GameDataManager() = default;
    ~GameDataManager() = default;

    /// Extract and load all core definitions from a data.bar archive.
    /// Call once per archive mount (base first, expansion second).
    bool load_from_bar(const BarArchive& bar);

    /// --- Individual XMB parsers ---

    /// Parse proto.xmb / protox.xmb — complete unit definitions
    bool load_proto_xmb(const XmbElement& root);

    /// Parse majorgods.xmb — major gods
    bool load_majorgods_xmb(const XmbElement& root);

    /// Parse minorgods.xml — minor gods
    bool load_minorgods_xmb(const XmbElement& root);

    /// Parse techtree.xmb / techtreex.xmb — technology tree
    bool load_techtree_xmb(const XmbElement& root);

    /// Parse waterbodies.xmb — rivers, oceans, lakes
    bool load_waterbodies_xmb(const XmbElement& root);

    /// Parse unittransform.xmb — unit transformations (Wall Long -> Gate, etc.)
    bool load_unittransform_xmb(const XmbElement& root);

    /// Parse soundsets.xml + *_snds.xml — sound definitions
    bool load_soundsets_xmb(const XmbElement& root);
    bool load_protounit_sounds_xmb(const XmbElement& root);

    /// --- Lookups ---

    /// Find unit definition by case-insensitive name
    [[nodiscard]] const ProtoUnitData* find_unit(const std::string& name) const;

    /// Find major god by tech name
    [[nodiscard]] const MajorGodData* find_god(const std::string& tech_name) const;

    /// Find technology by name
    [[nodiscard]] const TechData* find_tech(const std::string& name) const;

    /// Find minor god by tech name
    [[nodiscard]] const MinorGodData* find_minor_god(const std::string& tech_name) const;

    /// Find soundset by name
    [[nodiscard]] const SoundSetDef* find_soundset(const std::string& name) const;

    /// Find protounit sound definition by unit name
    [[nodiscard]] const ProtoUnitSoundDef* find_protounit_sound(const std::string& unit_name) const;

    /// --- Counts ---
    [[nodiscard]] size_t unit_count() const { return proto_units_.count(); }
    [[nodiscard]] size_t god_count() const { return gods_.major_count(); }
    [[nodiscard]] size_t tech_count() const { return techs_.size(); }
    [[nodiscard]] size_t minor_god_count() const { return gods_.minor_count(); }
    [[nodiscard]] size_t soundset_count() const { return sounds_.soundset_count(); }

    /// --- Accessors ---
    [[nodiscard]] const ProtoUnitRegistry& proto_units() const { return proto_units_; }
    [[nodiscard]] ProtoUnitRegistry& proto_units() { return proto_units_; }

    [[nodiscard]] const GodRegistry& gods() const { return gods_; }
    [[nodiscard]] GodRegistry& gods() { return gods_; }

    [[nodiscard]] const WaterBodyManager& water_bodies() const { return water_bodies_; }
    [[nodiscard]] WaterBodyManager& water_bodies() { return water_bodies_; }

    [[nodiscard]] const UnitTransformManager& unit_transforms() const { return unit_transforms_; }
    [[nodiscard]] UnitTransformManager& unit_transforms() { return unit_transforms_; }

    [[nodiscard]] const SoundDefManager& sounds() const { return sounds_; }
    [[nodiscard]] SoundDefManager& sounds() { return sounds_; }

    [[nodiscard]] const std::vector<ProtoUnitData>& all_units() const { return proto_units_.all(); }
    [[nodiscard]] const std::vector<MajorGodData>& all_gods() const { return gods_.all_majors(); }
    [[nodiscard]] const std::vector<TechData>& all_techs() const { return techs_; }

private:
    /// Helper: carrega um XMB do archive, se existir. Retorna false se não existir (não é erro).
    template <typename LoaderFn>
    bool load_xmb(const char* filename, LoaderFn loader) {
        if (!bar_) return false;
        const BarEntry* entry = bar_->find(filename);
        if (!entry) return false;
        const std::vector<uint8_t> bytes = bar_->extract(*entry);
        if (bytes.empty()) return false;
        auto root = XmbParser::parse(bytes);
        if (!root) return false;
        return (this->*loader)(*root);
    }

    const BarArchive* bar_ = nullptr;  ///< Último archive carregado (para referência)

    ProtoUnitRegistry proto_units_;        ///< proto.xmb + protox.xmb
    GodRegistry gods_;                     ///< majorgods.xmb + minorgods.xml
    std::unordered_map<std::string, size_t> tech_index_;

    WaterBodyManager water_bodies_;        ///< waterbodies.xmb
    UnitTransformManager unit_transforms_; ///< unittransform.xmb
    SoundDefManager sounds_;               ///< soundsets.xml + *_snds.xml
    std::vector<TechData> techs_;          ///< techtree.xmb + techtreex.xmb
};

} // namespace aom
