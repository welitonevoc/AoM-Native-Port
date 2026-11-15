#include "engine/engine.h"
#include "engine_internal.h"
#include "ui/ui.h"
#include "ui/ui_layout.h"
#include "ecs/gameplay.h"
#include "campaign/campaign_mission.h"
#include <algorithm>
#include <string>
#include <vector>

namespace aom {

void Engine::create_in_game_hud() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // ── P5.6: Tenta carregar commandpanel.xmb para o painel de comandos ──
    create_command_panel_from_xmb();

    // ── 1. Top Resource Banner (y = 8..36) ──────────────────────────
    if (show_fps_) {
        fps_label_ = ui_->add_label("FPS: 60", {16.0f, 9.0f}, 16.0f, 0xFF88FF88);
    } else {
        fps_label_ = nullptr;
    }

    float res_x = 110.0f;
    const float res_y = 9.0f;
    const float res_step = std::min(150.0f, (vw - 320.0f) / 5.0f);
    res_x += res_step;
    hud_pop_label_ = ui_->add_label("Pop: 0/25", {res_x, res_y}, 16.0f, 0xFFFFFFFF);

    // Menu button at top-right
    ui_->add_button({vw - 110.0f, 5.0f}, {96.0f, 26.0f}, "MENU",
                    [this]() { set_state(EngineState::MAIN_MENU); }, 0xAA221105, 0xAA44220A);

    // ── 2. Bottom RTS Selection & Action Panel (y = vh - 96) ──────────
    const float panel_y = vh - 96.0f;
    hud_sel_title_ = ui_->add_label("Toque em uma unidade para selecionar", {160.0f, panel_y + 8.0f}, 17.0f, 0xFFFFD000);
    hud_sel_hp_    = ui_->add_label("", {160.0f, panel_y + 32.0f}, 15.0f, 0xFFFFFFFF);
    hud_sel_info_  = ui_->add_label("", {160.0f, panel_y + 54.0f}, 14.0f, 0xFF88DDFF);

    // 4 Action Buttons on the bottom-right
    const float btn_w = 140.0f;
    const float btn_h = 36.0f;
    const float btn_gap = 8.0f;
    float btn_start_x = std::max(520.0f, vw - (btn_w * 2.0f + btn_gap + 16.0f));

