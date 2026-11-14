#pragma once

#include "ecs/ecs.h"
#include "assets/xmb_parser.h"
#include "anim/animation_system.h"
#include "anim/grn_animation.h"
#include "data/unit_transform.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>
#include <queue>
#include <functional>
#include <limits>
#include <string>

namespace aom {

// Forward declarations (evita include circular com game/victory.h)
enum class VictoryCondition : uint8_t;
enum class VictoryResult : uint8_t;

enum class Team : uint8_t { FRIENDLY, ENEMY };

// ---------------------------------------------------------------------------
// AnimationComponent — attached to every entity that has a BRG animation clip.
// Stores the per-entity playback state and the dynamic GPU mesh handle.
// ---------------------------------------------------------------------------
struct AnimationComponent : Component {
    // The loaded multi-frame BRG clip (owned by AnimationSystem, pointer stable)
    const aom::anim::BrgAnimationClip* clip = nullptr;

    // Per-entity playback state machine
    aom::anim::UnitAnimationState anim_state;

    // Dynamic GPU mesh handle (created once, updated every frame via glBufferSubData)
    MeshHandle dynamic_mesh = INVALID_MESH;

    // Texture handle for this model (looked up from textures.bar at load time)
    TextureHandle texture = INVALID_TEXTURE;

    // Working buffers — reused each frame to avoid per-frame allocation
    std::vector<glm::vec3> morph_positions;
    std::vector<glm::vec3> morph_normals;

    // GPU Morphing support (for future GPU-side morphing)
    MeshHandle morph_positions_buffer = INVALID_MESH;  // VBO with all frame positions
    MeshHandle morph_normals_buffer = INVALID_MESH;    // VBO with all frame normals
    uint32_t morph_frame_count = 0;                    // Number of frames in the clip
    uint32_t morph_vertex_count = 0;                   // Vertices per frame

    bool is_ready() const {
        return clip != nullptr && clip->is_valid() && dynamic_mesh != INVALID_MESH;
    }
};

/// GrnAnimationComponent — for skeletal (GRN) animations.
/// Uses GrnPlayer to evaluate bone matrices on CPU, uploads to GPU as uniform buffer.
struct GrnAnimationComponent : Component {
    std::unique_ptr<aom::anim::GrnPlayer> player;
    const aom::anim::GrnAnimation* animation = nullptr;
    MeshHandle static_mesh = INVALID_MESH;      // Base mesh (single frame, rigged)
    TextureHandle texture = INVALID_TEXTURE;
    std::vector<glm::mat4> bone_matrices;       // Uploaded as uniform buffer

    bool is_ready() const {
        return animation != nullptr && player && player->animation() != nullptr && static_mesh != INVALID_MESH;
    }
};

struct UnitStats {
    std::string name = "Unit";
    float max_hp = 100.0f;
    float move_speed = 3.8f;
    float attack_damage = 10.0f;
    float attack_range = 2.0f;
    float attack_cooldown = 1.0f;
    float los = 16.0f;
    int cost_food = 0;
    int cost_wood = 0;
    int cost_gold = 0;
    int cost_favor = 0;
    int pop_cost = 1;
    bool worker = false;
};

inline UnitStats extract_unit_stats_from_xmb(const XmbElement* proto_root, const std::string& unit_name) {
    UnitStats stats;
    stats.name = unit_name;

    // Archetype classification must also work without proto data (tests or a
    // missing/corrupt proto.xmb), otherwise trained villagers never become
    // workers and cannot gather or build.
    //
    // O custo populacional real vem do <populationcount> do proto.xmb. A
    // heuristica por nome e apenas um FALLBACK: sobrescrever o dado real
    // faria um Colosso/Hidra custar 1 pop em vez de 5.
    bool pop_cost_from_data = false;
    const auto apply_unit_class = [&unit_name, &stats, &pop_cost_from_data]() {
        if (unit_name.find("Villager") != std::string::npos || unit_name.find("worker") != std::string::npos) {
            stats.worker = true;
            if (!pop_cost_from_data) stats.pop_cost = 1;
        } else if (unit_name.find("Hoplite") != std::string::npos || unit_name.find("Toxotes") != std::string::npos) {
            if (!pop_cost_from_data) stats.pop_cost = 2;
        } else if (unit_name.find("Minotaur") != std::string::npos) {
            if (!pop_cost_from_data) stats.pop_cost = 4;
        } else {
            if (!pop_cost_from_data) stats.pop_cost = 1;
        }
    };

    if (!proto_root) {
        apply_unit_class();
        return stats;
    }

    for (const auto& unit_elem : proto_root->children) {
        if (!unit_elem) continue;
        if (unit_elem->get_attribute("name") == unit_name) {
            for (const auto& child : unit_elem->children) {
                if (!child) continue;
                if (child->name == "maxhitpoints") {
                    stats.max_hp = child->text_as_float(stats.max_hp);
                } else if (child->name == "maxvelocity") {
                    stats.move_speed = child->text_as_float(stats.move_speed);
                } else if (child->name == "los") {
                    stats.los = child->text_as_float(stats.los);
                } else if (child->name == "populationcount") {
                    stats.pop_cost = child->text_as_int(stats.pop_cost);
                    pop_cost_from_data = true;
                } else if (child->name == "cost") {
                    const std::string res = child->get_attribute("resourcetype");
                    const int amt = child->text_as_int(0);
                    if (res == "Food") stats.cost_food = amt;
                    else if (res == "Wood") stats.cost_wood = amt;
                    else if (res == "Gold") stats.cost_gold = amt;
                    else if (res == "Favor") stats.cost_favor = amt;
                } else if (child->name == "action") {
                    for (const auto& param : child->children) {
                        if (!param) continue;
                        const std::string pname = param->get_attribute("name");
                        if (pname == "Damage") {
                            const std::string val = param->get_attribute("value1");
                            if (!val.empty()) {
                                try { stats.attack_damage = std::stof(val); } catch (...) {}
                            }
                        } else if (pname == "MaximumRange") {
                            const std::string val = param->get_attribute("value1");
                            if (!val.empty()) {
                                try {
                                    float r = std::stof(val);
                                    stats.attack_range = (r < 1.0f) ? 2.0f : r;
                                } catch (...) {}
                            }
                        }
                    }
                }
            }
            apply_unit_class();
            break;
        }
    }
    return stats;
}

enum class BuildingType {
    NONE,
    HOUSE,
    BARRACKS,
    TOWER,
    TOWN_CENTER
};

enum class ResourceType { FOOD, WOOD, GOLD, FAVOR };

struct UnitComponent : Component {
    std::string unit_type_name = "Unit";
    Team team = Team::FRIENDLY;
    float move_speed = 2.0f;
    float attack_range = 2.5f;
    float attack_damage = 10.0f;
    float attack_cooldown = 1.0f;
    float attack_timer = 0.0f;
    float gather_timer = 0.0f;
    bool worker = false;
    int pop_cost = 1;
    int carried_resource_amount = 0;
    ResourceType carried_resource_type = ResourceType::FOOD;
    int max_carry_capacity = 10;
    bool returning_cargo = false;
    int gather_node = -1;
    Entity build_target = INVALID_ENTITY;
    Entity attack_target = INVALID_ENTITY;
    glm::vec3 move_target{0.0f};
    bool has_move_order = false;
    std::vector<glm::vec3> waypoints;
    size_t waypoint_index = 0;
    float los = 14.0f;
    MeshHandle mesh = INVALID_MESH;
    TextureHandle texture = INVALID_TEXTURE;
};

struct SelectionComponent : Component { bool selected = false; };

struct BuildingComponent : Component {
    BuildingType type = BuildingType::BARRACKS;
    std::string name = "Quartel Militar";
    Team team = Team::FRIENDLY;
    float progress = 1.0f;
    float build_time = 5.0f;
    bool complete = true;
    float train_timer = 0.0f;
    float total_train_time = 4.0f;
    std::string training_unit = "";
    std::queue<std::string> train_queue;
    float los = 16.0f;
    MeshHandle mesh = INVALID_MESH;
    TextureHandle texture = INVALID_TEXTURE;
};

struct ResourcePool {
    int food = 500;
    int wood = 500;
    int gold = 300;
    int favor = 100;
};

struct BuildingCost {
    int food = 0;
    int wood = 100;
    int gold = 50;
    int favor = 0;
};

struct ResourceNode {
    ResourceType type = ResourceType::FOOD;
    glm::vec3 position{0.0f};
    int remaining = 1000;
};

/// Objeto decorativo de cenário (rocha, árvore na beira da água, etc.)
/// Extraído dos object_clusters do waterbodies.xmb.
struct SceneryObject {
    std::string name;           ///< Nome do protounit (ex: "Rock River Sandy", "Oak Tree")
    glm::vec3 position{0.0f};   ///< Posição no mundo
    float scale = 1.0f;         ///< Escala do objeto
    TextureHandle texture = INVALID_TEXTURE;  ///< Textura do objeto (carregada do textures.bar)
    MeshHandle mesh = INVALID_MESH;           ///< Malha 3D do objeto (.brg)
};

struct MinimapBlip {
    glm::vec3 position{0.0f};
    uint32_t color = 0xFFFFFFFF; // 0xRRGGBBAA format
    float size = 4.0f;
    bool is_selected = false;
};

class NavigationGrid {
public:
    NavigationGrid(int width = 32, int height = 32, float cell = 1.0f) : width_(width), height_(height), cell_(cell), blocked_(static_cast<size_t>(width * height), false) {}
    void set_blocked(int x, int y, bool blocked = true) { if (valid(x, y)) blocked_[index(x, y)] = blocked; }
    bool walkable(int x, int y) const { return valid(x, y) && !blocked_[index(x, y)]; }
    std::vector<glm::vec3> find_path(const glm::vec3& from, const glm::vec3& to) const {
        glm::ivec2 c = cell_for(from), goal = cell_for(to);
        if (!valid(c.x, c.y) || !valid(goal.x, goal.y)) return {};

        // If destination is blocked (e.g. a 4x4 Town Center footprint), search
        // outward rings for the closest walkable cell. A 1-cell nudge is not
        // enough for the center of large buildings, which used to make workers
        // unable to path back and deposit resources.
        if (!walkable(goal.x, goal.y)) {
            glm::ivec2 nudged;
            if (nearest_walkable(goal.x, goal.y, c.x, c.y, 4, nudged)) {
                goal = nudged;
            } else {
                return {};
            }
        }

        // If start is blocked (unit standing inside a footprint), nudge it to
        // the closest walkable cell toward the goal.
        if (!walkable(c.x, c.y)) {
            glm::ivec2 nudged;
            if (nearest_walkable(c.x, c.y, goal.x, goal.y, 4, nudged)) {
                c = nudged;
            } else {
                return {};
            }
        }

        if (c == goal) {
            return {world_for(goal)};
        }

        struct Node { int f; int id; bool operator>(const Node& n) const { return f > n.f; } };
        // 4 dir no A*: movement grade-alinhado - diagonais são alcançadas por
        // múltiplos passos cardinais, garantindo predibilidade do movement
        // e compatibilidade com o sistema de navegação por grade do RTS.
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        std::vector<int> parent(blocked_.size(), -1), cost(blocked_.size(), std::numeric_limits<int>::max());
        const int start = index(c.x, c.y), target = index(goal.x, goal.y);
        open.push({0, start}); cost[start] = 0;
        const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        while (!open.empty()) {
            const int id = open.top().id;
            open.pop();
            if (id == target) break;
            const int x = id % width_, y = id / width_;
            for (const auto& d : dirs) {
                const int nx = x + d[0], ny = y + d[1];
                if (!walkable(nx, ny)) continue;
                const int ni = index(nx, ny), nc = cost[id] + 1;
                if (nc < cost[ni]) {
                    cost[ni] = nc;
                    parent[ni] = id;
                    open.push({nc + std::abs(nx - goal.x) + std::abs(ny - goal.y), ni});
                }
            }
        }
        if (parent[target] < 0 && target != start) return {};
        std::vector<glm::vec3> result;
        for (int id = target;; id = parent[id]) {
            result.push_back(world_for({id % width_, id / width_}));
            if (id == start) break;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }
private:
    int width_, height_; float cell_; std::vector<bool> blocked_;
    bool valid(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    int index(int x, int y) const { return y * width_ + x; }

    /// Scans rings outward from (x, y) and returns a walkable cell from the
    /// first ring that has one. Within that ring, prefers the cell closest
    /// (Manhattan distance) to (target_x, target_y). Prioritising the nearest
    /// ring matters: otherwise a far cell beside the approaching unit can win
    /// and produce a one-node path that never leaves the blocked footprint.
    bool nearest_walkable(int x, int y, int target_x, int target_y,
                          int max_radius, glm::ivec2& out) const {
        for (int r = 1; r <= max_radius; ++r) {
            int best_d = std::numeric_limits<int>::max();
            bool found_ring = false;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    const int nx = x + dx, ny = y + dy;
                    if (!walkable(nx, ny)) continue;
                    const int d = std::abs(nx - target_x) + std::abs(ny - target_y);
                    if (d < best_d) {
                        best_d = d;
                        out = {nx, ny};
                        found_ring = true;
                    }
                }
            }
            if (found_ring) return true;
        }
        return false;
    }
    glm::ivec2 cell_for(const glm::vec3& p) const { return {static_cast<int>(p.x / cell_ + width_ * 0.5f), static_cast<int>(p.z / cell_ + height_ * 0.5f)}; }
    glm::vec3 world_for(const glm::ivec2& c) const { return {(c.x - width_ * 0.5f + 0.5f) * cell_, 0.0f, (c.y - height_ * 0.5f + 0.5f) * cell_}; }
};

/** Deterministic, allocation-free-per-frame combat step for small unit counts. */
class CombatSystem final : public System {
public:
    std::function<void(Entity attacker, Entity target, const glm::vec3& hit_pos)> on_combat_hit;

