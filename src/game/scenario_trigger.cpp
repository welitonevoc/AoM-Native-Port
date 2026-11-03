#include "game/scenario_trigger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

#define LOG_TAG "AoM-Trigger"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

std::string clean_name(std::string s) {
    // Strip $$12345$$ localization prefixes if present
    if (s.rfind("$$", 0) == 0) {
        size_t end_idx = s.find("$$", 2);
        if (end_idx != std::string::npos) {
            s = s.substr(end_idx + 2);
        }
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    if (start > 0) s = s.substr(start);
    return s;
}

std::string extract_tag_attr(const std::string& line, const std::string& attr) {
    std::string needle = attr + "=\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) {
        needle = attr + " = \"";
        pos = line.find(needle);
    }
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

std::string extract_tag_content(const std::string& text, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t start = text.find(open);
    if (start == std::string::npos) return "";
    start += open.size();
    size_t end = text.find(close, start);
    if (end == std::string::npos) return "";
    return text.substr(start, end - start);
}

} // anonymous namespace

ScenarioTriggerSystem::ScenarioTriggerSystem() = default;
ScenarioTriggerSystem::~ScenarioTriggerSystem() = default;

ScenarioHeader ScenarioTriggerSystem::parse_scenario_header(const uint8_t* data, size_t size, bool is_expansion) {
    ScenarioHeader header;
    header.is_expansion = is_expansion;
    header.file_size = static_cast<uint32_t>(size);

    if (!data || size < 64) {
        return header;
    }

    // Search for "RTS3" in UTF-16LE or ASCII within first 256 bytes
    bool found_rts3 = false;
    for (size_t i = 0; i + 8 <= std::min<size_t>(size, 256); ++i) {
        if (std::memcmp(data + i, "RTS3", 4) == 0) {
            found_rts3 = true;
            header.creator_app = "RTS3.exe";
            break;
        }
        // UTF-16LE "R\0T\0S\03\0"
        if (data[i] == 'R' && data[i+1] == 0 &&
            data[i+2] == 'T' && data[i+3] == 0 &&
            data[i+4] == 'S' && data[i+5] == 0 &&
            data[i+6] == '3' && data[i+7] == 0) {
            found_rts3 = true;
            header.creator_app = "RTS3.exe";
            break;
        }
    }

    // Search for scenario format magic "R3SG" or "SM4"
    for (size_t i = 0; i + 4 <= std::min<size_t>(size, 1024); ++i) {
        if (std::memcmp(data + i, "R3SG", 4) == 0) {
            header.format_magic = "R3SG";
            break;
        }
        if (std::memcmp(data + i, "SM4", 3) == 0) {
            header.version = 4; // SM4 format
            if (header.format_magic.empty()) header.format_magic = "SM4";
        }
    }

    if (found_rts3 || !header.format_magic.empty()) {
        header.valid = true;
        header.player_count = 2; // Default 2 players in skirmish/campaign
    }

    return header;
}

bool ScenarioTriggerSystem::load_trigger_dictionary(const std::string& xml_content) {
    std::istringstream stream(xml_content);
    std::string line;

    bool in_conditions = false;
    bool in_effects = false;

    TriggerConditionDef current_cond;
    TriggerEffectDef current_eff;
    bool building_cond = false;
    bool building_eff = false;

    while (std::getline(stream, line)) {
        if (line.find("<Conditions>") != std::string::npos) {
            in_conditions = true;
            in_effects = false;
            continue;
        }
        if (line.find("</Conditions>") != std::string::npos) {
            in_conditions = false;
            continue;
        }
        if (line.find("<Effects>") != std::string::npos) {
            in_effects = true;
            in_conditions = false;
            continue;
        }
        if (line.find("</Effects>") != std::string::npos) {
            in_effects = false;
            continue;
        }

        if (in_conditions) {
            if (line.find("<Condition ") != std::string::npos) {
                current_cond = TriggerConditionDef();
                std::string raw_name = extract_tag_attr(line, "name");
                current_cond.name = clean_name(raw_name);
                building_cond = true;
            } else if (line.find("</Condition>") != std::string::npos) {
                if (building_cond && !current_cond.name.empty()) {
                    conditions_[current_cond.name] = current_cond;
                }
                building_cond = false;
            } else if (building_cond) {
                if (line.find("<Param ") != std::string::npos) {
                    TriggerParam p;
                    p.name = extract_tag_attr(line, "name");
                    p.display_name = clean_name(extract_tag_attr(line, "dispName"));
                    p.var_type = extract_tag_attr(line, "VarType");
                    p.default_val = extract_tag_content(line, "Param");
                    current_cond.params.push_back(p);
                } else if (line.find("<Expression>") != std::string::npos) {
                    current_cond.expression_template = extract_tag_content(line, "Expression");
                } else if (line.find("<Command>") != std::string::npos) {
                    current_cond.command_template = extract_tag_content(line, "Command");
                }
            }
        } else if (in_effects) {
            if (line.find("<Effect ") != std::string::npos) {
                current_eff = TriggerEffectDef();
                std::string raw_name = extract_tag_attr(line, "name");
                current_eff.name = clean_name(raw_name);
                building_eff = true;
            } else if (line.find("</Effect>") != std::string::npos) {
                if (building_eff && !current_eff.name.empty()) {
                    effects_[current_eff.name] = current_eff;
                }
                building_eff = false;
            } else if (building_eff) {
                if (line.find("<Param ") != std::string::npos) {
                    TriggerParam p;
                    p.name = extract_tag_attr(line, "name");
                    p.display_name = clean_name(extract_tag_attr(line, "dispName"));
                    p.var_type = extract_tag_attr(line, "VarType");
                    p.default_val = extract_tag_content(line, "Param");
                    current_eff.params.push_back(p);
                } else if (line.find("<Expression>") != std::string::npos) {
                    current_eff.expression_template = extract_tag_content(line, "Expression");
                } else if (line.find("<Command>") != std::string::npos) {
                    current_eff.command_template = extract_tag_content(line, "Command");
                }
            }
        }
    }

    LOGI("Loaded %zu trigger conditions and %zu trigger effects", conditions_.size(), effects_.size());
    return !conditions_.empty() || !effects_.empty();
}

