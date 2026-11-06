#include "ecs/gameplay.h"
#include "game/save_load.h"
#include "game/victory.h"
#include "assets/cube_map.h"
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

    // ── Sprint 9: Save / Load State Persistence (Roundtrip) ────────────────
    {
        world.resources_mutable().food  = 888;
        world.resources_mutable().wood  = 666;
        world.resources_mutable().gold  = 444;
        world.resources_mutable().favor = 222;

        const std::string save_path = "build-win/test_roundtrip.dat";
        bool save_ok = aom::save_game_state(save_path, world, 123.5f, 9999);
        ok &= check(save_ok, "save_game_state successfully exported world snapshot");

        aom::SaveGameData loaded{};
        bool load_ok = aom::load_game_state(save_path, loaded);
        ok &= check(load_ok, "load_game_state successfully read snapshot");
        ok &= check(loaded.magic == aom::kSaveMagic, "Save magic header matches AOMS");
        ok &= check(loaded.food[0] == 888 && loaded.wood[0] == 666 &&
                    loaded.gold[0] == 444 && loaded.favor[0] == 222, "Save resources roundtrip matches exactly");
        ok &= check(loaded.game_time_seconds == 123.5f, "Save game time persists accurately");
        ok &= check(loaded.map_seed == 9999, "Save map seed persists accurately");
        ok &= check(aom::has_valid_save(save_path), "has_valid_save returns true for created save");

        // ── Sprint 12: reconstrução das entidades (unidades e edificios) ─────
        ok &= check(loaded.units.size() == world.units().size(),
                    "Save roundtrip restores the exact unit count");
        ok &= check(loaded.buildings.size() == world.buildings().size(),
                    "Save roundtrip restores the exact building count");

        // Amostra a primeira unidade: posicao, HP, time e tipo precisam bater.
        bool unit_found = false;
        for (const auto& su : loaded.units) {
            const auto* orig = world.get_unit(static_cast<aom::Entity>(su.entity));
            if (!orig) continue;
            unit_found = true;
            const auto* tr = world.get_transform(static_cast<aom::Entity>(su.entity));
            ok &= check(su.unit_type == orig->unit_type_name,
                        "Restored unit keeps its type name");
            ok &= check(su.team == (orig->team == aom::Team::FRIENDLY ? 0 : 1),
                        "Restored unit keeps its team");
            ok &= check(su.worker == orig->worker,
                        "Restored unit keeps the worker flag");
            ok &= check(tr && std::abs(su.pos_x - tr->position.x) < 0.01f &&
                        std::abs(su.pos_z - tr->position.z) < 0.01f,
                        "Restored unit keeps its position");
            break;
        }
        ok &= check(unit_found, "At least one unit was serialized with its entity id");

        // Amostra o primeiro edificio: tipo, conclusao e progresso.
        bool building_found = false;
        for (const auto& sb : loaded.buildings) {
            const auto* orig = world.get_building(static_cast<aom::Entity>(sb.entity));
            if (!orig) continue;
            building_found = true;
            ok &= check(static_cast<int>(sb.building_type) == static_cast<int>(orig->type),
                        "Restored building keeps its type");
            ok &= check(sb.complete == orig->complete,
                        "Restored building keeps the completion flag");
            ok &= check(sb.name == orig->name,
                        "Restored building keeps its name");
            break;
        }
        ok &= check(building_found, "At least one building was serialized with its entity id");
    }

    // ── Sprint 9: Victory Conditions (Conquest & Sudden Death) ─────────────
    {
        aom::GameplayWorld victory_world(nullptr);
        const aom::Entity tc_f = victory_world.place_town_center(aom::Team::FRIENDLY, {0.0f, 0.0f, 0.0f});
        const aom::Entity tc_e = victory_world.place_town_center(aom::Team::ENEMY,    {20.0f, 0.0f, 0.0f});

        // 1. Both TCs alive -> NONE
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::CONQUEST) == aom::VictoryResult::NONE,
                    "Conquest: game ongoing when both Town Centers are standing");
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::SUDDEN_DEATH) == aom::VictoryResult::NONE,
                    "Sudden Death: game ongoing when both Town Centers are standing");

        // 2. Destroy Enemy TC -> Player WINS
        victory_world.set_entity_hp(tc_e, 0);
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::CONQUEST) == aom::VictoryResult::WIN,
                    "Conquest: player wins when enemy Town Center is destroyed");
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::SUDDEN_DEATH) == aom::VictoryResult::WIN,
                    "Sudden Death: player wins when enemy Town Center is destroyed");

        // 3. Destroy Friendly TC as well -> LOSE
        victory_world.set_entity_hp(tc_f, 0);
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::CONQUEST) == aom::VictoryResult::LOSE,
                    "Conquest: player loses when friendly Town Center is destroyed");
        ok &= check(aom::check_victory(victory_world, aom::VictoryCondition::SUDDEN_DEATH) == aom::VictoryResult::LOSE,
                    "Sudden Death: player loses when friendly Town Center is destroyed");
    }

    // ── Sprint 1: Combate integrado via GameplayWorld ───────────────────────
    {
        aom::GameplayWorld combat_world(nullptr);
        const aom::Entity attacker = combat_world.spawn_unit(aom::Team::FRIENDLY, {0.0f, 0.0f, 0.0f});
        const aom::Entity target   = combat_world.spawn_unit(aom::Team::ENEMY,   {1.0f, 0.0f, 0.0f});

        auto* atk = combat_world.unit_component_mutable(attacker);
        auto* tgt = combat_world.unit_component_mutable(target);
        if (atk && tgt) {
            atk->attack_damage = 50.0f;
            atk->attack_range = 2.0f;
            atk->attack_cooldown = 0.1f;
            atk->attack_target = target;
            tgt->max_hp = 100;
            tgt->current_hp = 100;
        }

        bool target_died = false;
        for (int step = 0; step < 200; ++step) {
            combat_world.update(0.016f);
            auto* t = combat_world.get_health(target);
            if (t && t->current_hp <= 0) {
                target_died = true;
                break;
            }
        }
        ok &= check(target_died, "Combate integrado: inimigo morre após dano acumulado");
        ok &= check(combat_world.units().size() == 1,
                    "Combate integrado: entidade morta removida do mundo");
    }

    // ── Sprint 9: Skybox / Cube Map (.cub) Parser ─────────────────────────
    {
        const std::string mock_cub =
            "// Up\r\nSFX A Shiny Water\r\n\r\n"
            "// Down\r\nSFX A Shiny Water\r\n\r\n"
            "// Front\r\nSFX A Shiny Water\r\n\r\n"
            "// Back\r\nSFX A Shiny Water\r\n\r\n"
            "// Left\r\nSFX A Shiny Water\r\n\r\n"
            "// Right\r\nSFX A Shiny Water\r\n";
        aom::CubeMapDef cub = aom::parse_cub_file(mock_cub);
        ok &= check(cub.is_valid(), "parse_cub_file returns valid definition");
        ok &= check(cub.up == "SFX A Shiny Water", "CubeMap face Up matches");
        ok &= check(cub.down == "SFX A Shiny Water", "CubeMap face Down matches");
        ok &= check(cub.front == "SFX A Shiny Water", "CubeMap face Front matches");
        ok &= check(cub.back == "SFX A Shiny Water", "CubeMap face Back matches");
        ok &= check(cub.left == "SFX A Shiny Water", "CubeMap face Left matches");
        ok &= check(cub.right == "SFX A Shiny Water", "CubeMap face Right matches");
    }

    return ok ? 0 : 1;
}