    CombatSystem(std::unordered_map<Entity, TransformComponent>& transforms,
                 std::unordered_map<Entity, HealthComponent>& health,
                 std::unordered_map<Entity, UnitComponent>& units)
        : transforms_(transforms), health_(health), units_(units) {}

    void update(float dt) override {
        dead_.clear();
        for (auto& [attacker, unit] : units_) {
            unit.attack_timer = std::max(0.0f, unit.attack_timer - dt);
            if (unit.attack_timer > 0.0f) continue;
            auto at = transforms_.find(attacker);
            if (at == transforms_.end()) continue;

            Entity target = INVALID_ENTITY;

            // Prioritize assigned attack target if alive and in range
            if (unit.attack_target != INVALID_ENTITY) {
                auto target_t = transforms_.find(unit.attack_target);
                auto target_hp = health_.find(unit.attack_target);
                if (target_t != transforms_.end() && target_hp != health_.end() && target_hp->second.current_hp > 0) {
                    if (glm::length(target_t->second.position - at->second.position) <= unit.attack_range + 0.6f) {
                        target = unit.attack_target;
                    }
                }
            }

            // Auto-acquire closest hostile in range if no valid focus target
            if (target == INVALID_ENTITY) {
                float best = unit.attack_range;
                for (const auto& [candidate, other] : units_) {
                    if (candidate == attacker || other.team == unit.team) continue;
                    auto ct = transforms_.find(candidate);
                    auto hp = health_.find(candidate);
                    if (ct == transforms_.end() || hp == health_.end() || hp->second.current_hp <= 0) continue;
                    const float distance = glm::length(ct->second.position - at->second.position);
                    if (distance <= best) { best = distance; target = candidate; }
                }
            }

            if (target == INVALID_ENTITY) continue;
            auto hp = health_.find(target);
            hp->second.current_hp -= static_cast<int>(unit.attack_damage);
            unit.attack_timer = unit.attack_cooldown;
            if (on_combat_hit) {
                auto target_t = transforms_.find(target);
                glm::vec3 hit_p = (target_t != transforms_.end()) ? target_t->second.position : at->second.position;
                on_combat_hit(attacker, target, hit_p + glm::vec3(0.0f, 0.8f, 0.0f));
            }
            if (hp->second.current_hp <= 0) {
                dead_.push_back(target);
                if (unit.attack_target == target) {
                    unit.attack_target = INVALID_ENTITY;
                    unit.has_move_order = false;
                }
            }
        }
    }

    const std::vector<Entity>& dead_entities() const { return dead_; }
    void clear_dead() { dead_.clear(); }

private:
    std::unordered_map<Entity, TransformComponent>& transforms_;
    std::unordered_map<Entity, HealthComponent>& health_;
    std::unordered_map<Entity, UnitComponent>& units_;
    std::vector<Entity> dead_;
};

class SelectionSystem final : public System {
public:
    SelectionSystem(std::unordered_map<Entity, TransformComponent>& transforms,
                    std::unordered_map<Entity, SelectionComponent>& selections)
        : transforms_(transforms), selections_(selections) {}

    void update(float) override {}

    Entity select_at(const glm::vec3& point, float radius = 1.5f) {
        Entity selected = INVALID_ENTITY;
        float best = radius;
        for (const auto& [entity, transform] : transforms_) {
            const float distance = glm::length(transform.position - point);
            if (distance < best) { best = distance; selected = entity; }
        }
        for (auto& [entity, marker] : selections_) marker.selected = entity == selected;
        return selected;
    }

private:
    std::unordered_map<Entity, TransformComponent>& transforms_;
    std::unordered_map<Entity, SelectionComponent>& selections_;
};

class BuildingSystem final : public System {
public:
    struct SpawnOrder { Entity building; std::string unit_type; };

    std::function<void(const glm::vec3& pos)> on_building_sparks;
    std::function<void(Entity building, const glm::vec3& pos)> on_building_completed;

    BuildingSystem(std::unordered_map<Entity, TransformComponent>& transforms,
                   std::unordered_map<Entity, BuildingComponent>& buildings,
                   std::unordered_map<Entity, UnitComponent>& units)
        : transforms_(transforms), buildings_(buildings), units_(units) {}

    void update(float dt) override {
        // 1. Building construction progression
        for (auto& [b_entity, building] : buildings_) {
            if (!building.complete) {
                auto b_transform = transforms_.find(b_entity);
                if (b_transform == transforms_.end()) continue;

                // Check if any worker is tasked to build this structure and is adjacent
                bool worker_present = false;
                for (const auto& [w_entity, worker] : units_) {
                    if (worker.worker && worker.build_target == b_entity) {
                        auto w_transform = transforms_.find(w_entity);
                        if (w_transform != transforms_.end()) {
                            float dist = glm::length(w_transform->second.position - b_transform->second.position);
                            if (dist <= 2.8f) {
                                worker_present = true;
                                break;
                            }
                        }
                    }
                }

                // Construction progresses 1.0x if worker present, 0.08x passive scaffolding
                float rate = worker_present ? 1.0f : 0.08f;
                building.progress = std::min(1.0f, building.progress + (dt / std::max(0.1f, building.build_time)) * rate);

                if (worker_present && on_building_sparks) {
                    on_building_sparks(b_transform->second.position);
                }

                if (building.progress >= 1.0f) {
                    building.complete = true;
                    if (on_building_completed) {
                        on_building_completed(b_entity, b_transform->second.position);
                    }
                    // Release any worker tasked to this completed building
                    for (auto& [w_entity, worker] : units_) {
                        if (worker.build_target == b_entity) {
                            worker.build_target = INVALID_ENTITY;
                            worker.has_move_order = false;
                            transforms_[w_entity].velocity = glm::vec3(0.0f);
                        }
                    }
                }
            }

            // 2. Unit Training timer in completed military/economic structures
            if (building.complete && !building.training_unit.empty()) {
                building.train_timer -= dt;
                if (building.train_timer <= 0.0f) {
                    spawn_orders_.push_back({b_entity, building.training_unit});
                    building.training_unit = "";
                    if (!building.train_queue.empty()) {
                        building.training_unit = building.train_queue.front();
                        building.train_queue.pop();
                        building.train_timer = (building.type == BuildingType::TOWN_CENTER) ? 4.5f : 4.0f;
                        building.total_train_time = building.train_timer;
                    }
                }
            }
        }
    }

    std::deque<SpawnOrder> drain_spawn_orders() {
        std::deque<SpawnOrder> orders = std::move(spawn_orders_);
        spawn_orders_.clear();
        return orders;
    }

    /// Re-queue an order that could not be spawned this frame (e.g. pop cap).
    /// It stays at the front so it is retried before newer orders.
    void requeue_spawn_order(const SpawnOrder& order) {
        spawn_orders_.push_front(order);
    }

private:
    std::unordered_map<Entity, TransformComponent>& transforms_;
    std::unordered_map<Entity, BuildingComponent>& buildings_;
    std::unordered_map<Entity, UnitComponent>& units_;
    std::deque<SpawnOrder> spawn_orders_;
};

class FogOfWar {
public:
    static constexpr int GRID_SIZE = 64;
    static constexpr float WORLD_SIZE = 40.0f;
    static constexpr float HALF_WORLD = 20.0f;

    FogOfWar() {
        explored_.resize(GRID_SIZE * GRID_SIZE, false);
        visible_.resize(GRID_SIZE * GRID_SIZE, false);
        texture_data_.resize(GRID_SIZE * GRID_SIZE * 4, 0);
    }

    void reset() {
        std::fill(explored_.begin(), explored_.end(), false);
        std::fill(visible_.begin(), visible_.end(), false);
        std::fill(texture_data_.begin(), texture_data_.end(), 0);
    }

    void update_visibility(const std::vector<std::pair<glm::vec3, float>>& vision_sources) {
        std::fill(visible_.begin(), visible_.end(), false);

        for (const auto& [pos, radius] : vision_sources) {
            float u = (pos.x + HALF_WORLD) / WORLD_SIZE;
            float v = (pos.z + HALF_WORLD) / WORLD_SIZE;
            int cx = static_cast<int>(u * GRID_SIZE);
            int cy = static_cast<int>(v * GRID_SIZE);
            int cell_rad = static_cast<int>((radius / WORLD_SIZE) * GRID_SIZE) + 1;

            int min_x = std::max(0, cx - cell_rad);
            int max_x = std::min(GRID_SIZE - 1, cx + cell_rad);
            int min_y = std::max(0, cy - cell_rad);
            int max_y = std::min(GRID_SIZE - 1, cy + cell_rad);

            float r_sq = (radius / WORLD_SIZE) * GRID_SIZE;
            r_sq = r_sq * r_sq;

            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float dx = static_cast<float>(x - cx);
                    float dy = static_cast<float>(y - cy);
                    if (dx * dx + dy * dy <= r_sq) {
                        int idx = y * GRID_SIZE + x;
                        visible_[idx] = true;
                        explored_[idx] = true;
                    }
                }
            }
        }

        // Texture RGBA: Red channel is fog brightness (0 = black unexplored, 110 = grey in fog, 255 = fully lit)
        for (int i = 0; i < GRID_SIZE * GRID_SIZE; ++i) {
            uint8_t val = 0;
            if (visible_[i]) {
                val = 255;
            } else if (explored_[i]) {
                val = 110;
            }
            texture_data_[i * 4 + 0] = val;
            texture_data_[i * 4 + 1] = val;
            texture_data_[i * 4 + 2] = val;
            texture_data_[i * 4 + 3] = 255;
        }
    }

    bool is_visible(const glm::vec3& world_pos) const {
        float u = (world_pos.x + HALF_WORLD) / WORLD_SIZE;
        float v = (world_pos.z + HALF_WORLD) / WORLD_SIZE;
        if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) return false;
        int x = std::clamp(static_cast<int>(u * GRID_SIZE), 0, GRID_SIZE - 1);
        int y = std::clamp(static_cast<int>(v * GRID_SIZE), 0, GRID_SIZE - 1);
        return visible_[y * GRID_SIZE + x];
    }

    bool is_explored(const glm::vec3& world_pos) const {
        float u = (world_pos.x + HALF_WORLD) / WORLD_SIZE;
        float v = (world_pos.z + HALF_WORLD) / WORLD_SIZE;
        if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) return false;
        int x = std::clamp(static_cast<int>(u * GRID_SIZE), 0, GRID_SIZE - 1);
        int y = std::clamp(static_cast<int>(v * GRID_SIZE), 0, GRID_SIZE - 1);
        return explored_[y * GRID_SIZE + x];
    }

    const uint8_t* raw_texture_data() const { return texture_data_.data(); }
    int grid_size() const { return GRID_SIZE; }