const TriggerConditionDef* ScenarioTriggerSystem::find_condition(const std::string& name) const {
    std::string clean = clean_name(name);
    auto it = conditions_.find(clean);
    if (it != conditions_.end()) return &it->second;
    return nullptr;
}

const TriggerEffectDef* ScenarioTriggerSystem::find_effect(const std::string& name) const {
    std::string clean = clean_name(name);
    auto it = effects_.find(clean);
    if (it != effects_.end()) return &it->second;
    return nullptr;
}

void ScenarioTriggerSystem::add_trigger(const TriggerInstance& trigger) {
    triggers_.push_back(trigger);
}

void ScenarioTriggerSystem::update(float delta_time) {
    for (auto& tr : triggers_) {
        if (!tr.active) continue;
        if (tr.run_once && tr.executed) continue;

        tr.timer += delta_time;
        if (tr.timer >= tr.min_interval) {
            // In a full script evaluator, expressions are executed.
            // Here we mark conditions met and execute.
            tr.conditions_met = true;
            tr.executed = true;
            if (tr.run_once) {
                tr.active = false;
            } else {
                tr.timer = 0.0f;
            }
        }
    }
}

void ScenarioTriggerSystem::set_diplomacy(uint32_t from_player, uint32_t to_player, DiplomacyStatus status) {
    uint64_t key = (static_cast<uint64_t>(from_player) << 32) | to_player;
    diplomacy_matrix_[key] = status;
}

DiplomacyStatus ScenarioTriggerSystem::get_diplomacy(uint32_t from_player, uint32_t to_player) const {
    if (from_player == to_player) return DiplomacyStatus::ALLY;
    uint64_t key = (static_cast<uint64_t>(from_player) << 32) | to_player;
    auto it = diplomacy_matrix_.find(key);
    if (it != diplomacy_matrix_.end()) return it->second;
    return DiplomacyStatus::ENEMY; // Default enemy in RTS
}

VictoryState ScenarioTriggerSystem::evaluate_conquest_victory(uint32_t human_player,
                                                             const std::vector<uint32_t>& player_alive_units) {
    if (human_player >= player_alive_units.size()) return VictoryState::IN_PROGRESS;

    // Check if human player lost all units
    if (player_alive_units[human_player] == 0) {
        return VictoryState::DEFEAT;
    }

    // Check if all enemies have 0 units
    bool any_enemy_alive = false;
    for (size_t i = 1; i < player_alive_units.size(); ++i) {
        if (i == human_player) continue;
        if (get_diplomacy(human_player, static_cast<uint32_t>(i)) == DiplomacyStatus::ENEMY) {
            if (player_alive_units[i] > 0) {
                any_enemy_alive = true;
                break;
            }
        }
    }

    if (!any_enemy_alive) {
        return VictoryState::VICTORY;
    }

    return VictoryState::IN_PROGRESS;
}

VictoryState ScenarioTriggerSystem::evaluate_sudden_death(uint32_t human_player,
                                                         bool human_citadel_alive,
                                                         const std::vector<bool>& enemy_citadels_alive) {
    (void)human_player;
    if (!human_citadel_alive) {
        return VictoryState::DEFEAT;
    }

    bool any_enemy_citadel = false;
    for (bool alive : enemy_citadels_alive) {
        if (alive) {
            any_enemy_citadel = true;
            break;
        }
    }

    if (!any_enemy_citadel && !enemy_citadels_alive.empty()) {
        return VictoryState::VICTORY;
    }

    return VictoryState::IN_PROGRESS;
}

} // namespace aom
