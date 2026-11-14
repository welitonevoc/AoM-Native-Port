#include "engine/engine.h"
#include "engine_internal.h"
#include "ui/ui.h"
#include <algorithm>
#include <cmath>

namespace aom {

void Engine::handle_touch(const TouchEvent& touch) {
    // Cinematic: qualquer toque pula diretamente para o menu
    if (state_ == EngineState::CINEMATIC) {
        if (touch.state == TouchEvent::State::DOWN) {
            cinematic_skip_requested_ = true;
        }
        return;
    }

    if (state_ == EngineState::SPLASH) {
        if (touch.state == TouchEvent::State::DOWN) {
            set_state(EngineState::LOADING);
        }
        return;
    }

    // Forward to UI first
    if (ui_ && ui_->handle_touch(touch)) {
        return;
    }

    // In-game touch handling (Unit selection, movement orders, placement, camera pan)
    if (state_ == EngineState::IN_GAME) {
        const float vw = static_cast<float>(config_.window_width);
        const float vh = static_cast<float>(config_.window_height);

        // Ignore 3D world raycast if tapping inside HUD top banner or bottom selection panel
        if (touch.y <= 38.0f || touch.y >= vh - 96.0f) {
            return;
        }

        // Raycast touch coordinates to 3D ground plane (y = 0)
        float ndc_x = (2.0f * touch.x) / vw - 1.0f;
        float ndc_y = 1.0f - (2.0f * touch.y) / vh;
        glm::mat4 inv_vp = glm::inverse(camera_.projection_matrix() * camera_.view_matrix());
        glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
        near_pt /= (std::abs(near_pt.w) > 0.0001f ? near_pt.w : 1.0f);
        glm::vec4 far_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
        far_pt /= (std::abs(far_pt.w) > 0.0001f ? far_pt.w : 1.0f);

        glm::vec3 ray_origin = glm::vec3(near_pt);
        glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt - near_pt));

        glm::vec3 ground_hit{0.0f};
        bool hit_ground = false;
        if (std::abs(ray_dir.y) > 0.0001f) {
            float t = -ray_origin.y / ray_dir.y;
            if (t > 0.0f) {
                ground_hit = ray_origin + ray_dir * t;
                hit_ground = true;
            }
        }

