#include "ecs/gameplay.h"
#include "game/victory.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace aom;

static bool g_all_passed = true;

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (!condition) g_all_passed = false;
    return condition;
}

static void test_conquest_victory_all_enemies_dead() {
    std::cout << "\n--- Testing Conquest: All Enemies Defeated ---" << std::endl;
    GameplayWorld world(nullptr);

    // Criar um TC amigável completo para que friendly_tc > 0
    Entity friendly_tc = world.place_town_center(Team::FRIENDLY, glm::vec3(0,0,0), true);
    check(friendly_tc != INVALID_ENTITY, "Spawned friendly Town Center");

    // Sem TCs inimigos -> vitória por Conquest
    auto result = check_victory(world, VictoryCondition::CONQUEST);
    check(result == VictoryResult::WIN, "Conquest: WIN when no enemy TCs");
}

static void test_conquest_victory_enemy_alive() {
    std::cout << "\n--- Testing Conquest: Enemy Still Alive ---" << std::endl;
    GameplayWorld world(nullptr);

    // TC amigável completo
    Entity friendly_tc = world.place_town_center(Team::FRIENDLY, glm::vec3(0,0,0), true);
    check(friendly_tc != INVALID_ENTITY, "Spawned friendly Town Center");

    // TC inimigo completo (vivo)
    Entity enemy_tc = world.place_town_center(Team::ENEMY, glm::vec3(30,0,0), true);
    check(enemy_tc != INVALID_ENTITY, "Spawned enemy Town Center");

    // Com TCs dos dois lados, não há vitória ainda
    auto result = check_victory(world, VictoryCondition::CONQUEST);
    check(result == VictoryResult::NONE, "Conquest: NONE while both TCs alive");
}

static void test_sudden_death_enemy_tc_destroyed() {
    std::cout << "\n--- Testing Sudden Death: Enemy TC Destroyed ---" << std::endl;
    GameplayWorld world(nullptr);

    // TC amigável completo
    Entity friendly_tc = world.place_town_center(Team::FRIENDLY, glm::vec3(0,0,0), true);
    check(friendly_tc != INVALID_ENTITY, "Spawned friendly Town Center");

    // TC inimigo completo
    Entity enemy_tc = world.place_town_center(Team::ENEMY, glm::vec3(30,0,0), true);
    check(enemy_tc != INVALID_ENTITY, "Spawned enemy TC");

    // Destruir o TC inimigo
    world.set_entity_hp(enemy_tc, 0);

    // Sudden Death: TC inimigo destruído -> vitória
    auto result = check_victory(world, VictoryCondition::SUDDEN_DEATH);
    check(result == VictoryResult::WIN, "Sudden Death: WIN when enemy TC destroyed");
}

static void test_sudden_death_own_tc_destroyed() {
    std::cout << "\n--- Testing Sudden Death: Own TC Destroyed (Defeat) ---" << std::endl;
    GameplayWorld world(nullptr);

    // TC amigável completo
    Entity friendly_tc = world.place_town_center(Team::FRIENDLY, glm::vec3(0,0,0), true);
    check(friendly_tc != INVALID_ENTITY, "Spawned own Town Center");

    // Destruir o TC amigável
    world.set_entity_hp(friendly_tc, 0);

    // Sudden Death: TC próprio destruído -> derrota
    auto result = check_victory(world, VictoryCondition::SUDDEN_DEATH);
    check(result == VictoryResult::LOSE, "Sudden Death: LOSE when own TC destroyed");
}

static void test_victory_result_enum_values() {
    std::cout << "\n--- Testing Victory Result Enum ---" << std::endl;
    check(static_cast<uint8_t>(VictoryResult::NONE) == 0, "VictoryResult::NONE == 0");
    check(static_cast<uint8_t>(VictoryResult::WIN) == 1, "VictoryResult::WIN == 1");
    check(static_cast<uint8_t>(VictoryResult::LOSE) == 2, "VictoryResult::LOSE == 2");

    check(static_cast<uint8_t>(VictoryCondition::CONQUEST) == 0, "VictoryCondition::CONQUEST == 0");
    check(static_cast<uint8_t>(VictoryCondition::SUDDEN_DEATH) == 1, "VictoryCondition::SUDDEN_DEATH == 1");
    std::cout << "Victory enum values correct" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.3 — Victory Conditions Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_victory_result_enum_values();
    test_conquest_victory_all_enemies_dead();
    test_conquest_victory_enemy_alive();
    test_sudden_death_enemy_tc_destroyed();
    test_sudden_death_own_tc_destroyed();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL VICTORY CONDITION TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME VICTORY CONDITION TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
