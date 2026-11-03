#include "ai/ai_system.h"
#include "ecs/gameplay.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>

namespace aom {
namespace ai {

AiSystem::AiSystem() {
    initialize(static_cast<Team>(1), "default");
}

void AiSystem::initialize(Team team, const std::string& personality) {
    team_ = team;
    personality_ = personality;

    // Resolve personality profile
    std::string lower_p = personality;
    std::transform(lower_p.begin(), lower_p.end(), lower_p.begin(), ::tolower);
    if (lower_p == "default") lower_p = "standard";

    profile_ = kAiPersonalities[0]; // fallback standard
    for (const auto& p : kAiPersonalities) {
        if (p.name && lower_p == p.name) {
            profile_ = p;
            break;
        }
    }

    // Initialize core rule scheduler from generated AoM XS rules
    rules_.clear();
    rules_.reserve(sizeof(kCoreAiRules) / sizeof(kCoreAiRules[0]));
    for (const auto& r : kCoreAiRules) {
        ActiveRule ar;
        ar.id = r.id;
        ar.name = r.name;
        ar.interval_sec = static_cast<float>(r.min_interval_ms) / 1000.0f;
        ar.timer = r.run_immediately ? ar.interval_sec : 0.0f;
        ar.active = r.active_initially;
        rules_.push_back(ar);
    }

    // Default strategic AI plans (matches AoM cPlan*)
    plans_.clear();
    create_plan("cPlanGather", AiPlanType::GATHER, static_cast<int>(profile_.economy_priority * 100));
    create_plan("cPlanBuild", AiPlanType::BUILD, static_cast<int>(profile_.defense_priority * 90));
    create_plan("cPlanTrain", AiPlanType::TRAIN, static_cast<int>(profile_.aggression * 95));
    create_plan("cPlanAttack", AiPlanType::ATTACK, static_cast<int>(profile_.aggression * 100));
    create_plan("cPlanExplore", AiPlanType::EXPLORE, 40);

    current_age_ = 1;
    civilian_pop_ = 0;
    military_pop_ = 0;
    attack_readiness_ = 0.0f;
    base_center_ = glm::vec3(6.0f, 0.0f, 0.0f);
}

void AiSystem::reset() {
    rules_.clear();
    plans_.clear();
    next_plan_id_ = 1;
    civilian_pop_ = 0;
    military_pop_ = 0;
    current_age_ = 1;
    attack_readiness_ = 0.0f;
}

void AiSystem::enable_rule(CoreAiRule rule) {
    for (auto& r : rules_) {
        if (r.id == rule) {
            r.active = true;
            return;
        }
    }
}

void AiSystem::disable_rule(CoreAiRule rule) {
    for (auto& r : rules_) {
        if (r.id == rule) {
            r.active = false;
            return;
        }
    }
}

bool AiSystem::is_rule_active(CoreAiRule rule) const {
    for (const auto& r : rules_) {
        if (r.id == rule) {
            return r.active;
        }
    }
    return false;
}

uint32_t AiSystem::create_plan(const std::string& name, AiPlanType type, int priority) {
    uint32_t id = next_plan_id_++;
    AiPlan plan;
    plan.id = id;
    plan.name = name;
    plan.type = type;
    plan.priority = priority;
    plan.active = true;
    plans_[id] = plan;
    return id;
}

void AiSystem::destroy_plan(uint32_t plan_id) {
    plans_.erase(plan_id);
}

void AiSystem::update(float dt, GameplayWorld* world) {
    if (!world) return;

    // 1. Census update (Knowledge Base / kbCensus)
    civilian_pop_ = 0;
    military_pop_ = 0;
    glm::vec3 base_accum{0.0f};
    int base_count = 0;

    for (const auto& [entity, unit] : world->units()) {
        if (unit.team == team_) {
            if (unit.worker) {
                civilian_pop_++;
            } else {
                military_pop_++;
            }
        }
    }

    for (const auto& [entity, bld] : world->buildings()) {
        if (bld.team == team_) {
            auto it = world->transforms().find(entity);
            if (it != world->transforms().end()) {
                base_accum += it->second.position;
                base_count++;
            }
        }
    }

    if (base_count > 0) {
        base_center_ = base_accum / static_cast<float>(base_count);
    }

    const int req_force = std::max(1, profile_.min_attack_force);
    attack_readiness_ = std::clamp(static_cast<float>(military_pop_) / static_cast<float>(req_force), 0.0f, 1.0f);

    // 2. Poll active rules according to minInterval (AoM Rule Scheduler)
    for (auto& r : rules_) {
        if (!r.active) continue;
        r.timer += dt;
        if (r.timer >= r.interval_sec) {
            r.timer = 0.0f;
            execute_rule(r.id, world);
        }
    }
}

void AiSystem::execute_rule(CoreAiRule rule, GameplayWorld* world) {
    switch (rule) {
        case CoreAiRule::updatePlayerToAttack:
            rule_update_player_to_attack(world);
            break;

        case CoreAiRule::updateFoodBreakdown:
        case CoreAiRule::updateGoldBreakdown:
        case CoreAiRule::updateWoodBreakdown:
        case CoreAiRule::updateEconGathererPercentCaps:
        case CoreAiRule::setEarlyEcon:
            rule_update_econ_gatherers(world);
            break;

        case CoreAiRule::buildHouse:
        case CoreAiRule::autoBuildOutpost:
        case CoreAiRule::buildSettlements:
        case CoreAiRule::buildSettlementsEarly:
        case CoreAiRule::buildMonuments:
            rule_auto_build(world);
            break;

        case CoreAiRule::landMilitaryManagerRule:
        case CoreAiRule::milPopMonitor:
        case CoreAiRule::trainMythUnit:
        case CoreAiRule::setAge3Units:
        case CoreAiRule::setAge4Units:
        case CoreAiRule::setFinalUnits:
            rule_military_training(world);
            break;

        case CoreAiRule::attackMonitor:
            rule_launch_attack(world);
            break;

        case CoreAiRule::startLandScouting:
        case CoreAiRule::airScouting:
            rule_scouting(world);
            break;

        case CoreAiRule::age1Progress:
        case CoreAiRule::age2Progress:
        case CoreAiRule::age3Progress:
        case CoreAiRule::goAge4:
        case CoreAiRule::okToAgeUp:
            // Progression logic (age advancing)
            if (current_age_ == 1 && world->enemy_resources().food >= 400) {
                world->enemy_resources().food -= 400;
                current_age_ = 2;
            } else if (current_age_ == 2 && world->enemy_resources().food >= 800 && world->enemy_resources().gold >= 500) {
                world->enemy_resources().food -= 800;
                world->enemy_resources().gold -= 500;
                current_age_ = 3;
            } else if (current_age_ == 3 && world->enemy_resources().food >= 1000 && world->enemy_resources().gold >= 1000) {
                world->enemy_resources().food -= 1000;
                world->enemy_resources().gold -= 1000;
                current_age_ = 4;
            }
            break;

        default:
            break;
    }
}

void AiSystem::rule_update_player_to_attack(GameplayWorld* world) {
    // Find closest hostile entity to AI base
    Entity best_target = INVALID_ENTITY;
    float best_dist = std::numeric_limits<float>::max();
    glm::vec3 best_pos{0.0f};

    for (const auto& [entity, unit] : world->units()) {
        if (unit.team != team_) {
            auto it = world->transforms().find(entity);
            if (it != world->transforms().end()) {
                float dist = glm::length(it->second.position - base_center_);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_target = entity;
                    best_pos = it->second.position;
                }
            }
        }
    }

