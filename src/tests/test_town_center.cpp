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

static void test_town_center_building() {
    std::cout << "\n--- Testing Town Center Building ---" << std::endl;
    GameplayWorld world(nullptr);

    check(BuildingType::TOWN_CENTER != BuildingType::NONE, "TOWN_CENTER building type exists");

    int cap_before = world.population_cap(Team::FRIENDLY);

    glm::vec3 tc_pos(0.0f, 0.0f, 0.0f);
    Entity tc_entity = world.place_town_center(Team::FRIENDLY, tc_pos, true);
    check(tc_entity != INVALID_ENTITY, "Spawned Town Center building");

    int cap_after = world.population_cap(Team::FRIENDLY);
    check(cap_after > cap_before, "Town Center increases population cap (+15 each)");
    std::cout << "Pop cap before: " << cap_before << ", after TC: " << cap_after << std::endl;
}

static void test_town_center_resource_drop() {
    std::cout << "\n--- Testing Town Center Resource Drop-off ---" << std::endl;
    GameplayWorld world(nullptr);

    Entity tc = world.place_town_center(Team::FRIENDLY, glm::vec3(0,0,0), true);
    check(tc != INVALID_ENTITY, "TC spawned for drop-off test");

    Entity nearest = world.get_nearest_town_center(Team::FRIENDLY, glm::vec3(0,0,0));
    check(nearest == tc, "Nearest TC to (0,0,0) is the spawned TC");

    UnitStats worker_stats;
    worker_stats.name = "Villager";
    worker_stats.max_hp = 100;
    Entity worker = world.spawn_unit(Team::FRIENDLY, glm::vec3(1.0f, 0.0f, 0.0f), worker_stats);
    check(worker != INVALID_ENTITY, "Spawned worker for drop-off test");

    (void)worker;
}

static void test_tc_count_for_pop_cap() {
    std::cout << "\n--- Testing TC Count for Population Cap ---" << std::endl;
    GameplayWorld world(nullptr);

    int cap_0_tc = world.population_cap(Team::FRIENDLY);

    Entity tc1 = world.place_town_center(Team::FRIENDLY, glm::vec3(10.0f, 0.0f, 0.0f), true);
    Entity tc2 = world.place_town_center(Team::FRIENDLY, glm::vec3(20.0f, 0.0f, 0.0f), true);
    check(tc1 != INVALID_ENTITY, "TC1 spawned");
    check(tc2 != INVALID_ENTITY, "TC2 spawned");

    int cap_2_tc = world.population_cap(Team::FRIENDLY);
    check(cap_2_tc > cap_0_tc, "Population cap increased with more TCs");
    int expected_increase = 2 * 15;
    check(cap_2_tc >= cap_0_tc + expected_increase, "Each TC adds +15 population cap");
    std::cout << "Cap 0 TC: " << cap_0_tc << ", Cap 2 TC: " << cap_2_tc << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.2 — Town Center Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_town_center_building();
    test_town_center_resource_drop();
    test_tc_count_for_pop_cap();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL TOWN CENTER TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME TOWN CENTER TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