private:
    std::vector<bool> explored_;
    std::vector<bool> visible_;
    std::vector<uint8_t> texture_data_;
};

class GameplayWorld {
public:
    std::function<void(const glm::vec3&)> on_unit_death;
    std::function<void(ResourceType type, const glm::vec3& pos)> on_resource_harvested;
    std::function<void(Entity attacker, Entity target, const glm::vec3& hit_pos)> on_combat_hit;
    std::function<void(const glm::vec3& pos)> on_building_sparks;
    std::function<void(Entity building, const glm::vec3& pos)> on_building_completed;

    // Flag para desabilitar renderização de barras de HP (útil no MAIN_MENU)
    bool render_hp_bars_ = true;

    void set_render_hp_bars(bool enable) { render_hp_bars_ = enable; }
    [[nodiscard]] bool render_hp_bars() const { return render_hp_bars_; }

    explicit GameplayWorld(const XmbElement* proto_root = nullptr)
        : proto_root_(proto_root),
          ecs_world_(),
          unit_transforms_(nullptr) {
        // Registrar sistemas no ECSWorld (MovementSystem e RenderSystem usam
        // component storage do ECSWorld; Combat/Selection/Building usam storage
        // local do GameplayWorld para componentes específicos de gameplay)
        movement_system_ = ecs_world_.add_system<MovementSystem>(ecs_world_.transforms_mut());
        combat_system_ = ecs_world_.add_system<CombatSystem>(
            ecs_world_.transforms_mut(), ecs_world_.healths_mut(), units_);
        selection_system_ = ecs_world_.add_system<SelectionSystem>(
            ecs_world_.transforms_mut(), selections_);
        building_system_ = ecs_world_.add_system<BuildingSystem>(
            ecs_world_.transforms_mut(), building_components_, units_);
        render_system_ = ecs_world_.add_system<RenderSystem>(
            ecs_world_.transforms_mut(), ecs_world_.renderables_mut(), nullptr);

        combat_system_->on_combat_hit = [this](Entity a, Entity t, const glm::vec3& p) {
            if (on_combat_hit) on_combat_hit(a, t, p);
        };
        building_system_->on_building_sparks = [this](const glm::vec3& p) {
            if (on_building_sparks) on_building_sparks(p);
        };
        building_system_->on_building_completed = [this](Entity b, const glm::vec3& p) {
            if (on_building_completed) on_building_completed(b, p);
        };
    }

    void set_default_animation(const aom::anim::BrgAnimationClip* clip, TextureHandle texture) {
        default_clip_ = clip;
        default_clip_tex_ = texture;
    }

    /// Verifica se o cenário de fundo do menu (ui background xpack.scn) já foi carregado
    [[nodiscard]] bool has_background_scenario() const {
        // Verifica se há entidades além das básicas (Town Centers, unidades de fundo)
        // O cenário de fundo do menu tem Town Centers, unidades costeiras, etc.
        return !units_.empty() || !building_components_.empty();
    }

    Entity spawn_unit(Team team, const glm::vec3& position, const UnitStats& stats = {}) {
        Entity id = ecs_world_.create_entity();

        auto& tr = ecs_world_.transforms_mut()[id];
        tr.position = position;

        auto& hp = ecs_world_.healths_mut()[id];
        hp.max_hp = static_cast<int>(stats.max_hp);
        hp.current_hp = hp.max_hp;

        UnitComponent u;
        u.unit_type_name = stats.name.empty() ? "Unit" : stats.name;
        u.team = team;
        u.move_speed = stats.move_speed;
        u.attack_range = stats.attack_range;
        u.attack_damage = stats.attack_damage;
        u.attack_cooldown = stats.attack_cooldown;
        u.worker = stats.worker;
        u.pop_cost = (stats.pop_cost > 0) ? stats.pop_cost : 1;
        u.carried_resource_amount = 0;
        u.max_carry_capacity = 10;
        u.returning_cargo = false;
        u.los = (stats.los > 0.0f) ? stats.los : 14.0f;
        u.gather_timer = 0.0f;
        units_[id] = u;

        selections_[id] = SelectionComponent{};

        // Auto-attach default animation if available
        if (default_clip_ && default_clip_->is_valid() && anim_renderer_) {
            attach_animation(id, default_clip_, default_clip_tex_, anim_renderer_);
        }

        return id;
    }

    Entity spawn_worker(const glm::vec3& position, const UnitStats& stats = {}) {
        UnitStats w_stats = stats;
        w_stats.worker = true;
        w_stats.pop_cost = 1;
        if (w_stats.name.empty() || w_stats.name == "Unit") {
            w_stats.name = "Villager Greek";
        }
        return spawn_unit(Team::FRIENDLY, position, w_stats);
    }

    Entity spawn_scenery_object(const std::string& name, const glm::vec3& position,
                                 float scale = 1.0f, TextureHandle texture = INVALID_TEXTURE,
                                 MeshHandle mesh = INVALID_MESH) {
        SceneryObject so;
        so.name = name;
        so.position = position;
        so.scale = scale;
        so.texture = texture;
        so.mesh = mesh;
        scenery_objects_.push_back(std::move(so));
        // Cria uma entidade ECS leve para o objeto (transform apenas, sem unidade/edifício)
        Entity id = ecs_world_.create_entity();
        auto& tr = ecs_world_.transforms_mut()[id];
        tr.position = position;
        tr.scale = glm::vec3(scale);
        return id;
    }

    void set_unit_visuals(Entity unit, MeshHandle mesh, TextureHandle texture = INVALID_TEXTURE) {
        auto it = units_.find(unit);
        if (it != units_.end()) {
            it->second.mesh = mesh;
            it->second.texture = texture;
        }
    }

    void set_building_visuals(Entity building, MeshHandle mesh, TextureHandle texture = INVALID_TEXTURE) {
        auto it = building_components_.find(building);
        if (it != building_components_.end()) {
            it->second.mesh = mesh;
            it->second.texture = texture;
        }
    }

    /// Injeta o gerenciador de transformacoes de unidade (carregado do unittransform.xmb).
    void set_unit_transform_manager(const UnitTransformManager* utm) {
        unit_transforms_ = utm;
    }

    /// Verifica se existe alguma transformacao disponível para a entidade dada.
    /// Retorna a lista de transformacoes possíveis (from -> to).
    [[nodiscard]] std::vector<UnitTransform> available_transforms(Entity entity) const {
        std::vector<UnitTransform> result;
        if (!unit_transforms_) return result;
        auto u_it = units_.find(entity);
        if (u_it == units_.end()) return result;
        return unit_transforms_->find_from(u_it->second.unit_type_name);
    }

    /// Transforma uma unidade existente em outro tipo (ex: "Wall Long" -> "Gate").
    /// Procura no unittransform.xmb se existe uma transformacao de from -> to.
    /// Preserva posicao, time, HP, selecao e ordem de movimento quando possivel.
    /// Retorna a entidade resultante (nova) ou INVALID_ENTITY se nao houver transformacao valida.
    Entity transform_unit(Entity from_entity, const std::string& target_name) {
        if (!unit_transforms_) return INVALID_ENTITY;

        auto u_it = units_.find(from_entity);
        if (u_it == units_.end()) return INVALID_ENTITY;

        const auto& from_unit = u_it->second;

        // Verifica se existe transformacao de from -> target_name
        bool found = false;
        for (const auto& t : unit_transforms_->find_from(from_unit.unit_type_name)) {
            if (t.to == target_name) { found = true; break; }
        }
        if (!found) return INVALID_ENTITY;

        // Salva estado da entidade original
        glm::vec3 position{0.0f};
        Team team = static_cast<Team>(from_unit.team);
        float hp_ratio = 1.0f;
        bool selected = false;
        bool has_move = from_unit.has_move_order;
        glm::vec3 move_target = from_unit.move_target;
        Entity attack_target = from_unit.attack_target;

        auto t_it = ecs_world_.transforms().find(from_entity);
        if (t_it != ecs_world_.transforms().end()) position = t_it->second.position;

        auto h_it = ecs_world_.healths().find(from_entity);
        if (h_it != ecs_world_.healths().end() && h_it->second.max_hp > 0) {
            hp_ratio = static_cast<float>(h_it->second.current_hp) / h_it->second.max_hp;
        }

        auto s_it = selections_.find(from_entity);
        if (s_it != selections_.end()) selected = s_it->second.selected;

        // Spawn a nova unidade com stats do proto.xmb (ou defaults se ausente)
        UnitStats stats = extract_unit_stats_from_xmb(proto_root_, target_name);
        Entity new_id = spawn_unit(team, position, stats);

        // Aplica HP proporcional
        auto new_h_it = ecs_world_.healths_mut().find(new_id);
        if (new_h_it != ecs_world_.healths_mut().end()) {
            new_h_it->second.current_hp = static_cast<int>(new_h_it->second.max_hp * hp_ratio);
            if (new_h_it->second.current_hp <= 0) new_h_it->second.current_hp = 1;
        }

        // Restaura selecao
        if (selected) selections_[new_id].selected = true;

        // Restaura ordem de ataque/movimento se houver
        if (has_move) {
            units_[new_id].has_move_order = true;
            units_[new_id].move_target = move_target;
            units_[new_id].attack_target = attack_target;
            auto path = navigation_.find_path(position, move_target);
            if (!path.empty()) {
                units_[new_id].waypoints = path;
                units_[new_id].waypoint_index = 0;
            }
        }

        // Remove a entidade original (limpa transforms, health, units, selections, animations)
        remove_entity(from_entity);

        return new_id;
    }

    int current_population(Team team) const {
        int pop = 0;
        for (const auto& [entity, unit] : units_) {
            if (static_cast<Team>(unit.team) == team) {
                pop += unit.pop_cost;
            }
        }
        return pop;
    }

    int population_cap(Team team) const {
        int tc_count = 0;
        int house_count = 0;
        for (const auto& [entity, bld] : building_components_) {
            if (bld.team == team && bld.complete) {
                if (bld.type == BuildingType::TOWN_CENTER) {
                    tc_count++;
                } else if (bld.type == BuildingType::HOUSE) {
                    house_count++;
                }
            }
        }
        int cap = 10 + (tc_count * 15) + (house_count * 10);
        return std::min(200, cap);
    }

    Entity get_nearest_town_center(Team team, const glm::vec3& pos) const {
        Entity best_tc = INVALID_ENTITY;
        float best_dist = std::numeric_limits<float>::max();
        for (const auto& [id, bld] : building_components_) {
            if (bld.team == team && bld.type == BuildingType::TOWN_CENTER && bld.complete) {
                auto t_it = ecs_world_.transforms().find(id);
                if (t_it != ecs_world_.transforms().end()) {
                    float d = glm::length(t_it->second.position - pos);
                    if (d < best_dist) {
                        best_dist = d;
                        best_tc = id;
                    }
                }
            }
        }
        return best_tc;
    }

    void add_resource_node(ResourceType type, const glm::vec3& position, int amount = 1000) {
        resources_nodes_.push_back({type, position, amount});
    }

