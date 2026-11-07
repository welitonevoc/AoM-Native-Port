#include "aom/campaign_select_system.h"

#include "renderer/gles_renderer.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace aom {

CampaignSelectSystem::CampaignSelectSystem() = default;
CampaignSelectSystem::~CampaignSelectSystem() = default;

bool CampaignSelectSystem::initialize(Engine* engine, UIManager* ui, Engine* engine_ptr, int screen_w, int screen_h) {
    if (!engine || !ui || !engine_ptr) {
        std::cerr << "ERRO: Ponteiros nulos em CampaignSelectSystem::initialize()" << std::endl;
        return false;
    }

    engine_ = engine;
    ui_ = ui;
    engine_ptr_ = engine_ptr;
    screen_w_ = screen_w;
    screen_h_ = screen_h;

    // Carregar cenários disponíveis
    load_available_scenarios();
    std::cout << "Campanha selecionada: " << scenarios_.size() << " cenários disponíveis" << std::endl;

    // Posicionamento dos botões
    setup_scenario_positions();

    // Criar botões no UIManager
    create_scenario_buttons();

    if (scenarios_.empty()) {
        std::cout << "AVISO: Nenhum cenário encontrado - usando cenário padrão" << std::endl;
        // Criar um cenário padrão caso none sejam encontrados
        ScenarioInfo default_scenario;
        default_scenario.name = "Scenario 1";
        default_scenario.description = "Primeira campanha disponível";
        default_scenario.filename = "scenario001";
        default_scenario.index = 1;
        scenarios_.push_back(default_scenario);
        create_scenario_buttons();
    }

    std::cout << "CampaignSelectSystem inicializado com sucesso!" << std::endl;
    return true;
}

void CampaignSelectSystem::shutdown() {
    // Remover botões do UIManager
    for (auto* btn : scenario_buttons_) {
        if (btn && ui_) {
            // UIManager não tem remove_button, apenas limpar referência
        }
    }
    scenario_buttons_.clear();
    scenarios_.clear();
}

void CampaignSelectSystem::update(float delta_time) {
    // Atualizações do sistema de campanha (animations, etc.)
    // As interações acontecem via on_touch_event
}

void CampaignSelectSystem::render() {
    if (!renderer_) return;

    // Limpar tela com cor apropriada
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Desenhar título
    // TODO: desenhar título "Select Campaign" ou similar
    // Usar o renderer para desenhar texto

    // Descrever cada botão scenario
    for (size_t i = 0; i < scenario_buttons_.size(); ++i) {
        UIButton* btn = scenario_buttons_[i];
        if (!btn) continue;

        // Desenhar retângulo do botão
        glm::vec2 pos = btn->pos;
        glm::vec2 size = btn->size;

        // Cor do botão - azul escuro do AoM
        glColor4f(0.1f, 0.2f, 0.4f, 1.0f);
        // glRectf(pos.x, pos.y, pos.x + size.x, pos.y + size.y);
    }
}

void CampaignSelectSystem::on_touch_event(const TouchEvent& touch) {
    if (!ui_) return;

    // Processar evento no UIManager
    ui_->on_touch_event(touch);

    // Verificar quais botões foram clicados
    const auto& buttons = ui_->buttons();
    for (size_t i = 0; i < buttons.size() && i < scenario_buttons_.size(); ++i) {
        const UIButton& btn = buttons[i];
        if (!btn.onClick) continue;

        // Hit test - verificar se o toque está dentro do botão
        if (UIManager::hit_test(btn, touch.x, touch.y)) {
            // Disparar a campanha selecionada
            int idx = static_cast<int>(i);
            if (idx >= 0 && static_cast<size_t>(idx) < scenarios_.size()) {
                const auto& selected = scenarios_[idx];
                std::cout << "Campanha selecionada: \"" << selected.name
                          << "\" (índice: " << selected.index << ")" << std::endl;

                // Chamar start_campaign_mission no engine
                engine_ptr_->start_campaign_mission(selected.index);

                // Depois de iniciar, podemos fechar este ecrã
                // ou transitar para o estado IN_GAME
                // engine_->set_state(EngineState::IN_GAME);
            }
            break;
        }
    }
}

