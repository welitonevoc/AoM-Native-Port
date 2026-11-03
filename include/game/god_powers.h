#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace aom {

class BarArchive;
struct XmbElement;

enum class GodPowerTargetType {
    UNIT,
    AREA,
    GROUND,
    GLOBAL,
    WATER,
    UNKNOWN
};

enum class GodPowerCategory {
    OFFENSIVE,   // Bolt, Meteor, Earthquake, Lightning Storm, Implode
    DEFENSIVE,   // Cease Fire, Restoration, Bronze, Sentinel, Frost
    ECONOMIC,    // Rain, Prosperity, Great Hunt, Plenty, Gaia Forest
    SUMMONING,   // Serpents, Nidhogg, Son of Osiris, Tartarian Gate
    UTILITY      // Vision, Spy, Underworld Passage, Vortex, Reverse Time
};

struct GodPowerDef {
    std::string name;
    std::string tech_name;
    GodPowerTargetType target_type = GodPowerTargetType::UNIT;
    GodPowerCategory category = GodPowerCategory::OFFENSIVE;
    
    float active_time = 0.0f;
    float buildup_time = 0.0f;
    float radius = 0.0f;
    float accuracy = 1.0f;
    
    // Target constraints
    std::string valid_target_type;
    std::string match_type; // enemy, ally, self, all
    
    // Damage model
    float damage_unit_percent = 0.0f;
    float damage_villager_percent = 0.0f;
    float damage_building_percent = 0.0f;
    float min_hp_damage_unit = 0.0f;
    float max_hp_damage_unit = 0.0f;
    
    // Physics throwing
    float unit_throw_chance = 0.0f;
    float min_throw_distance = 0.0f;
    float max_throw_distance = 0.0f;
    float min_throw_height = 0.0f;
    
    // Audio / Visual triggers
    std::string sound_set;
    std::string strike_vfx;
    std::string sparks_vfx;
    std::string scorch_vfx;
    std::string icon;
    std::string used_icon;
    
    bool is_expansion = false; // Titans / godpowers2.bar
};

struct ActiveGodPower {
    std::string power_name;
    uint32_t caster_player_id = 0;
    glm::vec3 target_pos{0.0f};
    uint32_t target_unit_id = 0;
    float elapsed_time = 0.0f;
    float total_duration = 0.0f;
    bool is_active = true;
};

class GodPowerSystem {
public:
    GodPowerSystem();
    ~GodPowerSystem();

    /// Load god power definitions from godpowers.bar and optionally godpowers2.bar
    bool load_from_bar(BarArchive* archive, bool is_expansion = false);

    /// Load directly from an XMB element or memory buffer
    bool load_power_xmb(const uint8_t* data, size_t size, bool is_expansion = false);
    bool load_power_from_element(const XmbElement& root, bool is_expansion = false);
    void register_power(const GodPowerDef& def);

    /// Query god powers
    [[nodiscard]] size_t power_count() const { return powers_.size(); }
    [[nodiscard]] const GodPowerDef* find_power(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<std::string, GodPowerDef>& all_powers() const { return powers_; }

    /// Cast a god power
    bool cast_power(const std::string& name, uint32_t player_id, const glm::vec3& target_pos, uint32_t target_unit_id = 0);

    /// Update active god powers
    void update(float delta_time);

    /// Active power count
    [[nodiscard]] size_t active_count() const { return active_powers_.size(); }
    [[nodiscard]] const std::vector<ActiveGodPower>& active_powers() const { return active_powers_; }

    /// Callbacks
    using OnPowerCastCallback = std::function<void(const ActiveGodPower& power)>;
    void set_on_power_cast(OnPowerCastCallback cb) { on_power_cast_ = std::move(cb); }

private:
    std::unordered_map<std::string, GodPowerDef> powers_;
    std::vector<ActiveGodPower> active_powers_;
    OnPowerCastCallback on_power_cast_;
};

} // namespace aom
