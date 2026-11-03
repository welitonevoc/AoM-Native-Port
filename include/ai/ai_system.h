#pragma once

#include "ai/xs_rules_gen.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace aom {

class GameplayWorld;
enum class Team : uint8_t;
using Entity = uint32_t;

namespace ai {

enum class AiPlanType : uint8_t {
    GATHER,
    BUILD,
    TRAIN,
    EXPLORE,
    ATTACK
};

struct AiPlan {
    uint32_t id = 0;
    std::string name;
    AiPlanType type = AiPlanType::GATHER;
    int priority = 50;
    bool active = true;
    Entity target_entity = 0;
    glm::vec3 target_pos{0.0f};
    std::vector<Entity> assigned_units;
};

struct ActiveRule {
    CoreAiRule id;
    std::string name;
    float interval_sec = 1.0f;
    float timer = 0.0f;
    bool active = true;
};

class AiSystem {
public:
    AiSystem();
    ~AiSystem() = default;

    void initialize(Team team, const std::string& personality = "default");
    void update(float dt, GameplayWorld* world);
    void reset();

    // Strategy, Profile & Stats
    Team team() const { return team_; }
    const std::string& personality() const { return personality_; }
    PersonalityArchetype archetype() const { return profile_.archetype; }
    const PersonalityProfile& profile() const { return profile_; }
    int military_pop() const { return military_pop_; }
    int civilian_pop() const { return civilian_pop_; }
    int current_age() const { return current_age_; }
    float attack_readiness() const { return attack_readiness_; }

    // Rule Control (matches AoM xsEnableRule / xsDisableRule)
    void enable_rule(CoreAiRule rule);
    void disable_rule(CoreAiRule rule);
    bool is_rule_active(CoreAiRule rule) const;

    // Plans (matches AoM aiPlanCreate / aiPlanDestroy)
    uint32_t create_plan(const std::string& name, AiPlanType type, int priority);
    void destroy_plan(uint32_t plan_id);
    const std::unordered_map<uint32_t, AiPlan>& plans() const { return plans_; }
    const std::vector<ActiveRule>& rules() const { return rules_; }
    // Rule Execution
    void execute_rule(CoreAiRule rule, GameplayWorld* world);

private:

    // Core AoM AI Rules from aomdefaultai.xs & aomxai.xs
    void rule_update_player_to_attack(GameplayWorld* world);
    void rule_update_econ_gatherers(GameplayWorld* world);
    void rule_auto_build(GameplayWorld* world);
    void rule_military_training(GameplayWorld* world);
    void rule_launch_attack(GameplayWorld* world);
    void rule_scouting(GameplayWorld* world);

    Team team_ = static_cast<Team>(1); // Team::ENEMY
    std::string personality_ = "default";
    PersonalityProfile profile_ = kAiPersonalities[0];
    int current_age_ = 1;
    int civilian_pop_ = 0;
    int military_pop_ = 0;
    float attack_readiness_ = 0.0f;
    glm::vec3 base_center_{6.0f, 0.0f, 0.0f};

    std::vector<ActiveRule> rules_;
    std::unordered_map<uint32_t, AiPlan> plans_;
    uint32_t next_plan_id_ = 1;

    // Timers (reserved)
};

} // namespace ai
} // namespace aom