void CampaignSelectSystem::setup_scenario_positions() {
    // Definir posições padrão para os cenários
    // O sistema usa resolução virtual 1024x768
    float scale_x = static_cast<float>(screen_w_) / 1024.0f;
    float scale_y = static_cast<float>(screen_h_) / 768.0f;

    // Título do ecrã
    int title_y = 100 * scale_y;
    int button_start_y = 200 * scale_y;
    int button_height = 60 * scale_y;
    int spacing = 20 * scale_y;
    int button_width = 400 * scale_x;

    // Posicionar cenários verticalmente centralizados
    for (size_t i = 0; i < scenarios_.size() && i < 7; ++i) {
        ScenarioInfo& scenario = scenarios_[i];
        scenario.rect = glm::vec4(
            (screen_w_ - button_width) / 2.0f,  // centralizado horizontalmente
            button_start_y + (i * (button_height + spacing)), // posições verticais
            button_width,
            button_height
        );
    }
}

void CampaignSelectSystem::load_available_scenarios() {
    // Tentar carregar cenários do game_config ou scenario archive
    // O engine já tem accessor para game_config

    // Por agora, usar cenários mock/baseados em arquivos conhecidos
    // Em um projeto completo, isso viria do scenario_archive ou game_config

    // Lista de cenários padrão do Age of Mythology
    // (nomes reais viriam dos .scn files ou do game config)
    scenarios_.clear();

    // Cenários de exemplo - em produção viriam dos arquivos .scn do data_bar
    const char* campaign_names[] = {
        "The Titans: The New Atlantis",
        "The Titans: Ragnarok",
        "The Titans: Fall of a God",
        "The Titans: Gold of the Gods",
        "The Titans: Quest for the Oracle",
        "The Titans: Trial of the Gods"
    };

    const char* campaign_descriptions[] = {
        "Defenda a Nova Atlântica das forças de Cronos",
        "Supere o deus do trovão em batalha épica",
        "Recupere o poder divino roubado",
        "Busque o ouro sagrado dos atlantes",
        "Encontre a profecia da sacerdotisa",
        "Prove sua devoção aos deuses olímpicos"
    };

    const char* campaign_filenames[] = {
        "scenario001", "scenario002", "scenario003",
        "scenario004", "scenario005", "scenario006"
    };

    int num_scenarios = sizeof(campaign_names) / sizeof(campaign_names[0]);
    for (int i = 0; i < num_scenarios; ++i) {
        ScenarioInfo info;
        info.name = campaign_names[i];
        info.description = campaign_descriptions[i];
        info.filename = campaign_filenames[i];
        info.index = i + 1; // Índice 1-based para start_campaign_mission
        scenarios_.push_back(info);
    }

    std::cout << "Carregados " << scenarios_.size() << " cenários de campanha" << std::endl;
}

void CampaignSelectSystem::create_scenario_buttons() {
    if (!ui_) return;

    scenario_buttons_.clear();

    int button_width = 400;
    int button_height = 60;
    int start_y = 200;
    int spacing = 20;

    for (size_t i = 0; i < scenarios_.size() && i < 7; ++i) {
        ScenarioInfo& scenario = scenarios_[i];

        // Criar callback que inicia a campanha selecionada
        auto callback = [scenario_idx = scenarios_[i].index]() {
            std::cout << "Iniciando campanha índice: " << scenario_idx << " - \""
                      << scenarios_[scenario_idx - 1].name << "\"" << std::endl;

            // Chamar o método do engine para iniciar a campanha
            if (engine_ptr_) {
                engine_ptr_->start_campaign_mission(scenario_idx);
                // Depois de iniciar, transitar para IN_GAME
                // engine_ptr_->set_state(EngineState::IN_GAME);
            }
        };

        // Cores do AoM: azul escuro para campanha
        uint32_t normal_color = 0xFF1A5E9A; // azul escuro
        uint32_t pressed_color = 0xFF2A7AE0; // azul mais claro ao toque

        UIButton* button = ui_->add_button(
            glm::vec2(scenarios_[i].rect.xy()),           // pos
            glm::vec2(button_width, button_height),       // size
            scenario.name,                                // label
            callback,                                     // onClick
            normal_color,
            pressed_color
        );

        if (button) {
            scenario_buttons_.push_back(button);
        }
    }

    std::cout << "Criados " << scenario_buttons_.size() << " botões de campanha" << std::endl;
}

} // namespace aom