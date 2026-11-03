#include "ecs/gameplay.h"
#include <iostream>

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

int main() {
    std::unordered_map<aom::Entity, aom::TransformComponent> transforms;
    std::unordered_map<aom::Entity, aom::HealthComponent> health;
    std::unordered_map<aom::Entity, aom::UnitComponent> units;

    constexpr aom::Entity friendly = 1;
    constexpr aom::Entity enemy = 2;
    transforms[friendly].position = {0.0f, 0.0f, 0.0f};
    transforms[enemy].position = {1.0f, 0.0f, 0.0f};
    units[friendly].team = aom::Team::FRIENDLY;
    units[enemy].team = aom::Team::ENEMY;
    units[friendly].attack_damage = 25.0f;
    units[friendly].attack_range = 2.0f;
    health[friendly].current_hp = 100;
    health[enemy].current_hp = 25;

    aom::CombatSystem combat(transforms, health, units);
    combat.update(0.016f);

    aom::GameplayWorld world;
    const aom::Entity mover = world.spawn_unit(aom::Team::FRIENDLY, {-2.0f, 0.0f, 0.0f});
    const bool order_ok = world.order_move(mover, {2.0f, 0.0f, 0.0f});
    world.update(1.0f);
    bool ok = true;
    ok &= check(order_ok, "unit accepts move order");
    aom::NavigationGrid grid;
    const auto path = grid.find_path({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    ok &= check(!path.empty() && path.size() > 1, "navigation grid produces waypoints");
    grid.set_blocked(16, 16);
    const auto blocked_path = grid.find_path({-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    ok &= check(!blocked_path.empty(), "navigation routes around blocked cell");

    // Regression: destination inside a 4x4 Town Center footprint. The nudge
    // must pick the nearest exit ring, not the worker's own distant cell,
    // otherwise the path collapses to one node and the worker never moves.
    aom::NavigationGrid footprint_grid;
    for (int dx = -1; dx <= 2; ++dx) {
        for (int dz = -1; dz <= 2; ++dz) {
            footprint_grid.set_blocked(10 + dx, 16 + dz);
        }
    }
    const auto footprint_path = footprint_grid.find_path({-9.5f, 0.0f, -0.5f}, {-6.0f, 0.0f, 0.0f});
    ok &= check(footprint_path.size() > 1, "navigation escapes a 4x4 building footprint");
    const aom::Entity building = world.place_building(aom::Team::FRIENDLY, {0.0f, 0.0f, 0.0f});
    ok &= check(building != aom::INVALID_ENTITY, "building occupies a navigation cell");
    ok &= check(world.demolish_building({0.0f, 0.0f, 0.0f}), "building can be demolished");
    ok &= check(health[enemy].current_hp <= 0, "friendly unit damages nearby enemy");
    ok &= check(!combat.dead_entities().empty() && combat.dead_entities()[0] == enemy,
                "combat reports dead enemy");

    // --- Town Center & Villager Training Loop Tests ---
    const aom::Entity tc = world.place_town_center(aom::Team::FRIENDLY, {-6.0f, 0.0f, 0.0f}, true);
    ok &= check(tc != aom::INVALID_ENTITY, "town center placed successfully");
    const auto* tc_bld = world.get_building(tc);
    ok &= check(tc_bld != nullptr && tc_bld->type == aom::BuildingType::TOWN_CENTER, "building type is TOWN_CENTER");
    ok &= check(tc_bld != nullptr && tc_bld->complete, "town center is complete");
    const auto* tc_hp = world.get_health(tc);
    ok &= check(tc_hp != nullptr && tc_hp->max_hp == 2400, "town center has 2400 max HP");

    // Test queueing villagers (Greek Villager: 50 Food each)
    ok &= check(world.queue_villager(tc), "queued first villager in town center");
    ok &= check(world.queue_villager(tc), "queued second villager in town center");
    ok &= check(world.get_training_queue_count(tc) == 2, "town center queue contains 2 villagers");

    // Advance time past first villager duration (4.5s)
    world.update(4.6f);
    ok &= check(world.get_training_queue_count(tc) == 1, "first villager completed, 1 remaining in queue");
    ok &= check(world.units().size() == 2, "first trained villager actually spawned in the world");

    // Advance time past second villager duration
    world.update(4.6f);
    ok &= check(world.get_training_queue_count(tc) == 0, "second villager completed, queue empty");
    ok &= check(world.units().size() == 3, "second trained villager actually spawned in the world");

    bool trained_villager_is_worker = false;
    for (const auto& [id, unit] : world.units()) {
        if (unit.team == aom::Team::FRIENDLY && unit.unit_type_name == "Villager Greek") {
            trained_villager_is_worker = unit.worker;
            break;
        }
    }
    ok &= check(trained_villager_is_worker, "trained villager has the worker flag");

    // ── Sprint 2: Population Cap ($Pop/Cap$) Verification ───────────────
    // Base pop: 10 + 15 (Town Center) = 25
    ok &= check(world.population_cap(aom::Team::FRIENDLY) == 25, "population cap with 1 Town Center is 25");
    int initial_friendly_pop = world.current_population(aom::Team::FRIENDLY);
    ok &= check(initial_friendly_pop > 0, "current friendly population is tracked correctly");

    // Adding a complete House adds +10 to population cap
    const aom::Entity house = world.place_building(aom::Team::FRIENDLY, {-10.0f, 0.0f, 0.0f}, 1.0f, aom::BuildingType::HOUSE, "Casa");
    world.buildings()[house].complete = true;
    ok &= check(world.population_cap(aom::Team::FRIENDLY) == 35, "population cap with 1 TC and 1 House is 35");

    // ── Sprint 2: Autonomous Harvesting & Cargo Deposit Cycle ───────────
    // Spawn outside the Town Center footprint (it blocks a 4x4 cell area).
    const aom::Entity worker = world.spawn_worker({-12.0f, 0.0f, 0.0f});
    world.add_resource_node(aom::ResourceType::FOOD, {-12.5f, 0.0f, 0.0f}, 50);
    const int food_before = world.resources().food;

    ok &= check(world.order_gather(worker, {-12.5f, 0.0f, 0.0f}), "worker accepts gather order");
    // Advance ~4.2s in 60 Hz steps: worker harvests 10 Food (carrying capacity).
    // Large dt steps (>= 1s) overshoot 1 m waypoints, so keep the test realistic.
    constexpr float kFrame = 1.0f / 60.0f;
    for (int step = 0; step < 250; ++step) {
        world.update(kFrame);
    }
    const auto* w_comp = world.get_unit(worker);
    ok &= check(w_comp != nullptr && w_comp->carried_resource_amount > 0, "worker harvested resources into cargo");

    // Advance time for worker to walk to Town Center and deposit cargo
    for (int step = 0; step < 600; ++step) {
        world.update(kFrame);
    }
    ok &= check(world.resources().food > food_before, "worker successfully deposited cargo at Town Center");

    // ── Sprint 2: Worker Construction Order ─────────────────────────────
    const int wood_before = world.resources().wood;
    ok &= check(world.order_worker_build(worker, aom::BuildingType::HOUSE, {-8.0f, 0.0f, -4.0f}), "worker accepted build order for House");
    ok &= check(world.resources().wood == wood_before - 100, "100 wood deducted for House construction");

    // Advance time for worker to walk to the site and build the house
    for (int step = 0; step < 600; ++step) {
        world.update(kFrame);
    }
    const auto* w_after = world.get_unit(worker);
    ok &= check(w_after != nullptr, "worker entity is valid after construction");

    return ok ? 0 : 1;
}