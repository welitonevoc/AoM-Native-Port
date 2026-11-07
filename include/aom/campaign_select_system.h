#pragma once

#include "ui/menu_parser.h"
#include "ui/ui.h"
#include "engine/engine.h"

#include <memory>
#include <vector>
#include <string>

namespace aom {

/// Sistema de seleção de campanha - mostra lista de cenários disponíveis
/// após o usuário clicar em "Campaign" no menu principal
class CampaignSelectSystem {
public:
    CampaignSelectSystem() = default;
    ~CampaignSelectSystem() = default;

    CampaignSelectSystem(const CampaignSelectSystem&) = delete;
    CampaignSelectSystem& operator=(const CampaignSelectSystem&) = delete;

    /// Inicializa o sistema de seleção de campanha
    /// @param engine Referência ao engine (para start_campaign_mission)
    /// @param ui UIManager onde os botões serão criados
    /// @param engine_ptr Ponteiro para engine (para chamar start_campaign_mission)
    /// @param screen_w Largura da tela em pixels
    /// @param screen_h Altura da tela em pixels
    /// @return true se inicializou com sucesso
    bool initialize(Engine* engine, UIManager* ui, Engine* engine_ptr, int screen_w, int screen_h);

    /// Shutdown - limpa os elementos da seleção
    void shutdown();

    /// Atualiza o sistema de seleção
    void update(float delta_time);

    /// Renderiza o ecrã de seleção
    void render();

    /// Processa evento de toque
    void on_touch_event(const TouchEvent& touch);

    /// Obter cenário selecionado (índice ou -1 se nenhum)
    [[nodiscard]] int selected_scenario_index() const { return selected_index_; }

private:
    Engine* engine_ = nullptr;
    UIManager* ui_ = nullptr;
    Engine* engine_ptr_ = nullptr; // Para chamar start_campaign_mission

    int screen_w_ = 1024;
    int screen_h_ = 768;

    // Dados dos cenários disponíveis (carregados do game config ou scenario archive)
    struct ScenarioInfo {
        std::string name;        ///< Nome exibido ao jogador
        std::string description; ///< Descrição curta
        std::string filename;    ///< Nome do arquivo do cenário
        int index;              ///< Índice para start_campaign_mission
    };
    std::vector<ScenarioInfo> scenarios_;

    // Botões da UI
    std::vector<UIButton*> scenario_buttons_;

    // Menu parser para carregar layouts
    MenuParser menu_parser_;

    /// Posições padrão dos botões de cenário
    void setup_scenario_positions();

    /// Carregar cenários disponíveis do game config / scenario archive
    void load_available_scenarios();

    /// Converter ScenarioInfo em UIButton no UIManager
    void create_scenario_buttons();
};

} // namespace aom