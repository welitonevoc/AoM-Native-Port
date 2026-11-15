#pragma once

/**
 * ProtoxParser — Parses Age of Mythology protox.xml for unit stats
 * 
 * The original game reads ALL gameplay values from protox.xml at runtime.
 * The executable (aomx.exe) is packed and contains NO gameplay functions.
 * 
 * This parser extracts the exact fields documented in MECHANICS.md.
 * 
 * Expected protox.xml structure (from Voobly Balance Patch 6.0):
 * <unit id="503" name="Hoplite">
 *   <maxhitpoints>115.0000</maxhitpoints>
 *   <maxvelocity>4.2000</maxvelocity>
 *   <turnrate>18.0000</turnrate>
 *   <armor damagetype="Hack" percentflag="1">0.35</armor>
 *   <armor damagetype="Pierce" percentflag="1">0.15</armor>
 *   <armor damagetype="Crush" percentflag="1">0.99</armor>
 *   <HandAttack>
 *     <param name="Damage" type="Hack" value1="8"/>
 *     <param name="MaximumRange" value1="0.3"/>
 *     <param name="Rate" type="All" value1="5.0"/>
 *   </HandAttack>
 *   <RangedAttack>  (for Toxotes)
 *     <param name="Damage" type="Pierce" value1="6.5"/>
 *     <param name="MaximumRange" value1="15"/>
 *     <param name="Rate" type="All" value1="1.0"/>
 *     <param name="Accuracy" value1="0.8"/>
 *     <param name="MaxSpread" value1="5.0"/>
 *   </RangedAttack>
 * </unit>
 */

#include "unit_stats.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <filesystem>

namespace aom {

struct ProtoxParseResult {
    std::unordered_map<uint32_t, ProtoxUnitStats> units;  // keyed by proto_id
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    bool HasErrors() const { return !errors.empty(); }
    bool HasUnit(uint32_t proto_id) const { return units.contains(proto_id); }
    const ProtoxUnitStats* GetUnit(uint32_t proto_id) const {
        auto it = units.find(proto_id);
        return it != units.end() ? &it->second : nullptr;
    }
};

class ProtoxParser {
public:
    // Parse protox.xml from file path
    static ProtoxParseResult ParseFile(const std::filesystem::path& path);
    
    // Parse protox.xml from string content
    static ProtoxParseResult ParseString(std::string_view xml_content);
    
    // Parse a single <unit> node (for testing/extension)
    static std::optional<ProtoxUnitStats> ParseUnitNode(/* xml node */);
    
private:
    // Damage type string to enum
    static DamageType ParseDamageType(std::string_view type_str);
    
    // Extract float value from XML attribute/element
    static std::optional<float> ParseFloat(std::string_view str);
    
    // Extract int value
    static std::optional<int> ParseInt(std::string_view str);
    
    // Extract uint32_t value
    static std::optional<uint32_t> ParseUint(std::string_view str);
};

} // namespace aom