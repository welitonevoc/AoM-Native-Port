#include "ecs/gameplay.h"
#include "anim/animation_system.h"
#include "renderer/renderer.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

namespace {

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

class MockTestRenderer : public aom::IRenderer {
public:
    uint32_t dynamic_mesh_updates = 0;
    std::vector<std::pair<glm::vec3, glm::vec3>> drawn_lines;

    bool initialize(void*, int, int) override { return true; }
    void shutdown() override {}
    void resize(int, int) override {}
    void begin_frame() override {}
    void end_frame() override {}

    aom::TextureHandle create_texture(const aom::DdtTexture&) override { return 1; }
    aom::TextureHandle create_texture(uint32_t, uint32_t, const uint8_t*) override { return 1; }
    void update_texture(aom::TextureHandle, uint32_t, uint32_t, const uint8_t*) override {}
    void destroy_texture(aom::TextureHandle) override {}

    aom::MeshHandle create_mesh(const aom::Vertex*, uint32_t, const uint32_t*, uint32_t) override { return 1; }
    aom::MeshHandle create_terrain_mesh(const aom::TerrainVertex*, uint32_t, const uint32_t*, uint32_t) override { return 1; }
    void destroy_mesh(aom::MeshHandle) override {}

    void set_camera(const aom::Camera&) override {}
    void set_light(const aom::DirectionalLight&) override {}
    void draw_mesh(aom::MeshHandle, const glm::mat4&, aom::TextureHandle, aom::BlendMode) override {}
    void draw_terrain(aom::MeshHandle, aom::TextureHandle, aom::TextureHandle,
                      aom::TextureHandle, aom::TextureHandle) override {}
    void draw_water(const glm::vec4&, aom::TextureHandle, float,
                       const glm::vec3& = glm::vec3(0, 50, 170),
                       float = -0.4f, float = -0.4f, aom::TextureHandle = aom::INVALID_TEXTURE) override {}
    void draw_mesh_skinned(aom::MeshHandle, const glm::mat4&,
                           aom::TextureHandle,
                           const std::vector<glm::mat4>&,
                           aom::BlendMode = aom::BlendMode::OPAQUE) override {}
    void draw_billboard(const glm::vec3&, const glm::vec2&,
                         aom::TextureHandle, uint32_t) override {}

    void begin_ui() override {}
    void draw_ui_quad(const glm::vec2&, const glm::vec2&, aom::TextureHandle, uint32_t) override {}
    void draw_ui_quad_nineslice(const glm::vec2&, const glm::vec2&, aom::TextureHandle,
                                 uint32_t, uint32_t, float, float, float, float,
                                 uint32_t) override {}
    void draw_ui_text(const std::string&, const glm::vec2&, float, uint32_t) override {}
    void end_ui() override {}

    void draw_debug_line(const glm::vec3& from, const glm::vec3& to, uint32_t) override {
        drawn_lines.push_back({from, to});
    }

    float get_fps() const override { return 60.0f; }
    const char* get_name() const override { return "MockTestRenderer"; }

    bool frustum_test_sphere(const glm::vec3&, float) const override { return true; }

    aom::MeshHandle create_dynamic_mesh(const aom::Vertex*, uint32_t, const uint32_t*, uint32_t) override {
        return 10;
    }

    void update_mesh_vertices(aom::MeshHandle, const glm::vec3*, const glm::vec3*, uint32_t) override {
        ++dynamic_mesh_updates;
    }
};

struct TestParticle {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float life = 0.0f;
    float max_life = 1.0f;
    uint32_t color = 0xFFFFD000;
    float size = 0.15f;
};

class TestParticleSystem {
public:
    std::vector<TestParticle> particles;

    void spawn(const glm::vec3& pos, const glm::vec3& vel, float life, uint32_t color, float size = 0.15f) {
        if (particles.size() >= 1000) return;
        TestParticle p;
        p.pos = pos;
        p.vel = vel;
        p.life = 0.0f;
        p.max_life = life;
        p.color = color;
        p.size = size;
        particles.push_back(p);
    }

