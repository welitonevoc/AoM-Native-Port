#include "aom/main_menu.h"

#include "ui/ui.h"
#include "renderer/gles_renderer.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace aom {

MainMenuSystem::MainMenuSystem() = default;
MainMenuSystem::~MainMenuSystem() = default;

bool MainMenuSystem::initialize(Engine* engine, UIManager* ui, int screen_w, int screen_h) {
    if (!engine || !ui) {
        std::cerr << "ERRO: Engine ou UIManager nulos em MainMenuSystem::initialize()" << std::endl;
        return false;
    }

    engine_ = engine;
    ui_ = ui;
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    // Carregar menu XML - usamos o caminho relativo ou absoluto
    // O uimainmenu.xml está em tests/reference/databar/
    // No build final, ele viria dos assets do jogo
    std::string xml_path = "databar/uimainmenu.xml";

    // Temporário: usar conteúdo inline se arquivo não existir
    std::string xml_content;
    std::ifstream file(xml_path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        xml_content = buffer.str();
        file.close();
    } else {
        // Conteúdo do uimainmenu.xml como fallback
        xml_content = R"(
<?xml version="1.0" encoding="utf-8"?>
<menu>
  <button>
    <label>Learn To Play</label>
    <action>start_game</action>
  </button>
  <button>
    <label>Campaign</label>
    <action>create_campaign_menu</action>
  </button>
  <button>
    <label>Single Player</label>
    <action>create_single_player_menu</action>
  </button>
  <button>
    <label>Multiplayer</label>
    <action>open_multiplayer</action>
  </button>
  <button>
    <label>Options</label>
    <action>open_options</action>
  </button>
  <button>
    <label>Credits</label>
    <action>open_credits</action>
  </button>
  <button>
    <label>Exit</label>
    <action>quit_app</action>
  </button>
</menu>
)";
    }

    // Parsear o menu
    MenuParseResult result;
    if (!menu_parser_.load_from_file(xml_content, result)) {
        std::cerr << "ERRO ao parsear menu XML: " << result.error_message << std::endl;
        return false;
    }

    menu_buttons_ = std::move(result.buttons);
    std::cout << "Menu principal carregado: " << menu_buttons_.size() << " botões" << std::endl;

    // Posicionamento padrão (será ajustado posteriormente pelo layout)
    setup_default_positions();

    // Criar botões no UIManager
    create_ui_buttons();

    std::cout << "MainMenuSystem inicializado com sucesso!" << std::endl;
    return true;
}

void MainMenuSystem::shutdown() {
    // Remover botões do UIManager
    for (auto* btn : ui_buttons_) {
        if (btn && ui_) {
            // O UIManager não tem remove_button, então vamos apenas limpar a referência
            // Em um projeto completo, seria melhor rastrear e destruir corretamente
        }
    }
    ui_buttons_.clear();
    menu_buttons_.clear();
}

void MainMenuSystem::update(float delta_time) {
    // Atualizações do menu (animations, etc.)
    // Por enquanto, nada aqui - as interações acontecem via on_touch_event
}

