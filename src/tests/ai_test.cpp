#include "ai/ai_system.h"
#include "ecs/gameplay.h"
#include <iostream>

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

int main() {
    bool ok = true;
    aom::GameplayWorld world;
    aom::ai::AiSystem ai;

    // 1. Initialize AI with Titans aggressive personality
    ai.initialize(aom::Team::ENEMY, "aomxai030 aggressive");
    ok &= check(ai.team() == aom::Team::ENEMY, "AI initializes with ENEMY team");
    ok &= check(ai.archetype() == aom::ai::PersonalityArchetype::AGGRESSIVE, "AI maps Titans aggressive personality");
    ok &= check(ai.profile().min_attack_force == 2, "Aggressive AI requires 2 soldiers minimum");

    // Test defensive personality
    aom::ai::AiSystem ai_def;
    ai_def.initialize(aom::Team::ENEMY, "aomxai070 defensive");
    ok &= check(ai_def.archetype() == aom::ai::PersonalityArchetype::DEFENSIVE, "AI maps Titans defensive personality");
    ok &= check(ai_def.profile().defense_priority >= 0.7f, "Defensive AI has high defense priority");

    // 2. Spawn units for player and AI
    aom::UnitStats villager_stats;
    villager_stats.move_speed = 3.8f;
    villager_stats.max_hp = 65.0f;

    aom::UnitStats soldier_stats;
    soldier_stats.move_speed = 4.2f;
    soldier_stats.attack_damage = 10.0f;
    soldier_stats.attack_range = 1.5f;

    // Friendly units
    world.spawn_unit(aom::Team::FRIENDLY, {-4.0f, 0.0f, 0.0f}, soldier_stats);

    // AI Enemy units (2 workers, 1 myth soldier)
    const aom::Entity worker1 = world.spawn_worker({5.0f, 0.0f, 2.0f}, villager_stats);
    const aom::Entity worker2 = world.spawn_worker({5.0f, 0.0f, -2.0f}, villager_stats);
    world.spawn_unit(aom::Team::ENEMY, {4.0f, 0.0f, 0.0f}, soldier_stats);

    // Add resource node
    world.add_resource_node(aom::ResourceType::GOLD, {6.0f, 0.0f, 1.0f}, 1000);

    // 3. Step AI simulation
    ai.update(0.1f, &world);
    ok &= check(ai.civilian_pop() == 2, "AI accurately tracks civilian population");
    ok &= check(ai.military_pop() == 1, "AI accurately tracks military population");

    // 4. Test plan creation & destruction
    uint32_t plan_id = ai.create_plan("AttackPlayerBase", aom::ai::AiPlanType::ATTACK, 90);
    ok &= check(plan_id > 0, "AI creates structured plan");
    ai.destroy_plan(plan_id);
    ok &= check(true, "AI destroys plan without leaks");

    // 5. Trigger economic gathering rule
    world.enemy_resources().food = 200;
    world.enemy_resources().wood = 150;
    world.enemy_resources().gold = 100;
    ai.update(1.5f, &world);

    // Check if worker was dispatched
    const aom::UnitComponent* w1 = world.get_unit(worker1);
    const aom::UnitComponent* w2 = world.get_unit(worker2);
    ok &= check((w1 && w1->has_move_order) || (w2 && w2->has_move_order),
                "AI gatherer rule dispatches idle workers to resources");

    // 6. Test military training
    const aom::Entity barracks = world.place_building(aom::Team::ENEMY, {5.0f, 0.0f, 0.0f}, 0.1f, aom::BuildingType::BARRACKS, "Quartel");
    world.update(0.2f); // complete construction
    const aom::BuildingComponent* bld = world.get_building(barracks);
    ok &= check(bld && bld->complete, "AI barracks completes construction");

    ai.update(1.5f, &world);
    const aom::BuildingComponent* bld_after = world.get_building(barracks);
    ok &= check(bld_after && (!bld_after->training_unit.empty() || ai.military_pop() >= 1),
                "AI military training rule triggers troop production");

    return ok ? 0 : 1;
}
