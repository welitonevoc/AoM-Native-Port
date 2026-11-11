#include "ecs/ecs.h"
#include "renderer/renderer.h"

namespace aom {

void RenderSystem::update(float dt) {
    (void)dt;
    for (auto& kv : renderables_) {
        Entity e = kv.first;
        auto it = transforms_.find(e);
        if (it == transforms_.end()) continue;
        const TransformComponent& tr = it->second;
        const RenderableComponent& rc = kv.second;
        if (rc.mesh == aom::INVALID_MESH) continue;
        // Build model matrix from position/rotation/scale
        glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
        model = glm::rotate(model, glm::radians(tr.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(tr.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(tr.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, tr.scale);
        renderer_->draw_mesh(rc.mesh, model, rc.texture, aom::BlendMode::OPAQUE);
    }
}

} // namespace aom