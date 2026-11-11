#include "engine/engine.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AoM_OptionsMenu", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "AoM_OptionsMenu", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#endif

namespace aom {

// ── Sistema de Abas (Tabs) para Options Menu ──
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

void Engine::create_options_menu() {
    // PRIMEIRO: Tenta usar o layout XMB autêntico do jogo
    // uioptions.xmb = menu de opções principal
    // uisoundsetdlg.xmb = configurações de som específicas
    if (show_ui_layout("uioptions")) {
        LOGI("create_options_menu: usando layout autêntico 'uioptions.xmb' do data.bar");
        const float vw = static_cast<float>(config_.window_width);
        const float vh = static_cast<float>(config_.window_height);
        
        const float import_w = 280.0f;
        const float import_h = 40.0f;
        const float import_x = vw - import_w - 24.0f;
        const float import_y = vh * 0.08f;
        
        ui_->add_button({import_x, import_y}, {import_w, import_h},
                        "IMPORTAR DADOS DO JOGO",
                        [this]() {
                            if (!request_game_data_import()) {
                                LOGW("Nao foi possivel abrir o seletor de pasta do jogo");
                            }
                        });
        
        if (!archives_loaded_) {
            ui_->add_label("Dados do jogo: carregados",
                           {import_x, import_y + import_h + 6.0f}, 14.0f, 0xFF88FF88);
        }
        return;
    }
    
    if (show_ui_layout("uisoundsetdlg")) {
        LOGI("create_options_menu: usando layout autêntico 'uisoundsetdlg.xmb' do data.bar");
        const float vw = static_cast<float>(config_.window_width);
        const float vh = static_cast<float>(config_.window_height);
        const float import_w = 280.0f;
        const float import_h = 40.0f;
        const float import_x = vw - import_w - 24.0f;
        const float import_y = vh * 0.08f;
        
        ui_->add_button({import_x, import_y}, {import_w, import_h},
                        "IMPORTAR DADOS DO JOGO",
                        [this]() {
                            if (!request_game_data_import()) {
                                LOGW("Nao foi possivel abrir o seletor de pasta do jogo");
                            }
                        });
        return;
    }

    // FALLBACK: Menu hardcoded fiel ao original Age of Mythology: The Titans
    LOGI("create_options_menu: layouts XMB não disponíveis, usando fallback hardcoded fiel ao original");
    
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
    ui_->add_label("CONFIGURACOES", {vw * 0.5f - 160.0f * sx, vh * 0.08f * sy}, 28.0f * sy, 0xFFFFD000);

    // ── Importação dos dados originais do jogo (Android SAF) ────────────────
    const float import_w = 280.0f * sx;
    const float import_h = 40.0f * sy;
    const float import_x = vw - import_w - 24.0f * sx;
    const float import_y = vh * 0.08f * sy;

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

    // ── Sistema de Abas (Tabs) ──
    static const TabDef tabs[] = {
        {"AUDIO", EngineState::UI_OPTIONS_AUDIO},
        {"VIDEO", EngineState::UI_OPTIONS_VIDEO},
        {"GAMEPLAY", EngineState::UI_OPTIONS_GAMEPLAY},
        {"NETWORK", EngineState::UI_OPTIONS_NETWORK},
        {"CONTROLS", EngineState::UI_OPTIONS_CONTROLS},
    };
    
    const float tab_w = 140.0f * sx;
    const float tab_h = 36.0f * sy;
    const float tabs_start_x = vw * 0.5f - (sizeof(tabs)/sizeof(tabs[0]) * tab_w + 4.0f * sx * (sizeof(tabs)/sizeof(tabs[0]) - 1)) * 0.5f;
    const float tabs_y = vh * 0.18f * sy;
    
    for (size_t i = 0; i < sizeof(tabs)/sizeof(tabs[0]); ++i) {
        const TabDef& tab = tabs[i];
        float tx = tabs_start_x + i * (tab_w + 4.0f * sx);
        
        bool is_active = (state_ == tab.state);
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
            btn->text_size = 16.0f * sy;
            if (is_active) {
                ui_->add_background_quad({tx, tabs_y + tab_h - 2.0f}, {tab_w, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
            }
        }
    }

    // Área de conteúdo
    const float content_x = vw * 0.1f;
    const float content_y = tabs_y + tab_h + 20.0f * sy;
    const float content_w = vw * 0.8f;
    const float content_h = vh * 0.7f;
    
    // Fundo da área de conteúdo
    ui_->add_background_quad({content_x, content_y}, {content_w, content_h}, INVALID_TEXTURE, 0xFF0A0806);
    ui_->add_background_quad({content_x, content_y}, {content_w, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);

    // Renderiza o painel da aba ativa
    int tab_index = 0;
    if (state_ == EngineState::UI_OPTIONS_AUDIO) tab_index = 0;
    else if (state_ == EngineState::UI_OPTIONS_VIDEO) tab_index = 1;
    else if (state_ == EngineState::UI_OPTIONS_GAMEPLAY) tab_index = 2;
    else if (state_ == EngineState::UI_OPTIONS_NETWORK) tab_index = 3;
    else if (state_ == EngineState::UI_OPTIONS_CONTROLS) tab_index = 4;
    render_options_tab(tab_index, content_x, content_y, content_w, content_h, sx, sy);

    // Botão Voltar
    float back_w = 200.0f * sx;
    float back_h = 44.0f * sy;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, back_h}, "VOLTAR",
                   [this]() { 
                       if (state_ != EngineState::OPTIONS) {
                           set_state(EngineState::OPTIONS);
                       } else {
                           return_to_main_menu(); 
                       }
                   });
}

// ── Renderiza painel de uma aba específica ──
void Engine::render_options_tab(int tab_index, float x, float y, float w, float h, float sx, float sy) {
    const float ctrl_h = 40.0f * sy;
    const float gap = 12.0f * sy;
    const float label_h = 24.0f * sy;
    const float col_w = w * 0.9f;
    const float start_x = x + w * 0.05f;
    float cy = y + 20.0f * sy;

    switch (tab_index) {
        case 0: render_audio_options(start_x, cy, col_w, ctrl_h, gap, 24.0f * sy, sx, sy); break;
        case 1: render_video_options(start_x, cy, col_w, ctrl_h, gap, 24.0f * sy, sx, sy); break;
        case 2: render_gameplay_options(start_x, cy, col_w, ctrl_h, gap, 24.0f * sy, sx, sy); break;
        case 3: render_network_options(start_x, cy, col_w, ctrl_h, gap, 24.0f * sy, sx, sy); break;
        case 4: render_controls_options(start_x, cy, col_w, ctrl_h, gap, 24.0f * sy, sx, sy); break;
    }
}

void Engine::render_audio_options(float x, float& cy, float w, float ctrl_h, float gap, float label_h, float sx, float sy) {
    ui_->add_label("VOLUME", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, 40.0f * sy}, "Volume Geral", master_volume_,
                    [this](float val) { master_volume_ = val; save_options(); });
    cy += 40.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, 40.0f * sy}, "Volume da Musica", music_volume_,
                    [this](float val) {
                        music_volume_ = val;
                        if (audio_) audio_->set_music_volume(val);
                        save_options();
                    });
    cy += 40.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, 40.0f * sy}, "Volume dos Efeitos", sfx_volume_,
                    [this](float val) {
                        sfx_volume_ = val;
                        if (audio_) audio_->set_sfx_volume(val);
                        save_options();
                    });
    cy += 40.0f * sy + gap;

    ui_->add_label("OPCOES DE AUDIO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Musica Ativada", music_enabled_,
                      [this](bool val) {
                          music_enabled_ = val;
                          if (audio_) audio_->set_music_enabled(val);
                          save_options();
                      });
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Efeitos Sonoros Ativados", sfx_enabled_,
                      [this](bool val) {
                          sfx_enabled_ = val;
                          if (audio_) audio_->set_sfx_enabled(val);
                          save_options();
                      });
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Voz das Unidades", true,
                      [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Som Ambiente", true,
                      [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, 40.0f * sy}, "Qualidade de Som", 0.75f,
                    [](float) {});
    cy += 40.0f * sy + gap;
}

