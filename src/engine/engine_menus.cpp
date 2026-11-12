#include "engine/engine.h"
#include "engine_internal.h"
#include "ui/ui.h"
#include "ui/ui_layout.h"
#include "aom/campaign_select_system.h"
#include "assets/xmb_parser.h"
#include "assets/bar_archive.h"
#include "audio/audio.h"
#include "network/network_manager.h"

extern "C" {
#include "stb_image.h"
}
#include <algorithm>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <cstdio>
#include <cmath>

namespace aom {

void Engine::create_main_menu() {
    create_main_menu_from_xmb();
}

// ── Seleção de Campanha ─────────────────────────────────────────────
void Engine::create_campaign_selection_menu() {
    LOGI("Opening campaign selection menu...");
    
    // Criar o CampaignSelectSystem para mostrar lista de cenários
    // Usar os mesmos parâmetros do menu principal
    campaign_select_system_.reset(new aom::CampaignSelectSystem());
    if (campaign_select_system_->initialize(this, ui_.get(), this, 
                config_.window_width, config_.window_height)) {
        LOGI("Campaign selection menu initialized successfully");
        // O CampaignSelectSystem já criou seus botões e está gerenciando toques
        // O usuário agora pode selecionar um cenário
    } else {
        LOGW("Failed to initialize campaign selection menu");
        // Fallback: tentar o menu de campanha original
        create_campaign_menu();
    }
}

// ── uigamelist — lista de jogos salvos, campanhas e cenarios ─────────────────
void Engine::create_game_list_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uigamelist.xmb) ─────────
    if (load_ui_layout("uigamelist.xmb")) {
        size_t created = show_ui_layout("uigamelist.xmb");
        if (created > 0) {
            LOGI("create_game_list_menu: uigamelist.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_game_list_menu: falha ao carregar uigamelist.xmb, usando fallback procedural");

    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;
    ui_->add_label("LISTA DE JOGOS", {vw * 0.5f - 130.0f, vh * 0.08f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uialternates_dialog ──
void Engine::create_alternates_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ALTERNATES DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uianimation_dlg ──
void Engine::create_animation_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ANIMATION DLG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiarena_setup ──
void Engine::create_arena_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ARENA SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiarmy_edit ──
void Engine::create_army_edit_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ARMY EDIT", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiautopatcher_dialog ──
void Engine::create_autopatcher_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("AUTOPATCHER DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucamera_editor ──
void Engine::create_camera_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMERA EDITOR", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign ──
void Engine::create_campaign_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uicampaign.xmb) ────────
    if (load_ui_layout("uicampaign.xmb")) {
        size_t created = show_ui_layout("uicampaign.xmb");
        if (created > 0) {
            LOGI("create_campaign_menu: uicampaign.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_campaign_menu: falha ao carregar uicampaign.xmb, usando fallback procedural");

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CAMPAIGN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_editor ──
void Engine::create_campaign_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN EDITOR", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_nloanding ──
void Engine::create_campaign_nloanding_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN NLOANDING", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_setup ──
void Engine::create_campaign_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uichat_dialog ──
void Engine::create_chat_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CHAT DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiliff_dialog ──
void Engine::create_liff_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LIFF DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uicombat_comparison ──
void Engine::create_combat_comparison_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("COMBAT COMPARISON", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uicustomize_opp ──
void Engine::create_customize_opp_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CUSTOMIZE OPPONENT", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uidefault_loading ──
void Engine::create_default_loading_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("DEFAULT LOADING", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieditor_menu ──
void Engine::create_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDITOR MENU", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieditor_palettes ──
void Engine::create_editor_palettes_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDITOR PALETTES", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiedit_unit_dlg ──
void Engine::create_edit_unit_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDIT UNIT DLG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieula ──
void Engine::create_eula_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EULA", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uifaketechtree ──
void Engine::create_fake_techtree_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FAKE TECHTREE", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f,
                   0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}



// ── uifile_manager ──
void Engine::create_file_manager_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FILE MANAGER", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uifriends_dialogs ──
void Engine::create_friends_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FRIENDS DIALOGS", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigame_palettes ──
void Engine::create_game_palettes_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GAME PALETTES", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigamesettingsdialog ──
void Engine::create_game_settings_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GAME SETTINGS DIALOG", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigenericentrydialog ──
void Engine::create_generic_entry_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GENERIC ENTRY DIALOG", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigodbuffetdialog ──
void Engine::create_god_buffet_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GOD BUFFET DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigodpicker ──
void Engine::create_god_picker_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Título
    ui_->add_label("SELECIONAR CIVILIZAÇÃO E DEUS", {vw * 0.5f - 180.0f, vh * 0.10f}, 32.0f, 0xFFFFD000);

    // Linha de informação
    std::string info_text = "Civilização: " + god_picker_civilization_ + " | Deus Menor: " + god_picker_minor_god_;
    ui_->add_label(info_text, {vw * 0.5f - 150.0f, vh * 0.16f}, 18.0f, 0xFFCCCCCC);

    const float col_w = vw * 0.47f;
    const float col1_x = vw * 0.04f;
    const float col2_x = vw * 0.56f;
    const float btn_h = 38.0f;
    const float gap = 8.0f;

    // --- Coluna Esquerda: Civilizações ---
    float y = vh * 0.18f;
    ui_->add_label("CIVILIZAÇÕES", {col1_x, y}, 20.0f, 0xFFFFD000);
    y += 30.0f;

    // Botões de civilização - baseadas no Age of Mythology The Titans
    static const char* civilizations[] = {
        "Grego", "Egípcio", "Nórdico", "Atlante", "Aleatório"
    };

    // Associated minor god indices for each civilization (index into minor_gods[] array)
    // Format: start index, count
    static const int civ_god_start[] = {0, 3, 6, 10, 0};
    static const int civ_god_count[] = {6, 3, 4, 3, 13};

    for (int i = 0; i < 5; ++i) {
        std::string civ_name = civilizations[i];
        // Highlight selected civilization
        uint32_t color = (god_picker_civilization_ == civ_name) ? 0xAA184818 : 0xAA2A1808;
        ui_->add_button({col1_x, y}, {col_w, btn_h}, civ_name, [this, i, civ_name]() {
            god_picker_civilization_ = civ_name;
            // Set minor god to first associated god for this civilization
            int start = civ_god_start[i];
            god_picker_minor_god_ = aom::net::major_god_to_string(static_cast<aom::net::MajorGod>(start));
            create_god_picker_menu(); // Recreate to show updated info and god list
        }, color, 0xAA553311);
        y += btn_h + gap;
    }

    // --- Coluna Direita: Deuses Menores ---
    y = vh * 0.18f;
    ui_->add_label("DEUSES MENORES", {col2_x, y}, 20.0f, 0xFFFFD000);
    y += 30.0f;

    // Mostrar todos os deuses, highlightar o selecionado e associar à civilização
    static const char* minor_gods[] = {
        "Zeus", "Poseidon", "Hades", "Rá", "Isis", "Set",
        "Thor", "Odin", "Loki", "Kronos", "Oranos", "Gaia", "Aleatório"
    };

    y = vh * 0.18f;
    for (int i = 0; i < 13; ++i) {
        std::string god_name = minor_gods[i];
        // Highlight the currently selected one
        uint32_t color = (god_picker_minor_god_ == god_name) ? 0xAA184818 : 0xAA552211;
        
        // Check if this god is associated with the current civilization
        int start = civ_god_start[0]; // default Grego
        int count = civ_god_count[0];
        if (god_picker_civilization_ == "Grego") { start = civ_god_start[0]; count = civ_god_count[0]; }
        else if (god_picker_civilization_ == "Egípcio") { start = civ_god_start[1]; count = civ_god_count[1]; }
        else if (god_picker_civilization_ == "Nórdico") { start = civ_god_start[2]; count = civ_god_count[2]; }
        else if (god_picker_civilization_ == "Atlante") { start = civ_god_start[3]; count = civ_god_count[3]; }
        else if (god_picker_civilization_ == "Aleatório") { start = civ_god_start[4]; count = civ_god_count[4]; }
        
        bool associated = (i >= start && i < start + count);

        ui_->add_button({col2_x, y}, {col_w, btn_h}, god_name, [this, i]() {
            god_picker_minor_god_ = aom::net::major_god_to_string(static_cast<aom::net::MajorGod>(i));
            create_god_picker_menu(); // Recreate to show updated selection
        }, color, 0xAA553311);
        y += btn_h + gap;
    }

    // --- Botão Finalizar Seleção ---
    float back_w = 220.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.82f}, {back_w, 44.0f}, "FINALIZAR SELEÇÃO",
                    [this]() {
                        god_picker_selection_made_ = true;
                        LOGI("God Picker selection finalized: civilization=%s, minor_god=%s",
                             god_picker_civilization_.c_str(), god_picker_minor_god_.c_str());
                        set_state(EngineState::MAIN_MENU); // Return to main menu, game start will use the selection
                    }, 0xAA204060, 0xAA306090);
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihelpdialog ──
void Engine::create_help_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HELP DIALOG", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihostgamedialog ──
void Engine::create_host_game_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOST GAME DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihotkeys ──
void Engine::create_hotkeys_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOTKEYS", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihotkysetup ──
void Engine::create_hotkey_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOTKEY SETUP", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiimagedialog ──
void Engine::create_image_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("IMAGE DIALOG", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingamemsg1 ──
void Engine::create_ingame_msg_1_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 1", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingamemsg2 ──
void Engine::create_ingame_msg_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 2", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingamemsg3 ──
void Engine::create_ingame_msg_3_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 3", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingamemsgdialog ──
void Engine::create_ingame_msg_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG DIALOG", {vw * 0.5f - 170.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilaunchscreen ──
void Engine::create_launch_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LAUNCH SCREEN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilayout ──
void Engine::create_layout_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LAYOUT", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilowdetailbackground ──
void Engine::create_low_detail_background_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LOW DETAIL BACKGROUND", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimain ──
void Engine::create_main_alt_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MAIN", {vw * 0.5f - 80.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimappicker ──
void Engine::create_map_picker_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MAP PICKER", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimessagedialogs ──
void Engine::create_message_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MESSAGE DIALOGS", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimotd ──
void Engine::create_motd_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MOTD", {vw * 0.5f - 80.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpcustom ──
void Engine::create_mp_custom_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP CUSTOM", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpeso ──
void Engine::create_mp_eso_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP ESO", {vw * 0.5f - 80.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpfriendslist ──
void Engine::create_mp_friends_list_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP FRIENDS LIST", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimphelp ──
void Engine::create_mp_help_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP HELP", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimplanip ──
void Engine::create_mp_planip_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PLANIP", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimplaunchscreen ──
void Engine::create_mp_launch_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP LAUNCH SCREEN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimplogin ──
void Engine::create_mp_login_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP LOGIN", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpothersettings ──
void Engine::create_mp_other_settings_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP OTHER SETTINGS", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimppassport ──
void Engine::create_mp_passport_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PASSPORT", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimppassporttaken ──
void Engine::create_mp_passport_taken_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PASSPORT TAKEN", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpsetupfinal ──
void Engine::create_mp_setup_final_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP SETUP FINAL", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimpvotedialog ──
void Engine::create_mp_vote_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP VOTE DIALOG", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uinewdialogs ──
void Engine::create_new_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("NEW DIALOGS", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiobjectives ──
void Engine::create_objectives_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OBJECTIVES", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiobjectivesdialog ──
void Engine::create_objectives_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OBJECTIVES DIALOG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiothersettings ──
void Engine::create_others_settings_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OTHER SETTINGS", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiplayerlogin ──
void Engine::create_player_login_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER LOGIN", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiplayerstatusdialog ──
void Engine::create_player_status_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER STATUS DIALOG", {vw * 0.5f - 170.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiplayerstatusdialog2 ──
void Engine::create_player_status_dialog_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER STATUS DIALOG 2", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipostgamescreen ──
void Engine::create_post_game_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("POST GAME SCREEN", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregame ──
void Engine::create_pregame_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemptest ──
void Engine::create_pregame_mp_test_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MP TEST", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsg1 ──
void Engine::create_pregame_msg_1_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG 1", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsg2 ──
void Engine::create_pregame_msg_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG 2", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsgdialog ──
void Engine::create_pregame_msg_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipreamenew ──
void Engine::create_pregame_new_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME NEW", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamestress ──
void Engine::create_pregame_stress_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME STRESS", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiprofileselect ──
void Engine::create_profile_select_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PROFILE SELECT", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uireceive ──
void Engine::create_receive_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RECEIVE", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uireceiveconnect ──
void Engine::create_receive_connect_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RECEIVE CONNECT", {vw * 0.5f - 130.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uirmgeneration ──
void Engine::create_rm_generation_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RM GENERATION", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiroomdialogs ──
void Engine::create_room_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ROOM DIALOGS", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisample ──
void Engine::create_sample_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SAMPLE", {vw * 0.5f - 70.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiscensetdlg ──
void Engine::create_scenario_set_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SCENARIO SET DLG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisessionconnect ──
void Engine::create_session_connect_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SESSION CONNECT", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisingleplayersetup ──
void Engine::create_single_player_setup_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uisingleplayersetup.xmb) ──
    if (load_ui_layout("uisingleplayersetup.xmb")) {
        size_t created = show_ui_layout("uisingleplayersetup.xmb");
        if (created > 0) {
            LOGI("create_single_player_setup_menu: uisingleplayersetup.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_single_player_setup_menu: falha ao carregar uisingleplayersetup.xmb, usando fallback procedural");

    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SINGLE PLAYER SETUP", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisounddlg ──
void Engine::create_sound_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND DLG", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisoundeditdlg ──
void Engine::create_sound_edit_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND EDIT DLG", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisoundsetdlg ──
void Engine::create_sound_set_dlg_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uisoundsetdlg.xmb) ──────
    if (load_ui_layout("uisoundsetdlg.xmb")) {
        size_t created = show_ui_layout("uisoundsetdlg.xmb");
        if (created > 0) {
            LOGI("create_sound_set_dlg_menu: uisoundsetdlg.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_sound_set_dlg_menu: falha ao carregar uisoundsetdlg.xmb, usando fallback procedural");

    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND SET DLG", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisprmsetup ──
void Engine::create_sprm_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SPRM SETUP", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uistatsupload ──
void Engine::create_stats_upload_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("STATS UPLOAD", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uistatusmsg ──
void Engine::create_status_msg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("STATUS MSG", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uistrategicmap ──
void Engine::create_strategic_map_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("STRATEGIC MAP", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uitechtree ──
void Engine::create_techtree_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uitechtree.xmb) ────────
    if (load_ui_layout("uitechtree.xmb")) {
        size_t created = show_ui_layout("uitechtree.xmb");
        if (created > 0) {
            LOGI("create_techtree_menu: uitechtree.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_techtree_menu: falha ao carregar uitechtree.xmb, usando fallback procedural");

    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("TECHTREE", {vw * 0.5f - 90.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uitributedlg ──
void Engine::create_tribute_dlg_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uitributedlg.xmb) ───────
    if (load_ui_layout("uitributedlg.xmb")) {
        size_t created = show_ui_layout("uitributedlg.xmb");
        if (created > 0) {
            LOGI("create_tribute_dlg_menu: uitributedlg.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_tribute_dlg_menu: falha ao carregar uitributedlg.xmb, usando fallback procedural");

    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("TRIBUTE DLG", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uitriggereditor ──
void Engine::create_trigger_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("TRIGGER EDITOR", {vw * 0.5f - 120.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uitriggroupeditor ──
void Engine::create_trig_group_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("TRIG GROUP EDITOR", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiwaterdialog ──
void Engine::create_water_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("WATER DIALOG", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uixsdebugger ──
void Engine::create_xs_debugger_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("XS DEBUGGER", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiaiprompt ──
void Engine::create_ai_prompt_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("AI PROMPT", {vw * 0.5f - 100.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

void Engine::create_credits_menu() {
    clear_ui();

    // ── Layout Autêntico Age of Mythology: The Titans (uicredits.xmb) ─────────
    if (load_ui_layout("uicredits.xmb")) {
        size_t created = show_ui_layout("uicredits.xmb");
        if (created > 0) {
            LOGI("create_credits_menu: uicredits.xmb aplicado com %zu elementos", created);
            return;
        }
    }

    LOGW("create_credits_menu: falha ao carregar uicredits.xmb, usando fallback procedural");

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CREDITOS", {vw * 0.5f - 80.0f, vh * 0.12f}, 32.0f, 0xFFFFD000);

    float text_y = vh * 0.25f;
    const float step = 28.0f;
    ui_->add_label("Age of Mythology Original por Ensemble Studios (2002)", {vw * 0.5f - 240.0f, text_y}, 18.0f, 0xFFFFFFFF);
    text_y += step;
    ui_->add_label("Porte Nativo Android ARM64 por Antigravity Studio", {vw * 0.5f - 220.0f, text_y}, 18.0f, 0xFFFFD000);
    text_y += step * 1.5f;
    ui_->add_label("Lead Architecture: Technical Director", {vw * 0.5f - 160.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("OpenGL ES 3 Shaders & Pipeline: Graphics Lead", {vw * 0.5f - 190.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("BAR, DDT & BRG Parsers: Asset Pipeline Lead", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("OpenSL ES Low-Latency Audio: Audio Specialist", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("Reverse Engineering & XMB UI: REA Specialist", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);

    float back_w = 200.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                   [this]() { return_to_main_menu(); });
}

void Engine::open_credits() {
    set_state(EngineState::CREDITS);
}

void Engine::create_single_player_menu() {
    start_game();
}

void Engine::create_random_map_menu() {
    start_game();
}

// ── Layouts de UI do data.bar (Passo 4) ──────────────────────────────────

bool Engine::load_ui_layout(const std::string& xmb_name) {
    if (!data_bar_) {
        LOGW("load_ui_layout: data_bar_ nao disponível");
        return false;
    }
    auto result = ui_layout_manager_.load_layout(*data_bar_, xmb_name);
    if (!result.success) {
        LOGE("load_ui_layout: %s", result.error_message.c_str());
        return false;
    }
    LOGI("load_ui_layout: '%s' carregado (%zu gadgets)", xmb_name.c_str(),
         result.root_gadgets.size());
    return true;
}

size_t Engine::show_ui_layout(const std::string& layout_name) {
    if (!ui_) return 0;

    const auto* layout = ui_layout_manager_.get_layout(layout_name);
    if (!layout) {
        LOGW("show_ui_layout: layout '%s' não encontrado — tente load_ui_layout() primeiro",
             layout_name.c_str());
        return 0;
    }

    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Mapa de ações do AoM para callbacks do Engine
    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"start_game", [this]() { start_game(); }},
        {"open_campaign", [this]() { create_campaign_selection_menu(); }},
        {"open_campaign_selection", [this]() { create_campaign_selection_menu(); }},
        {"open_single_player", [this]() { create_campaign_menu(); }},
        {"open_multiplayer", [this]() { open_multiplayer(); }},
        {"open_options", [this]() { open_options(); }},
        {"open_credits", [this]() { open_credits(); }},
        {"open_more", [this]() { open_credits(); }},
        {"quit_app", [this]() { quit_app(); }},
        {"exit", [this]() { quit_app(); }},
        {"return_to_main_menu", [this]() { return_to_main_menu(); }},
        {"back", [this]() { return_to_main_menu(); }},
        {"uiOpenSavedGameBrowser", [this]() { open_saved_game_browser(); }},
        {"load_selected_save", [this]() { load_selected_save(); }},
        {"delete_selected_save", [this]() { delete_selected_save(); }},
        {"save_game", [this]() { open_save_game_menu(); }},
    };

    // Resolver de texturas: carrega bg_texture do textures.bar e retorna handle + dimensões
    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        // 1) Cache primeiro
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }

        // 2) Normalizar: NÃO converter \ → / aqui. O BarArchive::normalize()
        //    já trata barras invertidas e barras normais de forma bidirecional.
        //    Apenas garantir extensão .ddt.
        std::string normalized = name;
        if (normalized.size() < 4 ||
            normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }

        // 3) Carregar de textures_bar_ (UI textures ficam em textures.bar, não data.bar)
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            LOGI("UI texture resolved: '%s' -> '%s' (handle=%u, %ux%u)",
                 name.c_str(), normalized.c_str(), h, rt.width, rt.height);
        } else {
            // 4) Se não encontrou no BAR, tentar carregar de PNG em assets (ex: titans_menu_parts/)
            std::string png_name = name;
            if (png_name.rfind("ui\\", 0) == 0 || png_name.rfind("ui/", 0) == 0) {
                png_name = png_name.substr(3);
            }
            for (char& c : png_name) {
                if (c == ' ' || c == '\\' || c == '/') c = '_';
                else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (png_name.size() < 4 || png_name.substr(png_name.size() - 4) != ".png") {
                png_name += ".png";
            }
            int pw = 0, ph = 0;
            h = load_png_asset("titans_menu_parts/" + png_name, &pw, &ph);
            if (h == INVALID_TEXTURE) {
                h = load_png_asset(png_name, &pw, &ph);
            }
            if (h != INVALID_TEXTURE) {
                rt.handle = h;
                rt.width = static_cast<uint32_t>(pw);
                rt.height = static_cast<uint32_t>(ph);
                texture_cache_[name] = h;
                texture_dimensions_[name] = glm::ivec2(pw, ph);
                LOGI("UI texture resolved from PNG asset: '%s' -> '%s' (handle=%u, %ux%u)",
                     name.c_str(), png_name.c_str(), h, pw, ph);
            } else {
                LOGE("UI texture NOT FOUND: '%s' (tried '%s', '%s')",
                     name.c_str(), normalized.c_str(), png_name.c_str());
            }
        }
        return rt;
    };

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                         static_cast<int>(vw), static_cast<int>(vh),
                                         action_callbacks, texture_resolver);
    LOGI("[DIAG] show_ui_layout('uipregamenew.xmb'): %zu elementos criados", created);

    // Resolver textos $$ID$$ usando as strings do Engine
    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    LOGI("show_ui_layout: '%s' exibido (%zu elementos)", layout_name.c_str(), created);
    return created;
}

bool Engine::show_techtree() {
    if (!load_ui_layout("uitechtree.xmb")) return false;
    show_ui_layout("uitechtree.xmb");
    return true;
}

bool Engine::show_tribute() {
    if (!load_ui_layout("uitributedlg.xmb")) return false;
    show_ui_layout("uitributedlg.xmb");
    return true;
}

bool Engine::show_sound_settings() {
    if (!load_ui_layout("uisoundsetdlg.xmb")) return false;
    show_ui_layout("uisoundsetdlg.xmb");
    return true;
}



void Engine::create_cinematics_menu() {
    replay_opening_cinematics();
}

void Engine::create_history_menu() {
    open_credits();
}

void Engine::create_menu_screen(
    const std::string& title,
    const std::string& subtitle,
    const std::vector<std::pair<std::string, std::function<void()>>>& actions,
    const std::function<void()>& back_action) {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    if (!title.empty()) {
        ui_->add_label(title, {vw * 0.5f - 110.0f, vh * 0.12f}, 32.0f, 0xFFFFD000);
    }
    if (!subtitle.empty()) {
        ui_->add_label(subtitle, {vw * 0.5f - 140.0f, vh * 0.18f}, 18.0f, 0xFFCCCCCC);
    }

    float y = vh * 0.28f;
    const float btn_w = 280.0f;
    const float btn_h = 44.0f;
    const float gap = 12.0f;
    for (const auto& act : actions) {
        ui_->add_button({(vw - btn_w) * 0.5f, y}, {btn_w, btn_h}, act.first, act.second);
        y += btn_h + gap;
    }

    float back_w = 200.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                   back_action ? back_action : [this]() { return_to_main_menu(); });
}

void Engine::create_main_menu_from_xmb() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // ── Tenta uimain.xmb primeiro ──
    if (load_ui_layout("uimain.xmb")) {
        size_t created = show_ui_layout("uimain.xmb");
        if (created > 0) {
            LOGI("create_main_menu_from_xmb: uimain.xmb aplicado com %zu elementos", created);
            if (!archives_loaded_) {
                ui_->add_label("DADOS DO JOGO NAO ENCONTRADOS",
                    {24.0f, vh - 64.0f}, 16.0f, 0xFFFF6666);
                ui_->add_label("OPCOES > IMPORTAR DADOS DO JOGO",
                    {24.0f, vh - 42.0f}, 13.0f, 0xFFFFD000);
            }
            if (show_fps_) {
                fps_label_ = ui_->add_label("FPS: 60",
                    {vw - 90.0f, vh - 24.0f}, 14.0f, 0x88FFFFFF);
            }
            return;
        }
    }

    LOGW("create_main_menu_from_xmb: falha ao carregar uimain.xmb, tentando uipregamenew.xmb");

    // ── Fallback: uipregamenew.xmb ──
    if (load_ui_layout("uipregamenew.xmb")) {
        size_t created = show_ui_layout("uipregamenew.xmb");
        if (created > 0) {
            LOGI("create_main_menu_from_xmb: uipregamenew.xmb aplicado com %zu elementos", created);
            ui_->add_label("TESTE DE FONTE", {100.0f, 100.0f}, 32.0f, 0xFFFF0000);
            if (!archives_loaded_) {
                ui_->add_label("DADOS DO JOGO NAO ENCONTRADOS",
                    {24.0f, vh - 64.0f}, 16.0f, 0xFFFF6666);
                ui_->add_label("OPCOES > IMPORTAR DADOS DO JOGO",
                    {24.0f, vh - 42.0f}, 13.0f, 0xFFFFD000);
            }
            if (show_fps_) {
                fps_label_ = ui_->add_label("FPS: 60",
                    {vw - 90.0f, vh - 24.0f}, 14.0f, 0x88FFFFFF);
            }
            return;
        }
    }

    // ── Fallback procedural (hard-coded) ──
    create_main_menu_fallback();
}

void Engine::create_main_menu_fallback() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    const float btn_w = 190.0f * sx;
    const float btn_h = 36.0f * sy;
    const float btn_x = (417.0f - 8.0f) * sx;

    // Ordem exata dos 7 botões do menu The Titans
    struct TitansBtn {
        float y;
        const char* label_key;
        const char* fallback_label;
        std::function<void()> action;
    };

    const TitansBtn btns[] = {
        { 325.0f, "MENU_LEARN_TO_PLAY", "LEARN TO PLAY", [this]() { start_game(); }    },
        { 365.0f, "MENU_CAMPAIGN", "CAMPAIGN", [this]() { create_campaign_selection_menu(); } },
        { 405.0f, "MENU_SINGLE_PLAYER", "SINGLE PLAYER", [this]() { create_campaign_menu(); } },
        { 445.0f, "MENU_MULTIPLAYER", "MULTIPLAYER", [this]() { open_multiplayer(); } },
        { 485.0f, "MENU_OPTIONS", "OPTIONS", [this]() { open_options(); } },
        { 525.0f, "MENU_MORE", "MORE", [this]() { open_credits(); } },
        { 573.0f, "MENU_EXIT", "EXIT", [this]() { quit_app(); }     },
    };

    size_t btns_created = 0;
    for (const auto& b : btns) {
        std::string label = tr(b.label_key);
        if (label == b.label_key && b.fallback_label) label = b.fallback_label;

        auto* btn = ui_->add_button(
            {btn_x, (b.y - 2.0f) * sy},
            {btn_w, btn_h},
            label,
            b.action,
            0x88203010,
            0x55FFD700
        );
        if (btn) {
            btn->normal_tex = button_main_tex_;
            btn->pressed_tex = button_clicked_tex_;
            btn->text_color = 0xFFFFFFFF;
            btn->text_size = 20.0f * sy;
            btns_created++;
        }
    }
    LOGI("[DIAG] create_main_menu fallback: %zu/%zu botões criados", btns_created, sizeof(btns)/sizeof(btns[0]));

    if (!archives_loaded_) {
        ui_->add_label("DADOS DO JOGO NAO ENCONTRADOS",
            {24.0f, vh - 64.0f}, 16.0f, 0xFFFF6666);
        ui_->add_label("OPCOES > IMPORTAR DADOS DO JOGO",
            {24.0f, vh - 42.0f}, 13.0f, 0xFFFFD000);
    }

    if (show_fps_) {
        fps_label_ = ui_->add_label("FPS: 60",
            {vw - 90.0f, vh - 24.0f}, 14.0f, 0x88FFFFFF);
    }
}

bool Engine::create_command_panel_from_xmb() {
    if (!gameplay_world_) return false;

    // ── Tenta carregar commandpanel.xmb (sem clear_ui: aplica sobre o UI existente) ──
    if (load_ui_layout("commandpanel.xmb")) {
        const float vw = static_cast<float>(config_.window_width);
        const float vh = static_cast<float>(config_.window_height);

        const auto* layout = ui_layout_manager_.get_layout("commandpanel.xmb");
        if (layout) {
            // Callbacks de ação (preenchidos dinamicamente por update_in_game_hud)
            std::unordered_map<std::string, std::function<void()>> action_callbacks;

            auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
                ResolvedTexture rt;
                auto it = texture_cache_.find(name);
                if (it != texture_cache_.end()) {
                    rt.handle = it->second;
                    auto dit = texture_dimensions_.find(name);
                    if (dit != texture_dimensions_.end()) {
                        rt.width = static_cast<uint32_t>(dit->second.x);
                        rt.height = static_cast<uint32_t>(dit->second.y);
                    }
                    return rt;
                }
                const BarEntry* entry = textures_bar_->find(name);
                if (entry) {
                    auto data = textures_bar_->extract(*entry);
                    if (!data.empty()) {
                        int w = 0, h = 0, comp = 0;
                        unsigned char* png = stbi_load_from_memory(data.data(),
                            static_cast<int>(data.size()), &w, &h, &comp, 4);
                        if (png) {
                            rt.handle = renderer_->create_texture(w, h, png);
                            rt.width = w;
                            rt.height = h;
                            stbi_image_free(png);
                            texture_cache_[name] = rt.handle;
                            texture_dimensions_[name] = {w, h};
                        }
                    }
                }
                return rt;
            };

            size_t created = apply_layout_to_ui(*layout, ui_.get(),
                static_cast<int>(vw), static_cast<int>(vh),
                action_callbacks, texture_resolver);
            LOGI("create_command_panel_from_xmb: commandpanel.xmb aplicado (%zu elementos)", created);
            command_panel_from_xmb_ = true;
            return true;
        }
    }

    command_panel_from_xmb_ = false;
    return false;
}


} // namespace aom