    if (best_target == INVALID_ENTITY) {
        for (const auto& [entity, bld] : world->buildings()) {
            if (bld.team != team_) {
                auto it = world->transforms().find(entity);
                if (it != world->transforms().end()) {
                    float dist = glm::length(it->second.position - base_center_);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_target = entity;
                        best_pos = it->second.position;
                    }
                }
            }
        }
    }

    // Update attack plan target
    for (auto& [id, plan] : plans_) {
        if (plan.type == AiPlanType::ATTACK) {
            plan.target_entity = best_target;
            plan.target_pos = best_pos;
        }
    }
}

void AiSystem::rule_update_econ_gatherers(GameplayWorld* world) {
    const auto& nodes = world->resource_nodes();
    if (nodes.empty()) return;

    for (auto& [entity, unit] : world->units()) {
        if (unit.team != team_ || !unit.worker) continue;

        // If worker is idle or current node depleted
        bool is_idle = !unit.has_move_order && unit.build_target == INVALID_ENTITY && unit.attack_target == INVALID_ENTITY;
        bool node_invalid = (unit.gather_node < 0) ||
                            (unit.gather_node < static_cast<int>(nodes.size()) && nodes[unit.gather_node].remaining <= 0);

        if (is_idle && node_invalid) {
            auto it = world->transforms().find(entity);
            if (it == world->transforms().end()) continue;
            const glm::vec3 worker_pos = it->second.position;

            // Prioritize based on current inventory
            const auto& res = world->enemy_resources();
            ResourceType preferred = ResourceType::FOOD;
            if (res.food >= 150 && res.gold < 100) {
                preferred = ResourceType::GOLD;
            } else if (res.food >= 150 && res.wood < 100) {
                preferred = ResourceType::WOOD;
            }

            int best_node = -1;
            float best_dist = std::numeric_limits<float>::max();

            // First pass: try preferred resource
            for (size_t i = 0; i < nodes.size(); ++i) {
                if (nodes[i].remaining > 0 && nodes[i].type == preferred) {
                    float d = glm::length(nodes[i].position - worker_pos);
                    if (d < best_dist) {
                        best_dist = d;
                        best_node = static_cast<int>(i);
                    }
                }
            }

            // Fallback: any available resource
            if (best_node < 0) {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    if (nodes[i].remaining > 0) {
                        float d = glm::length(nodes[i].position - worker_pos);
                        if (d < best_dist) {
                            best_dist = d;
                            best_node = static_cast<int>(i);
                        }
                    }
                }
            }

            if (best_node >= 0) {
                world->order_gather(entity, nodes[best_node].position);
            }
        }
    }
}

