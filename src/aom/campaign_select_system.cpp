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
    width_ = width;
    height_ = height;
    selected_index_ = 0;
    current_page_ = 0;
    difficulty_ = 1;

    if (!ui_ || !provider_) return false;

    // Obter progresso atual das campanhas
    size_t base_progress = 0;
    if (provider_->campaign_manager() && provider_->campaign_manager()->current_campaign()) {
        base_progress = provider_->campaign_manager()->current_index();
    }
    size_t xpack_progress = 0;
    if (provider_->expansion_campaign_manager() && provider_->expansion_campaign_manager()->current_campaign()) {
        xpack_progress = provider_->expansion_campaign_manager()->current_index();
    }

    // ── Coleta os cenarios jogaveis da campanha base e da expansao ─────────
    auto collect = [this, base_progress, xpack_progress](CampaignManager* mgr, bool expansion, size_t progress) {
        if (!mgr) return;
        const Campaign* campaign = mgr->current_campaign();
        if (!campaign) return;
        for (size_t i = 0; i < campaign->scenarios.size(); ++i) {
            const CampaignScenario& scenario = campaign->scenarios[i];
            if (scenario.unlisted) continue; // cutscenes nao aparecem na lista
            // Cenario esta desbloqueado se: ja foi concluido (i < progress) OU e o proximo a jogar (i == progress)
            bool is_completed = (i < progress);
            bool is_unlocked = (i <= progress);
            // So adicionamos a lista se estiver desbloqueado
            if (is_unlocked) {
                scenarios_.push_back(Entry{&scenario, expansion, is_completed});
            }
        }
    };
    collect(provider_->campaign_manager(), false, base_progress);
    collect(provider_->expansion_campaign_manager(), true, xpack_progress);

    if (scenarios_.empty()) return false;

    rebuild_ui();
    initialized_ = true;
    return true;
}

void CampaignSelectSystem::select_scenario(size_t index) {
    if (index >= scenarios_.size()) return;
    selected_index_ = index;
    rebuild_ui();
}

void CampaignSelectSystem::change_page(int delta) {
    constexpr size_t kItemsPerPage = 7;
    const size_t total_pages = (scenarios_.size() + kItemsPerPage - 1) / kItemsPerPage;
    if (total_pages == 0) return;

    if (delta > 0 && current_page_ + 1 < total_pages) {
        ++current_page_;
        rebuild_ui();
    } else if (delta < 0 && current_page_ > 0) {
        --current_page_;
        rebuild_ui();
    }
}

void CampaignSelectSystem::set_difficulty(int diff) {
    difficulty_ = std::clamp(diff, 0, 3);
    rebuild_ui();
}

std::string CampaignSelectSystem::get_map_for_scenario(const std::string& filename, bool expansion) const {
    if (expansion) return "ui/campmap_new_atlantis7.ddt";
    std::string lower = filename;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower.find("aomsp01") != std::string::npos || lower.find("aomsp02") != std::string::npos ||
        lower.find("aomsp03") != std::string::npos || lower.find("aomsp04") != std::string::npos ||
        lower.find("omens") != std::string::npos) {
        return "ui/campmap_atlantis7.ddt";
    }
    if (lower.find("aomsp05") != std::string::npos || lower.find("aomsp06") != std::string::npos ||
        lower.find("aomsp07") != std::string::npos || lower.find("aomsp08") != std::string::npos ||
        lower.find("aomsp09") != std::string::npos || lower.find("aomsp10") != std::string::npos ||
        lower.find("aomsp11") != std::string::npos) {
        return "ui/campmap_greece7.ddt";
    }
    if (lower.find("aomsp12") != std::string::npos || lower.find("aomsp13") != std::string::npos ||
        lower.find("aomsp14") != std::string::npos || lower.find("aomsp15") != std::string::npos ||
        lower.find("aomsp16") != std::string::npos || lower.find("aomsp17") != std::string::npos ||
        lower.find("aomsp18") != std::string::npos || lower.find("aomsp19") != std::string::npos) {
        return "ui/campmap_egypt7.ddt";
    }
    if (lower.find("aomsp20") != std::string::npos || lower.find("aomsp21") != std::string::npos ||
        lower.find("aomsp22") != std::string::npos || lower.find("aomsp23") != std::string::npos ||
        lower.find("aomsp24") != std::string::npos || lower.find("aomsp25") != std::string::npos ||
        lower.find("aomsp26") != std::string::npos || lower.find("aomsp27") != std::string::npos ||
        lower.find("aomsp28") != std::string::npos || lower.find("aomsp29") != std::string::npos) {
        return "ui/campmap_norselands3.ddt";
    }
    return "ui/campmap_atlantis7.ddt";
}

