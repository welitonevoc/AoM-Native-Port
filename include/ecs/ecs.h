#pragma once

/**
 * Minimal Entity‑Component‑System (ECS) for AoM‑Native‑Port.
 *
 * Provides:
 *   - Entity identifier (uint32_t)
 *   - Base Component struct (empty marker)
 *   - Concrete components: Transform, Health, Renderable
 *   - Base System interface
 *   - Two sample systems: MovementSystem (updates position from a velocity vector)
 *     and RenderSystem (draws meshes via IRenderer).
 *   - ECSWorld class that stores components in separate unordered_maps and runs systems each frame.
 */

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer/renderer.h"

namespace aom {

using Entity = uint32_t;
constexpr Entity INVALID_ENTITY = 0;

/** Base marker for all components */
struct Component {};

/** Transform component – position, rotation (Euler degrees), scale and a simple velocity vector */
struct TransformComponent : public Component {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // degrees
    glm::vec3 scale{1.0f};
    glm::vec3 velocity{0.0f}; // world units per second (used by MovementSystem)
};

/** Health component – for future combat */
struct HealthComponent : public Component {
    int max_hp = 100;
    int current_hp = 100;
};

/** Renderable component – holds mesh and texture handles */
struct RenderableComponent : public Component {
    aom::MeshHandle mesh = aom::INVALID_MESH;
    aom::TextureHandle texture = aom::INVALID_TEXTURE;
};

/** Base class for systems */
class System {
public:
    virtual ~System() = default;
    virtual void update(float dt) = 0;
};

/** MovementSystem – updates Transform.position using Transform.velocity */
class MovementSystem : public System {
public:
    explicit MovementSystem(std::unordered_map<Entity, TransformComponent>& transforms)
        : transforms_(transforms) {}
    void update(float dt) override {
        for (auto& kv : transforms_) {
            TransformComponent& tr = kv.second;
            tr.position += tr.velocity * dt;
        }
    }
private:
    std::unordered_map<Entity, TransformComponent>& transforms_;
};

/** RenderSystem – draws all entities that have both Transform and Renderable */
class RenderSystem : public System {
public:
    RenderSystem(std::unordered_map<Entity, TransformComponent>& transforms,
                 std::unordered_map<Entity, RenderableComponent>& renderables,
                 IRenderer* renderer)
        : transforms_(transforms), renderables_(renderables), renderer_(renderer) {}
    void update(float dt) override {
        // dt not used for static rendering, kept for interface compatibility
        for (auto& kv : renderables_) {
            Entity e = kv.first;
            auto it = transforms_.find(e);
            if (it == transforms_.end()) continue;
            const TransformComponent& tr = it->second;
            const RenderableComponent& rc = kv.second;
            if (rc.mesh == aom::INVALID_MESH) continue;
            // Build model matrix from position/rotation/scale
            glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.x), glm::vec3(1,0,0));
            model = glm::rotate(model, glm::radians(tr.rotation.y), glm::vec3(0,1,0));
            model = glm::rotate(model, glm::radians(tr.rotation.z), glm::vec3(0,0,1));
            model = glm::scale(model, tr.scale);
            renderer_->draw_mesh(rc.mesh, model, rc.texture, aom::BlendMode::OPAQUE);
        }
    }
private:
    std::unordered_map<Entity, TransformComponent>& transforms_;
    std::unordered_map<Entity, RenderableComponent>& renderables_;
    IRenderer* renderer_ = nullptr;
};

/** ECSWorld – container for entities, components and systems */
class ECSWorld {
public:
    ECSWorld() = default;
    ~ECSWorld() = default;

    // Entity management ---------------------------------------------------
    Entity create_entity() {
        return ++next_id_;
    }
    void destroy_entity(Entity e) {
        transforms_.erase(e);
        healths_.erase(e);
        renderables_.erase(e);
    }

    // Component add / accessor --------------------------------------------
    TransformComponent& add_transform(Entity e) { return transforms_[e]; }
    HealthComponent&    add_health(Entity e)    { return healths_[e]; }
    RenderableComponent& add_renderable(Entity e){ return renderables_[e]; }


    // System registration -------------------------------------------------
    template <typename T, typename... Args>
    T* add_system(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = static_cast<T*>(sys.get());
        systems_.push_back(std::move(sys));
        return ptr;
    }

    // Main update --------------------------------------------------------
    void update(float dt) {
        for (auto& sys : systems_) {
            sys->update(dt);
        }
    }

    // Component storage accessors (read‑only) ---------------------------
    const std::unordered_map<Entity, TransformComponent>& transforms() const { return transforms_; }
    const std::unordered_map<Entity, RenderableComponent>& renderables() const { return renderables_; }

private:
    Entity next_id_ = 0;
    std::unordered_map<Entity, TransformComponent> transforms_;
    std::unordered_map<Entity, HealthComponent> healths_;
    std::unordered_map<Entity, RenderableComponent> renderables_;
    std::vector<std::unique_ptr<System>> systems_;
};

} // namespace aom