void Engine::render_video_options(float x, float& cy, float w, float ctrl_h, float gap, float label_h, float sx, float sy) {
    ui_->add_label("QUALIDADE GRAFICA", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Qualidade Geral", graphics_quality_ / 2.0f,
                    [this](float val) { graphics_quality_ = static_cast<int>(val * 2.0f); save_options(); });
    cy += ctrl_h + gap;

    ui_->add_label("RESOLUCAO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "1920x1080", []() {});
    cy += ctrl_h + gap;
    ui_->add_button({x, cy}, {w, ctrl_h}, "1280x720", []() {});
    cy += ctrl_h + gap;
    ui_->add_button({x, cy}, {w, ctrl_h}, "960x540", []() {});
    cy += ctrl_h + gap;

    ui_->add_label("OPCOES AVANCADAS", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "VSync", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Filtragem Trilinear", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Compressao de Texturas", false, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Sombras", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Reflexos na Agua", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Particulas", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Exibir FPS", show_fps_,
                      [this](bool val) { show_fps_ = val; save_options(); });
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Brilho", 0.5f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Contraste", 0.5f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Gamma", 1.0f, [](float) {});
    cy += ctrl_h + gap;
}

void Engine::render_gameplay_options(float x, float& cy, float w, float ctrl_h, float gap, float label_h, float sx, float sy) {
    ui_->add_label("DIFICULDADE", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Facil", []() {});
    cy += ctrl_h + gap;
    ui_->add_button({x, cy}, {w, ctrl_h}, "Normal", []() {});
    cy += ctrl_h + gap;
    ui_->add_button({x, cy}, {w, ctrl_h}, "Dificil", []() {});
    cy += ctrl_h + gap;
    ui_->add_button({x, cy}, {w, ctrl_h}, "Titanic", []() {});
    cy += ctrl_h + gap;

    ui_->add_label("OPCOES DE JOGO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Dicas de Jogo", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Confirmar Exclusao", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Auto-Save", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Marcadores de Unidade", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Marcadores de Construcao", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Velocidade do Jogo", 1.0f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_label("MULTIPLAYER", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Permitir Observadores", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Chat Ativado", true, [](bool) {});
    cy += 32.0f * sy + gap;
}

void Engine::render_network_options(float x, float& cy, float w, float ctrl_h, float gap, float label_h, float sx, float sy) {
    ui_->add_label("CONEXAO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "UPnP Ativado", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Porta", 2300.0f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_label("MULTIPLAYER LAN", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Criar Sala", [this]() { open_multiplayer(); });
    cy += ctrl_h + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Entrar na Sala", [this]() { open_multiplayer(); });
    cy += ctrl_h + gap;

    ui_->add_label("SERVIDOR DEDICADO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Configurar Servidor", []() {});
    cy += ctrl_h + gap;
}

void Engine::render_controls_options(float x, float& cy, float w, float ctrl_h, float gap, float label_h, float sx, float sy) {
    ui_->add_label("TECLADO E MOUSE", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Inverter Eixo Y do Mouse", false, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Sensibilidade do Mouse", 1.0f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Velocidade de Scroll", 1.0f, [](float) {});
    cy += ctrl_h + gap;

    ui_->add_label("JOYSTICK / TOUCH", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Joystick Virtual", enable_virtual_joystick_,
                      [this](bool val) { enable_virtual_joystick_ = val; save_options(); });
    cy += 32.0f * sy + gap;

    ui_->add_slider({x, cy}, {w, ctrl_h}, "Sensibilidade Touch", touch_sensitivity_,
                    [this](float val) { touch_sensitivity_ = val; save_options(); });
    cy += ctrl_h + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Botao Direito = Comando", true, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_checkbox({x, cy}, {w, 32.0f * sy}, "Toque Duplo = Correr", false, [](bool) {});
    cy += 32.0f * sy + gap;

    ui_->add_label("ATALHOS DE TECLADO", {x, cy}, 24.0f * sy, 0xFFFFD000);
    cy += 24.0f * sy + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Configurar Atalhos", []() {});
    cy += ctrl_h + gap;

    ui_->add_button({x, cy}, {w, ctrl_h}, "Restaurar Padrao", []() {});
    cy += ctrl_h + gap;
}

} // namespace aom