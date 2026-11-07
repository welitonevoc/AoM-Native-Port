#pragma once

#include "ui/ui.h"
#include "engine/engine.h"

#include <memory>
#include <vector>
#include <string>

namespace aom {

/// Sistema de multiplayer - tela de host/join para jogos LAN
class MultiplayerSystem {
public:
    MultiplayerSystem() = default;
    ~MultiplayerSystem() = default;

    MultiplayerSystem(const MultiplayerSystem&) = delete;
    MultiplayerSystem& operator=(const MultiplayerSystem&) = delete;

    /// Inicializa o sistema multiplayer
    /// @param engine Referência ao engine (para configurar rede)
    /// @param ui UIManager onde os elementos serão criados
    /// @param screen_w Largura da tela em pixels
    /// @param screen_h Altura da tela em pixels
    /// @return true se inicializou com sucesso
    bool initialize(Engine* engine, UIManager* ui, int screen_w, int screen_h);

    /// Shutdown - limpa os elementos da UI
    void shutdown();

    /// Atualiza o sistema multiplayer
    void update(float delta_time);

    /// Renderiza a tela multiplayer
    void render();

    /// Processa evento de toque
    void on_touch_event(const TouchEvent& touch);

    /// Obter o modo atual (0 = host, 1 = join)
    [[nodiscard]] int mode() const { return mode_; }

private:
    Engine* engine_ = nullptr;
    UIManager* ui_ = nullptr;

    int screen_w_ = 1024;
    int screen_h_ = 768;

    // Estado do multiplayer
    int mode_ = 0; // 0 = Host, 1 = Join
    std::string ip_address_;     // IP para conexão
    std::string port_;           // Porta para conexão
    std::string server_name_;    // Nome do servidor

    // Botões da UI
    std::vector<UIButton*> buttons_;

    /// Posições padrão dos elementos da tela
    void setup_positions();

    /// Criar os botões da tela multiplayer
    void create_buttons();

    /// Texto de instrução
    void setup_instructions();
};

} // namespace aom