    void spawn_construction_sparks(const glm::vec3& pos, int count = 8) {
        for (int i = 0; i < count; ++i) {
            float vx = 0.5f * (i % 3 - 1);
            float vz = 0.5f * (i % 2);
            float vy = 1.5f + 0.2f * i;
            spawn(pos, glm::vec3(vx, vy, vz), 0.6f, 0xFFFFCC00u, 0.12f);
        }
    }

    void spawn_combat_hit(const glm::vec3& pos, int count = 10) {
        for (int i = 0; i < count; ++i) {
            float vx = (i % 2 == 0 ? 1.0f : -1.0f) * 2.0f;
            float vy = 1.2f + 0.1f * i;
            float vz = (i % 3 == 0 ? 1.0f : -1.0f) * 1.5f;
            spawn(pos, glm::vec3(vx, vy, vz), 0.4f, 0xFFFFFFFFu, 0.14f);
        }
    }

    void spawn_harvest_chips(aom::ResourceType type, const glm::vec3& pos, int count = 6) {
        uint32_t col = 0xFFFFD700u; // Gold
        if (type == aom::ResourceType::WOOD) col = 0xFF8B5A2Bu;
        else if (type == aom::ResourceType::FOOD) col = 0xFF66BB33u;

        for (int i = 0; i < count; ++i) {
            spawn(pos, glm::vec3(0.3f * i, 1.0f, -0.3f * i), 0.5f, col, 0.10f);
        }
    }

    void spawn_death_smoke(const glm::vec3& pos, int count = 12) {
        for (int i = 0; i < count; ++i) {
            spawn(pos, glm::vec3(0.1f * (i - 6), 0.8f, 0.1f * (i - 6)), 1.2f, 0xAAEEEEFFu, 0.20f);
        }
    }

    void update(float dt) {
        for (auto& p : particles) {
            p.life += dt;
            p.pos += p.vel * dt;
            p.vel.y -= 9.8f * dt;
            if (p.pos.y < 0.02f) {
                p.pos.y = 0.02f;
                p.vel.y = -p.vel.y * 0.35f;
                p.vel.x *= 0.75f;
                p.vel.z *= 0.75f;
            }
        }
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                           [](const TestParticle& p) { return p.life >= p.max_life; }),
            particles.end());
    }
};

} // namespace

