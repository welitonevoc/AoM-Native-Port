#pragma once

/**
 * Unit Stats — Authoritative gameplay constants from protox.xml
 * 
 * SOURCE: Craft Anything reverse engineering of Age of Mythology: The Titans
 * Original data file: Voobly Balance Patch 6.0\data\protox.xml
 * 
 * IMPORTANT: These values are read from protox.xml at runtime in the original game.
 * The executable (aomx.exe) is packed and stripped — NO gameplay functions exist in code.
 * This header mirrors the data-driven approach: stats are loaded from parsed protox.xml,
 * but compile-time defaults are provided here for fallback/testing.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <vector>

namespace aom {

// Damage types as they appear in protox.xml (percentflag="1" = percentage reduction)
enum class DamageType : uint8_t {
    Hack    = 0,  // Melee / hacking damage
    Pierce  = 1,  // Ranged / piercing damage  
    Crush   = 2,  // Siege / crushing damage
    COUNT   = 3
};

inline constexpr std::string_view DamageTypeName(DamageType dt) {
    switch (dt) {
        case DamageType::Hack:   return "Hack";
        case DamageType::Pierce: return "Pierce";
        case DamageType::Crush:  return "Crush";
        default:                 return "Unknown";
    }
}

// Damage formula: dealt = raw_damage * (1 - armor[damage_type])
// Armor is a fraction (0.35 = 35% reduction)
inline float ApplyDamage(float raw_damage, float armor_value) {
    return raw_damage * (1.0f - armor_value);
}

// Unit type identifiers
enum class UnitType : uint8_t {
    Hoplite = 0,
    Toxotes = 1,
    COUNT   = 2
};

inline constexpr std::string_view UnitTypeName(UnitType ut) {
    switch (ut) {
        case UnitType::Hoplite: return "hoplite";
        case UnitType::Toxotes: return "toxotes";
        default:                return "unknown";
    }
}

// Unit stats structure — matches protox.xml fields
struct ProtoxUnitStats {
    uint32_t        proto_id;           // protox.xml unit id (503, 508)
    std::string     name;               // Unit name
    
    // Core stats
    float           max_hp;             // <maxhitpoints>
    float           initial_hp;         // <initialhitpoints> (may differ from max_hp)
    float           move_speed;         // <maxvelocity> (world units/sec)
    float           turn_rate_deg_s;    // <turnrate> (degrees/sec)
    float           los;                // <los> line of sight
    
    // Economy
    int             pop_cost = 1;       // <populationcount>
    int             cost_food = 0;      // <cost resourcetype="Food">
    int             cost_wood = 0;      // <cost resourcetype="Wood">
    int             cost_gold = 0;      // <cost resourcetype="Gold">
    int             cost_favor = 0;     // <cost resourcetype="Favor">
    float           train_points = 0.0f; // <trainpoints>
    int             allowed_age = 0;    // <allowedage>
    
    // Combat
    float           damage;             // HandAttack/RangedAttack Damage value1
    DamageType      damage_type;        // Damage type (Hack/Pierce/Crush)
    float           attack_range;       // MaximumRange
    float           attack_cooldown;    // Derived from Rate (seconds per attack)
    float           accuracy;           // RangedAttack Accuracy (0.0-1.0), 1.0 for melee
    float           max_spread;         // RangedAttack MaxSpread (world units at target)
    float           spread_factor = 0.0f; // RangedAttack SpreadFactor
    float           track_rating = 0.0f;  // RangedAttack TrackRating
    std::string     projectile_proto;   // <projectileprotounit>
    
    // Armor (percentage reduction, percentflag="1")
    std::array<float, 3> armor;         // [Hack, Pierce, Crush]
    
    // Movement (estimated, not in protox.xml)
    float           acceleration = 0.0f;
    float           deceleration = 0.0f;
    
    // Damage bonuses (vs specific unit types)
    struct DamageBonus {
        std::string target_type;
        float multiplier = 1.0f;
    };
    std::vector<DamageBonus> damage_bonuses;
};

// Compile-time default stats (from protox.xml Voobly Balance Patch 6.0)
// ProtoxParser should override these with parsed values at runtime.
inline const ProtoxUnitStats kDefaultUnitStats[static_cast<size_t>(UnitType::COUNT)] = {
    // Hoplite (id=503, protox.xml line 24036)
    ProtoxUnitStats{
        .proto_id       = 503,
        .name           = "Hoplite",
        .max_hp         = 115.0f,
        .initial_hp     = 115.0f,
        .move_speed     = 4.2f,
        .turn_rate_deg_s = 18.0f,
        .los            = 16.0f,
        .pop_cost       = 2,
        .cost_food      = 50,
        .cost_wood      = 0,
        .cost_gold      = 40,
        .cost_favor     = 0,
        .train_points   = 14.0f,
        .allowed_age    = 2,
        .damage         = 8.0f,
        .damage_type    = DamageType::Hack,
        .attack_range   = 0.3f,
        .attack_cooldown = 1.0f,
        .accuracy       = 1.0f,
        .max_spread     = 0.0f,
        .spread_factor  = 0.0f,
        .track_rating   = 0.0f,
        .projectile_proto = "",
        .armor          = {0.35f, 0.15f, 0.99f},  // Hack, Pierce, Crush
        .acceleration   = 0.0f,
        .deceleration   = 0.0f,
        .damage_bonuses = {
            {"Javelin Cavalry", 1.5f},
            {"Javelin Cavalry Hero", 1.5f}
        }
    },
    // Toxotes (id=508, protox.xml line 24590)
    ProtoxUnitStats{
        .proto_id       = 508,
        .name           = "Toxotes",
        .max_hp         = 60.0f,
        .initial_hp     = 60.0f,
        .move_speed     = 4.0f,
        .turn_rate_deg_s = 18.0f,
        .los            = 19.0f,
        .pop_cost       = 2,
        .cost_food      = 0,
        .cost_wood      = 55,
        .cost_gold      = 35,
        .cost_favor     = 0,
        .train_points   = 15.0f,
        .allowed_age    = 2,
        .damage         = 6.5f,
        .damage_type    = DamageType::Pierce,
        .attack_range   = 15.0f,
        .attack_cooldown = 1.0f,
        .accuracy       = 0.8f,
        .max_spread     = 5.0f,
        .spread_factor  = 0.25f,
        .track_rating   = 5.0f,
        .projectile_proto = "Arrow Flaming",
        .armor          = {0.15f, 0.15f, 0.99f},  // Hack, Pierce, Crush
        .acceleration   = 0.0f,
        .deceleration   = 0.0f,
        .damage_bonuses = {
            {"Raiding Cavalry", 0.9f}
        }
    }
};

// Estimated values (not in protox.xml, executable is packed)
namespace estimated {
    inline constexpr float kAcceleration = 12.0f;           // world units/s^2
    inline constexpr float kCameraFovDeg = 45.0f;
    inline constexpr float kCameraDistance = 12.0f;         // clamped 6-24
    inline constexpr float kCameraPitchDeg = 45.0f;
    inline constexpr float kCameraOrbitSpeedDegS = 90.0f;
    inline constexpr float kCameraZoomMin = 6.0f;
    inline constexpr float kCameraZoomMax = 24.0f;
} // namespace estimated

// Helper: get default stats for a unit type
inline const ProtoxUnitStats& GetDefaultStats(UnitType type) {
    return kDefaultUnitStats[static_cast<size_t>(type)];
}

// Helper: get armor value for a damage type
inline float GetArmor(const ProtoxUnitStats& stats, DamageType dt) {
    return stats.armor[static_cast<size_t>(dt)];
}

// Helper: calculate dealt damage using original formula
inline float CalculateDealtDamage(const ProtoxUnitStats& attacker, const ProtoxUnitStats& defender) {
    float armor = GetArmor(defender, attacker.damage_type);
    return ApplyDamage(attacker.damage, armor);
}

} // namespace aom