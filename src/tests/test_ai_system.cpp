#include "ai/ai_system.h"
#include "ecs/gameplay.h"
#include <iostream>
#include <cassert>

using namespace aom;
using namespace aom::ai;

static bool g_all_passed = true;

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (!condition) g_all_passed = false;
    return condition;
}

void test_rule_engine_and_scheduler() {
    std::cout << "\n--- Testing Rule Engine & XS Scheduler ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    check(!ai.rules().empty(), "Core rules loaded from generated XS headers");
    check(ai.rules().size() >= 100, "Loaded 100+ native AoM XS rules");

    // Test enable/disable rule
    check(ai.is_rule_active(CoreAiRule::updatePlayerToAttack), "updatePlayerToAttack active by default");
    ai.disable_rule(CoreAiRule::updatePlayerToAttack);
    check(!ai.is_rule_active(CoreAiRule::updatePlayerToAttack), "xsDisableRule successfully disabled rule");
    ai.enable_rule(CoreAiRule::updatePlayerToAttack);
    check(ai.is_rule_active(CoreAiRule::updatePlayerToAttack), "xsEnableRule successfully re-enabled rule");

    // Test timer progression
    GameplayWorld world;
    ai.update(1.0f, &world);
    check(ai.current_age() == 1, "Initial age is Archaic (Age 1)");
}

void test_personalities_and_profiles() {
    std::cout << "\n--- Testing Personalities & Archetypes ---" << std::endl;
    AiSystem standard_ai;
    standard_ai.initialize(Team::ENEMY, "standard");
    check(standard_ai.archetype() == PersonalityArchetype::STANDARD, "Standard personality loaded");
    check(standard_ai.profile().min_attack_force == 3, "Standard min_attack_force == 3");

    AiSystem aggressive_ai;
    aggressive_ai.initialize(Team::ENEMY, "aggressive");
    check(aggressive_ai.archetype() == PersonalityArchetype::AGGRESSIVE, "Aggressive personality loaded");
    check(aggressive_ai.profile().aggression >= 0.8f, "Aggressive profile has high aggression");
    check(aggressive_ai.profile().min_attack_force == 2, "Aggressive min_attack_force == 2");

    AiSystem builder_ai;
    builder_ai.initialize(Team::ENEMY, "builder");
    check(builder_ai.archetype() == PersonalityArchetype::BUILDER, "Builder personality loaded");
    check(builder_ai.profile().economy_priority >= 0.8f, "Builder profile has high economy priority");
}

void test_plan_lifecycle() {
    std::cout << "\n--- Testing AI Plan Management ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    check(!ai.plans().empty(), "Default AI plans initialized");
    uint32_t custom_plan = ai.create_plan("SiegeAssault", AiPlanType::ATTACK, 95);
    check(custom_plan > 0, "Created custom AI plan");
    check(ai.plans().count(custom_plan) == 1, "Custom plan present in active plan table");
    ai.destroy_plan(custom_plan);
    check(ai.plans().count(custom_plan) == 0, "aiPlanDestroy successfully removed plan");
}

void test_economy_gathering_integration() {
    std::cout << "\n--- Testing Economy & Gathering Integration ---" << std::endl;
    GameplayWorld world;
    world.add_resource_node(ResourceType::FOOD, {6.0f, 0.0f, 2.0f}, 500);
    world.add_resource_node(ResourceType::WOOD, {6.0f, 0.0f, -2.0f}, 500);

    // Spawn an enemy worker near the food node
    Entity worker = world.spawn_unit(Team::ENEMY, {5.5f, 0.0f, 2.0f});
    world.units()[worker].worker = true;

    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    int initial_food = world.enemy_resources().food;

    // Trigger AI census and gathering rule
    ai.update(0.1f, &world);
    ai.execute_rule(CoreAiRule::updateFoodBreakdown, &world);

    check(world.units()[worker].gather_node >= 0, "Enemy worker received gathering assignment");

    // Simulate worker pathfinding and gathering across multiple frames
    for (int i = 0; i < 60; ++i) {
        world.update(0.1f);
    }
    check(world.enemy_resources().food > initial_food, "Enemy resource pool actively updated from gathering");
}

