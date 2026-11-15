/**
 * ProtoxParser Implementation
 * 
 * Parses protox.xml (text XML) and extracts ALL unit stats matching the binary XMB format.
 * Based on reverse engineering of Voobly Balance Patch 6.0 protox.xmb
 * 
 * For production: consider using pugixml, rapidxml, or tinyxml2 for robust XML parsing.
 */

#include "aom/protox_parser.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <vector>

namespace aom {

namespace {

// Simple XML tokenizer for protox.xml (not a full XML parser)
struct XmlTokenizer {
    std::string_view content;
    size_t pos = 0;
    
    void SkipWhitespace() {
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) {
            ++pos;
        }
    }
    
    bool StartsWith(std::string_view prefix) const {
        return pos + prefix.size() <= content.size() && 
               content.substr(pos, prefix.size()) == prefix;
    }
    
    std::string_view ReadUntil(char delim) {
        size_t start = pos;
        while (pos < content.size() && content[pos] != delim) ++pos;
        std::string_view result = content.substr(start, pos - start);
        if (pos < content.size() && content[pos] == delim) ++pos;
        return result;
    }
    
    std::string_view ReadTagName() {
        SkipWhitespace();
        if (pos >= content.size() || content[pos] != '<') return {};
        ++pos; // skip '<'
        bool closing = false;
        if (pos < content.size() && content[pos] == '/') {
            closing = true;
            ++pos;
        }
        size_t start = pos;
        while (pos < content.size() && content[pos] != '>' && content[pos] != ' ' && content[pos] != '/') ++pos;
        std::string_view name = content.substr(start, pos - start);
        return name;
    }
    
    std::optional<std::string_view> ReadAttribute(std::string_view attr_name) {
        std::string_view search = " " + std::string(attr_name) + "=\"";
        size_t found = content.find(search, pos);
        if (found == std::string_view::npos) return std::nullopt;
        size_t start = found + search.size();
        size_t end = content.find('"', start);
        if (end == std::string_view::npos) return std::nullopt;
        return content.substr(start, end - start);
    }
    
    std::string_view ReadElementContent(std::string_view tag_name) {
        std::string open_tag = "<" + std::string(tag_name) + ">";
        std::string close_tag = "</" + std::string(tag_name) + ">";
        
        size_t open_pos = content.find(open_tag, pos);
        if (open_pos == std::string_view::npos) return {};
        open_pos += open_tag.size();
        
        size_t close_pos = content.find(close_tag, open_pos);
        if (close_pos == std::string_view::npos) return {};
        
        std::string_view result = content.substr(open_pos, close_pos - open_pos);
        pos = close_pos + close_tag.size();
        return result;
    }
    
    // Find all occurrences of a tag and return their contents
    std::vector<std::string_view> FindAllElements(std::string_view tag_name) {
        std::vector<std::string_view> results;
        std::string open_tag = "<" + std::string(tag_name);
        std::string close_tag = "</" + std::string(tag_name) + ">";
        
        size_t search_pos = 0;
        while (true) {
            size_t open_pos = content.find(open_tag, search_pos);
            if (open_pos == std::string_view::npos) break;
            
            // Find the closing '>' of the opening tag
            size_t tag_end = content.find('>', open_pos);
            if (tag_end == std::string_view::npos) break;
            open_pos = tag_end + 1;
            
            size_t close_pos = content.find(close_tag, open_pos);
            if (close_pos == std::string_view::npos) break;
            
            std::string_view result = content.substr(open_pos, close_pos - open_pos);
            results.push_back(result);
            search_pos = close_pos + close_tag.size();
        }
        return results;
    }
};

} // anonymous namespace

DamageType ProtoxParser::ParseDamageType(std::string_view type_str) {
    if (type_str == "Hack") return DamageType::Hack;
    if (type_str == "Pierce") return DamageType::Pierce;
    if (type_str == "Crush") return DamageType::Crush;
    return DamageType::Hack; // default
}