int main() {
    std::cout << "=== Running Sprint 4: Unit Animations & Particles Test Suite ===\n";
    bool ok = true;

    // ────────────────────────────────────────────────────────────────────────
    // Test 1: UnitAnimationState playback, time looping and non-looping death
    // ────────────────────────────────────────────────────────────────────────
    aom::anim::UnitAnimationState anim_state;
    ok &= check(anim_state.current_state() == "Idle", "initial state defaults to Idle");
    ok &= check(anim_state.current_time() == 0.0f, "initial time is 0.0");

    anim_state.play("walk", true);
    ok &= check(anim_state.current_state() == "walk", "playing walk state sets state name");
    ok &= check(anim_state.is_looping(), "walk state is looping");
    ok &= check(anim_state.is_playing(), "walk state is actively playing");

    anim_state.update(0.5f);
    ok &= check(anim_state.current_time() >= 0.49f, "clock advances with delta time");

    // Death animation does not loop and stops at duration
    anim_state.play("death", false);
    ok &= check(anim_state.current_state() == "death", "playing death state sets state name");
    ok &= check(!anim_state.is_looping(), "death state does not loop");
    anim_state.update(2.0f);
    ok &= check(!anim_state.is_playing(), "death state stops playing after duration");

    // ────────────────────────────────────────────────────────────────────────
    // Test 2: BRG Animation Clip Geometric Vertex Morphing Evaluation
    // ────────────────────────────────────────────────────────────────────────
    aom::anim::BrgAnimationClip clip;
    clip.name = "test_morph.brg";
    clip.aset.duration = 1.0f;
    clip.aset.num_frames = 2;
    clip.num_vertices = 3;
    clip.indices = {0, 1, 2};
    clip.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};

    aom::anim::BrgFrame f0, f1;
    f0.positions = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}};
    f0.normals   = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

    f1.positions = {{0.0f, 4.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}};
    f1.normals   = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

    clip.frames.push_back(std::move(f0));
    clip.frames.push_back(std::move(f1));
    ok &= check(clip.is_valid(), "synthetic BRG clip is valid");

    std::vector<glm::vec3> eval_pos, eval_norm;
    clip.evaluate(0.25f, eval_pos, eval_norm);
    ok &= check(eval_pos.size() == 3, "evaluated positions have 3 vertices");
    // At t = 0.25s (alpha = 0.5 between frame 0 and frame 1 in a 2-frame 1.0s clip), vertex 0 pos.y is 2.0
    ok &= check(std::abs(eval_pos[0].y - 2.0f) < 0.01f, "vertex morphing interpolates vertex 0 Y to 2.0");
    ok &= check(std::abs(eval_pos[1].x - 3.0f) < 0.01f, "vertex morphing interpolates vertex 1 X to 3.0");

    // ────────────────────────────────────────────────────────────────────────
    // Test 3: GameplayWorld Auto-attachment & Full Animation State Transitions
    // ────────────────────────────────────────────────────────────────────────
    MockTestRenderer mock_renderer;
    aom::GameplayWorld world;
    world.set_anim_renderer(&mock_renderer);
    world.set_default_animation(&clip, 1);

    // Spawning a unit automatically attaches dynamic GPU mesh & animation component
    aom::Entity hoplite = world.spawn_unit(aom::Team::FRIENDLY, {0.0f, 0.0f, 0.0f});
    ok &= check(world.animations().count(hoplite) == 1, "spawn_unit auto-attaches dynamic animated mesh");
    ok &= check(world.animations().at(hoplite).anim_state.current_state() == "idle",
                "spawned unit starts in idle animation");

    // Movement transition: ordering move updates state to "walk"
    world.order_move(hoplite, {5.0f, 0.0f, 0.0f});
    world.update(0.1f);
    ok &= check(world.animations().at(hoplite).anim_state.current_state() == "walk",
                "moving unit transitions animation state to walk");
    ok &= check(mock_renderer.dynamic_mesh_updates > 0,
                "GPU dynamic mesh vertices uploaded during animation update");

    // Attack transition: setting attack target transitions animation to "attack"
    aom::Entity enemy = world.spawn_unit(aom::Team::ENEMY, {5.0f, 0.0f, 0.0f});
    world.order_attack(hoplite, enemy);
    world.update(0.1f);
    ok &= check(world.animations().at(hoplite).anim_state.current_state() == "attack",
                "unit targeting enemy transitions animation state to attack");

    // Worker Harvesting transition: worker gathering transitions to "gather"
    aom::Entity worker = world.spawn_worker({-5.0f, 0.0f, 0.0f});
    world.add_resource_node(aom::ResourceType::GOLD, {-4.5f, 0.0f, 0.0f}, 500);
    world.order_gather(worker, {-4.5f, 0.0f, 0.0f});
    // Advance so worker arrives and begins harvesting
    for (int i = 0; i < 4; ++i) world.update(0.5f);
    ok &= check(world.animations().at(worker).anim_state.current_state() == "gather",
                "worker actively harvesting transitions animation state to gather");

    // Worker Construction transition: worker building transitions to "build"
    aom::Entity foundation = world.place_building(aom::Team::FRIENDLY, {-4.0f, 0.0f, -2.0f}, 5.0f, aom::BuildingType::HOUSE, "Casa");
    world.order_build(worker, foundation);
    for (int i = 0; i < 4; ++i) world.update(0.5f);
    ok &= check(world.animations().at(worker).anim_state.current_state() == "build",
                "worker adjacent to foundation transitions animation state to build");

    // ────────────────────────────────────────────────────────────────────────
    // Test 4: Interactive Particle System Kinematics, Gravity & Lifetime
    // ────────────────────────────────────────────────────────────────────────
    TestParticleSystem ps;
    ps.spawn(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f), 1.0f, 0xFFFFCC00u);
    ok &= check(ps.particles.size() == 1, "spawned 1 particle");

    // Update 0.1s: X advances, gravity accelerates downward
    ps.update(0.1f);
    ok &= check(std::abs(ps.particles[0].pos.x - 0.2f) < 0.01f, "particle integrates horizontal velocity");
    ok &= check(ps.particles[0].vel.y < -0.9f, "particle accelerates downward under gravity");

    // Advance until ground impact (pos.y <= 0.02)
    for (int step = 0; step < 20 && !ps.particles.empty(); ++step) {
        ps.update(0.1f);
    }
    // Particle expires when life >= max_life (1.0s)
    ps.update(1.2f);
    ok &= check(ps.particles.empty(), "expired particle removed from active vector");

    // Max particle capping at 1000
    for (int i = 0; i < 1100; ++i) {
        ps.spawn({0, 1, 0}, {0, 1, 0}, 2.0f, 0xFFFFFFFFu);
    }
    ok &= check(ps.particles.size() == 1000, "particles capped at 1000 to protect mobile 60 FPS budget");
    ps.particles.clear();

    // ────────────────────────────────────────────────────────────────────────
    // Test 5: GameplayWorld Callbacks & Particle Spawning Triggers
    // ────────────────────────────────────────────────────────────────────────
    int harvest_events = 0;
    int combat_hits = 0;
    int spark_events = 0;
    int death_events = 0;

    world.on_resource_harvested = [&](aom::ResourceType type, const glm::vec3& p) {
        ++harvest_events;
        ps.spawn_harvest_chips(type, p, 6);
    };
    world.on_combat_hit = [&](aom::Entity, aom::Entity, const glm::vec3& p) {
        ++combat_hits;
        ps.spawn_combat_hit(p, 8);
    };
    world.on_building_sparks = [&](const glm::vec3& p) {
        ++spark_events;
        ps.spawn_construction_sparks(p, 4);
    };
    world.on_unit_death = [&](const glm::vec3& p) {
        ++death_events;
        ps.spawn_death_smoke(p, 10);
    };

    // Worker harvesting trigger
    aom::Entity gatherer = world.spawn_worker({10.0f, 0.0f, 10.0f});
    world.add_resource_node(aom::ResourceType::WOOD, {10.5f, 0.0f, 10.0f}, 100);
    world.order_gather(gatherer, {10.5f, 0.0f, 10.0f});
    for (int i = 0; i < 5; ++i) world.update(0.5f);
    ok &= check(harvest_events > 0, "on_resource_harvested callback fired during harvesting");
    ok &= check(!ps.particles.empty(), "wood harvest chip particles spawned");

    // Worker building trigger
    ps.particles.clear();
    aom::Entity builder = world.spawn_worker({-10.0f, 0.0f, 10.0f});
    aom::Entity site = world.place_building(aom::Team::FRIENDLY, {-9.5f, 0.0f, 10.0f}, 2.0f, aom::BuildingType::BARRACKS, "Quartel");
    world.order_build(builder, site);
    for (int i = 0; i < 6; ++i) world.update(0.5f);
    ok &= check(spark_events > 0, "on_building_sparks callback fired during construction");
    ok &= check(!ps.particles.empty(), "construction spark particles spawned");

    std::cout << "\n=== All Sprint 4 Tests Completed: " << (ok ? "ALL PASS (100%)" : "FAILURES DETECTED") << " ===\n";
    return ok ? 0 : 1;
}
