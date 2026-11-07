#pragma once

#include "ui/menu_parser.h"
#include "ui/ui.h"
#include "engine/engine.h"

#include <memory>
#include <vector>
#include <string>

namespace aom {

/// Sistema de menu principal usando formato XML próprio
class MainMenuSystem {
public:
    MainMenuSystem() = default;
    ~MainMenuSystem() = default;

    MainMenuSystem(const MainMenuSystem&) = delete;
    MainMenuSystem& operator=(const MainMenuSystem&) = delete;

    /// Inicializa o menu principal carregando o XML
    /// @param engine Referência ao engine (para callbacks)
    /// @param ui UIManager onde os botões serão criados
    /// @param screen_w Largura da tela em pixels
    /// @param screen_h Altura da tela em pixels
    /// @return true se inicializou com sucesso
    bool initialize(Engine* engine, UIManager* ui, int screen_w, int screen_h);

    /// Shutdown - limpa os botões
    void shutdown();

    /// Atualiza o menu (processa toque, etc.)
    void update(float delta_time);

    /// Renderiza o menu na tela
    void render();

    /// Processa evento de toque
    /// @param touch Evento de toque (x, y, state)
    void on_touch_event(const TouchEvent& touch);

    /// Obter os botões para inspeção/debug
    [[nodiscard]] const std::vector<MenuButton>& buttons() const { return menu_buttons_; }
    [[nodiscard]] const std::vector<UIButton*>& ui_buttons() const { return ui_buttons_; }

private:
    Engine* engine_ = nullptr;
    UIManager* ui_ = nullptr;

    int screen_w_ = 1024;
    int screen_h_ = 768;

    // Dados do menu carregados do XML
    std::vector<MenuButton> menu_buttons_;
    std::vector<UIButton*> ui_buttons_; // botões criados no UIManager

    // Parser de menu
    MenuParser menu_parser_;

    /// Posições padrão dos botões (serão ajustadas pelo parser/ layout)
    void setup_default_positions();

    /// Converter MenuButton em UIButton no UIManager
    void create_ui_buttons();
};

} // namespace aom