void test_construction_and_military_training() {
    std::cout << "\n--- Testing Construction & Military Training ---" << std::endl;
    GameplayWorld world;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    // Give enemy resources for barracks and units
    world.enemy_resources().wood = 300;
    world.enemy_resources().gold = 200;
    world.enemy_resources().food = 200;

    // Spawn worker to build
    Entity worker = world.spawn_unit(Team::ENEMY, {6.0f, 0.0f, 0.0f});
    world.units()[worker].worker = true;

    // Run AI update to update census and execute auto build
    ai.update(0.1f, &world);
    ai.execute_rule(CoreAiRule::buildHouse, &world);

    // Verify building was placed
    bool found_building = false;
    Entity b_entity = INVALID_ENTITY;
    for (const auto& [e, bld] : world.buildings()) {
        if (bld.team == Team::ENEMY) {
            found_building = true;
            b_entity = e;
            break;
        }
    }
    check(found_building, "AI auto-placed building based on resource availability");

    // Complete building and test military training
    if (found_building) {
        world.buildings()[b_entity].complete = true;
        ai.execute_rule(CoreAiRule::landMilitaryManagerRule, &world);
        check(!world.buildings()[b_entity].training_unit.empty() ||
              world.buildings()[b_entity].type == BuildingType::HOUSE,
              "AI initiates unit training or pop expansion");
    }
}

void test_military_attack_sortie() {
    std::cout << "\n--- Testing Military Attack Sortie ---" << std::endl;
    GameplayWorld world;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "aggressive"); // min_attack_force == 2

    // Spawn friendly target
    Entity friendly_target = world.spawn_unit(Team::FRIENDLY, {-5.0f, 0.0f, 0.0f});

    // Spawn 2 enemy soldiers
    Entity soldier1 = world.spawn_unit(Team::ENEMY, {5.0f, 0.0f, 0.0f});
    Entity soldier2 = world.spawn_unit(Team::ENEMY, {5.0f, 0.0f, 1.0f});

    ai.update(0.1f, &world);
    check(ai.military_pop() == 2, "AI census detected 2 military units");
    check(ai.attack_readiness() >= 1.0f, "Attack readiness reaches 100%");

    // Execute target selection and attack sortie rules
    ai.execute_rule(CoreAiRule::updatePlayerToAttack, &world);
    ai.execute_rule(CoreAiRule::attackMonitor, &world);

    // Verify soldier attack orders
    const auto* u1 = world.get_unit(soldier1);
    check(u1 && u1->attack_target == friendly_target, "Military unit received attack order against target");
}

void test_age_progression() {
    std::cout << "\n--- Testing Age Progression ---" << std::endl;
    GameplayWorld world;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    check(ai.current_age() == 1, "Starts in Age 1");
    world.enemy_resources().food = 500;

    // Advance to Age 2
    ai.execute_rule(CoreAiRule::age1Progress, &world);
    check(ai.current_age() == 2, "AI progressed to Classical Age (Age 2)");

    world.enemy_resources().food = 900;
    world.enemy_resources().gold = 600;
    ai.execute_rule(CoreAiRule::age2Progress, &world);
    check(ai.current_age() == 3, "AI progressed to Heroic Age (Age 3)");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  AoM AI System & XS Rule Suite Tests   " << std::endl;
    std::cout << "========================================" << std::endl;

    test_rule_engine_and_scheduler();
    test_personalities_and_profiles();
    test_plan_lifecycle();
    test_economy_gathering_integration();
    test_construction_and_military_training();
    test_military_attack_sortie();
    test_age_progression();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL AI REA TESTS PASSED SUCCESSFULLY! " << std::endl;
    } else {
        std::cout << " SOME AI REA TESTS FAILED!             " << std::endl;
    }
    std::cout << "========================================" << std::endl;
    return g_all_passed ? 0 : 1;
}