std::string CampaignSelectSystem::get_briefing_for_scenario(const std::string& filename, const std::string& title) const {
    std::string lower = filename;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower.find("aomsp01") != std::string::npos || lower.find("omens") != std::string::npos) {
        return "Defenda as praias e a cidadela de Atlantida contra o ataque repentino dos piratas sob o comando de Kamos.";
    }
    if (lower.find("aomsp02") != std::string::npos) {
        return "Persiga os invasores sobreviventes para proteger o porto e recuperar as reliquias sagradas de Poseidon.";
    }
    if (lower.find("aomsp03") != std::string::npos) {
        return "Desembarque na ilha dos piratas e destrua o acampamento fortificado para restaurar a ordem nos mares.";
    }
    if (lower.find("aomsp04") != std::string::npos) {
        return "Reuna a frota atlante e navegue pelo Mar Egeu rumo a Troia para responder ao chamado de Agamemnon.";
    }
    return "Comande suas tropas com sabedoria, desenvolva a economia e alcance os objetivos estrategicos do cenario.";
}

void CampaignSelectSystem::rebuild_ui() {
    if (!ui_ || !provider_) return;
    ui_->clear();

    const float vw = static_cast<float>(width_);
    const float vh = static_cast<float>(height_);
    const float sx = (vw > 0.0f) ? (vw / 1024.0f) : 1.0f;
    const float sy = (vh > 0.0f) ? (vh / 768.0f) : 1.0f;

    // ── 1. Fundo Autêntico e Moldura ──
    ui_->add_background_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xFF0A121A);

    // Barra superior de cabeçalho
    ui_->add_background_quad({0.0f, 0.0f}, {vw, 54.0f * sy}, INVALID_TEXTURE, 0xEE141E28);
    ui_->add_background_quad({0.0f, 54.0f * sy}, {vw, 2.0f * sy}, INVALID_TEXTURE, 0xFFFFD000);

    ui_->add_label("CAMPANHA: A QUEDA DO TRIDENTE", {28.0f * sx, 15.0f * sy}, 22.0f * sy, 0xFFFFD000);

    // ── Dimensões do Layout em Duas Colunas ──
    const float margin_x = 26.0f * sx;
    const float gap_x = 20.0f * sx;
    const float content_y = 68.0f * sy;
    const float content_h = vh - 130.0f * sy;

    const float col_left_w = (vw - margin_x * 2.0f - gap_x) * 0.42f;
    const float col_right_w = (vw - margin_x * 2.0f - gap_x) * 0.58f;
    const float col_right_x = margin_x + col_left_w + gap_x;

    // ── Coluna Esquerda: Lista de Cenários ──
    ui_->add_background_quad({margin_x, content_y}, {col_left_w, content_h}, INVALID_TEXTURE, 0xDD101A24);
    ui_->add_background_quad({margin_x, content_y}, {col_left_w, 2.0f * sy}, INVALID_TEXTURE, 0xFF886622);
    ui_->add_background_quad({margin_x, content_y + content_h - 2.0f * sy}, {col_left_w, 2.0f * sy}, INVALID_TEXTURE, 0xFF886622);

    ui_->add_label("SELECAO DE CENARIOS", {margin_x + 14.0f * sx, content_y + 12.0f * sy}, 16.0f * sy, 0xFFFFD000);

    // Paginação
    constexpr size_t kItemsPerPage = 7;
    const size_t total_scenarios = scenarios_.size();
    const size_t total_pages = (total_scenarios + kItemsPerPage - 1) / kItemsPerPage;
    if (current_page_ >= total_pages && total_pages > 0) current_page_ = total_pages - 1;

    const size_t page_start = current_page_ * kItemsPerPage;
    const size_t page_end = std::min(page_start + kItemsPerPage, total_scenarios);

    const float btn_x = margin_x + 12.0f * sx;
    const float btn_w = col_left_w - 24.0f * sx;
    const float btn_h = 40.0f * sy;
    const float btn_gap = 6.0f * sy;
    float cur_y = content_y + 38.0f * sy;

    CampaignSelectSystem* self = this;

    for (size_t i = page_start; i < page_end; ++i) {
        const Entry& entry = scenarios_[i];
        std::string label = std::to_string(i + 1) + ". ";
        if (!entry.scenario->display_name.empty()) {
            label += entry.scenario->display_name;
        } else {
            label += entry.scenario->filename;
        }

        // Indicar status: concluido (check), atual (seta), proximo (cadeado)
        bool is_last_completed = entry.completed;
        bool is_current_mission = (!entry.completed && i == page_start); // primeiro da pagina = proximo a jogar

        if (is_last_completed) {
            label += " ✓";
        } else if (is_current_mission) {
            label += " ▶";
        } else {
            label += " 🔒";
        }

        const bool is_selected = (i == selected_index_);
        uint32_t bg_color;
        uint32_t text_color;
        if (is_selected) {
            bg_color = 0xFF4A3810;
            text_color = 0xFFFFFFFF;
        } else if (is_last_completed) {
            bg_color = 0xFF1C3A1C; // verde escuro para concluido
            text_color = 0xFF88FF88;
        } else if (is_current_mission) {
            bg_color = 0xFF3A3010; // dourado para missao atual
            text_color = 0xFFFFD000;
        } else {
            bg_color = 0xFF1C2632;
            text_color = 0xFF888888; // cinza para bloqueado
        }
        const uint32_t border_color = is_selected ? 0xFFFFD000 : 0xFF3A4858;

        if (is_selected) {
            ui_->add_background_quad({btn_x - 1.5f * sx, cur_y - 1.5f * sy},
                                     {btn_w + 3.0f * sx, btn_h + 3.0f * sy},
                                     INVALID_TEXTURE, border_color);
        }

        size_t clicked_idx = i;
        UIButton* btn = ui_->add_button({btn_x, cur_y}, {btn_w, btn_h}, label,
                        [self, clicked_idx]() { self->select_scenario(clicked_idx); },
                        bg_color, 0xFF6A5018);
        if (btn) {
            btn->text_color = text_color;
            btn->text_size = 14.0f * sy;
        }

        cur_y += btn_h + btn_gap;
    }

    // Rodapé de navegação da lista
    const float nav_y = content_y + content_h - 44.0f * sy;
    const float nav_btn_w = 85.0f * sx;
    const float nav_btn_h = 32.0f * sy;

    if (current_page_ > 0) {
        ui_->add_button({btn_x, nav_y}, {nav_btn_w, nav_btn_h}, "< Anterior",
                        [self]() { self->change_page(-1); }, 0xFF223040, 0xFF3A4E66);
    }

    std::string page_str = "Pag. " + std::to_string(current_page_ + 1) + " / " +
                           std::to_string(std::max<size_t>(1, total_pages));
    ui_->add_label(page_str, {btn_x + (btn_w - 70.0f * sx) * 0.5f, nav_y + 8.0f * sy}, 13.0f * sy, 0xFFCCCCCC);

    if (current_page_ + 1 < total_pages) {
        ui_->add_button({btn_x + btn_w - nav_btn_w, nav_y}, {nav_btn_w, nav_btn_h}, "Proximo >",
                        [self]() { self->change_page(1); }, 0xFF223040, 0xFF3A4E66);
    }

    // ── Coluna Direita: Detalhes da Missão & Mapa Regional ──
    ui_->add_background_quad({col_right_x, content_y}, {col_right_w, content_h}, INVALID_TEXTURE, 0xDD101A24);
    ui_->add_background_quad({col_right_x, content_y}, {col_right_w, 2.0f * sy}, INVALID_TEXTURE, 0xFF886622);
    ui_->add_background_quad({col_right_x, content_y + content_h - 2.0f * sy}, {col_right_w, 2.0f * sy}, INVALID_TEXTURE, 0xFF886622);

    if (selected_index_ < scenarios_.size()) {
        const Entry& sel_entry = scenarios_[selected_index_];
        const auto* scn = sel_entry.scenario;

        std::string title = scn->display_name.empty() ? scn->filename : scn->display_name;
        std::string scn_num_title = "CENARIO " + std::to_string(selected_index_ + 1) + ": " + title;
        ui_->add_label(scn_num_title, {col_right_x + 18.0f * sx, content_y + 14.0f * sy}, 19.0f * sy, 0xFFFFD000);

        std::string subtitle = sel_entry.expansion ? "The Titans Expansion • " : "Campanha: A Queda do Tridente • ";
        subtitle += scn->filename;
        ui_->add_label(subtitle, {col_right_x + 18.0f * sx, content_y + 38.0f * sy}, 12.0f * sy, 0xFFAAAAAA);

        // Mapa regional autêntico
        std::string map_tex_name = get_map_for_scenario(scn->filename, sel_entry.expansion);
        TextureHandle map_tex = provider_->load_ddt_texture(map_tex_name);

        const float map_w = std::min(260.0f * sx, col_right_w - 36.0f * sx);
        const float map_h = 150.0f * sy;
        const float map_x = col_right_x + (col_right_w - map_w) * 0.5f;
        const float map_y = content_y + 60.0f * sy;

        ui_->add_background_quad({map_x - 2.0f * sx, map_y - 2.0f * sy},
                                 {map_w + 4.0f * sx, map_h + 4.0f * sy},
                                 INVALID_TEXTURE, 0xFF886622);
        ui_->add_background_quad({map_x, map_y}, {map_w, map_h}, map_tex, 0xFFFFFFFF);

        // Sinopse
        const float brief_y = map_y + map_h + 12.0f * sy;
        ui_->add_label("OBJETIVO & SINOPSE:", {col_right_x + 18.0f * sx, brief_y}, 14.0f * sy, 0xFFFFD000);

        std::string briefing = get_briefing_for_scenario(scn->filename, title);
        ui_->add_label(briefing, {col_right_x + 18.0f * sx, brief_y + 20.0f * sy}, 13.0f * sy, 0xFFDDDDDD);

        // Seletor de Dificuldade
        const float diff_y = brief_y + 54.0f * sy;
        ui_->add_label("DIFICULDADE:", {col_right_x + 18.0f * sx, diff_y}, 13.0f * sy, 0xFFFFD000);

        static const char* kDiffNames[] = {"Facil", "Moderado", "Dificil", "Tita"};
        const float diff_btn_w = (col_right_w - 36.0f * sx - 3.0f * 6.0f * sx) / 4.0f;
        const float diff_btn_h = 28.0f * sy;
        for (int d = 0; d < 4; ++d) {
            float d_x = col_right_x + 18.0f * sx + d * (diff_btn_w + 6.0f * sx);
            const bool is_d_selected = (d == difficulty_);
            uint32_t d_bg = is_d_selected ? 0xFF5A4414 : 0xFF1E2834;
            uint32_t d_txt = is_d_selected ? 0xFFFFFFFF : 0xFFC0A860;
            if (is_d_selected) {
                ui_->add_background_quad({d_x - 1.0f * sx, diff_y + 18.0f * sy - 1.0f * sy},
                                         {diff_btn_w + 2.0f * sx, diff_btn_h + 2.0f * sy},
                                         INVALID_TEXTURE, 0xFFFFD000);
            }
            UIButton* d_btn = ui_->add_button({d_x, diff_y + 18.0f * sy}, {diff_btn_w, diff_btn_h},
                                              kDiffNames[d], [self, d]() { self->set_difficulty(d); },
                                              d_bg, 0xFF6A5018);
            if (d_btn) {
                d_btn->text_color = d_txt;
                d_btn->text_size = 12.0f * sy;
            }
        }

        // Botão INICIAR MISSÃO
        const float start_w = col_right_w - 36.0f * sx;
        const float start_h = 44.0f * sy;
        const float start_x = col_right_x + 18.0f * sx;
        const float start_y = content_y + content_h - 52.0f * sy;

        bool is_locked = (!sel_entry.completed && selected_index_ != page_start);
        // page_start is the first item on the current page; but we need the actual current mission index
        // Find the first non-completed scenario in the full list
        size_t current_mission_idx = 0;
        for (size_t idx = 0; idx < scenarios_.size(); ++idx) {
            if (!scenarios_[idx].completed) {
                current_mission_idx = idx;
                break;
            }
        }
        is_locked = (selected_index_ != current_mission_idx && !sel_entry.completed);

        uint32_t start_bg = is_locked ? 0xFF2A2A2A : 0xFF5E4210;
        uint32_t start_hover = is_locked ? 0xFF3A3A3A : 0xFF8A6218;
        uint32_t start_text = is_locked ? 0xFF888888 : 0xFFFFFFFF;
        std::string start_label = is_locked ? "BLOQUEADO" : "INICIAR MISSAO";
        uint32_t border_col = is_locked ? 0xFF555555 : 0xFFFFD000;

        ui_->add_background_quad({start_x - 2.0f * sx, start_y - 2.0f * sy},
                                 {start_w + 4.0f * sx, start_h + 4.0f * sy},
                                 INVALID_TEXTURE, border_col);

        Entry start_entry = sel_entry;
        UIButton* start_btn = ui_->add_button({start_x, start_y}, {start_w, start_h},
                                              start_label,
                                              [self, start_entry, is_locked]() {
                                                  if (!is_locked) self->start_scenario(start_entry);
                                              },
                                              start_bg, start_hover);
        if (start_btn) {
            start_btn->text_color = start_text;
            start_btn->text_size = 16.0f * sy;
            start_btn->is_big_button = true;
        }
    }

    // ── Barra Inferior: Botão Voltar ──
    const float back_w = 180.0f * sx;
    const float back_h = 36.0f * sy;
    const float back_y = vh - 48.0f * sy;
    ui_->add_button({margin_x, back_y}, {back_w, back_h}, "< VOLTAR",
                    [this]() {
                        if (provider_) provider_->return_to_main_menu();
                    }, 0xFF222830, 0xFF3A4452);

    if (ui_ && provider_) {
        ui_->resolve_text([this](const std::string& k) { return provider_->tr(k); });
    }
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
    provider_->start_campaign_mission(entry.scenario);
}

} // namespace aom