std::optional<float> ProtoxParser::ParseFloat(std::string_view str) {
    try {
        float value = std::stof(std::string(str));
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> ProtoxParser::ParseInt(std::string_view str) {
    try {
        int value = std::stoi(std::string(str));
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<uint32_t> ProtoxParser::ParseUint(std::string_view str) {
    try {
        uint32_t value = std::stoul(std::string(str));
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

ProtoxParseResult ProtoxParser::ParseFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        ProtoxParseResult result;
        result.errors.push_back("Failed to open file: " + path.string());
        return result;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseString(buffer.str());
}

ProtoxParseResult ProtoxParser::ParseString(std::string_view xml_content) {
    ProtoxParseResult result;
    XmlTokenizer tokenizer{xml_content};
    
    // Find all <unit> elements
    size_t search_pos = 0;
    while (true) {
        size_t unit_start = xml_content.find("<unit", search_pos);
        if (unit_start == std::string_view::npos) break;
        
        // Find closing </unit>
        size_t unit_end = xml_content.find("</unit>", unit_start);
        if (unit_end == std::string_view::npos) {
            result.warnings.push_back("Unclosed <unit> tag at position " + std::to_string(unit_start));
            break;
        }
        unit_end += 7; // include </unit>
        
        std::string_view unit_xml = xml_content.substr(unit_start, unit_end - unit_start);
        search_pos = unit_end;
        
        // Create tokenizer for this unit
        XmlTokenizer unit_tok{unit_xml};
        unit_tok.pos = 5; // skip "<unit"
        
        auto id_attr = unit_tok.ReadAttribute("id");
        auto name_attr = unit_tok.ReadAttribute("name");
        
        if (!id_attr) {
            result.warnings.push_back("Unit missing id attribute");
            continue;
        }
        
        auto proto_id_opt = ParseUint(*id_attr);
        if (!proto_id_opt) {
            result.warnings.push_back("Invalid unit id: " + std::string(*id_attr));
            continue;
        }
        uint32_t proto_id = *proto_id_opt;
        
        // Parse child elements
        ProtoxUnitStats stats = GetDefaultStats(UnitType::Hoplite); // will override
        stats.proto_id = proto_id;
        stats.name = name_attr ? std::string(*name_attr) : "Unknown";
        
        // ===== CORE STATS =====
        
        // <maxhitpoints>
        if (auto hp = unit_tok.ReadElementContent("maxhitpoints"); !hp.empty()) {
            if (auto val = ParseFloat(hp)) stats.max_hp = *val;
        }
        
        // <initialhitpoints>
        if (auto hp = unit_tok.ReadElementContent("initialhitpoints"); !hp.empty()) {
            if (auto val = ParseFloat(hp)) stats.initial_hp = *val;
        }
        
        // <maxvelocity>
        if (auto mv = unit_tok.ReadElementContent("maxvelocity"); !mv.empty()) {
            if (auto val = ParseFloat(mv)) stats.move_speed = *val;
        }
        
        // <turnrate>
        if (auto tr = unit_tok.ReadElementContent("turnrate"); !tr.empty()) {
            if (auto val = ParseFloat(tr)) stats.turn_rate_deg_s = *val;
        }
        
        // <los>
        if (auto los = unit_tok.ReadElementContent("los"); !los.empty()) {
            if (auto val = ParseFloat(los)) stats.los = *val;
        }
        
        // <populationcount>
        if (auto pop = unit_tok.ReadElementContent("populationcount"); !pop.empty()) {
            if (auto val = ParseInt(pop)) stats.pop_cost = *val;
        }
        
        // <trainpoints>
        if (auto tp = unit_tok.ReadElementContent("trainpoints"); !tp.empty()) {
            if (auto val = ParseFloat(tp)) stats.train_points = *val;
        }
        
        // <allowedage>
        if (auto age = unit_tok.ReadElementContent("allowedage"); !age.empty()) {
            if (auto val = ParseInt(age)) stats.allowed_age = *val;
        }
        
        // <cost> elements (multiple with resourcetype attribute)
        {
            XmlTokenizer cost_tok{unit_xml};
            size_t cost_search = 0;
            while (true) {
                size_t cost_start = unit_xml.find("<cost", cost_search);
                if (cost_start == std::string_view::npos) break;
                cost_search = cost_start + 5;
                
                XmlTokenizer ctok{unit_xml.substr(cost_start)};
                auto rtype_attr = ctok.ReadAttribute("resourcetype");
                std::string_view cost_content = ctok.ReadElementContent("cost");
                
                if (rtype_attr && !cost_content.empty()) {
                    if (auto val = ParseInt(cost_content)) {
                        std::string rtype = std::string(*rtype_attr);
                        if (rtype == "Food") stats.cost_food = *val;
                        else if (rtype == "Wood") stats.cost_wood = *val;
                        else if (rtype == "Gold") stats.cost_gold = *val;
                        else if (rtype == "Favor") stats.cost_favor = *val;
                    }
                }
            }
        }
        
        // ===== ARMOR =====
        {
            size_t armor_search = 0;
            while (true) {
                size_t armor_start = unit_xml.find("<armor", armor_search);
                if (armor_start == std::string_view::npos) break;
                armor_search = armor_start + 6;
                
                XmlTokenizer armor_tok{unit_xml.substr(armor_start)};
                auto dtype_attr = armor_tok.ReadAttribute("damagetype");
                auto percent_attr = armor_tok.ReadAttribute("percentflag");
                
                if (dtype_attr) {
                    DamageType dt = ParseDamageType(*dtype_attr);
                    std::string_view content = armor_tok.ReadElementContent("armor");
                    if (!content.empty()) {
                        if (auto val = ParseFloat(content)) {
                            stats.armor[static_cast<size_t>(dt)] = *val;
                        }
                    }
                }
            }
        }
        
        // ===== HAND ATTACK (melee) =====
        if (auto hand = unit_tok.ReadElementContent("HandAttack"); !hand.empty()) {
            XmlTokenizer hand_tok{hand};
            
            size_t param_search = 0;
            while (true) {
                size_t param_start = hand.find("<param", param_search);
                if (param_start == std::string_view::npos) break;
                param_search = param_start + 6;
                
                XmlTokenizer param_tok{hand.substr(param_start)};
                auto name_attr = param_tok.ReadAttribute("name");
                auto type_attr = param_tok.ReadAttribute("type");
                auto value_attr = param_tok.ReadAttribute("value1");
                
                if (name_attr && value_attr) {
                    if (*name_attr == "Damage") {
                        if (type_attr) stats.damage_type = ParseDamageType(*type_attr);
                        if (auto val = ParseFloat(*value_attr)) stats.damage = *val;
                    } else if (*name_attr == "MaximumRange") {
                        if (auto val = ParseFloat(*value_attr)) stats.attack_range = *val;
                    } else if (*name_attr == "Rate") {
                        if (auto val = ParseFloat(*value_attr)) {
                            // Protox Rate -> cooldown conversion (estimated)
                            // Rate=5.0 ~ 1.0s, Rate=1.0 ~ 1.0s (for ranged)
                            stats.attack_cooldown = (*val >= 5.0f) ? 1.0f : *val;
                        }
                    } else if (*name_attr == "DamageBonus") {
                        if (type_attr && value_attr) {
                            if (auto val = ParseFloat(*value_attr)) {
                                ProtoxUnitStats::DamageBonus bonus;
                                bonus.target_type = std::string(*type_attr);
                                bonus.multiplier = *val;
                                stats.damage_bonuses.push_back(std::move(bonus));
                            }
                        }
                    }
                }
            }
        }
        
        // ===== RANGED ATTACK =====
        if (auto ranged = unit_tok.ReadElementContent("RangedAttack"); !ranged.empty()) {
            XmlTokenizer ranged_tok{ranged};
            
            size_t param_search = 0;
            while (true) {
                size_t param_start = ranged.find("<param", param_search);
                if (param_start == std::string_view::npos) break;
                param_search = param_start + 6;
                
                XmlTokenizer param_tok{ranged.substr(param_start)};
                auto name_attr = param_tok.ReadAttribute("name");
                auto type_attr = param_tok.ReadAttribute("type");
                auto value_attr = param_tok.ReadAttribute("value1");
                
                if (name_attr && value_attr) {
                    if (*name_attr == "Damage") {
                        if (type_attr) stats.damage_type = ParseDamageType(*type_attr);
                        if (auto val = ParseFloat(*value_attr)) stats.damage = *val;
                    } else if (*name_attr == "MaximumRange") {
                        if (auto val = ParseFloat(*value_attr)) stats.attack_range = *val;
                    } else if (*name_attr == "Rate") {
                        if (auto val = ParseFloat(*value_attr)) {
                            stats.attack_cooldown = (*val >= 1.0f) ? 1.0f : *val;
                        }
                    } else if (*name_attr == "Accuracy") {
                        if (auto val = ParseFloat(*value_attr)) stats.accuracy = *val;
                    } else if (*name_attr == "MaxSpread") {
                        if (auto val = ParseFloat(*value_attr)) stats.max_spread = *val;
                    } else if (*name_attr == "SpreadFactor") {
                        if (auto val = ParseFloat(*value_attr)) stats.spread_factor = *val;
                    } else if (*name_attr == "TrackRating") {
                        if (auto val = ParseFloat(*value_attr)) stats.track_rating = *val;
                    } else if (*name_attr == "DamageBonus") {
                        if (type_attr && value_attr) {
                            if (auto val = ParseFloat(*value_attr)) {
                                ProtoxUnitStats::DamageBonus bonus;
                                bonus.target_type = std::string(*type_attr);
                                bonus.multiplier = *val;
                                stats.damage_bonuses.push_back(std::move(bonus));
                            }
                        }
                    }
                }
            }
        }
        
        // ===== PROJECTILE PROTOUNIT =====
        if (auto proj = unit_tok.ReadElementContent("projectileprotounit"); !proj.empty()) {
            stats.projectile_proto = std::string(proj);
        }
        
        // Determine unit type from proto_id for default fallback
        if (proto_id == 503) {
            stats = GetDefaultStats(UnitType::Hoplite);
            stats.proto_id = proto_id;
        } else if (proto_id == 508) {
            stats = GetDefaultStats(UnitType::Toxotes);
            stats.proto_id = proto_id;
        }
        
        result.units[proto_id] = std::move(stats);
    }
    
    if (result.units.empty()) {
        result.warnings.push_back("No units parsed from protox.xml");
    }
    
    return result;
}

} // namespace aom