#include "engine/engine.h"
#include "engine_internal.h"
#include "ui/ui.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace aom {

void Engine::open_options() {
    set_state(EngineState::OPTIONS);
}


void Engine::create_options_menu() {
    // ── Sistema de Abas (Tabs) para Options Menu (aom-recover) ──
    struct TabDef {
        const char* label;
        EngineState state;
    };

    static const TabDef kOptionsTabs[] = {
        {"AUDIO", EngineState::UI_OPTIONS_AUDIO},
        {"VIDEO", EngineState::UI_OPTIONS_VIDEO},
        {"GAMEPLAY", EngineState::UI_OPTIONS_GAMEPLAY},
        {"NETWORK", EngineState::UI_OPTIONS_NETWORK},
        {"CONTROLS", EngineState::UI_OPTIONS_CONTROLS},
    };

    // Menu autêntico com sistema de abas fiéis ao Age of Mythology: The Titans
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    // Fundo estilo AoM (painel de pedra)
    ui_->add_background_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xEE110802);
    ui_->add_background_quad({vw * 0.05f, vh * 0.05f}, {vw * 0.9f, vh * 0.9f}, INVALID_TEXTURE, 0xFF1A1208);

    // Borda dourada
    ui_->add_background_quad({vw * 0.05f, vh * 0.05f}, {vw * 0.9f, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
    ui_->add_background_quad({vw * 0.05f, vh * 0.95f - 2.0f}, {vw * 0.9f, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
    ui_->add_background_quad({vw * 0.05f, vh * 0.05f}, {2.0f, vh * 0.9f}, INVALID_TEXTURE, 0xFFFFD000);
    ui_->add_background_quad({vw * 0.95f - 2.0f, vh * 0.05f}, {2.0f, vh * 0.9f}, INVALID_TEXTURE, 0xFFFFD000);

    // Título
    ui_->add_label("CONFIGURACOES", {vw * 0.5f - 140.0f * sx, vh * 0.08f}, 28.0f * sy, 0xFFFFD000);

    // ── Importação dos dados originais do jogo (Android SAF) ────────────────
    const float import_w = std::min(280.0f * sx, vw * 0.28f);
    const float import_h = 36.0f * sy;
    const float import_x = vw * 0.95f - import_w - 10.0f * sx;
    const float import_y = vh * 0.07f;

    ui_->add_button({import_x, import_y}, {import_w, import_h},
                    "IMPORTAR DADOS DO JOGO",
                    [this]() {
                        if (!request_game_data_import()) {
                            LOGW("Nao foi possivel abrir o seletor de pasta do jogo");
                        }
                    });

    if (!archives_loaded_) {
        ui_->add_label("Dados do jogo: carregados",
                       {import_x, import_y + import_h + 6.0f * sy}, 12.0f * sy, 0xFF88FF88);
        ui_->add_label("Selecione a pasta que contem data/ e textures/.",
                       {import_x, import_y + import_h + 26.0f * sy}, 10.0f * sy, 0xFFCCCCCC);
    }

    // ── Dimensionamento de layout seguro (confinado à tela) ──
    const float back_w = std::min(360.0f * sx, vw * 0.35f);
    const float back_h = 44.0f * sy;
    const float back_x = (vw - back_w) * 0.5f;
    const float back_y = vh * 0.95f - back_h - 10.0f * sy;

    // ── Sistema de Abas (Tabs) ──
    const size_t tab_count = sizeof(kOptionsTabs) / sizeof(kOptionsTabs[0]);
    const float tab_w = std::min(130.0f * sx, (vw * 0.80f) / tab_count - 4.0f * sx);
    const float tab_h = 36.0f * sy;
    const float tabs_start_x = vw * 0.5f - (tab_count * tab_w + 4.0f * sx * (tab_count - 1)) * 0.5f;
    const float tabs_y = vh * 0.15f;

    for (size_t i = 0; i < tab_count; ++i) {
        const TabDef& tab = kOptionsTabs[i];
        float tx = tabs_start_x + i * (tab_w + 4.0f * sx);

        bool is_active = (state_ == tab.state);
        if (i == 0 && state_ == EngineState::OPTIONS) is_active = true;

        uint32_t bg_color = is_active ? 0xFF332211 : 0xFF1A1208;
        uint32_t text_color = is_active ? 0xFFFFD000 : 0xFFCCCCCC;

        auto* btn = ui_->add_button(
            {tx, tabs_y}, {tab_w, tab_h},
            tab.label,
            [this, tab]() { set_state(tab.state); },
            bg_color,
            0xFFCCCCCC
        );
        if (btn) {
            btn->text_color = text_color;
            btn->text_size = 15.0f * sy;
            if (is_active) {
                ui_->add_background_quad({tx, tabs_y + tab_h - 2.0f}, {tab_w, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
            }
        }
    }

    // Área de conteúdo contida estritamente acima do botão VOLTAR
    const float content_x = vw * 0.10f;
    const float content_y = tabs_y + tab_h + 10.0f * sy;
    const float content_w = vw * 0.80f;
    const float content_h = back_y - content_y - 12.0f * sy;

    // Fundo da área de conteúdo
    ui_->add_background_quad({content_x, content_y}, {content_w, content_h}, INVALID_TEXTURE, 0xFF0A0806);
    ui_->add_background_quad({content_x, content_y}, {content_w, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);

    // Determina índice da aba ativa
    int tab_index = 0;
    if (state_ == EngineState::UI_OPTIONS_AUDIO) tab_index = 0;
    else if (state_ == EngineState::UI_OPTIONS_VIDEO) tab_index = 1;
    else if (state_ == EngineState::UI_OPTIONS_GAMEPLAY) tab_index = 2;
    else if (state_ == EngineState::UI_OPTIONS_NETWORK) tab_index = 3;
    else if (state_ == EngineState::UI_OPTIONS_CONTROLS) tab_index = 4;
    else if (state_ == EngineState::OPTIONS) tab_index = 0;

    const float col_w = std::min(content_w * 0.70f, 600.0f * sx);
    const float x = content_x + (content_w - col_w) * 0.5f;
    const float ctrl_h = 42.0f * sy;
    const float gap = 12.0f * sy;
    float cy = content_y + 18.0f * sy;

    // ── Aba: AUDIO ──
    if (tab_index == 0) {
        char buf[64];
        int master_pct = static_cast<int>(std::round(master_volume_ * 100.0f));
        std::snprintf(buf, sizeof(buf), "Volume Geral: %d%%", master_pct);
        auto* b_master = ui_->add_button({x, cy}, {col_w, ctrl_h}, buf, [this]() {
            int pct = static_cast<int>(std::round(master_volume_ * 100.0f));
            pct = (pct >= 100) ? 0 : (pct + 10);
            master_volume_ = pct / 100.0f;
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_master) { b_master->text_color = 0xFFFFD000; b_master->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        int music_pct = static_cast<int>(std::round(music_volume_ * 100.0f));
        std::snprintf(buf, sizeof(buf), "Volume da Musica: %d%%", music_pct);
        auto* b_music = ui_->add_button({x, cy}, {col_w, ctrl_h}, buf, [this]() {
            int pct = static_cast<int>(std::round(music_volume_ * 100.0f));
            pct = (pct >= 100) ? 0 : (pct + 10);
            music_volume_ = pct / 100.0f;
            if (audio_) audio_->set_music_volume(music_volume_);
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_music) { b_music->text_color = 0xFFFFD000; b_music->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        int sfx_pct = static_cast<int>(std::round(sfx_volume_ * 100.0f));
        std::snprintf(buf, sizeof(buf), "Volume dos Efeitos: %d%%", sfx_pct);
        auto* b_sfx = ui_->add_button({x, cy}, {col_w, ctrl_h}, buf, [this]() {
            int pct = static_cast<int>(std::round(sfx_volume_ * 100.0f));
            pct = (pct >= 100) ? 0 : (pct + 10);
            sfx_volume_ = pct / 100.0f;
            if (audio_) audio_->set_sfx_volume(sfx_volume_);
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_sfx) { b_sfx->text_color = 0xFFFFD000; b_sfx->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        std::string mus_label = music_enabled_ ? "Musica: LIGADA" : "Musica: DESLIGADA";
        auto* b_mus_toggle = ui_->add_button({x, cy}, {col_w, ctrl_h}, mus_label, [this]() {
            music_enabled_ = !music_enabled_;
            if (audio_) audio_->set_music_enabled(music_enabled_);
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_mus_toggle) {
            b_mus_toggle->text_color = music_enabled_ ? 0xFF88FF88 : 0xFFFF8888;
            b_mus_toggle->text_size = 17.0f * sy;
        }
        cy += ctrl_h + gap;

        std::string sfx_label = sfx_enabled_ ? "Efeitos Sonoros: LIGADOS" : "Efeitos Sonoros: DESLIGADOS";
        auto* b_sfx_toggle = ui_->add_button({x, cy}, {col_w, ctrl_h}, sfx_label, [this]() {
            sfx_enabled_ = !sfx_enabled_;
            if (audio_) audio_->set_sfx_enabled(sfx_enabled_);
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_sfx_toggle) {
            b_sfx_toggle->text_color = sfx_enabled_ ? 0xFF88FF88 : 0xFFFF8888;
            b_sfx_toggle->text_size = 17.0f * sy;
        }
        cy += ctrl_h + gap;

        auto* b_save = ui_->add_button({x, cy}, {col_w, ctrl_h}, "SALVAR CONFIGURACOES", [this]() {
            save_options();
        }, 0xFF332211, 0xFF553311);
        if (b_save) { b_save->text_color = 0xFFFFD000; b_save->text_size = 17.0f * sy; }
    }
    // ── Aba: VIDEO ──
    else if (tab_index == 1) {
        const char* q_str = (graphics_quality_ > 1.4f) ? "Alta" : (graphics_quality_ > 0.6f ? "Media" : "Baixa");
        char q_buf[64];
        std::snprintf(q_buf, sizeof(q_buf), "Qualidade Grafica: %s", q_str);
        auto* b_q = ui_->add_button({x, cy}, {col_w, ctrl_h}, q_buf, [this]() {
            if (graphics_quality_ < 0.6f) graphics_quality_ = 1.0f;
            else if (graphics_quality_ < 1.4f) graphics_quality_ = 2.0f;
            else graphics_quality_ = 0.0f;
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_q) { b_q->text_color = 0xFFFFD000; b_q->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        std::string fps_label = show_fps_ ? "Exibir FPS: LIGADO" : "Exibir FPS: DESLIGADO";
        auto* b_fps = ui_->add_button({x, cy}, {col_w, ctrl_h}, fps_label, [this]() {
            show_fps_ = !show_fps_;
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_fps) {
            b_fps->text_color = show_fps_ ? 0xFF88FF88 : 0xFFFF8888;
            b_fps->text_size = 17.0f * sy;
        }
        cy += ctrl_h + gap;

        auto* b_res = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Resolucao: Nativa da Tela", []() {}, 0xFF221810, 0xFF443020);
        if (b_res) { b_res->text_color = 0xFFCCCCCC; b_res->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_save = ui_->add_button({x, cy}, {col_w, ctrl_h}, "SALVAR CONFIGURACOES", [this]() {
            save_options();
        }, 0xFF332211, 0xFF553311);
        if (b_save) { b_save->text_color = 0xFFFFD000; b_save->text_size = 17.0f * sy; }
    }
    // ── Aba: GAMEPLAY ──
    else if (tab_index == 2) {
        auto* b_dif = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Dificuldade: Normal", []() {}, 0xFF221810, 0xFF443020);
        if (b_dif) { b_dif->text_color = 0xFFFFD000; b_dif->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_tips = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Dicas de Jogo: Ativadas", []() {}, 0xFF221810, 0xFF443020);
        if (b_tips) { b_tips->text_color = 0xFFCCCCCC; b_tips->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_spd = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Velocidade do Jogo: Normal (1.0x)", []() {}, 0xFF221810, 0xFF443020);
        if (b_spd) { b_spd->text_color = 0xFFCCCCCC; b_spd->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_save = ui_->add_button({x, cy}, {col_w, ctrl_h}, "SALVAR CONFIGURACOES", [this]() {
            save_options();
        }, 0xFF332211, 0xFF553311);
        if (b_save) { b_save->text_color = 0xFFFFD000; b_save->text_size = 17.0f * sy; }
    }
    // ── Aba: NETWORK ──
    else if (tab_index == 3) {
        auto* b_upnp = ui_->add_button({x, cy}, {col_w, ctrl_h}, "UPnP: Ativado", []() {}, 0xFF221810, 0xFF443020);
        if (b_upnp) { b_upnp->text_color = 0xFFCCCCCC; b_upnp->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_port = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Porta Padrao: 2300", []() {}, 0xFF221810, 0xFF443020);
        if (b_port) { b_port->text_color = 0xFFCCCCCC; b_port->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_mp = ui_->add_button({x, cy}, {col_w, ctrl_h}, "Abrir Multiplayer LAN", [this]() { open_multiplayer(); }, 0xFF332211, 0xFF553311);
        if (b_mp) { b_mp->text_color = 0xFFFFD000; b_mp->text_size = 17.0f * sy; }
    }
    // ── Aba: CONTROLS ──
    else if (tab_index == 4) {
        std::string joy_label = enable_virtual_joystick_ ? "Joystick Virtual: ATIVADO" : "Joystick Virtual: DESATIVADO";
        auto* b_joy = ui_->add_button({x, cy}, {col_w, ctrl_h}, joy_label, [this]() {
            enable_virtual_joystick_ = !enable_virtual_joystick_;
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_joy) {
            b_joy->text_color = enable_virtual_joystick_ ? 0xFF88FF88 : 0xFFFF8888;
            b_joy->text_size = 17.0f * sy;
        }
        cy += ctrl_h + gap;

        char sens_buf[64];
        std::snprintf(sens_buf, sizeof(sens_buf), "Sensibilidade Touch: %.1fx", touch_sensitivity_);
        auto* b_sens = ui_->add_button({x, cy}, {col_w, ctrl_h}, sens_buf, [this]() {
            if (touch_sensitivity_ < 0.9f) touch_sensitivity_ = 1.0f;
            else if (touch_sensitivity_ < 1.4f) touch_sensitivity_ = 1.5f;
            else touch_sensitivity_ = 0.5f;
            save_options();
            create_options_menu();
        }, 0xFF221810, 0xFF443020);
        if (b_sens) { b_sens->text_color = 0xFFFFD000; b_sens->text_size = 17.0f * sy; }
        cy += ctrl_h + gap;

        auto* b_save = ui_->add_button({x, cy}, {col_w, ctrl_h}, "SALVAR CONFIGURACOES", [this]() {
            save_options();
        }, 0xFF332211, 0xFF553311);
        if (b_save) { b_save->text_color = 0xFFFFD000; b_save->text_size = 17.0f * sy; }
    }

    // ── Botão VOLTAR (destacado com borda dourada e hitbox acessível) ──
    ui_->add_background_quad({back_x - 2.0f, back_y - 2.0f}, {back_w + 4.0f, back_h + 4.0f}, INVALID_TEXTURE, 0xFFFFD000);
    auto* back_btn = ui_->add_button({back_x, back_y}, {back_w, back_h}, "VOLTAR",
                                    [this]() {
                                        return_to_main_menu();
                                    }, 0xFF332211, 0xFF553311);
    if (back_btn) {
        back_btn->text_color = 0xFFFFD000;
        back_btn->text_size = 18.0f * sy;
    }
}

void Engine::load_options() {
    std::string path = config_.data_path + "/options.cfg";
    std::ifstream in(path);
    if (!in) {
        in.open("/storage/emulated/0/Documents/AoM-Native-Port/options.cfg");
    }
    if (!in) return;

    std::string key;
    float val;
    while (in >> key >> val) {
        if (key == "master_volume") master_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "music_volume") music_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "sfx_volume") sfx_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "graphics_quality") graphics_quality_ = std::clamp(val, 0.0f, 2.0f);
        else if (key == "show_fps") show_fps_ = (val > 0.5f);
        else if (key == "music_enabled") music_enabled_ = (val > 0.5f);
        else if (key == "sfx_enabled") sfx_enabled_ = (val > 0.5f);
        else if (key == "enable_virtual_joystick") enable_virtual_joystick_ = (val > 0.5f);
        else if (key == "touch_sensitivity") touch_sensitivity_ = std::clamp(val, 0.1f, 3.0f);
    }
    LOGI("Loaded options: master=%.2f, music=%.2f, sfx=%.2f", master_volume_, music_volume_, sfx_volume_);
}

void Engine::save_options() {
    std::string path = config_.data_path + "/options.cfg";
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        out.open("/storage/emulated/0/Documents/AoM-Native-Port/options.cfg", std::ios::trunc);
    }
    if (!out) return;

    out << "master_volume " << master_volume_ << "\n";
    out << "music_volume " << music_volume_ << "\n";
    out << "sfx_volume " << sfx_volume_ << "\n";
    out << "graphics_quality " << graphics_quality_ << "\n";
    out << "show_fps " << (show_fps_ ? 1.0f : 0.0f) << "\n";
    out << "music_enabled " << (music_enabled_ ? 1.0f : 0.0f) << "\n";
    out << "sfx_enabled " << (sfx_enabled_ ? 1.0f : 0.0f) << "\n";
    out << "enable_virtual_joystick " << (enable_virtual_joystick_ ? 1.0f : 0.0f) << "\n";
    out << "touch_sensitivity " << touch_sensitivity_ << "\n";
}

} // namespace aom
