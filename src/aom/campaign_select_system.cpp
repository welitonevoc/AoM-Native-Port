#include "aom/campaign_select_system.h"

/// @file campaign_select_system.cpp
/// @brief Implementacao da tela de selecao de cenario da campanha.

#include "engine/engine.h"
#include "ui/ui.h"

#include <algorithm>
#include <string>

namespace aom {

bool CampaignSelectSystem::initialize(Engine* engine, UIManager* ui, Engine* provider,
                                       int width, int height) {
    shutdown();

    engine_ = engine;
    ui_ = ui;
    provider_ = provider ? provider : engine;

    if (!ui_ || !provider_) return false;

    // ── Coleta os cenarios jogaveis da campanha base e da expansao ─────────
    auto collect = [this](CampaignManager* mgr, bool expansion) {
        if (!mgr) return;
        const Campaign* campaign = mgr->current_campaign();
        if (!campaign) return;
        for (const CampaignScenario& scenario : campaign->scenarios) {
            if (scenario.unlisted) continue; // cutscenes nao aparecem na lista
            scenarios_.push_back(Entry{&scenario, expansion});
        }
    };
    collect(provider_->campaign_manager(), false);
    collect(provider_->expansion_campaign_manager(), true);

    if (scenarios_.empty()) return false;

    ui_->clear();

    const float vw = static_cast<float>(width);
    const float vh = static_cast<float>(height);
    const float sx = (vw > 0.0f) ? (vw / 1024.0f) : 1.0f;
    const float sy = (vh > 0.0f) ? (vh / 768.0f) : 1.0f;

    ui_->add_label("SELECIONAR CENARIO", {vw * 0.5f - 150.0f, vh * 0.06f}, 30.0f,
                   0xFFFFD000);

    const float btn_w = 380.0f * sx;
    const float btn_h = 40.0f * sy;
    const float gap = 8.0f * sy;
    float y = vh * 0.16f;

    // Limite de itens visiveis: evita telas fora do viewport em campanhas longas.
    constexpr size_t kMaxVisible = 12;
    const size_t visible = std::min(scenarios_.size(), kMaxVisible);

    for (size_t i = 0; i < visible; ++i) {
        const Entry entry = scenarios_[i];
        std::string label = entry.scenario->display_name;
        if (label.empty()) label = entry.scenario->filename;
        if (label.empty()) label = "CENARIO " + std::to_string(i + 1);

        CampaignSelectSystem* self = this;
        ui_->add_button({(vw - btn_w) * 0.5f, y}, {btn_w, btn_h}, label,
                        [self, entry]() { self->start_scenario(entry); });
        y += btn_h + gap;
    }

    if (scenarios_.size() > visible) {
        ui_->add_label("... e mais " + std::to_string(scenarios_.size() - visible) +
                           " cenarios",
                       {vw * 0.5f - 80.0f, y + 4.0f}, 14.0f, 0xFFCCCCCC);
    }

    ui_->add_button({(vw - 220.0f) * 0.5f, vh * 0.88f}, {220.0f, 44.0f}, "VOLTAR",
                    [this]() {
                        if (provider_) provider_->return_to_main_menu();
                    });

    if (ui_ && provider_) {
        ui_->resolve_text([this](const std::string& k) { return provider_->tr(k); });
    }

    initialized_ = true;
    return true;
}

void CampaignSelectSystem::shutdown() {
    scenarios_.clear();
    engine_ = nullptr;
    ui_ = nullptr;
    provider_ = nullptr;
    initialized_ = false;
}

void CampaignSelectSystem::start_scenario(const Entry& entry) const {
    if (!provider_ || !entry.scenario) return;

    // Inicia a missao pelo cenario exato (a engine resolve o .scn correspondente).
    provider_->start_campaign_mission(entry.scenario);
}

} // namespace aom
