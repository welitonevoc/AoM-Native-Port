//===========================================================================//
//  Implementações do GameplayWorld - parte das definições do header.
//  As definições inline longas que podem ser usadas pelos testes são colocadas
//  aqui para evitar código não utilizado durante a compilação de fontes.
//===========================================================================//

#include "ecs/gameplay.h"
#include <algorithm>
#include <numeric>

namespace aom {

// Implementação inline de métodos auxiliares comuns

// Implementação de update_animations (já está no header mas precisamos
// de uma definição para o linker)
void GameplayWorld::update_animations(float dt) {
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

// Implementação de outros métodos auxiliares para testes

void GameplayWorld::attach_animation(Entity entity, const aom::anim::BrgAnimationClip* clip,
                                       TextureHandle texture, IRenderer* renderer) {
    if (!clip || !clip->is_valid() || !renderer) return;
    // Implementation would go here
}

void GameplayWorld::set_default_animation(const aom::anim::BrgAnimationClip* clip, TextureHandle texture) {
    default_clip_ = clip;
    default_clip_tex_ = texture;
}

void GameplayWorld::set_render_hp_bars(bool enable) { render_hp_bars_ = enable; }
bool GameplayWorld::render_hp_bars() const { return render_hp_bars_; }

// Implementação de funções utilitárias

bool GameplayWorld::has_entity(Entity id) const {
    return animations_.count(id) || grn_animations_.count(id) || units_.count(id) ||
           building_components_.count(id) || selections_.count(id);
}

void GameplayWorld::cleanup_entity(Entity id) {
    animations_.erase(id);
    grn_animations_.erase(id);
    units_.erase(id);
    building_components_.erase(id);
    selections_.erase(id);
    ecs_world_.destroy_entity(id);
}

// Implementação de funções de validação e helper para testes

bool GameplayWorld::has_background_scenario() const {
    return !units_.empty() || !building_components_.empty();
}

Entity GameplayWorld::get_nearest_town_center(Team team, const glm::vec3& position) const {
    Entity nearest = INVALID_ENTITY;
    float min_dist = std::numeric_limits<float>::max();
    for (const auto& [id, building] : building_components_) {
        if (building.type == BuildingType::TOWN_CENTER && building.team == team) {
            const auto& tr = ecs_world_.transforms().at(id);
            float dist = glm::distance(tr.position, position);
            if (dist < min_dist) {
                min_dist = dist;
                nearest = id;
            }
        }
    }
    return nearest;
}

} // namespace aom