    bool order_gather(Entity worker, const glm::vec3& point) {
        auto unit = units_.find(worker);
        if (unit == units_.end() || !unit->second.worker) return false;
        int best = -1; float distance = 2.5f;
        for (size_t i = 0; i < resources_nodes_.size(); ++i) {
            const float d = glm::length(resources_nodes_[i].position - point);
            if (d < distance && resources_nodes_[i].remaining > 0) { distance = d; best = static_cast<int>(i); }
        }
        if (best < 0) return false;
        unit->second.gather_node = best;
        unit->second.returning_cargo = false;
        unit->second.build_target = INVALID_ENTITY;
        unit->second.attack_target = INVALID_ENTITY;
        return order_move(worker, resources_nodes_[best].position);
    }

    bool order_build(Entity worker, Entity building) {
        auto w_it = units_.find(worker);
        auto b_it = building_components_.find(building);
        auto b_t = ecs_world_.transforms().find(building);
        if (w_it == units_.end() || !w_it->second.worker || b_it == building_components_.end() || b_t == ecs_world_.transforms().end()) {
            return false;
        }
        w_it->second.build_target = building;
        w_it->second.gather_node = -1;
        w_it->second.returning_cargo = false;
        w_it->second.attack_target = INVALID_ENTITY;
        return order_move(worker, b_t->second.position);
    }

    bool order_worker_build(Entity worker, BuildingType type, const glm::vec3& position) {
        auto w_it = units_.find(worker);
        if (w_it == units_.end() || !w_it->second.worker) return false;
        Team team = static_cast<Team>(w_it->second.team);
        ResourcePool& pool = (team == Team::ENEMY) ? enemy_resources_ : resources_;

        int food_cost = 0, wood_cost = 0, gold_cost = 0;
        std::string bname;
        float build_time = 5.0f;

        if (type == BuildingType::HOUSE) {
            wood_cost = 100;
            bname = "Casa";
            build_time = 5.0f;
        } else if (type == BuildingType::BARRACKS) {
            wood_cost = 100;
            gold_cost = 50;
            bname = "Quartel Militar";
            build_time = 8.0f;
        } else if (type == BuildingType::TOWER) {
            wood_cost = 100;
            gold_cost = 100;
            bname = "Torre de Guarda";
            build_time = 10.0f;
        } else {
            return false;
        }

        if (pool.food < food_cost || pool.wood < wood_cost || pool.gold < gold_cost) {
            return false;
        }

        pool.food -= food_cost;
        pool.wood -= wood_cost;
        pool.gold -= gold_cost;

        Entity foundation = place_building(team, position, build_time, type, bname);
        return order_build(worker, foundation);
    }

    bool start_training_unit(Entity barracks, const std::string& unit_type, int food_cost, int wood_cost, int gold_cost, int pop_cost = 1) {
        auto b_it = building_components_.find(barracks);
        if (b_it == building_components_.end() || !b_it->second.complete) return false;
        if (b_it->second.train_queue.size() >= 5) return false;

        Team team = static_cast<Team>(b_it->second.team);
        if (current_population(team) + pop_cost > population_cap(team)) {
            return false; // Population limit reached!
        }

        ResourcePool& pool = (team == Team::ENEMY) ? enemy_resources_ : resources_;
        if (pool.food < food_cost || pool.wood < wood_cost || pool.gold < gold_cost) return false;

        pool.food -= food_cost;
        pool.wood -= wood_cost;
        pool.gold -= gold_cost;

        float duration = (b_it->second.type == BuildingType::TOWN_CENTER) ? 4.5f : 4.0f;
        if (b_it->second.training_unit.empty()) {
            b_it->second.training_unit = unit_type;
            b_it->second.train_timer = duration;
            b_it->second.total_train_time = duration;
        } else {
            b_it->second.train_queue.push(unit_type);
        }
        return true;
    }

    bool can_afford(int food, int wood, int gold, int favor = 0) const {
        return resources_.food >= food && resources_.wood >= wood &&
               resources_.gold >= gold && resources_.favor >= favor;
    }

    bool deduct_resources(int food, int wood, int gold, int favor = 0) {
        if (!can_afford(food, wood, gold, favor)) return false;
        resources_.food -= food;
        resources_.wood -= wood;
        resources_.gold -= gold;
        resources_.favor -= favor;
        return true;
    }