void MainMenuSystem::render() {
    if (!renderer_) return;

    // Limpar tela com cor escura
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Desenhar fundo do menu (retângulo central)
    // TODO: usar texturas reais do AoM quando disponíveis
    glColor4f(0.2f, 0.2f, 0.3f, 1.0f);

    // Desenhar cada botão (simple rect + text)
    for (size_t i = 0; i < ui_buttons_.size(); ++i) {
        UIButton* btn = ui_buttons_[i];
        if (!btn) continue;

        // Desenhar retângulo do botão
        glm::vec2 pos = btn->pos;
        glm::vec2 size = btn->size;

        // Desenhar com a cor atual do botão
        uint32_t color = btn->current_color;
        float r = (color & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = ((color >> 16) & 0xFF) / 255.0f;
        // alpha = ((color >> 24) & 0xFF) / 255.0f;

        // Desenhar quad simplificado
        // glRectf(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    }
}

void MainMenuSystem::on_touch_event(const TouchEvent& touch) {
    if (!ui_) return;

    // Processar evento no UIManager
    ui_->on_touch_event(touch);

    // Verificar quais botões foram clicados
    const auto& buttons = ui_->buttons();
    for (size_t i = 0; i < buttons.size(); ++i) {
        const UIButton& btn = buttons[i];

        // Hit test - verificar se o toque está dentro do botão
        if (UIManager::hit_test(btn, touch.x, touch.y)) {
            // Disparar a ação associada ao botão
            // O callback foi definido ao criar o botão
            // A ação é o nome string (ex: "start_game")

            // Log para debug
            std::cout << "Botão clicado: \"" << btn.label << "\" - ação: " << std::endl;

            // Trigger engine action based on button action string
            if (engine_) {
                // Aqui conectaríamos com o Engine::start_game(), etc.
                // Por enquanto, apenas log
                // engine_->start_game(); // se for o botão "Learn To Play"
                // engine_->open_options(); // se for "Options"
                // engine_->open_credits(); // se for "Credits"
                // engine_->quit_app();     // se for "Exit"
            }

            // Só processar o DOWN, não repetir em MOVE/UP
            // O UIManager já lida com o estado do toque
            break;
        }
    }
}

void MainMenuSystem::setup_default_positions() {
    // Definir posições padrão para os botões do menu principal
    // O menu do AoM original usa resolução 1024x768 centralizada
    float scale_x = static_cast<float>(screen_w_) / 1024.0f;
    float scale_y = static_cast<float>(screen_h_) / 768.0f;

    // Posicionar verticalmente com espaçamento
    int start_y = 200 * scale_y; // Y inicial
    int spacing = 70 * scale_y;  // Espaço entre botões
    int button_width = 200 * scale_x;
    int button_height = 50 * scale_y;

    for (size_t i = 0; i < menu_buttons_.size() && i < 7; ++i) {
        MenuButton& btn = menu_buttons_[i];
        btn.rect = glm::vec4(
            (screen_w_ - button_width) / 2.0f,  // centralizado horizontalmente
            start_y + (i * (button_height + spacing)), // posições verticais
            button_width,
            button_height
        );
    }
}

void MainMenuSystem::create_ui_buttons() {
    if (!ui_) return;

    ui_buttons_.clear();

    int start_y = 200; // Y inicial em pixels base
    int spacing = 70;  // Espaço entre botões
    int button_width = 200;
    int button_height = 50;

    for (size_t i = 0; i < menu_buttons_.size() && i < 7; ++i) {
        MenuButton& btn = menu_buttons_[i];

        // Definir rect posicionado
        btn.rect = glm::vec4(
            (screen_w_ - button_width) / 2.0f,
            start_y + (i * (button_height + spacing)),
            button_width,
            button_height
        );

        // Criar callback que captura a ação do botão
        auto callback = [action = btn.action]() {
            // Disparar a ação correspondente
            // Em um projeto completo, isso chamaria o Engine correspondente
            std::cout << "Ação disparada: " << action << " - label: " << btn.label << std::endl;

            // Mapeamento simples para funções do Engine
            // if (engine_) {
            //     if (action == "start_game") engine_->start_game();
            //     else if (action == "open_options") engine_->open_options();
            //     else if (action == "open_credits") engine_->open_credits();
            //     else if (action == "quit_app") engine_->quit_app();
            // }
        };

        // Criar botão no UIManager
        // Cores do AoM: ouro #FFD000 para texto, fundo escuro
        uint32_t normal_color = 0xFFFFF000; // ouro
        uint32_t pressed_color = 0xFF888888; // cinza mais claro

        UIButton* button = ui_->add_button(
            btn.rect.xy(),           // pos (glm::vec2)
            glm::vec2(button_width, button_height), // size
            btn.label,               // label
            callback,                // onClick
            normal_color,
            pressed_color
        );

        if (button) {
            ui_buttons_.push_back(button);
        }
    }

    std::cout << "Criados " << ui_buttons_.size() << " botões no UIManager" << std::endl;
}

} // namespace aom