    // Inicializa botões de ação com elementos ocultos padrão
    hud_action_btn1_ = ui_->add_button({btn_start_x, panel_y + 8.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn2_ = ui_->add_button({btn_start_x + btn_w + btn_gap, panel_y + 8.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn3_ = ui_->add_button({btn_start_x, panel_y + 8.0f + btn_h + 4.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn4_ = ui_->add_button({btn_start_x + btn_w + btn_gap, panel_y + 8.0f + btn_h + 4.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);

    // Tooltip dinâmico (custo/descricao do comando), exibido abaixo do painel de selecao.
    hud_tooltip_ = ui_->add_label("", {160.0f, panel_y + 78.0f}, 13.0f, 0xFF88DDFF);

    // ── 3. Campaign & Mission HUD references ──────────────────────────
    hud_obj_title_        = ui_->add_label("", {vw * 0.5f - 240.0f, 38.0f}, 16.0f, 0xFFFFE066);
    hud_reinforce_label_  = ui_->add_label("", {vw - 230.0f, 38.0f}, 15.0f, 0xFF88DDFF);
    hud_subtitle_speaker_ = ui_->add_label("", {vw * 0.5f - 260.0f, vh - 138.0f}, 16.0f, 0xFFFFD700);
    hud_subtitle_text_    = ui_->add_label("", {vw * 0.5f - 260.0f, vh - 114.0f}, 14.0f, 0xFFFFFFFF);
    // Botão skip cinemático inicialmente oculto, será configurado via callback
    hud_skip_btn_         = ui_->add_button({vw * 0.5f - 90.0f, vh - 5.0f}, {180.0f, 36.0f}, "", nullptr, 0x00000000, 0x00000000);

    last_selected_entity_ = INVALID_ENTITY;
}

void Engine::update_in_game_hud(float dt) {
    (void)dt;
    if (!gameplay_world_ || state_ != EngineState::IN_GAME) return;

    // 1. Update Top Banner: Food, Wood, Gold, Favor, Pop/Cap
    const auto& res = gameplay_world_->resources();
    int cur_pop = gameplay_world_->current_population(Team::FRIENDLY);
    int max_pop = gameplay_world_->population_cap(Team::FRIENDLY);

    char buf[64];
    if (hud_food_label_) {
        std::snprintf(buf, sizeof(buf), "Comida: %d", res.food);
        hud_food_label_->text = buf;
    }
    if (hud_wood_label_) {
        std::snprintf(buf, sizeof(buf), "Madeira: %d", res.wood);
        hud_wood_label_->text = buf;
    }
    if (hud_gold_label_) {
        std::snprintf(buf, sizeof(buf), "Ouro: %d", res.gold);
        hud_gold_label_->text = buf;
    }
    if (hud_favor_label_) {
        std::snprintf(buf, sizeof(buf), "Favor: %d", res.favor);
        hud_favor_label_->text = buf;
    }
    if (hud_pop_label_) {
        std::snprintf(buf, sizeof(buf), "Pop: %d/%d", cur_pop, max_pop);
        hud_pop_label_->text = buf;
        hud_pop_label_->color = (cur_pop >= max_pop) ? 0xFFFF4444 : 0xFFFFFFFF;
    }

    // 2. Selection & Action Panel
    Entity sel = gameplay_world_->selected_entity();
    size_t sel_count = gameplay_world_->selected_count();

    auto hide_action_buttons = [this]() {
        if (hud_action_btn1_) { hud_action_btn1_->label = ""; hud_action_btn1_->normal_color = 0x00000000; }
        if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->normal_color = 0x00000000; }
        if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->normal_color = 0x00000000; }
        if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->normal_color = 0x00000000; }
    };

    if (sel == INVALID_ENTITY) {
        if (hud_sel_title_) hud_sel_title_->text = "Nenhuma unidade selecionada";
        if (hud_sel_hp_) hud_sel_hp_->text = "";
        if (hud_sel_info_) hud_sel_info_->text = (placement_mode_ != PlacementMode::NONE) ? "Toque no terreno para construir!" : "";
        hide_action_buttons();
        last_selected_entity_ = INVALID_ENTITY;
        return;
    }

    // Quando múltiplas estao selecionadas, mostra contagem no titulo
    std::string sel_prefix;
    if (sel_count > 1) {
        std::snprintf(buf, sizeof(buf), "%zu selecionadas", sel_count);
        sel_prefix = buf;
    }

    const auto* unit = gameplay_world_->get_unit(sel);
    const auto* bld  = gameplay_world_->get_building(sel);
    const auto* hp   = gameplay_world_->get_health(sel);

    if (unit) {
        if (hud_sel_title_) {
            std::string team_str = (unit->team == Team::FRIENDLY) ? " (Aliado)" : " (Inimigo)";
            if (!sel_prefix.empty()) {
                hud_sel_title_->text = sel_prefix + "  [" + unit->unit_type_name + team_str + "]";
                hud_sel_title_->color = (unit->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
            } else {
                hud_sel_title_->text = unit->unit_type_name + team_str;
                hud_sel_title_->color = (unit->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
            }
        }
        if (hud_sel_hp_ && hp) {
            std::snprintf(buf, sizeof(buf), "HP: %d / %d", hp->current_hp, hp->max_hp);
            hud_sel_hp_->text = buf;
        }
        if (hud_sel_info_) {
            if (unit->worker) {
                if (unit->returning_cargo) {
                    const char* rname = (unit->carried_resource_type == ResourceType::FOOD) ? "Comida" :
                                        (unit->carried_resource_type == ResourceType::WOOD) ? "Madeira" :
                                        (unit->carried_resource_type == ResourceType::GOLD) ? "Ouro" : "Favor";
                    std::snprintf(buf, sizeof(buf), "Depositando: %d/10 %s no Centro", unit->carried_resource_amount, rname);
                    hud_sel_info_->text = buf;
                } else if (unit->carried_resource_amount > 0) {
                    const char* rname = (unit->carried_resource_type == ResourceType::FOOD) ? "Comida" :
                                        (unit->carried_resource_type == ResourceType::WOOD) ? "Madeira" :
                                        (unit->carried_resource_type == ResourceType::GOLD) ? "Ouro" : "Favor";
                    std::snprintf(buf, sizeof(buf), "Carga: %d/10 %s", unit->carried_resource_amount, rname);
                    hud_sel_info_->text = buf;
                } else if (unit->gather_node >= 0) {
                    hud_sel_info_->text = "Coletando recurso...";
                } else if (unit->build_target != INVALID_ENTITY) {
                    hud_sel_info_->text = "Construindo estrutura...";
                } else {
                    hud_sel_info_->text = "Trabalhador ocioso";
                }
            } else {
                std::snprintf(buf, sizeof(buf), "Ataque: %.0f  Alcance: %.1f", unit->attack_damage, unit->attack_range);
                hud_sel_info_->text = buf;
            }
        }

        // Action Buttons for Worker
        if (unit->team == Team::FRIENDLY) {
            if (unit->worker) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Casa (100M)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this]() {
                        placement_mode_ = PlacementMode::HOUSE;
                        LOGI("Placement mode: HOUSE");
                    };
                }
                if (hud_action_btn2_) {
                    hud_action_btn2_->label = "Quartel (100M 50O)";
                    hud_action_btn2_->normal_color = 0xDD2A1C0E;
                    hud_action_btn2_->onClick = [this]() {
                        placement_mode_ = PlacementMode::BARRACKS;
                        LOGI("Placement mode: BARRACKS");
                    };
                }
                if (hud_action_btn3_) {
                    hud_action_btn3_->label = "Parar";
                    hud_action_btn3_->normal_color = 0xDD3A1C0E;
                    hud_action_btn3_->onClick = [this]() {
                        if (gameplay_world_) gameplay_world_->order_stop_selected();
                    };
                }
                if (hud_action_btn4_) {
                    hud_action_btn4_->label = "Centro (400M 200O)";
                    hud_action_btn4_->normal_color = 0xDD2A1C0E;
                    hud_action_btn4_->onClick = [this]() {
                        placement_mode_ = PlacementMode::TOWN_CENTER;
                        LOGI("Placement mode: TOWN_CENTER");
                    };
                }
            } else {
                // Military unit
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Parar";
                    hud_action_btn1_->normal_color = 0xDD3A1C0E;
                    hud_action_btn1_->onClick = [this]() {
                        if (gameplay_world_) gameplay_world_->order_stop_selected();
                    };
                }
                if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->normal_color = 0x00000000; }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->normal_color = 0x00000000; }
            }
        } else {
            hide_action_buttons();
        }
    } else if (bld) {
        if (hud_sel_title_) {
            if (!sel_prefix.empty()) {
                hud_sel_title_->text = sel_prefix + "  [" + bld->name + "]";
                hud_sel_title_->color = (bld->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
            } else {
                hud_sel_title_->text = bld->name + ((bld->team == Team::FRIENDLY) ? " (Aliado)" : " (Inimigo)");
                hud_sel_title_->color = (bld->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
            }
        }
        if (hud_sel_hp_ && hp) {
            std::snprintf(buf, sizeof(buf), "HP: %d / %d", hp->current_hp, hp->max_hp);
            hud_sel_hp_->text = buf;
        }
        if (hud_sel_info_) {
            if (!bld->complete) {
                std::snprintf(buf, sizeof(buf), "Construcao: %.0f%%", bld->progress * 100.0f);
                hud_sel_info_->text = buf;
            } else if (bld->type == BuildingType::TOWN_CENTER) {
                int q_count = gameplay_world_->get_training_queue_count(sel);
                if (q_count > 0) {
                    float prog = gameplay_world_->get_training_progress(sel);
                    std::snprintf(buf, sizeof(buf), "Treinando: Aldeao (%d na fila, %.0f%%)", q_count, prog * 100.0f);
                    hud_sel_info_->text = buf;
                } else {
                    hud_sel_info_->text = "Fila livre (+15 Populacao)";
                }
            } else if (bld->type == BuildingType::BARRACKS) {
                int q_count = gameplay_world_->get_training_queue_count(sel);
                if (q_count > 0) {
                    float prog = gameplay_world_->get_training_progress(sel);
                    std::snprintf(buf, sizeof(buf), "Treinando: %s (%d fila, %.0f%%)", bld->training_unit.c_str(), q_count, prog * 100.0f);
                    hud_sel_info_->text = buf;
                } else {
                    hud_sel_info_->text = "Quartel pronto para producao";
                }
            } else if (bld->type == BuildingType::HOUSE) {
                hud_sel_info_->text = "Suporta +10 de Populacao";
            } else {
                hud_sel_info_->text = "";
            }
        }

        if (bld->team == Team::FRIENDLY && bld->complete) {
            if (bld->type == BuildingType::TOWN_CENTER) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Aldeao (50C)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            if (gameplay_world_->queue_villager(sel)) {
                                LOGI("Queued Greek Villager at Town Center %u", sel);
                            } else {
                                LOGW("Cannot queue villager: insufficient food or pop cap reached");
                            }
                        }
                    };
                }
                if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->normal_color = 0x00000000; }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->normal_color = 0x00000000; }
            } else if (bld->type == BuildingType::BARRACKS) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Hoplite (60C 35O)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            gameplay_world_->start_training_unit(sel, "Hoplite", 60, 0, 35, 2);
                        }
                    };
                }
                if (hud_action_btn2_) {
                    hud_action_btn2_->label = "Toxotes (55M 35O)";
                    hud_action_btn2_->normal_color = 0xDD2A1C0E;
                    hud_action_btn2_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            gameplay_world_->start_training_unit(sel, "Toxotes", 0, 55, 35, 2);
                        }
                    };
                }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->normal_color = 0x00000000; }
            } else {
                hide_action_buttons();
            }
        } else {
            hide_action_buttons();
        }
    } else {
        if (hud_sel_title_) hud_sel_title_->text = "Nenhuma unidade selecionada";
        if (hud_sel_hp_) hud_sel_hp_->text = "";
        if (hud_sel_info_) hud_sel_info_->text = "";
        hide_action_buttons();
    }

    // ── Tooltip dinamico (custo/descricao do comando principal) ─────────
    if (hud_tooltip_) {
        std::string tip;
        if (sel != INVALID_ENTITY) {
            const auto* unit = gameplay_world_->get_unit(sel);
            const auto* bld  = gameplay_world_->get_building(sel);
            if (unit && unit->team == Team::FRIENDLY && unit->worker) {
                tip = "Casa: 100 Madeira (+10 Pop)  |  Quartel: 100M 50O  |  Parar: cancela ordem";
            } else if (bld && bld->team == Team::FRIENDLY && bld->complete) {
                if (bld->type == BuildingType::TOWN_CENTER) {
                    tip = "Aldeano: 50 Comida  (+1 Populacao)";
                } else if (bld->type == BuildingType::BARRACKS) {
                    tip = "Hoplite: 60C 35O  |  Toxotes: 55M 35O";
                }
            }
        }
        hud_tooltip_->text = tip;
    }

    // ── 3. Update Campaign Mission HUD (Objectives, Subtitles, Reinforcements, Skip button)
    if (mission_mgr_) {
        // Skip Button
        if (hud_skip_btn_) {
            if (mission_mgr_->is_cinematic_active()) {
                hud_skip_btn_->label = "PULAR (SKIP)";
                hud_skip_btn_->normal_color = 0xBB2A4010;
                hud_skip_btn_->pressed_color = 0xEE406018;
                hud_skip_btn_->onClick = [this]() {
                    if (mission_mgr_) mission_mgr_->skip_cinematic(&camera_);
                };
            } else {
                hud_skip_btn_->label = "";
                hud_skip_btn_->normal_color = 0x00000000;
                hud_skip_btn_->pressed_color = 0x00000000;
                hud_skip_btn_->onClick = nullptr;
            }
        }

        // Subtitles
        const auto* sub = mission_mgr_->current_subtitle();
        if (sub) {
            if (hud_subtitle_speaker_) {
                hud_subtitle_speaker_->text = sub->speaker + ":";
                hud_subtitle_speaker_->color = sub->color;
            }
            if (hud_subtitle_text_) {
                hud_subtitle_text_->text = sub->text;
            }
        } else {
            if (hud_subtitle_speaker_) hud_subtitle_speaker_->text = "";
            if (hud_subtitle_text_) hud_subtitle_text_->text = "";
        }

        // Objectives
        if (hud_obj_title_) {
            const auto& objs = mission_mgr_->objectives();
            std::string obj_text;
            for (const auto& obj : objs) {
                if (!obj.completed && obj.primary) {
                    obj_text = "Objetivo: " + obj.description;
                    break;
                }
            }
            if (obj_text.empty() && !objs.empty()) {
                obj_text = "Objetivos Cumpridos!";
            }
            hud_obj_title_->text = obj_text;
        }

        // Reinforcement Countdown
        if (hud_reinforce_label_) {
            if (mission_mgr_->stage() == MissionStage::REINFORCEMENTS_ARRIVED) {
                hud_reinforce_label_->text = "Reforcos: CHEGARAM!";
                hud_reinforce_label_->color = 0xFF76FF03;
            } else {
                int leagues = mission_mgr_->army_leagues_away();
                hud_reinforce_label_->text = "Exercito: " + std::to_string(leagues) + " leguas";
                hud_reinforce_label_->color = 0xFF88DDFF;
            }
        }

        // Victory / Defeat Notice
        if (mission_mgr_->result() == MissionResult::VICTORY) {
            if (hud_sel_title_) {
                hud_sel_title_->text = "VITORIA! ATLANTIDA DEFENDIDA COM SUCESSO!";
                hud_sel_title_->color = 0xFF76FF03;
            }
        } else if (mission_mgr_->result() == MissionResult::DEFEAT) {
            if (hud_sel_title_) {
                hud_sel_title_->text = "DERROTA! ARKANTOS TOMBOU EM BATALHA!";
                hud_sel_title_->color = 0xFFFF3333;
            }
        }
    }
}

Entity Engine::transform_unit(Entity from_entity, const std::string& target_name) {
    if (gameplay_world_) return gameplay_world_->transform_unit(from_entity, target_name);
    return INVALID_ENTITY;
}

std::vector<UnitTransform> Engine::available_transforms(Entity entity) const {
    if (gameplay_world_) return gameplay_world_->available_transforms(entity);
    return {};
}

} // namespace aom