    bool is_walkable(const glm::vec3& p) const {
        if (std::abs(p.x) > 17.0f || std::abs(p.z) > 17.0f) return false;
        glm::ivec2 cell = navigation_cell(p);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (!navigation_.walkable(cell.x + dx, cell.y + dz)) return false;
            }
        }
        return true;
    }

    Entity place_building(Team team, const glm::vec3& position, float build_time = 5.0f,
                          BuildingType type = BuildingType::BARRACKS, const std::string& name = "Quartel Militar") {
        Entity id = ecs_world_.create_entity();
        auto& tr = ecs_world_.transforms_mut()[id];
        tr.position = position;
        BuildingComponent b;
        b.type = type;
        b.name = name;
        b.team = team;
        b.progress = 0.0f;
        b.build_time = build_time;
        b.complete = false;
        b.los = (type == BuildingType::TOWER) ? 18.0f : ((type == BuildingType::TOWN_CENTER) ? 22.0f : 14.0f);
        building_components_[id] = b;

        auto& hp = ecs_world_.healths_mut()[id];
        if (type == BuildingType::HOUSE) hp.max_hp = 350;
        else if (type == BuildingType::TOWER) hp.max_hp = 600;
        else if (type == BuildingType::TOWN_CENTER) hp.max_hp = 2400;
        else hp.max_hp = 800; // BARRACKS
        hp.current_hp = hp.max_hp;

        const glm::ivec2 cell = navigation_cell(position);
        if (type == BuildingType::TOWN_CENTER) {
            for (int dx = -1; dx <= 2; ++dx) {
                for (int dz = -1; dz <= 2; ++dz) {
                    navigation_.set_blocked(cell.x + dx, cell.y + dz, true);
                }
            }
        } else {
            navigation_.set_blocked(cell.x, cell.y, true);
            navigation_.set_blocked(cell.x + 1, cell.y, true);
            navigation_.set_blocked(cell.x, cell.y + 1, true);
            navigation_.set_blocked(cell.x + 1, cell.y + 1, true);
        }
        return id;
    }

    Entity place_town_center(Team team, const glm::vec3& position, bool complete = true) {
        Entity tc = place_building(team, position, 10.0f, BuildingType::TOWN_CENTER, "Centro da Cidade");
        if (complete) {
            auto b_it = building_components_.find(tc);
            if (b_it != building_components_.end()) {
                b_it->second.complete = true;
                b_it->second.progress = 1.0f;
            }
        }
        return tc;
    }

    bool queue_villager(Entity town_center) {
        auto b_it = building_components_.find(town_center);
        if (b_it == building_components_.end() || b_it->second.type != BuildingType::TOWN_CENTER) return false;
        // Cost of Greek Villager: 50 Food, 1 Population
        return start_training_unit(town_center, "Villager Greek", 50, 0, 0, 1);
    }

    int get_training_queue_count(Entity building) const {
        auto b_it = building_components_.find(building);
        if (b_it == building_components_.end()) return 0;
        return (b_it->second.training_unit.empty() ? 0 : 1) + static_cast<int>(b_it->second.train_queue.size());
    }

    float get_training_progress(Entity building) const {
        auto b_it = building_components_.find(building);
        if (b_it == building_components_.end() || b_it->second.training_unit.empty()) return 0.0f;
        float total = std::max(0.1f, b_it->second.total_train_time);
        return std::clamp(1.0f - (b_it->second.train_timer / total), 0.0f, 1.0f);
    }

    bool demolish_building(const glm::vec3& point, float radius = 1.5f) {
        for (auto it = building_components_.begin(); it != building_components_.end(); ++it) {
            const Entity id = it->first;
            auto t_it = ecs_world_.transforms().find(id);
            if (t_it == ecs_world_.transforms().end()) continue;
            if (glm::length(t_it->second.position - point) > radius) continue;
            const glm::ivec2 cell = navigation_cell(t_it->second.position);
            if (it->second.type == BuildingType::TOWN_CENTER) {
                for (int dx = -1; dx <= 2; ++dx) {
                    for (int dz = -1; dz <= 2; ++dz) {
                        navigation_.set_blocked(cell.x + dx, cell.y + dz, false);
                    }
                }
            } else {
                navigation_.set_blocked(cell.x, cell.y, false);
                navigation_.set_blocked(cell.x + 1, cell.y, false);
                navigation_.set_blocked(cell.x, cell.y + 1, false);
                navigation_.set_blocked(cell.x + 1, cell.y + 1, false);
            }
            ecs_world_.destroy_entity(id); return true;
        }
        return false;
    }

    void order_stop_selected() {
        for (const auto& [entity, marker] : selections_) {
            if (marker.selected) {
                auto u = units_.find(entity);
                if (u != units_.end()) {
                    u->second.has_move_order = false;
                    u->second.attack_target = INVALID_ENTITY;
                    u->second.build_target = INVALID_ENTITY;
                    u->second.gather_node = -1;
                    ecs_world_.transforms_mut()[entity].velocity = glm::vec3(0.0f);
                }
            }
        }
    }

    const BuildingComponent* get_building(Entity id) const {
        auto it = building_components_.find(id);
        return (it != building_components_.end()) ? &it->second : nullptr;
    }
    const UnitComponent* get_unit(Entity id) const {
        auto it = units_.find(id);
        return (it != units_.end()) ? &it->second : nullptr;
    }
    const HealthComponent* get_health(Entity id) const {
        auto it = ecs_world_.healths().find(id);
        return (it != ecs_world_.healths().end()) ? &it->second : nullptr;
    }
    const TransformComponent* get_transform(Entity id) const {
        auto it = ecs_world_.transforms().find(id);
        return (it != ecs_world_.transforms().end()) ? &it->second : nullptr;
    }
    void clear_all_entities() {
        ecs_world_.clear();
        animations_.clear();
        grn_animations_.clear();
        resources_nodes_.clear();
        scenery_objects_.clear();
    }

    /// Remove todas as unidades e edificios, preservando os nos de recursos
    /// e a grade de navegacao. Usado pelo save/load para reconstruir o mundo
    /// a partir das entidades gravadas no arquivo.
    void clear_units_and_buildings() {
        std::vector<Entity> ids;
        ids.reserve(units_.size() + building_components_.size());
        for (const auto& [id, unit] : units_) ids.push_back(id);
        for (const auto& [id, bld] : building_components_) ids.push_back(id);
        for (Entity id : ids) remove_entity(id);
    }

    void update(float dt) {
        update_orders();
        movement_system_->update(dt);
        gather_resources(dt);

        // ── Combate: atualizar timers e processar mortes ──────────────────────
        // Ordem importante: combat_system_->update() popula dead_entities(),
        // depois removemos as entidades mortas de TODOS os maps.
        combat_system_->update(dt);
        for (Entity dead : combat_system_->dead_entities()) {
            auto tit = ecs_world_.transforms().find(dead);
            if (tit != ecs_world_.transforms().end() && on_unit_death) on_unit_death(tit->second.position);
            remove_entity(dead);
        }
        combat_system_->clear_dead();

        // Atualizações subsequentes (construção, neblina, animações)
        building_system_->update(dt);

        // ── Produção: drenar ordens de treino concluídas ─────────────────────
        // Executado DEPOIS da remoção das entidades mortas: spawn_unit() pode
        // inserir com segurança em transforms/units/health/animations.
        for (const auto& spawn : building_system_->drain_spawn_orders()) {
            auto b_t = ecs_world_.transforms().find(spawn.building);
            auto b_c = building_components_.find(spawn.building);
            if (b_t == ecs_world_.transforms().end() || b_c == building_components_.end()) continue;

            const Team team = static_cast<Team>(b_c->second.team);
            const UnitStats stats = extract_unit_stats_from_xmb(proto_root_, spawn.unit_type);

            // Re-checa o limite populacional no momento do nascimento: a fila
            // foi validada no início do treino, mas a população pode ter mudado.
            const int pop_cost = (stats.pop_cost > 0) ? stats.pop_cost : 1;
            if (current_population(team) + pop_cost > population_cap(team)) {
                building_system_->requeue_spawn_order(spawn);
                continue;
            }

            const glm::vec3 spawn_pos = find_spawn_position(spawn.building);
            Entity new_unit = spawn_unit(team, spawn_pos, stats);

            // Rally padrão: afasta-se do edifício para liberar a saída.
            glm::vec3 away = spawn_pos - b_t->second.position;
            away.y = 0.0f;
            const float away_len = glm::length(away);
            if (away_len > 0.01f) {
                order_move(new_unit, spawn_pos + (away / away_len) * 1.5f);
            }
        }

        // Fog of War
        std::vector<std::pair<glm::vec3, float>> vision_sources;
        for (const auto& [entity, unit] : units_) {
            if (static_cast<Team>(unit.team) == Team::FRIENDLY) {
                auto t = ecs_world_.transforms().find(entity);
                if (t != ecs_world_.transforms().end())
                    vision_sources.push_back({t->second.position, unit.los * 0.75f});
            }
        }
        for (const auto& [entity, bld] : building_components_) {
            if (static_cast<Team>(bld.team) == Team::FRIENDLY) {
                auto t = ecs_world_.transforms().find(entity);
                if (t != ecs_world_.transforms().end())
                    vision_sources.push_back({t->second.position, bld.los * 0.75f});
            }
        }
        fog_of_war_.update_visibility(vision_sources);

        // Vertex-morph animation — safe porque entidades mortas já foram removidas acima
        update_animations(dt);
    }

    /// Attach a BRG animation clip to an entity and create its dynamic GPU mesh.
    bool attach_animation(Entity entity,
                          const aom::anim::BrgAnimationClip* clip,
                          TextureHandle texture,
                          IRenderer* renderer) {
        if (!clip || !clip->is_valid() || !renderer) return false;
        if (animations_.count(entity)) return true;

        const aom::anim::BrgFrame& frame0 = clip->frames[0];
        const uint32_t nv = static_cast<uint32_t>(frame0.positions.size());
        if (nv == 0) return false;

        std::vector<Vertex> verts(nv);
        for (uint32_t i = 0; i < nv; ++i) {
            verts[i].position = frame0.positions[i];
            verts[i].normal   = frame0.normals.size() > i ? frame0.normals[i] : glm::vec3(0.f,1.f,0.f);
            verts[i].texcoord = clip->uvs.size() > i ? clip->uvs[i] : glm::vec2(0.f);
            verts[i].color    = glm::vec4(1.f);
        }

        MeshHandle dyn = renderer->create_dynamic_mesh(
            verts.data(), nv,
            clip->indices.data(), static_cast<uint32_t>(clip->indices.size()));
        if (dyn == INVALID_MESH) return false;

        AnimationComponent ac;
        ac.clip         = clip;
        ac.dynamic_mesh = dyn;
        ac.texture      = texture;
        ac.morph_positions.resize(nv);
        ac.morph_normals.resize(nv);
        ac.anim_state.set_clip(clip);
        ac.anim_state.play("idle", true);
        animations_[entity] = std::move(ac);
        return true;
    }

    /// Attach a GRN skeletal animation to an entity.
    bool attach_grn_animation(Entity entity,
                              const aom::anim::GrnAnimation* anim,
                              const aom::anim::BrgAnimationClip* base_clip,
                              TextureHandle texture,
                              IRenderer* renderer) {
        if (!anim || !anim->is_valid() || !renderer || !base_clip) return false;
        if (grn_animations_.count(entity)) return true;

        // Create static mesh from first frame of base BRG clip (for base geometry)
        const aom::anim::BrgFrame& frame0 = base_clip->frames[0];
        const uint32_t nv = static_cast<uint32_t>(frame0.positions.size());
        if (nv == 0) return false;

        std::vector<Vertex> verts(nv);
        for (uint32_t i = 0; i < nv; ++i) {
            verts[i].position = frame0.positions[i];
            verts[i].normal   = frame0.normals.size() > i ? frame0.normals[i] : glm::vec3(0.f,1.f,0.f);
            verts[i].texcoord = base_clip->uvs.size() > i ? base_clip->uvs[i] : glm::vec2(0.f);
            verts[i].color    = glm::vec4(1.f);
        }

        MeshHandle static_mesh = renderer->create_dynamic_mesh(
            verts.data(), nv,
            base_clip->indices.data(), static_cast<uint32_t>(base_clip->indices.size()));
        if (static_mesh == INVALID_MESH) return false;

        // Create GrnPlayer and set animation
        auto player = std::make_unique<aom::anim::GrnPlayer>();
        if (!player->set_animation(*anim)) return false;

        GrnAnimationComponent gac;
        gac.player = std::move(player);
        gac.animation = anim;
        gac.static_mesh = static_mesh;
        gac.texture = texture;
        gac.bone_matrices.resize(anim->bones.size());
        grn_animations_[entity] = std::move(gac);
        return true;
    }

    /// Drive state machine from unit orders, evaluate morph, upload to GPU.
    void update_animations(float dt) {
        if (!anim_renderer_) return;
        for (auto& [entity, ac] : animations_) {
            if (!ac.is_ready()) continue;

            // Pick state from unit logic
            const auto u_it = units_.find(entity);
            if (u_it != units_.end()) {
                const UnitComponent& u = u_it->second;
                const auto hp_it = ecs_world_.healths().find(entity);
                std::string desired;
                if (hp_it != ecs_world_.healths().end() && hp_it->second.current_hp <= 0) {
                    desired = "death";
                } else if (u.attack_target != INVALID_ENTITY) {
                    desired = "attack";
                } else if (u.build_target != INVALID_ENTITY) {
                    const auto t_it = ecs_world_.transforms().find(entity);
                    const float spd = (t_it != ecs_world_.transforms().end())
                        ? glm::length(t_it->second.velocity) : 0.f;
                    desired = (spd > 0.05f) ? "walk" : "build";
                } else if (u.returning_cargo) {
                    desired = "walk";
                } else if (u.gather_node >= 0) {
                    const auto t_it = ecs_world_.transforms().find(entity);
                    const float spd = (t_it != ecs_world_.transforms().end())
                        ? glm::length(t_it->second.velocity) : 0.f;
                    desired = (spd > 0.05f) ? "walk" : "gather";
                } else if (u.has_move_order) {
                    const auto t_it = ecs_world_.transforms().find(entity);
                    const float spd = (t_it != ecs_world_.transforms().end())
                        ? glm::length(t_it->second.velocity) : 0.f;
                    desired = (spd > 0.05f) ? "walk" : "idle";
                } else {
                    desired = "idle";
                }
                if (ac.anim_state.current_state() != desired)
                    ac.anim_state.play(desired, desired != "death");
            }
            ac.anim_state.update(dt, [&](const std::string& tag_name) {
                if (on_anim_tag) on_anim_tag(entity, tag_name);
            });

            // Evaluate interpolated geometry
            ac.clip->evaluate(ac.anim_state.current_time(),
                              ac.morph_positions, ac.morph_normals);

            // Upload to GPU
            anim_renderer_->update_mesh_vertices(
                ac.dynamic_mesh,
                ac.morph_positions.data(),
                ac.morph_normals.data(),
                static_cast<uint32_t>(ac.morph_positions.size()));
        }

        // ── Animação GRN esquelética — atualizar bone_matrices cada frame ──
        for (auto& [entity, gac] : grn_animations_) {
            if (!gac.is_ready()) continue;
            if (!gac.player) continue;

            // Avalia a animação no tempo atual do state machine da unidade
            float t = 0.0f;
            const auto u_it = units_.find(entity);
            if (u_it != units_.end()) {
                // Reutiliza o tempo da animação BRG se existir, senão avança livre
                const auto ac_it = animations_.find(entity);
                if (ac_it != animations_.end() && ac_it->second.is_ready()) {
                    t = ac_it->second.anim_state.current_time();
                } else {
                    // Sem clip BRG: avança o tempo livre baseado em dt acumulado não temos,
                    // usamos 0 para pose bind (estática). O playback real virá quando
                    // houver um clock dedicado por entidade.
                    t = 0.0f;
                }
            }

            gac.player->evaluate(t, gac.bone_matrices);
        }
    }

    const std::unordered_map<Entity, AnimationComponent>& animations() const { return animations_; }
    std::unordered_map<Entity, AnimationComponent>& animations() { return animations_; }

    /// Render all animated entities with their morphed GPU mesh.
    void render_animated(IRenderer* renderer, const Camera* camera = nullptr) const {
        if (!renderer) return;
        // ── Culling: frustum + distância ──────────────────────────────────────
        const Camera* cam = camera;
        Camera::FrustumPlanes planes;
        bool use_culling = cam != nullptr;
        if (use_culling) planes = cam->get_frustum_planes();
        const glm::vec3 cam_pos = use_culling ? cam->position : glm::vec3(0.0f);
        constexpr float kMaxDistSq = 3600.0f;
        auto culled = [&](const glm::vec3& pos, float radius) -> bool {
            if (!use_culling) return false;
            for (int i = 0; i < 6; ++i) {
                const float dist = planes.planes[i].x * pos.x +
                                   planes.planes[i].y * pos.y +
                                   planes.planes[i].z * pos.z +
                                   planes.planes[i].w;
                if (dist < -radius) return true;
            }
            return glm::dot(pos - cam_pos, pos - cam_pos) > kMaxDistSq;
        };
        for (const auto& [entity, ac] : animations_) {
            if (!ac.is_ready()) continue;
            const auto t_it = ecs_world_.transforms().find(entity);
            if (t_it == ecs_world_.transforms().end()) continue;
            const TransformComponent& tr = t_it->second;
            const float radius = std::max({std::abs(tr.scale.x), std::abs(tr.scale.y), std::abs(tr.scale.z)}) * 1.5f;
            if (culled(tr.position, radius)) continue;
            glm::mat4 model = glm::translate(glm::mat4(1.f), tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.y), glm::vec3(0.f,1.f,0.f));
            model = glm::scale(model, tr.scale);
            renderer->draw_mesh(ac.dynamic_mesh, model, ac.texture, BlendMode::OPAQUE);
        }
    }

    void set_anim_renderer(IRenderer* r) { anim_renderer_ = r; }

    const std::unordered_map<Entity, GrnAnimationComponent>& grn_animations() const { return grn_animations_; }
    std::unordered_map<Entity, GrnAnimationComponent>& grn_animations() { return grn_animations_; }

    /// Render all GRN skeletal animated entities.
    void render_grn_animated(IRenderer* renderer, const Camera* camera = nullptr) const {
        if (!renderer) return;
        // ── Culling: frustum + distância ──────────────────────────────────────
        const Camera* cam = camera;
        Camera::FrustumPlanes planes;
        bool use_culling = cam != nullptr;
        if (use_culling) planes = cam->get_frustum_planes();
        const glm::vec3 cam_pos = use_culling ? cam->position : glm::vec3(0.0f);
        constexpr float kMaxDistSq = 3600.0f;
        auto culled = [&](const glm::vec3& pos, float radius) -> bool {
            if (!use_culling) return false;
            for (int i = 0; i < 6; ++i) {
                const float dist = planes.planes[i].x * pos.x +
                                   planes.planes[i].y * pos.y +
                                   planes.planes[i].z * pos.z +
                                   planes.planes[i].w;
                if (dist < -radius) return true;
            }
            return glm::dot(pos - cam_pos, pos - cam_pos) > kMaxDistSq;
        };
        for (const auto& [entity, gac] : grn_animations_) {
            if (!gac.is_ready()) continue;
            const auto t_it = ecs_world_.transforms().find(entity);
            if (t_it == ecs_world_.transforms().end()) continue;
            const TransformComponent& tr = t_it->second;
            const float radius = std::max({std::abs(tr.scale.x), std::abs(tr.scale.y), std::abs(tr.scale.z)}) * 1.5f;
            if (culled(tr.position, radius)) continue;
            glm::mat4 model = glm::translate(glm::mat4(1.f), tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.y), glm::vec3(0.f,1.f,0.f));
            model = glm::scale(model, tr.scale);
            renderer->draw_mesh_skinned(gac.static_mesh, model, gac.texture, gac.bone_matrices, BlendMode::OPAQUE);
        }
    }

    std::function<void(Entity, const std::string&)> on_anim_tag;

    Entity select_at(const glm::vec3& point) {
        return selection_system_->select_at(point, 2.25f);
    }
    void clear_selection() {
        for (auto& [e, marker] : selections_) marker.selected = false;
    }
    void set_selected(Entity id, bool sel) {
        selections_[id].selected = sel;
    }
    const std::unordered_map<Entity, TransformComponent>& transforms() const { return ecs_world_.transforms(); }
    const std::unordered_map<Entity, UnitComponent>& units() const { return units_; }
    std::unordered_map<Entity, UnitComponent>& units() { return units_; }

    /// Acesso mutável ao componente de unidade (usado pelo save/load).
    UnitComponent* unit_component_mutable(Entity id) {
        auto it = units_.find(id);
        return (it != units_.end()) ? &it->second : nullptr;
    }

    /// Acesso mutável ao componente de edificio (usado pelo save/load).
    BuildingComponent* building_component_mutable(Entity id) {
        auto it = building_components_.find(id);
        return (it != building_components_.end()) ? &it->second : nullptr;
    }

    /// Acesso mutável ao componente de saúde (usado pelos testes).
    HealthComponent* health_component_mutable(Entity id) {
        auto it = ecs_world_.healths_mut().find(id);
        return (it != ecs_world_.healths_mut().end()) ? &it->second : nullptr;
    }

    const FogOfWar& fog_of_war() const { return fog_of_war_; }

    /// Acesso publico ao pathfinder da NavigationGrid (usado pelo save/load
    /// para reconstruir waypoints de unidades ao carregar uma partida).
    std::vector<glm::vec3> navigation_find_path(const glm::vec3& from, const glm::vec3& to) const {
        return navigation_.find_path(from, to);
    }

    /// Reconstrói a NavigationGrid a partir de um height-map.
    /// Marca como bloqueadas as células com inclinação excessiva (encostas íngremes)
    /// e atualiza as dimensões/escala da grade para cobrir o terreno.
    void rebuild_navigation_from_heightmap(const std::vector<float>& heightmap,
                                           int hm_width, int hm_height, float cell_size,
                                           float max_slope_radians = 0.7f) {
        // A grade de navegação deve cobrir toda a área do terreno.
        // Usamos o height-map como base: cada célula da grade corresponde a NxN
        // amostras do height-map (ou 1:1 se o height-map for menor).
        const int grid_w = std::max(32, hm_width);
        const int grid_h = std::max(32, hm_height);
        navigation_ = NavigationGrid(grid_w, grid_h, cell_size);

        if (heightmap.empty() || hm_width <= 1 || hm_height <= 1) {
            return; // Grade padrão (tudo transponível)
        }

        // Calcular inclinação (slope) via diferenças finitas no height-map
        // e bloquear células onde a inclinação excede o limite.
        auto get_h = [&](int x, int z) -> float {
            if (x < 0 || x >= hm_width || z < 0 || z >= hm_height) return 0.0f;
            return heightmap[static_cast<size_t>(z) * hm_width + x];
        };

        for (int gz = 0; gz < grid_h; ++gz) {
            for (int gx = 0; gx < grid_w; ++gx) {
                // Mapear coordenada da grade para coordenada do height-map
                const float fx = static_cast<float>(gx) / grid_w * hm_width;
                const float fz = static_cast<float>(gz) / grid_h * hm_height;
                const int x0 = static_cast<int>(fx);
                const int z0 = static_cast<int>(fz);
                const int x1 = std::min(x0 + 1, hm_width - 1);
                const int z1 = std::min(z0 + 1, hm_height - 1);

                // Bilinear sampling para altura no centro da célula
                const float h00 = get_h(x0, z0);
                const float h10 = get_h(x1, z0);
                const float h01 = get_h(x0, z1);
                const float h11 = get_h(x1, z1);

                // Diferenças finitas para estimar a normal/slope
                const float dx = (h10 - h00 + h11 - h01) * 0.5f;
                const float dz = (h01 - h00 + h11 - h10) * 0.5f;
                const float slope = std::atan(std::sqrt(dx * dx + dz * dz) / cell_size);

                if (slope > max_slope_radians) {
                    navigation_.set_blocked(gx, gz, true);
                }
            }
        }
    }

    std::vector<Entity> select_box(const glm::vec3& min_point, const glm::vec3& max_point) {
        std::vector<Entity> result;
        for (auto& [entity, transform] : ecs_world_.transforms_mut()) {
            auto unit = units_.find(entity);
            if (unit == units_.end() || static_cast<Team>(unit->second.team) != Team::FRIENDLY) continue;
            const auto& p = transform.position;
            const bool inside = p.x >= std::min(min_point.x, max_point.x) && p.x <= std::max(min_point.x, max_point.x) &&
                                p.z >= std::min(min_point.z, max_point.z) && p.z <= std::max(min_point.z, max_point.z);
            selections_[entity].selected = inside;
            if (inside) result.push_back(entity);
        }
        return result;
    }
    void order_move_selected(const glm::vec3& target) {
        for (const auto& [entity, marker] : selections_) if (marker.selected) order_move(entity, target);
    }
    Entity selected_entity() const {
        for (const auto& [entity, marker] : selections_) if (marker.selected) return entity;
        return INVALID_ENTITY;
    }
    size_t selected_count() const {
        size_t count = 0;
        for (const auto& [entity, marker] : selections_) if (marker.selected) ++count;
        return count;
    }
    std::vector<Entity> selected_entities() const {
        std::vector<Entity> list;
        for (const auto& [entity, marker] : selections_) if (marker.selected) list.push_back(entity);
        return list;
    }
    void select_entity(Entity entity, bool selected = true) {
        if (entity == INVALID_ENTITY) return;
        auto& sel = selections_[entity];
        sel.selected = selected;
    }
    Entity enemy_at(const glm::vec3& point, float radius = 1.5f) const {
        Entity result = INVALID_ENTITY; float best = radius;
        for (const auto& [entity, transform] : ecs_world_.transforms()) {
            const auto unit = units_.find(entity);
            if (unit == units_.end() || static_cast<Team>(unit->second.team) != Team::ENEMY) continue;
            const float distance = glm::length(transform.position - point);
            if (distance < best) { best = distance; result = entity; }
        }
        return result;
    }
    Entity building_at(const glm::vec3& point, float radius = 2.0f) const {
        Entity result = INVALID_ENTITY; float best = radius;
        for (const auto& [entity, transform] : ecs_world_.transforms()) {
            if (building_components_.find(entity) == building_components_.end()) continue;
            const float distance = glm::length(transform.position - point);
            if (distance < best) { best = distance; result = entity; }
        }
        return result;
    }
    void order_attack_selected(Entity target) {
        auto target_transform = ecs_world_.transforms().find(target);
        if (target_transform == ecs_world_.transforms().end()) return;
        for (const auto& [entity, marker] : selections_) if (marker.selected) {
            auto unit = units_.find(entity);
            if (unit != units_.end() && static_cast<Team>(unit->second.team) == Team::FRIENDLY) {
                unit->second.attack_target = target;
                unit->second.move_target = target_transform->second.position;
                unit->second.has_move_order = true;
                unit->second.waypoints = navigation_.find_path(ecs_world_.transforms().at(entity).position, target_transform->second.position);
                unit->second.waypoint_index = 0;
            }
        }
    }
    bool order_move(Entity entity, const glm::vec3& target) {
        if (ecs_world_.transforms().find(entity) == ecs_world_.transforms().end() || units_.find(entity) == units_.end()) return false;
        units_[entity].waypoints = navigation_.find_path(ecs_world_.transforms().at(entity).position, target);
        if (units_[entity].waypoints.empty()) return false;
        units_[entity].waypoint_index = 0;
        units_[entity].move_target = target;
        units_[entity].has_move_order = true;
        units_[entity].attack_target = INVALID_ENTITY;
        return true;
    }
    bool order_attack(Entity entity, Entity target) {
        auto target_transform = ecs_world_.transforms().find(target);
        if (target_transform == ecs_world_.transforms().end()) return false;
        auto unit = units_.find(entity);
        if (unit == units_.end()) return false;
        unit->second.attack_target = target;
        unit->second.move_target = target_transform->second.position;
        unit->second.has_move_order = true;
        unit->second.waypoints = navigation_.find_path(ecs_world_.transforms().at(entity).position, target_transform->second.position);
        unit->second.waypoint_index = 0;
        return true;
    }
    std::unordered_map<Entity, BuildingComponent>& buildings() { return building_components_; }
    const std::unordered_map<Entity, BuildingComponent>& buildings() const { return building_components_; }
    const std::vector<ResourceNode>& resource_nodes() const { return resources_nodes_; }
    ResourcePool& enemy_resources() { return enemy_resources_; }
    const ResourcePool& enemy_resources() const { return enemy_resources_; }

    Entity build_at(const glm::vec3& point, const BuildingCost& cost = {}) {
        if (resources_.food < cost.food || resources_.wood < cost.wood ||
            resources_.gold < cost.gold || resources_.favor < cost.favor) return INVALID_ENTITY;
        resources_.food -= cost.food; resources_.wood -= cost.wood;
        resources_.gold -= cost.gold; resources_.favor -= cost.favor;
        return place_building(Team::FRIENDLY, point);
    }
    const ResourcePool& resources() const { return resources_; }
    ResourcePool& resources_mutable() { return resources_; }
    const ResourcePool& resource_pool(Team team) const { return (team == Team::FRIENDLY) ? resources_ : enemy_resources_; }
    ResourcePool& resource_pool_mutable(Team team) { return (team == Team::FRIENDLY) ? resources_ : enemy_resources_; }
    // —— Estado de vitoria (skirmish) —— (publicos para save_load.cpp e engine.cpp)
    VictoryCondition victory_condition() const { return victory_condition_; }
    VictoryResult    victory_result()    const { return victory_result_; }
    float            victory_timer()     const { return victory_timer_; }
    void set_victory_condition(VictoryCondition c) { victory_condition_ = c; }
    void set_victory_result(VictoryResult r)       { victory_result_ = r; }
    void set_victory_timer(float t)                { victory_timer_ = t; }
    const std::unordered_map<Entity, HealthComponent>& health_components() const { return ecs_world_.healths(); }
    void set_entity_hp(Entity entity, int current_hp) {
        auto it = ecs_world_.healths_mut().find(entity);
        if (it != ecs_world_.healths_mut().end()) {
            it->second.current_hp = current_hp;
        }
    }
    void set_navigation_blocked(int x, int y, bool blocked = true) { navigation_.set_blocked(x, y, blocked); }

    std::vector<MinimapBlip> get_minimap_blips() const {
        std::vector<MinimapBlip> blips;
        blips.reserve(resources_nodes_.size() + ecs_world_.transforms().size());

        // 1. Resources
        for (const auto& node : resources_nodes_) {
            if (node.remaining <= 0) continue;
            if (!fog_of_war_.is_explored(node.position)) continue;
            uint32_t col = 0x55CC33FF; // Food (Lime)
            if (node.type == ResourceType::WOOD) col = 0x8B5A2BFF; // Wood (Brown)
            else if (node.type == ResourceType::GOLD) col = 0xFFD700FF; // Gold (Golden)
            else if (node.type == ResourceType::FAVOR) col = 0x9933FFFF; // Favor (Purple)
            blips.push_back({node.position, col, 3.5f, false});
        }

        // 2. Units & Buildings
        for (const auto& [entity, transform] : ecs_world_.transforms()) {
            auto u_it = units_.find(entity);
            if (u_it != units_.end()) {
                if (static_cast<Team>(u_it->second.team) == Team::ENEMY && !fog_of_war_.is_visible(transform.position)) {
                    continue; // Hidden from radar by Fog of War
                }
                const bool sel = selections_.count(entity) && selections_.at(entity).selected;
                uint32_t col = 0x33EE33FF; // Friendly
                if (static_cast<Team>(u_it->second.team) == Team::ENEMY) {
                    col = 0xFF3333FF; // Enemy
                } else if (u_it->second.worker) {
                    col = 0x33EEEEFF; // Worker Cyan
                }
                if (sel) {
                    col = 0xFFFF00FF; // Selected Highlight
                }
                blips.push_back({transform.position, col, sel ? 6.0f : 4.5f, sel});
            }
            auto b_it = building_components_.find(entity);
            if (b_it != building_components_.end()) {
                if (static_cast<Team>(b_it->second.team) == Team::ENEMY && !fog_of_war_.is_explored(transform.position)) {
                    continue; // Enemy building in unexplored territory
                }
                uint32_t col = (static_cast<Team>(b_it->second.team) == Team::ENEMY) ? 0xFF8822FF : 0x3388FFFF;
                blips.push_back({transform.position, col, 7.0f, false});
            }
        }
        return blips;
    }
    void render(IRenderer* renderer, MeshHandle mesh, TextureHandle texture,
                MeshHandle hoplite_mesh, TextureHandle hoplite_texture,
                const Camera* camera = nullptr) const {
        if (!renderer) return;

        // ── Culling: frustum + distância ──────────────────────────────────────
        // Calcula os planos do frustum UMA única vez por frame (não por entidade).
        const Camera* cam = camera;
        Camera::FrustumPlanes planes;
        bool use_culling = cam != nullptr;
        if (use_culling) planes = cam->get_frustum_planes();
        const glm::vec3 cam_pos = use_culling ? cam->position : glm::vec3(0.0f);
        constexpr float kMaxDistSq = 3600.0f; // 60 unidades de mundo (rede de segurança)
        auto culled = [&](const glm::vec3& pos, float radius) -> bool {
            if (!use_culling) return false;
            for (int i = 0; i < 6; ++i) {
                const float dist = planes.planes[i].x * pos.x +
                                   planes.planes[i].y * pos.y +
                                   planes.planes[i].z * pos.z +
                                   planes.planes[i].w;
                if (dist < -radius) return true; // fora do frustum
            }
            return glm::dot(pos - cam_pos, pos - cam_pos) > kMaxDistSq; // muito distante
        };

        for (const auto& node : resources_nodes_) {
            if (node.remaining <= 0) continue;
            if (!fog_of_war_.is_explored(node.position)) continue;
            const uint32_t color = node.type == ResourceType::FOOD ? 0xFF55AA33u :
                (node.type == ResourceType::WOOD ? 0xFF8B5A2Bu :
                 (node.type == ResourceType::GOLD ? 0xFFD4AF37u : 0xFF8844CCu));
            renderer->draw_debug_line(node.position + glm::vec3(-0.5f, 0.03f, 0.0f), node.position + glm::vec3(0.5f, 0.03f, 0.0f), color);
            renderer->draw_debug_line(node.position + glm::vec3(0.0f, 0.03f, -0.5f), node.position + glm::vec3(0.0f, 0.03f, 0.5f), color);
        }
        for (const auto& [entity, transform] : ecs_world_.transforms()) {
            auto u_it = units_.find(entity);
            auto b_it = building_components_.find(entity);
            if (u_it == units_.end() && b_it == building_components_.end()) continue;
            const auto* unit = (u_it != units_.end()) ? &u_it->second : nullptr;
            if (unit && static_cast<Team>(unit->team) == Team::ENEMY && !fog_of_war_.is_visible(transform.position)) {
                continue; // Enemy unit hidden in fog of war!
            }
            // Culling por frustum/distância: não renderiza o que não está na vista.
            const float radius = std::max({std::abs(transform.scale.x), std::abs(transform.scale.y), std::abs(transform.scale.z)}) * 1.5f;
            if (culled(transform.position, radius)) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
            model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
            glm::vec3 scale = transform.scale;
            const bool sel = selections_.count(entity) && selections_.at(entity).selected;
            if (sel) scale *= 1.15f;
            if (b_it != building_components_.end()) scale *= 0.8f;
            model = glm::scale(model, scale);
            MeshHandle entity_mesh = mesh;
            TextureHandle entity_texture = texture;
            if (b_it != building_components_.end()) {
                if (b_it->second.mesh != INVALID_MESH) {
                    entity_mesh = b_it->second.mesh;
                    entity_texture = (b_it->second.texture != INVALID_TEXTURE) ? b_it->second.texture : texture;
                }
            } else if (unit) {
                if (unit->mesh != INVALID_MESH) {
                    entity_mesh = unit->mesh;
                    entity_texture = (unit->texture != INVALID_TEXTURE) ? unit->texture : texture;
                } else if (unit->unit_type_name == "Hoplite" && hoplite_mesh != INVALID_MESH) {
                    entity_mesh = hoplite_mesh;
                    entity_texture = hoplite_texture;
                }
            }
            if (entity_mesh == INVALID_MESH) continue;
            renderer->draw_mesh(entity_mesh, model, entity_texture, BlendMode::OPAQUE);
            const uint32_t team_color = unit && static_cast<Team>(unit->team) == Team::ENEMY
                ? 0xFF3333FFu : 0xFF33CC33u;
            const glm::vec3 p = transform.position;
            if (sel) {
                // Four short debug segments form a ground selection marker.
                renderer->draw_debug_line(p + glm::vec3(-0.8f, 0.02f, -0.8f), p + glm::vec3(0.8f, 0.02f, -0.8f), 0xFFFFFF00u);
                renderer->draw_debug_line(p + glm::vec3(0.8f, 0.02f, -0.8f), p + glm::vec3(0.8f, 0.02f, 0.8f), 0xFFFFFF00u);
                renderer->draw_debug_line(p + glm::vec3(0.8f, 0.02f, 0.8f), p + glm::vec3(-0.8f, 0.02f, 0.8f), 0xFFFFFF00u);
                renderer->draw_debug_line(p + glm::vec3(-0.8f, 0.02f, 0.8f), p + glm::vec3(-0.8f, 0.02f, -0.8f), 0xFFFFFF00u);
            }
            if (unit) {
                const auto hp = ecs_world_.healths().find(entity);
                if (hp != ecs_world_.healths().end() && render_hp_bars_) {
                    const float ratio = std::clamp(static_cast<float>(hp->second.current_hp) /
                                                   std::max(1, hp->second.max_hp), 0.0f, 1.0f);
                    renderer->draw_debug_line(p + glm::vec3(-0.7f, 1.5f, 0.0f),
                                              p + glm::vec3(-0.7f + 1.4f * ratio, 1.5f, 0.0f),
                                              team_color);
                }
            }

            // Building footprint scaffolding & progress bars
            if (b_it != building_components_.end()) {
                const auto& b = b_it->second;
                if (!b.complete) {
                    renderer->draw_debug_line(p + glm::vec3(-1.2f, 0.05f, -1.2f), p + glm::vec3(1.2f, 0.05f, -1.2f), 0xFFFFCC00u);
                    renderer->draw_debug_line(p + glm::vec3(1.2f, 0.05f, -1.2f), p + glm::vec3(1.2f, 0.05f, 1.2f), 0xFFFFCC00u);
                    renderer->draw_debug_line(p + glm::vec3(1.2f, 0.05f, 1.2f), p + glm::vec3(-1.2f, 0.05f, 1.2f), 0xFFFFCC00u);
                    renderer->draw_debug_line(p + glm::vec3(-1.2f, 0.05f, 1.2f), p + glm::vec3(-1.2f, 0.05f, -1.2f), 0xFFFFCC00u);
                    renderer->draw_debug_line(p + glm::vec3(-1.2f, 0.05f, -1.2f), p + glm::vec3(1.2f, 0.05f, 1.2f), 0xFFFF8800u);
                    renderer->draw_debug_line(p + glm::vec3(-1.2f, 0.05f, 1.2f), p + glm::vec3(1.2f, 0.05f, -1.2f), 0xFFFF8800u);

                    // Yellow construction progress bar
                    renderer->draw_debug_line(p + glm::vec3(-1.0f, 2.2f, 0.0f),
                                              p + glm::vec3(-1.0f + 2.0f * b.progress, 2.2f, 0.0f),
                                              0xFFFFDD00u);
                }
                if (b.complete && !b.training_unit.empty()) {
                    float train_ratio = std::clamp(1.0f - (b.train_timer / 4.0f), 0.0f, 1.0f);
                    renderer->draw_debug_line(p + glm::vec3(-0.9f, 2.4f, 0.0f),
                                              p + glm::vec3(-0.9f + 1.8f * train_ratio, 2.4f, 0.0f),
                                              0xFF00FFFFu);
                }
            }
        }

        // Tactical Feedback: Waypoint paths, Attack targeting & Construction vectors
        for (const auto& [entity, unit] : units_) {
            auto t_it = ecs_world_.transforms().find(entity);
            if (t_it == ecs_world_.transforms().end()) continue;
            const glm::vec3 unit_pos = t_it->second.position;
            const bool selected = selections_.count(entity) && selections_.at(entity).selected;

            // 1. Attack targeting line (red vector)
            if (unit.attack_target != INVALID_ENTITY) {
                auto enemy_t = ecs_world_.transforms().find(unit.attack_target);
                if (enemy_t != ecs_world_.transforms().end()) {
                    renderer->draw_debug_line(unit_pos + glm::vec3(0.0f, 0.1f, 0.0f),
                                              enemy_t->second.position + glm::vec3(0.0f, 0.1f, 0.0f),
                                              0xFFFF2222u);
                }
            }
            // 2. Worker construction target line (gold vector)
            else if (unit.build_target != INVALID_ENTITY) {
                auto b_t = ecs_world_.transforms().find(unit.build_target);
                if (b_t != ecs_world_.transforms().end()) {
                    renderer->draw_debug_line(unit_pos + glm::vec3(0.0f, 0.1f, 0.0f),
                                              b_t->second.position + glm::vec3(0.0f, 0.1f, 0.0f),
                                              0xFFFFBB22u);
                }
            }
            // 3. Waypoint movement path (neon green) & target marker (yellow cross)
            else if (unit.has_move_order && (selected || static_cast<Team>(unit.team) == Team::FRIENDLY)) {
                glm::vec3 curr = unit_pos + glm::vec3(0.0f, 0.05f, 0.0f);
                for (size_t wi = unit.waypoint_index; wi < unit.waypoints.size(); ++wi) {
                    glm::vec3 next_pt = unit.waypoints[wi] + glm::vec3(0.0f, 0.05f, 0.0f);
                    renderer->draw_debug_line(curr, next_pt, 0xFF33FF55u);
                    curr = next_pt;
                }
                const glm::vec3 dest = unit.move_target + glm::vec3(0.0f, 0.05f, 0.0f);
                renderer->draw_debug_line(dest + glm::vec3(-0.35f, 0.0f, 0.0f),
                                          dest + glm::vec3(0.35f, 0.0f, 0.0f),
                                          0xFFFFFF00u);
                renderer->draw_debug_line(dest + glm::vec3(0.0f, 0.0f, -0.35f),
                                          dest + glm::vec3(0.0f, 0.0f, 0.35f),
                                          0xFFFFFF00u);
            }
        }

        // ── Scenery objects (rochas, árvores na beira da água, etc.) ──────────
        for (const auto& so : scenery_objects_) {
            MeshHandle m = (so.mesh != INVALID_MESH) ? so.mesh : mesh;
            if (m == INVALID_MESH) continue;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), so.position);
            model = glm::scale(model, glm::vec3(so.scale));
            TextureHandle t = (so.texture != INVALID_TEXTURE) ? so.texture : texture;
            renderer->draw_mesh(m, model, t, BlendMode::OPAQUE);
        }
    }