        if (touch.state == TouchEvent::State::DOWN) {
            dragging_ = false;
            box_selecting_ = false;
            box_start_ = {touch.x, touch.y};
            box_current_ = {touch.x, touch.y};
            last_touch_ = {touch.x, touch.y};
            touch_down_pos_ = {touch.x, touch.y};
        } else if (touch.state == TouchEvent::State::MOVE) {
            float dx = touch.x - last_touch_.x;
            float dy = touch.y - last_touch_.y;
            float total_dx = touch.x - touch_down_pos_.x;
            float total_dy = touch.y - touch_down_pos_.y;
            float dist = std::sqrt(total_dx * total_dx + total_dy * total_dy);
            box_current_ = {touch.x, touch.y};

            if (dist > 14.0f) {
                dragging_ = true;
                box_selecting_ = true;
            }

            if (dragging_ && box_selecting_) {
                // Box selection: update the second corner of the selection rectangle
                // (no camera pan while box-selecting)
                last_touch_ = {touch.x, touch.y};
            } else if (dragging_) {
                // Pan RTS camera along ground plane
                camera_.target.x -= dx * 0.02f;
                camera_.target.z -= dy * 0.02f;
                camera_.target.x = std::clamp(camera_.target.x, -20.0f, 20.0f);
                camera_.target.z = std::clamp(camera_.target.z, -20.0f, 20.0f);
                last_touch_ = {touch.x, touch.y};
            }
        } else if (touch.state == TouchEvent::State::UP) {
            if (box_selecting_ && hit_ground && gameplay_world_) {
                // Box selection complete: convert screen corners to world coordinates
                // and select all friendly units inside the rectangle.
                auto raycast_ground = [&](float sx, float sy) -> glm::vec3 {
                    float ndc_x = (2.0f * sx) / vw - 1.0f;
                    float ndc_y = 1.0f - (2.0f * sy) / vh;
                    glm::mat4 inv_vp = glm::inverse(camera_.projection_matrix() * camera_.view_matrix());
                    glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
                    near_pt /= (std::abs(near_pt.w) > 0.0001f ? near_pt.w : 1.0f);
                    glm::vec4 far_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
                    far_pt /= (std::abs(far_pt.w) > 0.0001f ? far_pt.w : 1.0f);
                    glm::vec3 ro = glm::vec3(near_pt);
                    glm::vec3 rd = glm::normalize(glm::vec3(far_pt - near_pt));
                    if (std::abs(rd.y) > 0.0001f) {
                        float t = -ro.y / rd.y;
                        if (t > 0.0f) return ro + rd * t;
                    }
                    return glm::vec3(0.0f);
                };

                glm::vec3 p1 = raycast_ground(box_start_.x, box_start_.y);
                glm::vec3 p2 = raycast_ground(box_current_.x, box_current_.y);
                gameplay_world_->select_box(p1, p2);
                LOGI("Box selection: %zu units selected", gameplay_world_->selected_count());
                box_selecting_ = false;
                dragging_ = false;
            } else if (!dragging_ && hit_ground && gameplay_world_) {
                if (placement_mode_ != PlacementMode::NONE) {
                    Entity sel = gameplay_world_->selected_entity();
                    if (placement_mode_ == PlacementMode::HOUSE) {
                        if (sel != INVALID_ENTITY) {
                            gameplay_world_->order_worker_build(sel, BuildingType::HOUSE, ground_hit);
                        } else {
                            if (gameplay_world_->deduct_resources(0, 100, 0, 0)) {
                                gameplay_world_->place_building(Team::FRIENDLY, ground_hit, 5.0f, BuildingType::HOUSE, "Casa");
                            }
                        }
                    } else if (placement_mode_ == PlacementMode::BARRACKS) {
                        if (sel != INVALID_ENTITY) {
                            gameplay_world_->order_worker_build(sel, BuildingType::BARRACKS, ground_hit);
                        } else {
                            if (gameplay_world_->deduct_resources(0, 100, 50, 0)) {
                                gameplay_world_->place_building(Team::FRIENDLY, ground_hit, 8.0f, BuildingType::BARRACKS, "Quartel Militar");
                            }
                        }
                    } else if (placement_mode_ == PlacementMode::TOWN_CENTER) {
                        if (sel != INVALID_ENTITY) {
                            gameplay_world_->order_worker_build(sel, BuildingType::TOWN_CENTER, ground_hit);
                        } else {
                            if (gameplay_world_->deduct_resources(0, 400, 200, 0)) {
                                gameplay_world_->place_building(Team::FRIENDLY, ground_hit, 10.0f, BuildingType::TOWN_CENTER, "Centro da Cidade");
                            }
                        }
                    }
                    placement_mode_ = PlacementMode::NONE;
                } else {
                    // RTS Command Dispatch
                    Entity enemy = gameplay_world_->enemy_at(ground_hit, 2.2f);
                    if (enemy != INVALID_ENTITY && gameplay_world_->selected_count() > 0) {
                        gameplay_world_->order_attack_selected(enemy);
                        LOGI("Ordered attack on enemy %u", enemy);
                        if (audio_ && !gameplay_world_->selected_entities().empty()) {
                            Entity fsel = *gameplay_world_->selected_entities().begin();
                            const auto* u = gameplay_world_->get_unit(fsel);
                            const auto* t = gameplay_world_->get_transform(fsel);
                            glm::vec3 pos = t ? t->position : ground_hit;
                            std::string uname = u ? u->unit_type_name : "greek";
                            if (!play_sound_bank(uname, "attack", pos, u ? u->worker : false)) {
                                audio_->play_unit_voice_3d("attack", pos, uname, u ? u->worker : false);
                            }
                        }
                    } else {
                        // Check if a worker is selected and tapped a resource or construction site
                        bool worker_ordered = false;
                        for (Entity e : gameplay_world_->selected_entities()) {
                            const auto* u = gameplay_world_->get_unit(e);
                            if (u && u->worker) {
                                if (gameplay_world_->order_gather(e, ground_hit)) {
                                    LOGI("Worker %u ordered to gather resource", e);
                                    if (audio_) {
                                        const auto* t = gameplay_world_->get_transform(e);
                                        glm::vec3 pos = t ? t->position : ground_hit;
                                        if (!play_sound_bank("greek", "move", pos, true)) {
                                            audio_->play_unit_voice_3d("move", pos, "greek", true);
                                        }
                                    }
                                    worker_ordered = true;
                                    break;
                                }
                                Entity bld = gameplay_world_->building_at(ground_hit, 2.5f);
                                if (bld != INVALID_ENTITY) {
                                    const auto* b_comp = gameplay_world_->get_building(bld);
                                    if (b_comp && !b_comp->complete) {
                                        gameplay_world_->order_build(e, bld);
                                        LOGI("Worker %u ordered to construct building %u", e, bld);
                                        if (audio_) {
                                            const auto* t = gameplay_world_->get_transform(e);
                                            glm::vec3 pos = t ? t->position : ground_hit;
                                            if (!play_sound_bank("greek", "move", pos, true)) {
                                                audio_->play_unit_voice_3d("move", pos, "greek", true);
                                            }
                                        }
                                        worker_ordered = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!worker_ordered) {
                            // Try selecting friendly unit or building
                            Entity tapped = gameplay_world_->select_at(ground_hit);
                            if (tapped != INVALID_ENTITY) {
                                LOGI("Selected entity %u at (%.1f, %.1f)", tapped, ground_hit.x, ground_hit.z);
                                if (audio_) {
                                    const auto* u = gameplay_world_->get_unit(tapped);
                                    const auto* t = gameplay_world_->get_transform(tapped);
                                    glm::vec3 pos = t ? t->position : ground_hit;
                                    if (u && u->team == Team::FRIENDLY) {
                                        if (!play_sound_bank(u->unit_type_name, "select", pos, u->worker)) {
                                            audio_->play_unit_voice_3d("select", pos, u->unit_type_name, u->worker);
                                        }
                                    }
                                }
                            } else if (gameplay_world_->selected_count() > 0) {
                                // Move order on empty ground
                                gameplay_world_->order_move_selected(ground_hit);
                                LOGI("Ordered move to (%.1f, %.1f)", ground_hit.x, ground_hit.z);
                                if (audio_) {
                                    Entity fsel = *gameplay_world_->selected_entities().begin();
                                    const auto* u = gameplay_world_->get_unit(fsel);
                                    const auto* t = gameplay_world_->get_transform(fsel);
                                    glm::vec3 pos = t ? t->position : ground_hit;
                                    std::string uname = u ? u->unit_type_name : "greek";
                                    if (!play_sound_bank(uname, "move", pos, u ? u->worker : false)) {
                                        audio_->play_unit_voice_3d("move", pos, uname, u ? u->worker : false);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            dragging_ = false;
        }
    }
}

void Engine::adjust_camera_zoom(float delta) {
    if (state_ != EngineState::IN_GAME) return;
    radius_ = std::clamp(radius_ - delta, 3.0f, 40.0f);
}

} // namespace aom
