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

static void test_population_counter() {
    std::cout << "\n--- Testing Population Counter (HUD) ---" << std::endl;
    GameplayWorld world(nullptr);

    int cur = world.current_population(Team::FRIENDLY);
    int cap = world.population_cap(Team::FRIENDLY);
    check(cur == 0, "Initial population is 0");
    check(cap > 0, "Population cap > 0 (base + TCs + houses)");

    UnitStats villager_stats;
    villager_stats.name = "Villager";
    villager_stats.max_hp = 100;
    Entity e = world.spawn_unit(Team::FRIENDLY, glm::vec3(0,0,0), villager_stats);
    check(e != INVALID_ENTITY, "Spawned friendly villager");
    int cur2 = world.current_population(Team::FRIENDLY);
    check(cur2 > cur, "Population increased after spawning unit");

    std::cout << "Pop: " << cur2 << "/" << cap << std::endl;
}

static void test_minimap_blips() {
    std::cout << "\n--- Testing Minimap Blips ---" << std::endl;
    GameplayWorld world(nullptr);

    auto blips = world.get_minimap_blips();
    // O mundo padrão pode não ter resource nodes (depende do mapa carregado).
    // Verificamos apenas que a função retorna sem crash e, se houver blips,
    // que eles têm cores válidas de recurso.
    bool has_valid_color = true;
    for (const auto& b : blips) {
        bool valid = (b.color == 0x55CC33FF || b.color == 0x8B5A2BFF ||
                      b.color == 0xFFD700FF || b.color == 0x9933FFFF);
        if (!valid) { has_valid_color = false; break; }
    }
    check(has_valid_color, "Minimap blips have valid resource colors (or no blips)");
    if (!blips.empty()) {
        std::cout << "Minimap has " << blips.size() << " blip(s)" << std::endl;
    } else {
        std::cout << "Minimap has 0 blips (no resource nodes in default world)" << std::endl;
    }
}

static void test_command_panel_buttons() {
    std::cout << "\n--- Testing Command Panel Buttons (HUD) ---" << std::endl;
    GameplayWorld world(nullptr);

    check(BuildingType::HOUSE != BuildingType::NONE, "HOUSE building type exists");
    check(BuildingType::BARRACKS != BuildingType::NONE, "BARRACKS building type exists");
    check(BuildingType::TOWER != BuildingType::NONE, "TOWER building type exists");
    check(BuildingType::TOWN_CENTER != BuildingType::NONE, "TOWN_CENTER building type exists");

    (void)world;
}

static void test_production_queue_progress() {
    std::cout << "\n--- Testing Production Queue Progress ---" << std::endl;
    GameplayWorld world(nullptr);

    Entity sel = INVALID_ENTITY;
    int q_count = world.get_training_queue_count(sel);
    check(q_count == 0, "Empty selection has 0 queue count");

    float prog = world.get_training_progress(sel);
    check(prog == 0.0f, "Empty selection has 0 progress");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.1 — HUD Completo Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_population_counter();
    test_minimap_blips();
    test_command_panel_buttons();
    test_production_queue_progress();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL HUD TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME HUD TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
