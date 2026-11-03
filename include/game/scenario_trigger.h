#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace aom {

class BarArchive;

/// Scenario SCN / SCX File Header Information
struct ScenarioHeader {
    std::string creator_app;   // e.g. "RTS3.exe"
    std::string scenario_name; // SCN / SCX file name or embedded title
    std::string format_magic;  // "R3SG"
    uint32_t version = 0;       // e.g. SM4
    uint32_t file_size = 0;
    uint32_t player_count = 0;
    bool is_expansion = false;  // .scx vs .scn
    bool valid = false;
};

/// Trigger Parameter Definition
struct TriggerParam {
    std::string name;
    std::string display_name;
    std::string var_type; // long, float, unit, operator, string
    std::string default_val;
};

/// Trigger Condition Definition (from typetest.xml)
struct TriggerConditionDef {
    std::string name;
    std::vector<TriggerParam> params;
    std::string expression_template;
    std::string command_template;
};

/// Trigger Effect Definition (from typetest.xml)
struct TriggerEffectDef {
    std::string name;
    std::vector<TriggerParam> params;
    std::string expression_template;
    std::string command_template;
};

/// Runtime Trigger Instance
struct TriggerInstance {
    uint32_t id = 0;
    std::string name;
    bool active = true;
    bool run_once = true;
    float min_interval = 0.0f;
    float max_interval = 0.0f;
    float timer = 0.0f;
    
    std::vector<std::string> condition_names;
    std::vector<std::string> effect_names;
    
    bool conditions_met = false;
    bool executed = false;
};

/// Diplomacy Status
enum class DiplomacyStatus {
    ALLY,
    ENEMY,
    NEUTRAL
};

/// Victory Condition State
enum class VictoryState {
    IN_PROGRESS,
    VICTORY,
    DEFEAT
};

class ScenarioTriggerSystem {
public:
    ScenarioTriggerSystem();
    ~ScenarioTriggerSystem();

    /// Parse typetest.xml trigger schema dictionary
    bool load_trigger_dictionary(const std::string& xml_content);

    /// Parse Scenario .scn / .scx file header
    static ScenarioHeader parse_scenario_header(const uint8_t* data, size_t size, bool is_expansion = false);

    /// Trigger dictionary queries
    [[nodiscard]] size_t condition_count() const { return conditions_.size(); }
    [[nodiscard]] size_t effect_count() const { return effects_.size(); }
    [[nodiscard]] const TriggerConditionDef* find_condition(const std::string& name) const;
    [[nodiscard]] const TriggerEffectDef* find_effect(const std::string& name) const;

    /// Triggers instance management
    void add_trigger(const TriggerInstance& trigger);
    void update(float delta_time);
    [[nodiscard]] size_t trigger_count() const { return triggers_.size(); }
    [[nodiscard]] const std::vector<TriggerInstance>& triggers() const { return triggers_; }

    /// Diplomacy Matrix (non-reciprocal support)
    void set_diplomacy(uint32_t from_player, uint32_t to_player, DiplomacyStatus status);
    [[nodiscard]] DiplomacyStatus get_diplomacy(uint32_t from_player, uint32_t to_player) const;

    /// Victory Condition Simulation (BasicVC1 / SuddenDeath)
    VictoryState evaluate_conquest_victory(uint32_t human_player, const std::vector<uint32_t>& player_alive_units);
    VictoryState evaluate_sudden_death(uint32_t human_player, bool human_citadel_alive, const std::vector<bool>& enemy_citadels_alive);

private:
    std::unordered_map<std::string, TriggerConditionDef> conditions_;
    std::unordered_map<std::string, TriggerEffectDef> effects_;
    std::vector<TriggerInstance> triggers_;
    std::unordered_map<uint64_t, DiplomacyStatus> diplomacy_matrix_; // (p1 << 32) | p2
};

} // namespace aom
