#include "ecs/gameplay.h"
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

static void test_multi_select_count() {
    std::cout << "\n--- Testing Multi-Selection Count ---" << std::endl;
    GameplayWorld world(nullptr);

    check(world.selected_count() == 0, "No entities selected initially");
    check(world.selected_entity() == INVALID_ENTITY, "Selected entity is INVALID initially");

    UnitStats villager_stats;
    villager_stats.name = "Villager";
    villager_stats.max_hp = 100;

    Entity u1 = world.spawn_unit(Team::FRIENDLY, glm::vec3(0,0,0), villager_stats);
    Entity u2 = world.spawn_unit(Team::FRIENDLY, glm::vec3(2,0,0), villager_stats);
    Entity u3 = world.spawn_unit(Team::FRIENDLY, glm::vec3(4,0,0), villager_stats);
    check(u1 != INVALID_ENTITY && u2 != INVALID_ENTITY && u3 != INVALID_ENTITY, "Spawned 3 units");

    world.select_entity(u1, true);
    world.select_entity(u2, true);
    world.select_entity(u3, true);

    check(world.selected_count() == 3, "Three units selected -> count is 3");
    // u1 deve estar entre as entidades selecionadas (ordem do unordered_map nao e deterministica)
    auto sel_list = world.selected_entities();
    bool u1_selected = false;
    for (Entity e : sel_list) { if (e == u1) u1_selected = true; }
    check(u1_selected, "u1 is among selected entities");
}

static void test_selection_rings() {
    std::cout << "\n--- Testing Selection Rings ---" << std::endl;
    GameplayWorld world(nullptr);

    UnitStats hoplite_stats;
    hoplite_stats.name = "Hoplite";
    hoplite_stats.max_hp = 100;

    Entity u = world.spawn_unit(Team::FRIENDLY, glm::vec3(0,0,0), hoplite_stats);
    check(u != INVALID_ENTITY, "Spawned military unit");

    world.select_entity(u, true);
    check(world.selected_count() == 1, "One unit selected");

    world.select_entity(u, false);
    check(world.selected_count() == 0, "After deselect, count is 0");
}

static void test_box_select_concept() {
    std::cout << "\n--- Testing Box Select Concept ---" << std::endl;
    GameplayWorld world(nullptr);

    UnitStats villager_stats;
    villager_stats.name = "Villager";
    villager_stats.max_hp = 100;

    Entity a = world.spawn_unit(Team::FRIENDLY, glm::vec3(0,0,0), villager_stats);
    Entity b = world.spawn_unit(Team::FRIENDLY, glm::vec3(1,0,1), villager_stats);
    Entity c = world.spawn_unit(Team::FRIENDLY, glm::vec3(2,0,2), villager_stats);
    check(a != INVALID_ENTITY && b != INVALID_ENTITY && c != INVALID_ENTITY, "Spawned 3 units in box area");

    world.select_entity(a, true);
    world.select_entity(b, true);
    world.select_entity(c, true);

    check(world.selected_count() == 3, "Box select selected 3 units");

    world.select_entity(a, false);
    world.select_entity(b, false);
    world.select_entity(c, false);
    check(world.selected_count() == 0, "Click on empty ground deselects all");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.2 — Selection Group Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_multi_select_count();
    test_selection_rings();
    test_box_select_concept();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL SELECTION GROUP TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME SELECTION GROUP TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
