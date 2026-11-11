#include "ecs/gameplay.h"
#include "ai/ai_system.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace aom;
using namespace aom::ai;

static bool g_all_passed = true;

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (!condition) g_all_passed = false;
    return condition;
}

static void test_ai_initialization() {
    std::cout << "\n--- Testing AI Initialization ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    check(ai.team() == Team::ENEMY, "AI team is ENEMY");
    check(ai.is_rule_active(CoreAiRule::updatePlayerToAttack), "updatePlayerToAttack rule active");
    check(ai.is_rule_active(CoreAiRule::updateFoodBreakdown), "updateFoodBreakdown rule active");
    check(ai.is_rule_active(CoreAiRule::updateWoodBreakdown), "updateWoodBreakdown rule active");
    check(ai.is_rule_active(CoreAiRule::updateGoldBreakdown), "updateGoldBreakdown rule active");
    check(!ai.rules().empty(), "AI has rules loaded");
}

static void test_ai_economic_rules() {
    std::cout << "\n--- Testing AI Economic Rules ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "standard");

    GameplayWorld world(nullptr);

    UnitStats villager_stats;
    villager_stats.name = "Villager";
    villager_stats.max_hp = 100;

    Entity tc = world.place_building(Team::ENEMY, glm::vec3(0,0,0), 10.0f,
                                     BuildingType::TOWN_CENTER, "Enemy TC");
    check(tc != INVALID_ENTITY, "Spawned enemy TC for AI economy");

    for (int i = 0; i < 3; i++) {
        Entity w = world.spawn_unit(Team::ENEMY, glm::vec3(1.0f + i, 0.0f, 0.0f), villager_stats);
        check(w != INVALID_ENTITY, "Spawned enemy villager");
    }

    ai.update(1.0f, &world);
    check(true, "AI economic update ran without crash");
}

static void test_ai_military_rules() {
    std::cout << "\n--- Testing AI Military Rules ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "aggressive");

    GameplayWorld world(nullptr);

    Entity tc = world.place_building(Team::ENEMY, glm::vec3(0,0,0), 10.0f,
                                     BuildingType::TOWN_CENTER, "Enemy TC");
    Entity barracks = world.place_building(Team::ENEMY, glm::vec3(5,0,0), 10.0f,
                                           BuildingType::BARRACKS, "Enemy Barracks");
    check(tc != INVALID_ENTITY && barracks != INVALID_ENTITY, "Spawned enemy buildings");

    UnitStats hoplite_stats;
    hoplite_stats.name = "Hoplite";
    hoplite_stats.max_hp = 100;
    Entity hoplite = world.spawn_unit(Team::ENEMY, glm::vec3(2,0,0), hoplite_stats);
    check(hoplite != INVALID_ENTITY, "Spawned enemy Hoplite");

    UnitStats target_stats;
    target_stats.name = "Target";
    target_stats.max_hp = 100;
    Entity target = world.spawn_unit(Team::FRIENDLY, glm::vec3(20,0,0), target_stats);
    check(target != INVALID_ENTITY, "Spawned friendly target");

    ai.update(1.0f, &world);
    check(true, "AI military update ran without crash");
}

static void test_ai_building_construction() {
    std::cout << "\n--- Testing AI Building Construction ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "builder");

    GameplayWorld world(nullptr);

    Entity tc = world.place_building(Team::ENEMY, glm::vec3(0,0,0), 10.0f,
                                     BuildingType::TOWN_CENTER, "Enemy TC");
    check(tc != INVALID_ENTITY, "Spawned enemy TC");

    for (int i = 0; i < 10; i++) {
        ai.update(1.0f, &world);
    }

    check(true, "AI building construction update ran for 10 ticks");
}

static void test_ai_attacks_player() {
    std::cout << "\n--- Testing AI Attacks Player ---" << std::endl;
    AiSystem ai;
    ai.initialize(Team::ENEMY, "aggressive");

    GameplayWorld world(nullptr);

    Entity tc = world.place_building(Team::ENEMY, glm::vec3(0,0,0), 10.0f,
                                     BuildingType::TOWN_CENTER, "Enemy TC");
    UnitStats unit_stats;
    unit_stats.name = "Unit";
    unit_stats.max_hp = 100;
    Entity enemy_unit = world.spawn_unit(Team::ENEMY, glm::vec3(5,0,0), unit_stats);
    Entity player_unit = world.spawn_unit(Team::FRIENDLY, glm::vec3(8,0,0), unit_stats);
    check(enemy_unit != INVALID_ENTITY && player_unit != INVALID_ENTITY, "Spawned enemy and player units");

    ai.update(1.0f, &world);
    check(true, "AI attack detection ran without crash");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.4 — IA Econômica e Militar Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_ai_initialization();
    test_ai_economic_rules();
    test_ai_military_rules();
    test_ai_building_construction();
    test_ai_attacks_player();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL AI BASIC TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME AI BASIC TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
