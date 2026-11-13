#include "engine/engine.h"
#include "engine_internal.h"
#include "audio/audio.h"
#include "ui/ui.h"
#include "ecs/gameplay.h"
#include <vector>

namespace aom {

void Engine::set_state(EngineState new_state) {
    state_ = new_state;
    state_timer_ = 0.0f;
    cinematic_skip_requested_ = false;

    // Limpeza total de UI em TODA transição de estado
    // Evita elementos residuais de menus anteriores ("camadas fantasmas")
    clear_ui();

    switch (new_state) {
        case EngineState::CINEMATIC:
            LOGI("Entered state: CINEMATIC");
            clear_ui();
            // Allocate a placeholder texture that will be updated each frame.
            // 640x480 RGBA matches the BIK native resolution.
            if (cinematic_frame_tex_ == INVALID_TEXTURE) {
                std::vector<uint8_t> black(640 * 480 * 4, 0);
                cinematic_frame_tex_ = renderer_->create_texture(640, 480, black.data());
            }
            break;
        case EngineState::SPLASH:
            LOGI("Entered state: SPLASH");
            clear_ui();
            break;
        case EngineState::LOADING:
            LOGI("Entered state: LOADING");
            clear_ui();
            loading_progress_ = 0.0f;
            break;
        case EngineState::MAIN_MENU: {
            LOGI("Entered state: MAIN_MENU (uipregamenew + fundo 3D animado)");
            if (audio_) {
                audio_->play_music("menu_music.mp3", true);
            }
            
            // Desabilitar barras de HP no menu principal
            if (gameplay_world_) {
                gameplay_world_->set_render_hp_bars(false);
            }
            
            // ── 1. Carregar archives e recursos primeiro para que cenários e modelos existam ──
            if (!resources_loaded_) {
                LOGI("Carregando recursos de jogo para MAIN_MENU...");
                if (!gameplay_world_ || model_mesh_ == INVALID_MESH || !prt_particles_) {
                    load_archives(config_.data_path);
                    load_textures();
                    load_models();
                    load_game_config();
                }
                resources_loaded_ = true;
            }

            // ── 2. Carregar cenário de fundo 3D do menu (modo menu background) ──
            if (!gameplay_world_ || !gameplay_world_->has_background_scenario()) {
                start_generic_mission("~aom_pgs_atlantis.scx", true);
            }

            // ── 3. Garantir terreno e skydome 3D ativos para o menu animado ──
            if (terrain_mesh_ == INVALID_MESH) {
                generate_terrain();
            }
            if (skydome_mesh_ == INVALID_MESH) {
                generate_skydome();
            }

            // ── 4. Configurar câmera isométrica estilo The Titans ──────────────────
            radius_ = 32.0f;
            yaw_ = -0.35f;
            pitch_ = 0.65f;
            camera_.target = glm::vec3(-4.0f, 0.5f, -2.0f);
            
            // ── 5. Povoar cena 3D de fundo com elementos do The Titans ─────────────
            if (gameplay_world_) {
                populate_menu_background_scene();
            }

            // ── Tentativa de carregar layout XMB original do data.bar ──────────────
            // Ordem de preferência:
            // 1. Se data_path foi fornecido → carrega uipregamenew.xmb do data.bar (layout authentic AoM)
            // 2. Se não encontrar → usa menu fallback hardcoded (ainda funciona, mas sem arte original)
            bool layout_loaded = false;
            
            if (data_bar_) {
                LOGI("[DIAG] data_bar_ disponível, %zu arquivos no BAR", data_bar_->file_count());
                // Tenta carregar o layout autêntico de The Titans (uipregamenew.xmb)
                // Este XMB contém os 7 botões originais com posições e ações corretas
                layout_loaded = load_ui_layout("uipregamenew.xmb");
                
                if (layout_loaded) {
                    LOGI("[DIAG] Layout XMB authentic 'uipregamenew.xmb' carregado com sucesso");
                    size_t created = show_ui_layout("uipregamenew.xmb");
                    LOGI("[DIAG] show_ui_layout criou %zu elementos", created);
                } else {
                    LOGW("[DIAG] Layout XMB 'uipregamenew.xmb' NÃO encontrado no data.bar");
                    LOGW("[DIAG] Verifique se o arquivo existe no data.bar/data2.bar");
                    // Diagnóstico: lista entradas do data.bar que contenham "pregame" ou ".xmb"
                    LOGI("[DIAG] Entradas do data.bar contendo 'pregame' ou '.xmb':");
                    size_t diag_shown = 0;
                    data_bar_->for_each([&](const BarEntry& e) {
                        const std::string& fn = e.filename;
                        if (fn.find("pregame") != std::string::npos ||
                            fn.find(".xmb") != std::string::npos) {
                            LOGI("[DIAG]   '%s'", fn.c_str());
                            ++diag_shown;
                        }
                    });
                    if (diag_shown == 0) {
                        LOGW("[DIAG]   (nenhuma entrada encontrada)");
                    }
                    // Fallback: menu hardcoded ainda funciona
                    create_main_menu();
                }
            } else {
                LOGW("[DIAG] data_bar_ não disponível - usando menu fallback hardcoded");
                create_main_menu();
            }
            
            // Se somehow load_ui_layout retornou false mas show_ui_layout não foi chamado
            if (!layout_loaded) {
                // Já caiu no fallback acima, mas por segurança:
                if (!ui_->buttons().empty() && ui_->buttons()[0].label.empty()) {
                    // Botões foram criados pelo fallback hardcoded
                    LOGI("Menu fallback hardcoded ativo (7 botões básicos)");
                }
            }
            break;
        }
        case EngineState::OPTIONS:
            LOGI("Entered state: OPTIONS");
            create_options_menu();
            break;
        case EngineState::CREDITS:
            LOGI("Entered state: CREDITS");
            create_credits_menu();
            break;
        case EngineState::MULTIPLAYER_LAN:
            LOGI("Entered state: MULTIPLAYER_LAN");
            create_multiplayer_lan_menu();
            break;
        case EngineState::MULTIPLAYER_LOBBY:
            LOGI("Entered state: MULTIPLAYER_LOBBY");
            create_multiplayer_lobby_menu();
            break;
        case EngineState::IN_GAME:
            LOGI("Entered state: IN_GAME");
            create_in_game_hud();
            break;
        case EngineState::UI_AI_PROMPT:
            LOGI("Entered state: UI_AI_PROMPT");
            create_ai_prompt_menu();
            break;
        case EngineState::UI_ALTERNATES_DIALOG:
            LOGI("Entered state: UI_ALTERNATES_DIALOG");
            create_alternates_dialog_menu();
            break;
        case EngineState::UI_ANIMATION_DLG:
            LOGI("Entered state: UI_ANIMATION_DLG");
            create_animation_dlg_menu();
            break;
        case EngineState::UI_ARENA_SETUP:
            LOGI("Entered state: UI_ARENA_SETUP");
            create_arena_setup_menu();
            break;
        case EngineState::UI_ARMY_EDIT:
            LOGI("Entered state: UI_ARMY_EDIT");
            create_army_edit_menu();
            break;
        case EngineState::UI_AUTOPATCHER_DIALOG:
            LOGI("Entered state: UI_AUTOPATCHER_DIALOG");
            create_autopatcher_dialog_menu();
            break;
        case EngineState::UI_CAMERA_EDITOR:
            LOGI("Entered state: UI_CAMERA_EDITOR");
            create_camera_editor_menu();
            break;
        case EngineState::UI_CAMPAIGN:
            LOGI("Entered state: UI_CAMPAIGN");
            create_campaign_menu();
            break;
        case EngineState::UI_CAMPAIGN_EDITOR:
            LOGI("Entered state: UI_CAMPAIGN_EDITOR");
            create_campaign_editor_menu();
            break;
        case EngineState::UI_CAMPAIGN_NLOANDING:
            LOGI("Entered state: UI_CAMPAIGN_NLOANDING");
            create_campaign_nloanding_menu();
            break;
        case EngineState::UI_CAMPAIGN_SETUP:
            LOGI("Entered state: UI_CAMPAIGN_SETUP");
            create_campaign_setup_menu();
            break;
        case EngineState::UI_CHAT_DIALOG:
            LOGI("Entered state: UI_CHAT_DIALOG");
            create_chat_dialog_menu();
            break;
        case EngineState::UI_LIFF_DIALOG:
            LOGI("Entered state: UI_LIFF_DIALOG");
            create_liff_dialog_menu();
            break;
        case EngineState::UI_COMBAT_COMPARISON:
            LOGI("Entered state: UI_COMBAT_COMPARISON");
            create_combat_comparison_menu();
            break;
        case EngineState::UI_CUSTOMIZE_OPP:
            LOGI("Entered state: UI_CUSTOMIZE_OPP");
            create_customize_opp_menu();
            break;
        case EngineState::UI_DEFAULT_LOADING:
            LOGI("Entered state: UI_DEFAULT_LOADING");
            create_default_loading_menu();
            break;
        case EngineState::UI_EDITOR_MENU:
            LOGI("Entered state: UI_EDITOR_MENU");
            create_editor_menu();
            break;
        case EngineState::UI_EDITOR_PALETTES:
            LOGI("Entered state: UI_EDITOR_PALETTES");
            create_editor_palettes_menu();
            break;
        case EngineState::UI_EDIT_UNIT_DLG:
            LOGI("Entered state: UI_EDIT_UNIT_DLG");
            create_edit_unit_dlg_menu();
            break;
        case EngineState::UI_EULA:
            LOGI("Entered state: UI_EULA");
            create_eula_menu();
            break;
        case EngineState::UI_FAKE_TECHTREE:
            LOGI("Entered state: UI_FAKE_TECHTREE");
            create_fake_techtree_menu();
            break;
        case EngineState::UI_FILE_MANAGER:
            LOGI("Entered state: UI_FILE_MANAGER");
            create_file_manager_menu();
            break;
        case EngineState::UI_FRIENDS_DIALOGS:
            LOGI("Entered state: UI_FRIENDS_DIALOGS");
            create_friends_dialogs_menu();
            break;
        case EngineState::UI_GAME_LIST:
            LOGI("Entered state: UI_GAME_LIST");
            create_game_list_menu();
            break;
        case EngineState::UI_GAME_PALETTES:
            LOGI("Entered state: UI_GAME_PALETTES");
            create_game_palettes_menu();
            break;
        case EngineState::UI_GAME_SETTINGS_DIALOG:
            LOGI("Entered state: UI_GAME_SETTINGS_DIALOG");
            create_game_settings_dialog_menu();
            break;
        case EngineState::UI_GENERIC_ENTRY_DIALOG:
            LOGI("Entered state: UI_GENERIC_ENTRY_DIALOG");
            create_generic_entry_dialog_menu();
            break;
        case EngineState::UI_GOD_BUFFET_DIALOG:
            LOGI("Entered state: UI_GOD_BUFFET_DIALOG");
            create_god_buffet_dialog_menu();
            break;
        case EngineState::UI_GOD_PICKER:
            LOGI("Entered state: UI_GOD_PICKER");
            create_god_picker_menu();
            break;
        case EngineState::UI_HELP_DIALOG:
            LOGI("Entered state: UI_HELP_DIALOG");
            create_help_dialog_menu();
            break;
        case EngineState::UI_HOST_GAME_DIALOG:
            LOGI("Entered state: UI_HOST_GAME_DIALOG");
            create_host_game_dialog_menu();
            break;
        case EngineState::UI_HOTKEYS:
            LOGI("Entered state: UI_HOTKEYS");
            create_hotkeys_menu();
            break;
        case EngineState::UI_HOTKEY_SETUP:
            LOGI("Entered state: UI_HOTKEY_SETUP");
            create_hotkey_setup_menu();
            break;
        case EngineState::UI_IMAGE_DIALOG:
            LOGI("Entered state: UI_IMAGE_DIALOG");
            create_image_dialog_menu();
            break;
        case EngineState::UI_INGAME_MSG_1:
            LOGI("Entered state: UI_INGAME_MSG_1");
            create_ingame_msg_1_menu();
            break;
        case EngineState::UI_INGAME_MSG_2:
            LOGI("Entered state: UI_INGAME_MSG_2");
            create_ingame_msg_2_menu();
            break;
        case EngineState::UI_INGAME_MSG_3:
            LOGI("Entered state: UI_INGAME_MSG_3");
            create_ingame_msg_3_menu();
            break;
        case EngineState::UI_INGAME_MSG_DIALOG:
            LOGI("Entered state: UI_INGAME_MSG_DIALOG");
            create_ingame_msg_dialog_menu();
            break;
        case EngineState::UI_LAUNCH_SCREEN:
            LOGI("Entered state: UI_LAUNCH_SCREEN");
            create_launch_screen_menu();
            break;
        case EngineState::UI_LAYOUT:
            LOGI("Entered state: UI_LAYOUT");
            create_layout_menu();
            break;
        case EngineState::UI_LOW_DETAIL_BACKGROUND:
            LOGI("Entered state: UI_LOW_DETAIL_BACKGROUND");
            create_low_detail_background_menu();
            break;
        case EngineState::UI_MAIN:
            LOGI("Entered state: UI_MAIN");
            create_main_alt_menu();
            break;
        case EngineState::UI_MAP_PICKER:
            LOGI("Entered state: UI_MAP_PICKER");
            create_map_picker_menu();
            break;
        case EngineState::UI_MESSAGE_DIALOGS:
            LOGI("Entered state: UI_MESSAGE_DIALOGS");
            create_message_dialogs_menu();
            break;
        case EngineState::UI_MOTD:
            LOGI("Entered state: UI_MOTD");
            create_motd_menu();
            break;
        case EngineState::UI_MP_CUSTOM:
            LOGI("Entered state: UI_MP_CUSTOM");
            create_mp_custom_menu();
            break;
        case EngineState::UI_MP_ESO:
            LOGI("Entered state: UI_MP_ESO");
            create_mp_eso_menu();
            break;
        case EngineState::UI_MP_FRIENDS_LIST:
            LOGI("Entered state: UI_MP_FRIENDS_LIST");
            create_mp_friends_list_menu();
            break;
        case EngineState::UI_MP_HELP:
            LOGI("Entered state: UI_MP_HELP");
            create_mp_help_menu();
            break;
        case EngineState::UI_MP_PLANIP:
            LOGI("Entered state: UI_MP_PLANIP");
            create_mp_planip_menu();
            break;
        case EngineState::UI_MP_LAUNCH_SCREEN:
            LOGI("Entered state: UI_MP_LAUNCH_SCREEN");
            create_mp_launch_screen_menu();
            break;
        case EngineState::UI_MP_LOGIN:
            LOGI("Entered state: UI_MP_LOGIN");
            create_mp_login_menu();
            break;
        case EngineState::UI_MP_OTHER_SETTINGS:
            LOGI("Entered state: UI_MP_OTHER_SETTINGS");
            create_mp_other_settings_menu();
            break;
        case EngineState::UI_MP_PASSPORT:
            LOGI("Entered state: UI_MP_PASSPORT");
            create_mp_passport_menu();
            break;
        case EngineState::UI_MP_PASSPORT_TAKEN:
            LOGI("Entered state: UI_MP_PASSPORT_TAKEN");
            create_mp_passport_taken_menu();
            break;
        case EngineState::UI_MP_SETUP_FINAL:
            LOGI("Entered state: UI_MP_SETUP_FINAL");
            create_mp_setup_final_menu();
            break;
        case EngineState::UI_MP_VOTE_DIALOG:
            LOGI("Entered state: UI_MP_VOTE_DIALOG");
            create_mp_vote_dialog_menu();
            break;
        case EngineState::UI_NEW_DIALOGS:
            LOGI("Entered state: UI_NEW_DIALOGS");
            create_new_dialogs_menu();
            break;
        case EngineState::UI_OBJECTIVES:
            LOGI("Entered state: UI_OBJECTIVES");
            create_objectives_menu();
            break;
        case EngineState::UI_OBJECTIVES_DIALOG:
            LOGI("Entered state: UI_OBJECTIVES_DIALOG");
            create_objectives_dialog_menu();
            break;
        case EngineState::UI_OPTIONS_MENU:
            LOGI("Entered state: UI_OPTIONS_MENU");
            create_options_menu();
            break;
        case EngineState::UI_OTHER_SETTINGS:
            LOGI("Entered state: UI_OTHER_SETTINGS");
            create_others_settings_menu();
            break;
        case EngineState::UI_PLAYER_LOGIN:
            LOGI("Entered state: UI_PLAYER_LOGIN");
            create_player_login_menu();
            break;
        case EngineState::UI_PLAYER_STATUS_DIALOG:
            LOGI("Entered state: UI_PLAYER_STATUS_DIALOG");
            create_player_status_dialog_menu();
            break;
        case EngineState::UI_PLAYER_STATUS_DIALOG_2:
            LOGI("Entered state: UI_PLAYER_STATUS_DIALOG_2");
            create_player_status_dialog_2_menu();
            break;
        case EngineState::UI_POST_GAME_SCREEN:
            LOGI("Entered state: UI_POST_GAME_SCREEN");
            create_post_game_screen_menu();
            break;
        case EngineState::UI_PREGAME:
            LOGI("Entered state: UI_PREGAME");
            create_pregame_menu();
            break;
        case EngineState::UI_PREGAME_MP_TEST:
            LOGI("Entered state: UI_PREGAME_MP_TEST");
            create_pregame_mp_test_menu();
            break;
        case EngineState::UI_PREGAME_MSG_1:
            LOGI("Entered state: UI_PREGAME_MSG_1");
            create_pregame_msg_1_menu();
            break;
        case EngineState::UI_PREGAME_MSG_2:
            LOGI("Entered state: UI_PREGAME_MSG_2");
            create_pregame_msg_2_menu();
            break;
        case EngineState::UI_PREGAME_MSG_DIALOG:
            LOGI("Entered state: UI_PREGAME_MSG_DIALOG");
            create_pregame_msg_dialog_menu();
            break;
        case EngineState::UI_PREGAME_NEW:
            LOGI("Entered state: UI_PREGAME_NEW");
            create_pregame_new_menu();
            break;
        case EngineState::UI_PREGAME_STRESS:
            LOGI("Entered state: UI_PREGAME_STRESS");
            create_pregame_stress_menu();
            break;
        case EngineState::UI_PROFILE_SELECT:
            LOGI("Entered state: UI_PROFILE_SELECT");
            create_profile_select_menu();
            break;
        case EngineState::UI_RECEIVE:
            LOGI("Entered state: UI_RECEIVE");
            create_receive_menu();
            break;
        case EngineState::UI_RECEIVE_CONNECT:
            LOGI("Entered state: UI_RECEIVE_CONNECT");
            create_receive_connect_menu();
            break;
        case EngineState::UI_RM_GENERATION:
            LOGI("Entered state: UI_RM_GENERATION");
            create_rm_generation_menu();
            break;
        case EngineState::UI_ROOM_DIALOGS:
            LOGI("Entered state: UI_ROOM_DIALOGS");
            create_room_dialogs_menu();
            break;
        case EngineState::UI_SAMPLE:
            LOGI("Entered state: UI_SAMPLE");
            create_sample_menu();
            break;
        case EngineState::UI_SCENARIO_SET_DLG:
            LOGI("Entered state: UI_SCENARIO_SET_DLG");
            create_scenario_set_dlg_menu();
            break;
        case EngineState::UI_SESSION_CONNECT:
            LOGI("Entered state: UI_SESSION_CONNECT");
            create_session_connect_menu();
            break;
        case EngineState::UI_SINGLE_PLAYER_SETUP:
            LOGI("Entered state: UI_SINGLE_PLAYER_SETUP");
            create_single_player_setup_menu();
            break;
        case EngineState::UI_SOUND_DLG:
            LOGI("Entered state: UI_SOUND_DLG");
            create_sound_dlg_menu();
            break;
        case EngineState::UI_SOUND_EDIT_DLG:
            LOGI("Entered state: UI_SOUND_EDIT_DLG");
            create_sound_edit_dlg_menu();
            break;
        case EngineState::UI_SOUND_SET_DLG:
            LOGI("Entered state: UI_SOUND_SET_DLG");
            create_sound_set_dlg_menu();
            break;
        case EngineState::UI_SPRM_SETUP:
            LOGI("Entered state: UI_SPRM_SETUP");
            create_sprm_setup_menu();
            break;
        case EngineState::UI_STATS_UPLOAD:
            LOGI("Entered state: UI_STATS_UPLOAD");
            create_stats_upload_menu();
            break;
        case EngineState::UI_STATUS_MSG:
            LOGI("Entered state: UI_STATUS_MSG");
            create_status_msg_menu();
            break;
        case EngineState::UI_STRATEGIC_MAP:
            LOGI("Entered state: UI_STRATEGIC_MAP");
            create_strategic_map_menu();
            break;
        case EngineState::UI_TECHTREE:
            LOGI("Entered state: UI_TECHTREE");
            create_techtree_menu();
            break;
        case EngineState::UI_TRIBUTE_DLG:
            LOGI("Entered state: UI_TRIBUTE_DLG");
            create_tribute_dlg_menu();
            break;
        case EngineState::UI_TRIGGER_EDITOR:
            LOGI("Entered state: UI_TRIGGER_EDITOR");
            create_trigger_editor_menu();
            break;
        case EngineState::UI_TRIG_GROUP_EDITOR:
            LOGI("Entered state: UI_TRIG_GROUP_EDITOR");
            create_trig_group_editor_menu();
            break;
        case EngineState::UI_WATER_DIALOG:
            LOGI("Entered state: UI_WATER_DIALOG");
            create_water_dialog_menu();
            break;
        case EngineState::UI_XS_DEBUGGER:
            LOGI("Entered state: UI_XS_DEBUGGER");
            create_xs_debugger_menu();
            break;
        default:
            break;
    }
}

} // namespace aom