void AiSystem::rule_auto_build(GameplayWorld* world) {
    // Check housing and military buildings
    int houses = 0;
    int barracks = 0;
    for (const auto& [entity, bld] : world->buildings()) {
        if (bld.team == team_) {
            if (bld.type == BuildingType::HOUSE) houses++;
            else if (bld.type == BuildingType::BARRACKS) barracks++;
        }
    }

    const int total_pop = civilian_pop_ + military_pop_;
    const int pop_cap = houses * 10 + 10;
    auto& res = world->enemy_resources();

    // 1. Need House?
    if (total_pop + 2 >= pop_cap && res.wood >= 100) {
        // Find idle worker
        Entity worker = INVALID_ENTITY;
        for (const auto& [e, u] : world->units()) {
            if (u.team == team_ && u.worker && u.build_target == INVALID_ENTITY) {
                worker = e;
                break;
            }
        }

        if (worker != INVALID_ENTITY) {
            // Find walkable build location near base center
            float offset_x = (houses % 3 - 1) * 3.5f;
            float offset_z = (houses / 3 + 1) * 3.5f;
            glm::vec3 b_pos = base_center_ + glm::vec3(offset_x, 0.0f, offset_z);

            res.wood -= 100;
            Entity house = world->place_building(team_, b_pos, 4.0f, BuildingType::HOUSE, "House");
            world->order_build(worker, house);
            return;
        }
    }

    // 2. Need Barracks?
    if (barracks == 0 && res.wood >= 100 && res.gold >= 50) {
        Entity worker = INVALID_ENTITY;
        for (const auto& [e, u] : world->units()) {
            if (u.team == team_ && u.worker && u.build_target == INVALID_ENTITY) {
                worker = e;
                break;
            }
        }

        if (worker != INVALID_ENTITY) {
            glm::vec3 b_pos = base_center_ + glm::vec3(-4.0f, 0.0f, 2.0f);
            res.wood -= 100;
            res.gold -= 50;
            Entity b_ent = world->place_building(team_, b_pos, 5.0f, BuildingType::BARRACKS, "Quartel Militar");
            world->order_build(worker, b_ent);
        }
    }
}

void AiSystem::rule_military_training(GameplayWorld* world) {
    for (const auto& [entity, bld] : world->buildings()) {
        if (bld.team == team_ && bld.type == BuildingType::BARRACKS && bld.complete) {
            if (bld.training_unit.empty()) {
                // Train Hoplite if affordable (Food: 60, Gold: 35)
                const auto& res = world->enemy_resources();
                if (res.food >= 60 && res.gold >= 35) {
                    world->start_training_unit(entity, "Hoplite", 60, 0, 35);
                }
            }
        }
    }
}

void AiSystem::rule_launch_attack(GameplayWorld* world) {
    if (military_pop_ < profile_.min_attack_force) return;

    // Find primary target (from plan or closest hostile)
    Entity target = INVALID_ENTITY;
    for (const auto& [id, plan] : plans_) {
        if (plan.type == AiPlanType::ATTACK && plan.target_entity != INVALID_ENTITY) {
            target = plan.target_entity;
            break;
        }
    }

    if (target == INVALID_ENTITY) {
        rule_update_player_to_attack(world);
        for (const auto& [id, plan] : plans_) {
            if (plan.type == AiPlanType::ATTACK) {
                target = plan.target_entity;
                break;
            }
        }
    }

    if (target == INVALID_ENTITY) return;

    // Order all unengaged military units to attack
    for (const auto& [entity, unit] : world->units()) {
        if (unit.team == team_ && !unit.worker) {
            if (unit.attack_target == INVALID_ENTITY) {
                world->order_attack(entity, target);
            }
        }
    }
}

void AiSystem::rule_scouting(GameplayWorld* world) {
    // If military units are idle and not ready to attack, perform perimeter scouting
    if (military_pop_ > 0 && military_pop_ < profile_.min_attack_force) {
        for (const auto& [entity, unit] : world->units()) {
            if (unit.team == team_ && !unit.worker && !unit.has_move_order && unit.attack_target == INVALID_ENTITY) {
                // Scout perimeter
                float angle = static_cast<float>(entity * 45) * 3.14159f / 180.0f;
                glm::vec3 scout_pos = base_center_ + glm::vec3(std::cos(angle) * 12.0f, 0.0f, std::sin(angle) * 12.0f);
                world->order_move(entity, scout_pos);
            }
        }
    }
}

} // namespace ai
} // namespace aom
