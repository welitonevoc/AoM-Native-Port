#pragma once

/// @file campaign_select_system.h
/// @brief Tela de seleção de cenário da campanha (lista de missões do .cpn/.cpx).
///
/// Monta a UI com um botão por cenário jogável da campanha base e da expansão
/// "The Titans" e permite iniciar a missão diretamente.

#include <cstddef>
#include <vector>

#include "campaign/campaign.h"

namespace aom {

class Engine;
class UIManager;

/// Gerencia a tela de seleção de cenário da campanha.
class CampaignSelectSystem {
public:
    CampaignSelectSystem() = default;
    ~CampaignSelectSystem() = default;

    CampaignSelectSystem(const CampaignSelectSystem&) = delete;
    CampaignSelectSystem& operator=(const CampaignSelectSystem&) = delete;

    /// Cria a UI de seleção.
    ///
    /// @param engine  Motor da aplicação (pode ser nullptr).
    /// @param ui      Gerenciador de UI onde os botoes serao criados.
    /// @param provider Motor usado para obter a campanha e iniciar a missao.
    /// @param width   Largura da janela em pixels.
    /// @param height  Altura da janela em pixels.
    /// @return true se a tela foi montada com pelo menos um cenario.
    bool initialize(Engine* engine, UIManager* ui, Engine* provider,
                    int width, int height);

    /// Limpa a UI criada e invalida as referencias.
    void shutdown();

    /// true quando a tela foi montada com sucesso.
    [[nodiscard]] bool initialized() const { return initialized_; }

    /// Numero de cenarios disponiveis na tela.
    [[nodiscard]] size_t scenario_count() const { return scenarios_.size(); }

private:
    /// Cenario exibido na lista.
    struct Entry {
        const CampaignScenario* scenario = nullptr; ///< Cenario (campanha base ou expansao)
        bool expansion = false;                     ///< true se veio da expansao The Titans
    };

    /// Inicia o cenario escolhido.
    void start_scenario(const Entry& entry) const;

    Engine* engine_ = nullptr;
    UIManager* ui_ = nullptr;
    Engine* provider_ = nullptr;
    std::vector<Entry> scenarios_;
    bool initialized_ = false;
};

} // namespace aom