private:
    const XmbElement* proto_root_ = nullptr;
    ECSWorld ecs_world_;
    std::unordered_map<Entity, UnitComponent> units_;
    std::unordered_map<Entity, SelectionComponent> selections_;
    std::unordered_map<Entity, BuildingComponent> building_components_;
    ResourcePool resources_;
    ResourcePool enemy_resources_{500, 400, 300, 50};

    // —— Estado de vitoria (skirmish) ——
    VictoryCondition victory_condition_ = static_cast<VictoryCondition>(0);
    VictoryResult    victory_result_    = static_cast<VictoryResult>(0);
    float            victory_timer_     = 0.0f;

    NavigationGrid navigation_;
    glm::ivec2 navigation_cell(const glm::vec3& p) const {
        return {static_cast<int>(p.x + 16.0f), static_cast<int>(p.z + 16.0f)};
    }

    /// Finds a walkable cell on a ring around the building, preferring the
    /// +X side (traditional rally direction) and falling back to the other
    /// cardinals/diagonals. Returns the building position if nothing is free.
    glm::vec3 find_spawn_position(Entity building) const {
        auto b_t = ecs_world_.transforms().find(building);
        if (b_t == ecs_world_.transforms().end()) return glm::vec3(0.0f);

        const glm::vec3 base = b_t->second.position;
        static constexpr float kDistances[] = {2.5f, 3.0f, 3.5f, 4.0f, 2.0f};
        static constexpr int kDirs[8][2] = {
            {1,0},{-1,0},{0,1},{0,-1},      // cardinais
            {1,1},{-1,1},{1,-1},{-1,-1}     // diagonais
        };

        for (float d : kDistances) {
            for (const auto& dir : kDirs) {
                const glm::vec3 p = base + glm::vec3(dir[0] * d, 0.0f, dir[1] * d);
                if (std::abs(p.x) > 16.5f || std::abs(p.z) > 16.5f) continue;
                const glm::ivec2 cell = navigation_cell(p);
                if (navigation_.walkable(cell.x, cell.y)) return p;
            }
        }
        return base;
    }
    std::vector<ResourceNode> resources_nodes_;
    std::vector<SceneryObject> scenery_objects_;
    // Sistemas registrados no ECSWorld (acesso via ponteiros para callbacks)
    MovementSystem* movement_system_ = nullptr;
    CombatSystem* combat_system_ = nullptr;
    SelectionSystem* selection_system_ = nullptr;
    BuildingSystem* building_system_ = nullptr;
    RenderSystem* render_system_ = nullptr;
    FogOfWar fog_of_war_;
    std::unordered_map<Entity, AnimationComponent> animations_;
    std::unordered_map<Entity, GrnAnimationComponent> grn_animations_;
    IRenderer* anim_renderer_ = nullptr;
    const aom::anim::BrgAnimationClip* default_clip_ = nullptr;
    TextureHandle default_clip_tex_ = INVALID_TEXTURE;
    const UnitTransformManager* unit_transforms_ = nullptr;

    void update_orders() {
        for (auto& [entity, unit] : units_) {
            // Cache da transform da entidade (uma única busca por frame/entidade)
            auto tr_it = ecs_world_.transforms_mut().find(entity);
            if (tr_it == ecs_world_.transforms_mut().end()) continue;
            TransformComponent& tr = tr_it->second;

            // Target tracking for attacks
            if (unit.attack_target != INVALID_ENTITY) {
                auto enemy_t = ecs_world_.transforms().find(unit.attack_target);
                auto enemy_hp = ecs_world_.healths().find(unit.attack_target);
                if (enemy_t == ecs_world_.transforms().end() || enemy_hp == ecs_world_.healths().end() || enemy_hp->second.current_hp <= 0) {
                    unit.attack_target = INVALID_ENTITY;
                    unit.has_move_order = false;
                    tr.velocity = glm::vec3(0.0f);
                    continue;
                }
                const float dist = glm::length(enemy_t->second.position - tr.position);
                if (dist <= unit.attack_range * 0.9f) {
                    tr.velocity = glm::vec3(0.0f);
                    continue;
                } else {
                    unit.move_target = enemy_t->second.position;
                    glm::vec3 dir = glm::normalize(enemy_t->second.position - tr.position);
                    tr.velocity = dir * unit.move_speed;
                    continue;
                }
            }

            // Target tracking for construction by workers
            if (unit.build_target != INVALID_ENTITY) {
                auto b_t = ecs_world_.transforms().find(unit.build_target);
                auto b_comp = building_components_.find(unit.build_target);
                if (b_t == ecs_world_.transforms().end() || b_comp == building_components_.end() || b_comp->second.complete) {
                    unit.build_target = INVALID_ENTITY;
                    unit.has_move_order = false;
                    tr.velocity = glm::vec3(0.0f);
                    continue;
                }
                const float dist = glm::length(b_t->second.position - tr.position);
                if (dist <= 2.2f) {
                    tr.velocity = glm::vec3(0.0f);
                    continue;
                } else {
                    unit.move_target = b_t->second.position;
                    glm::vec3 dir = glm::normalize(b_t->second.position - tr.position);
                    tr.velocity = dir * unit.move_speed;
                    continue;
                }
            }

            // Target tracking for resource gathering by workers
            if (unit.gather_node >= 0 && unit.gather_node < static_cast<int>(resources_nodes_.size()) && !unit.returning_cargo) {
                const auto& node = resources_nodes_[unit.gather_node];
                if (node.remaining <= 0) {
                    unit.gather_node = -1;
                } else {
                    const float dist = glm::length(node.position - tr.position);
                    if (dist <= 2.0f) {
                        tr.velocity = glm::vec3(0.0f);
                        unit.has_move_order = false;
                        continue;
                    }
                }
            }

            if (!unit.has_move_order) continue;
            if (unit.waypoint_index >= unit.waypoints.size()) {
                unit.has_move_order = false;
                tr.velocity = glm::vec3(0.0f);
                continue;
            }
            const glm::vec3 delta = unit.waypoints[unit.waypoint_index] - tr.position;
            const float distance = glm::length(delta);
            if (distance < 0.15f) {
                ++unit.waypoint_index;
                if (unit.waypoint_index >= unit.waypoints.size()) {
                    unit.has_move_order = false;
                    tr.velocity = glm::vec3(0.0f);
                }
            } else {
                tr.velocity = (delta / distance) * unit.move_speed;
            }
        }
    }

    void gather_resources(float dt) {
        for (auto& [entity, unit] : units_) {
            if (!unit.worker) continue;
            auto t_it = ecs_world_.transforms_mut().find(entity);
            if (t_it == ecs_world_.transforms_mut().end()) continue;
            auto& transform = t_it->second;

            // --- 1. Returning Cargo to Town Center ---
            if (unit.returning_cargo) {
                Entity tc = get_nearest_town_center(static_cast<Team>(unit.team), transform.position);
                if (tc == INVALID_ENTITY) {
                    ResourcePool& target_pool = (static_cast<Team>(unit.team) == Team::ENEMY) ? enemy_resources_ : resources_;
                    switch (static_cast<ResourceType>(unit.carried_resource_type)) {
                        case ResourceType::FOOD:  target_pool.food  += unit.carried_resource_amount; break;
                        case ResourceType::WOOD:  target_pool.wood  += unit.carried_resource_amount; break;
                        case ResourceType::GOLD:  target_pool.gold  += unit.carried_resource_amount; break;
                        case ResourceType::FAVOR: target_pool.favor += unit.carried_resource_amount; break;
                    }
                    unit.carried_resource_amount = 0;
                    unit.returning_cargo = false;
                    unit.has_move_order = false;
                    transform.velocity = glm::vec3(0.0f);
                    continue;
                }
                auto tc_t = ecs_world_.transforms().find(tc);
                if (tc_t == ecs_world_.transforms().end()) continue;

                float dist_tc = glm::length(transform.position - tc_t->second.position);
                if (dist_tc > 3.2f) {
                    if (!unit.has_move_order || glm::length(unit.move_target - tc_t->second.position) > 1.5f) {
                        order_move(entity, tc_t->second.position);
                    }
                } else {
                    // Reached Town Center: Deposit cargo!
                    ResourcePool& target_pool = (static_cast<Team>(unit.team) == Team::ENEMY) ? enemy_resources_ : resources_;
                    switch (static_cast<ResourceType>(unit.carried_resource_type)) {
                        case ResourceType::FOOD:  target_pool.food  += unit.carried_resource_amount; break;
                        case ResourceType::WOOD:  target_pool.wood  += unit.carried_resource_amount; break;
                        case ResourceType::GOLD:  target_pool.gold  += unit.carried_resource_amount; break;
                        case ResourceType::FAVOR: target_pool.favor += unit.carried_resource_amount; break;
                    }
                    unit.carried_resource_amount = 0;
                    unit.returning_cargo = false;
                    unit.has_move_order = false;
                    transform.velocity = glm::vec3(0.0f);

                    // Autonomously return to gather node if still valid
                    if (unit.gather_node >= 0 && unit.gather_node < static_cast<int>(resources_nodes_.size())) {
                        auto& node = resources_nodes_[unit.gather_node];
                        if (node.remaining > 0) {
                            order_move(entity, node.position);
                        } else {
                            unit.gather_node = -1;
                        }
                    }
                }
                continue;
            }

            // --- 2. Harvesting from Resource Node ---
            if (unit.gather_node < 0 || unit.gather_node >= static_cast<int>(resources_nodes_.size())) {
                continue;
            }

            auto& node = resources_nodes_[unit.gather_node];
            if (node.remaining <= 0) {
                if (unit.carried_resource_amount > 0) {
                    unit.returning_cargo = true;
                    Entity tc = get_nearest_town_center(static_cast<Team>(unit.team), transform.position);
                    if (tc != INVALID_ENTITY) {
                        auto tc_t = ecs_world_.transforms().find(tc);
                        if (tc_t != ecs_world_.transforms().end()) order_move(entity, tc_t->second.position);
                    }
                } else {
                    unit.gather_node = -1;
                }
                continue;
            }

            float dist_node = glm::length(transform.position - node.position);
            if (dist_node > 2.5f) {
                continue;
            }

            unit.gather_timer = std::max(0.0f, unit.gather_timer - dt);
            if (unit.gather_timer > 0.0f) continue;

            int space_left = unit.max_carry_capacity - unit.carried_resource_amount;
            int harvest = std::min(space_left, std::min(2, node.remaining));
            node.remaining -= harvest;
            unit.carried_resource_amount += harvest;
            unit.carried_resource_type = node.type;
            unit.gather_timer = 1.0f;

            if (harvest > 0 && on_resource_harvested) {
                on_resource_harvested(node.type, transform.position + glm::vec3(0.0f, 0.6f, 0.0f));
            }

            if (unit.carried_resource_amount >= unit.max_carry_capacity) {
                unit.returning_cargo = true;
                Entity tc = get_nearest_town_center(unit.team, transform.position);
                if (tc != INVALID_ENTITY) {
                    auto tc_t = ecs_world_.transforms().find(tc);
                    if (tc_t != ecs_world_.transforms().end()) {
                        order_move(entity, tc_t->second.position);
                    }
                }
            }
        }
    }

    void remove_entity(Entity id) {
        ecs_world_.destroy_entity(id);
        units_.erase(id);
        selections_.erase(id);
        building_components_.erase(id);
        animations_.erase(id);
        grn_animations_.erase(id);
        for (auto& [e, u] : units_) {
            if (u.attack_target == id) {
                u.attack_target = INVALID_ENTITY;
                u.has_move_order = false;
            }
            if (u.build_target == id) {
                u.build_target = INVALID_ENTITY;
                u.has_move_order = false;
            }
        }
    }
};

} // namespace aom
