#include "aom/multiplayer_system.h"

#include "renderer/gles_renderer.h"

#include <iostream>

namespace aom {

MultiplayerSystem::MultiplayerSystem() = default;
MultiplayerSystem::~MultiplayerSystem() = default;

bool MultiplayerSystem::initialize(Engine* engine, UIManager* ui, int screen_w, int screen_h) {
    if (!engine || !ui) {
        std::cerr << "ERRO: Ponteiros nulos em MultiplayerSystem::initialize()" << std::endl;
        return false;
    }

    engine_ = engine;
    ui_ = ui;
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    // Modo padrão: Host
    mode_ = 0;
    ip_address_ = "192.168.1.100"; // IP padrão LAN
    port_ = "20000"; // Porta padrão

    setup_positions();
    create_buttons();
    setup_instructions();

    std::cout << "MultiplayerSystem inicializado - Modo: " << (mode_ == 0 ? "Host" : "Join") << std::endl;
    return true;
}

void MultiplayerSystem::shutdown() {
    // Remover botões do UIManager
    for (auto* btn : buttons_) {
        if (btn && ui_) {
            // UIManager não tem remove_button, apenas limpar referência
        }
    }
    buttons_.clear();
}

void MultiplayerSystem::update(float delta_time) {
    // Atualizações do sistema multiplayer
    // As interações acontecem via on_touch_event
}

void MultiplayerSystem::render() {
    if (!renderer_) return;

    // Limpar tela com cor escura
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Desenhar título "Multiplayer"
    // TODO: desenhar título usando o renderer

    // Descrever cada botão
    for (size_t i = 0; i < buttons_.size(); ++i) {
        UIButton* btn = buttons_[i];
        if (!btn) continue;

        // Desenhar retângulo do botão
        glm::vec2 pos = btn->pos;
        glm::vec2 size = btn->size;

        // Cor do botão - azul do AoM
        glColor4f(0.1f, 0.2f, 0.4f, 1.0f);
        // glRectf(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    }
}

void MultiplayerSystem::on_touch_event(const TouchEvent& touch) {
    if (!ui_) return;

    // Processar evento no UIManager
    ui_->on_touch_event(touch);

    // Verificar quais botões foram clicados
    const auto& btns = ui_->buttons();
    for (size_t i = 0; i < btns.size() && i < buttons_.size(); ++i) {
        const UIButton& btn = btns[i];
        if (!btn.onClick) continue;

        // Hit test - verificar se o toque está dentro do botão
        if (UIManager::hit_test(btn, touch.x, touch.y)) {
            // Disparar a ação do botão
            std::cout << "Botão multiplayer clicado: índex " << i << std::endl;

            // Modo 0 = Host, 1 = Join
            if (mode_ == 0) {
                // TODO: Host game with IP:port
                std::cout << "Hosting game at " << ip_address_ << ":" << port_ << std::endl;
                // engine_->start_host_game(ip_address_, port_);
            } else if (mode_ == 1) {
                // TODO: Join game with IP:port
                std::cout << "Joining game at " << ip_address_ << ":" << port_ << std::endl;
                // engine_->join_game(ip_address_, port_);
            }

            // After multiplayer action, return to main menu or stay in multiplayer state
            break;
        }
    }
}

void MultiplayerSystem::setup_positions() {
    // Definir posições padrão para a tela multiplayer
    // Resolução virtual 1024x768
    float scale_x = static_cast<float>(screen_w_) / 1024.0f;
    float scale_y = static_cast<float>(screen_h_) / 768.0f;

    int title_y = 150 * scale_y;
    int button_start_y = 250 * scale_y;
    int button_height = 50 * scale_y;
    int spacing = 20 * scale_y;
    int button_width = 300 * scale_x;
    int ip_field_height = 30 * scale_y;

    // Título
    // TODO: posicionar título "Multiplayer LAN"

    // Botões: Host e Join
    // Botão Host (modo 0)
    // buttons_.push_back(...);

    // Botão Join (modo 1)
    // buttons_.push_back(...);

    // Campos de entrada IP e Porta
    // TODO: posicionar campos de texto para IP e Porta
}

void MultiplayerSystem::create_buttons() {
    if (!ui_) return;

    int button_width = 300;
    int button_height = 50;
    int start_y = 250;
    int spacing = 20;

    // Botão Host (Modo 0)
    mode_ = 0;
    auto host_callback = [this]() {
        std::cout << "Modo Host selecionado - IP: " << ip_address_ << ":" << port_ << std::endl;
        // TODO: Chamar engine_->start_host_game(ip_address_, port_)
        // engine_->set_state(EngineState::MULTIPLAYER_LAN);
    };

    uint32_t host_normal = 0xFF1A5E9A; // azul
    uint32_t host_pressed = 0xFF2A7AE0;

    UIButton* host_btn = ui_->add_button(
        glm::vec2((screen_w_ - button_width) / 2.0f, start_y),
        glm::vec2(button_width, button_height),
        "Host Game (LAN)",
        host_callback,
        host_normal,
        host_pressed
    );
    if (host_btn) buttons_.push_back(host_btn);

    // Botão Join (Modo 1)
    mode_ = 1;
    auto join_callback = [this]() {
        std::cout << "Modo Join selecionado - Conectando a " << ip_address_ << ":" << port_ << std::endl;
        // TODO: Chamar engine_->join_game(ip_address_, port_)
        // engine_->set_state(EngineState::MULTIPLAYER_LOBBY);
    };

    uint32_t join_normal = 0xFF1A5E9A; // azul
    uint32_t join_pressed = 0xFF2A7AE0;

    UIButton* join_btn = ui_->add_button(
        glm::vec2((screen_w_ - button_width) / 2.0f, start_y + button_height + spacing),
        glm::vec2(button_width, button_height),
        "Join Game (LAN)",
        join_callback,
        join_normal,
        join_pressed
    );
    if (join_btn) buttons_.push_back(join_btn);

    // TODO: Adicionar campos de entrada para IP e Porta
    // TODO: Adicionar botão "Refresh" para descoberta de jogos
}

void MultiplayerSystem::setup_instructions() {
    // TODO: Instruções na tela
    // "Use o ícone da dock flutuante para acessar multiplayer"
    // "Configure IP e porta para jogos na mesma rede"
}
} // namespace aom