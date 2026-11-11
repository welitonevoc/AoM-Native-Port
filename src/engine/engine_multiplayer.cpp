#include "engine/engine.h"
#include "network/network_manager.h"

#include <algorithm>
#include <string>

#define LOG_TAG "AoM-Engine"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {
void Engine::open_multiplayer() {
    LOGI("Opening Multiplayer...");
    auto& nm = net::NetworkManager::instance();
    nm.on_lobby_updated = [this]() {
        if (state_ == EngineState::MULTIPLAYER_LOBBY) {
            create_multiplayer_lobby_menu();
        } else if (state_ == EngineState::MULTIPLAYER_LAN) {
            create_multiplayer_lan_menu();
        }
    };
    nm.on_chat_received = [this](const net::ChatMessage&) {
        if (state_ == EngineState::MULTIPLAYER_LOBBY) {
            create_multiplayer_lobby_menu();
        }
    };
    nm.on_game_launched = [this]() {
        LOGI("Multiplayer game launched by host! Transitioning to game...");
        start_game();
    };
    nm.on_disconnected = [this](const std::string& reason) {
        LOGW("Multiplayer disconnected: %s", reason.c_str());
        set_state(EngineState::MULTIPLAYER_LAN);
    };

    nm.start_lan_discovery();
    set_state(EngineState::MULTIPLAYER_LAN);
}



void Engine::create_multiplayer_menu() {
    open_multiplayer();
}



void Engine::create_multiplayer_lan_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Titulo Autentico AoM
    ui_->add_label("MULTIJOGADOR - REDE LOCAL (LAN) / IP DIRETO", {vw * 0.5f - 300.0f, vh * 0.07f}, 26.0f, 0xFFFFD000);
    ui_->add_label("Jogue contra outros dispositivos Android ou PC na mesma rede Wi-Fi", {vw * 0.5f - 270.0f, vh * 0.12f}, 15.0f, 0xFFCCCCCC);

    const float col_w = vw * 0.42f;
    const float col1_x = vw * 0.06f;
    const float col2_x = vw * 0.52f;
    const float btn_h = 42.0f;
    const float gap = 12.0f;

    // â”€â”€ Coluna 1: Hospedar & Conexao Direta â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    float y1 = vh * 0.18f;
    ui_->add_label("COORDENAR PARTIDA (HOST):", {col1_x, y1}, 20.0f, 0xFFFFD000);
    y1 += 32.0f;

    ui_->add_button({col1_x, y1}, {col_w, btn_h}, "CRIAR SALA LAN (HOST)", [this]() {
        net::NetworkManager::instance().host_game(mp_session_name_, mp_player_name_, net::DEFAULT_AOM_PORT);
        set_state(EngineState::MULTIPLAYER_LOBBY);
    }, 0xAA2A1808, 0xAA553311);
    y1 += btn_h + gap * 1.5f;

    ui_->add_label("CONEXAO DIRETA VIA IP:", {col1_x, y1}, 20.0f, 0xFFFFD000);
    y1 += 30.0f;

    // Mostrador de IP selecionado
    std::string ip_label = "IP Alvo: " + mp_target_ip_;
    ui_->add_label(ip_label, {col1_x, y1 + 6.0f}, 17.0f, 0xFFFFFFFF);
    y1 += 34.0f;

    // Botao Conectar ao IP alvo
    ui_->add_button({col1_x, y1}, {col_w, btn_h}, "CONECTAR AO IP ALVO", [this]() {
        net::NetworkManager::instance().join_game(mp_target_ip_, net::DEFAULT_AOM_PORT, mp_player_name_);
        set_state(EngineState::MULTIPLAYER_LOBBY);
    }, 0xAA183818, 0xAA286028);
    y1 += btn_h + gap;

    // Botoes de ajuste de IP octeto (+1, +10, -1, -10)
    float mini_w = (col_w - 3.0f * 8.0f) / 4.0f;
    ui_->add_button({col1_x, y1}, {mini_w, 36.0f}, "-10", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ - 10, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + mini_w + 8.0f, y1}, {mini_w, 36.0f}, "-1", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ - 1, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + (mini_w + 8.0f) * 2.0f, y1}, {mini_w, 36.0f}, "+1", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ + 1, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + (mini_w + 8.0f) * 3.0f, y1}, {mini_w, 36.0f}, "+10", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ + 10, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    y1 += 36.0f + gap;

    // Presets de IP rapidos (Localhost 127.0.0.1 e Subnet 192.168.0.x / 192.168.1.x)
    float half_w = (col_w - 8.0f) * 0.5f;
    ui_->add_button({col1_x, y1}, {half_w, 36.0f}, "127.0.0.1 (Local)", [this]() {
        mp_target_ip_ = "127.0.0.1";
        create_multiplayer_lan_menu();
    });
    std::string subnet_text = (mp_ip_subnet_ == 1) ? "Sub-rede: .1.x" : "Sub-rede: .0.x";
    ui_->add_button({col1_x + half_w + 8.0f, y1}, {half_w, 36.0f}, subnet_text, [this]() {
        mp_ip_subnet_ = (mp_ip_subnet_ == 1) ? 0 : 1;
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });

    // â”€â”€ Coluna 2: Jogos em LAN Descobertos â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    float y2 = vh * 0.18f;
    ui_->add_label("JOGOS EM LAN DETECTADOS (UDP 2300):", {col2_x, y2}, 20.0f, 0xFFFFD000);
    y2 += 32.0f;

    auto games = net::NetworkManager::instance().get_discovered_games();
    if (games.empty()) {
        ui_->add_label("(Nenhum host encontrado na rede local ainda...)", {col2_x, y2}, 16.0f, 0xFFAAAAAA);
        y2 += 26.0f;
        ui_->add_label("Inicie o Host em outro aparelho ou use Conexao Direta.", {col2_x, y2}, 14.0f, 0xFF888888);
        y2 += 36.0f;
    } else {
        for (size_t i = 0; i < games.size() && i < 4; ++i) {
            const auto& g = games[i];
            char btn_txt[128];
            std::snprintf(btn_txt, sizeof(btn_txt), "%s (%s) [%d/%d]",
                          g.name.c_str(), g.ip_address.c_str(), g.current_players, g.max_players);
            std::string ip = g.ip_address;
            uint16_t port = g.port;
            ui_->add_button({col2_x, y2}, {col_w, btn_h}, btn_txt, [this, ip, port]() {
                net::NetworkManager::instance().join_game(ip, port, mp_player_name_);
                set_state(EngineState::MULTIPLAYER_LOBBY);
            }, 0xAA204060, 0xAA306090);
            y2 += btn_h + 8.0f;
        }
    }

    ui_->add_button({col2_x, vh * 0.68f}, {col_w, btn_h}, "ATUALIZAR LISTA (REFRESH)", [this]() {
        net::NetworkManager::instance().refresh_lan_games();
        create_multiplayer_lan_menu();
    });

    // â”€â”€ Barra Inferior: Voltar â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    float back_w = 260.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.86f}, {back_w, 46.0f}, "VOLTAR AO MENU", [this]() {
        net::NetworkManager::instance().stop_lan_discovery();
        return_to_main_menu();
    });
}



void Engine::create_multiplayer_lobby_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    auto& nm = net::NetworkManager::instance();
    auto slots = nm.get_slots();
    auto settings = nm.get_settings();
    auto chat = nm.get_chat_history();
    bool is_host = nm.is_host();
    int my_slot = nm.my_slot_id();

    // â”€â”€ Cabecalho do Lobby â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    std::string role_str = is_host ? "[HOST / COORDENADOR]" : "[CLIENTE]";
    char title_buf[128];
    std::snprintf(title_buf, sizeof(title_buf), "SALA MULTIJOGADOR: %s - %s",
                  role_str.c_str(), settings.map_name.c_str());
    ui_->add_label(title_buf, {vw * 0.05f, vh * 0.05f}, 24.0f, 0xFFFFD000);

    char sub_buf[128];
    std::snprintf(sub_buf, sizeof(sub_buf), "Porta UDP: %u | Seu Slot: #%d (%s)",
                  net::DEFAULT_AOM_PORT, my_slot + 1, is_host ? "Host" : "Jogador");
    ui_->add_label(sub_buf, {vw * 0.05f, vh * 0.09f}, 14.0f, 0xFFCCCCCC);

    // â”€â”€ Tabela de Slots (Esquerda / Centro) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    const float slot_x = vw * 0.05f;
    const float slot_w = vw * 0.58f;
    const float slot_h = 36.0f;
    const float gap = 6.0f;
    float y = vh * 0.14f;

    ui_->add_label("SLOT | JOGADOR | DEUS PRINCIPAL | EQUIPE | COR | STATUS", {slot_x, y}, 14.0f, 0xFFFFD000);
    y += 22.0f;

    for (size_t i = 0; i < slots.size() && i < 8; ++i) {
        const auto& s = slots[i];
        float row_y = y + i * (slot_h + gap);

        if (static_cast<int>(i) == my_slot) {
            // Meu slot: botoes interativos para alterar Deus, Equipe e Pronto
            char info_buf[64];
            std::snprintf(info_buf, sizeof(info_buf), "#%zu %s (VOCE)", i + 1, s.name.c_str());
            ui_->add_label(info_buf, {slot_x, row_y + 8.0f}, 15.0f, 0xFF00FF88);

            // Botao do Deus Principal
            float god_x = slot_x + slot_w * 0.32f;
            float god_w = slot_w * 0.28f;
            std::string god_txt = std::string(net::major_god_to_string(s.god)) + " >";
            ui_->add_button({god_x, row_y}, {god_w, slot_h}, god_txt, [this, &nm, s]() {
                uint8_t next_god = (static_cast<uint8_t>(s.god) + 1) % 13;
                nm.update_my_slot(static_cast<net::MajorGod>(next_god), s.team, s.color_idx, s.is_ready);
                create_multiplayer_lobby_menu();
            }, 0xAA2A2010, 0xAA504020);

            // Botao da Equipe
            float team_x = god_x + god_w + 6.0f;
            float team_w = slot_w * 0.16f;
            std::string team_txt = "Eq " + std::to_string(s.team);
            ui_->add_button({team_x, row_y}, {team_w, slot_h}, team_txt, [this, &nm, s]() {
                uint8_t next_team = (s.team % 4) + 1;
                nm.update_my_slot(s.god, next_team, s.color_idx, s.is_ready);
                create_multiplayer_lobby_menu();
            }, 0xAA182838, 0xAA284868);

            // Botao Pronto
            float ready_x = team_x + team_w + 6.0f;
            float ready_w = slot_w * 0.20f;
            std::string ready_txt = s.is_ready ? "[X] PRONTO" : "[ ] ESPERA";
            uint32_t ready_col = s.is_ready ? 0xAA184818 : 0xAA481818;
            ui_->add_button({ready_x, row_y}, {ready_w, slot_h}, ready_txt, [this, &nm, s]() {
                nm.update_my_slot(s.god, s.team, s.color_idx, !s.is_ready);
                create_multiplayer_lobby_menu();
            }, ready_col, 0xAA386838);

        } else if (is_host) {
            // Sou host gerenciando outro slot
            if (s.is_open && s.is_human && !s.name.empty()) {
                // Outro jogador humano conectado
                char buf[128];
                std::snprintf(buf, sizeof(buf), "#%zu %s | %s | Eq %d | %s (%dms)",
                              i + 1, s.name.c_str(), net::major_god_to_string(s.god),
                              s.team, s.is_ready ? "Pronto" : "Aguardando", s.ping_ms);
                ui_->add_label(buf, {slot_x, row_y + 8.0f}, 14.0f, s.is_ready ? 0xFFFFFFFF : 0xFFAAAAAA);
            } else {
                // Slot vazio ou Bot de IA gerenciavel pelo Host
                std::string slot_desc;
                if (!s.is_open) slot_desc = "#" + std::to_string(i + 1) + " [Slot Fechado]";
                else if (!s.is_human) slot_desc = "#" + std::to_string(i + 1) + " Bot IA (" + net::ai_difficulty_to_string(s.ai_diff) + ") - " + net::major_god_to_string(s.god);
                else slot_desc = "#" + std::to_string(i + 1) + " [Slot Aberto para Jogador]";

                ui_->add_button({slot_x, row_y}, {slot_w, slot_h}, slot_desc, [this, &nm, i, s]() {
                    // Ciclo: Aberto Humano -> Bot Moderado -> Bot Dificil -> Fechado -> Aberto
                    if (s.is_open && s.is_human) {
                        nm.set_slot_state(i, true, false, net::AIDifficulty::MODERATE);
                    } else if (s.is_open && !s.is_human && s.ai_diff == net::AIDifficulty::MODERATE) {
                        nm.set_slot_state(i, true, false, net::AIDifficulty::HARD);
                    } else if (s.is_open && !s.is_human) {
                        nm.set_slot_state(i, false, false);
                    } else {
                        nm.set_slot_state(i, true, true);
                    }
                    create_multiplayer_lobby_menu();
                }, 0x88202020, 0x88404040);
            }
        } else {
            // Sou cliente visualizando slot de outro jogador / bot / fechado
            char buf[128];
            if (!s.is_open) {
                std::snprintf(buf, sizeof(buf), "#%zu [Slot Fechado]", i + 1);
            } else if (!s.is_human) {
                std::snprintf(buf, sizeof(buf), "#%zu Bot IA (%s) | %s | Eq %d",
                              i + 1, net::ai_difficulty_to_string(s.ai_diff),
                              net::major_god_to_string(s.god), s.team);
            } else if (s.name.empty()) {
                std::snprintf(buf, sizeof(buf), "#%zu [Aguardando Jogador...]", i + 1);
            } else {
                std::snprintf(buf, sizeof(buf), "#%zu %s | %s | Eq %d | %s",
                              i + 1, s.name.c_str(), net::major_god_to_string(s.god),
                              s.team, s.is_ready ? "Pronto" : "Aguardando");
            }
            ui_->add_label(buf, {slot_x, row_y + 8.0f}, 14.0f, s.is_ready ? 0xFFFFFFFF : 0xFF999999);
        }
    }

    // â”€â”€ Coluna Direita: Configuracoes & Bate-Papo (Taunts) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    const float right_x = vw * 0.66f;
    const float right_w = vw * 0.29f;
    float right_y = vh * 0.14f;

    ui_->add_label("CONFIGURACOES DA SALA:", {right_x, right_y}, 18.0f, 0xFFFFD000);
    right_y += 28.0f;

    // Mapa
    std::string map_txt = "Mapa: " + settings.map_name + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, map_txt, [this, &nm, settings]() mutable {
            static const char* MAPS[] = { "Mapa Aleatorio", "Alfheim", "Oasis", "Mediterraneo", "Lago Fantasma", "Savana", "Acropole" };
            static int map_idx = 0;
            map_idx = (map_idx + 1) % 7;
            settings.map_name = MAPS[map_idx];
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(map_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 42.0f;

    // Modo de Jogo
    std::string mode_txt = "Modo: " + std::string(net::game_mode_to_string(settings.game_mode)) + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, mode_txt, [this, &nm, settings]() mutable {
            uint8_t next_mode = (static_cast<uint8_t>(settings.game_mode) + 1) % 4;
            settings.game_mode = static_cast<net::GameMode>(next_mode);
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(mode_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 42.0f;

    // Limite de Populacao
    std::string pop_txt = "Populacao Max: " + std::to_string(settings.pop_cap) + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, pop_txt, [this, &nm, settings]() mutable {
            settings.pop_cap = (settings.pop_cap == 150) ? 200 : (settings.pop_cap == 200 ? 300 : 150);
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(pop_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 50.0f;

    // Bate-papo & Taunts Originais AoM
    ui_->add_label("BATE-PAPO / PROVOCACOES:", {right_x, right_y}, 16.0f, 0xFFFFD000);
    right_y += 24.0f;

    // Exibir ultimas 3 mensagens
    size_t start_msg = chat.size() > 3 ? chat.size() - 3 : 0;
    for (size_t i = start_msg; i < chat.size(); ++i) {
        std::string line = chat[i].sender + ": " + chat[i].text;
        ui_->add_label(line, {right_x, right_y}, 13.0f, chat[i].color);
        right_y += 18.0f;
    }
    right_y = std::max(right_y, vh * 0.48f);

    // Botoes de Provocacao Rapida (Taunts)
    float tw = (right_w - 12.0f) / 3.0f;
    ui_->add_button({right_x, right_y}, {tw, 32.0f}, "1 (Sim)", [&nm]() { nm.send_chat("1"); });
    ui_->add_button({right_x + tw + 6.0f, right_y}, {tw, 32.0f}, "2 (Nao)", [&nm]() { nm.send_chat("2"); });
    ui_->add_button({right_x + (tw + 6.0f) * 2.0f, right_y}, {tw, 32.0f}, "11 (Risos)", [&nm]() { nm.send_chat("11"); });
    right_y += 36.0f;

    ui_->add_button({right_x, right_y}, {tw, 32.0f}, "14 (Inicie!)", [&nm]() { nm.send_chat("14"); });
    ui_->add_button({right_x + tw + 6.0f, right_y}, {tw, 32.0f}, "25 (Ataque)", [&nm]() { nm.send_chat("25"); });
    ui_->add_button({right_x + (tw + 6.0f) * 2.0f, right_y}, {tw, 32.0f}, "33 (GG)", [&nm]() { nm.send_chat("33"); });

    // â”€â”€ Barra Inferior: Acoes â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (is_host) {
        float launch_w = 300.0f;
        ui_->add_button({(vw - launch_w) * 0.5f, vh * 0.86f}, {launch_w, 46.0f},
                       "INICIAR PARTIDA (LAUNCH)", [this, &nm]() {
            if (nm.launch_game()) {
                start_game();
            }
        }, 0xAA664411, 0xAA996622);
    }

    ui_->add_button({slot_x, vh * 0.86f}, {200.0f, 46.0f}, "SAIR DA SALA", [this, &nm]() {
        nm.leave_session();
        set_state(EngineState::MULTIPLAYER_LAN);
    });
}



void Engine::network_host_game(const std::string& game_name, const std::string& player_name, uint16_t port) {
    LOGI("Iniciando host multiplayer...");
    if (network_manager_) {
        network_manager_->host_game(game_name, player_name, port);
    }
}



void Engine::network_join_game(const std::string& host_ip, uint16_t port, const std::string& player_name) {
    LOGI("Tentando conectar em: %s:%d", host_ip.c_str(), port);
    if (network_manager_) {
        network_manager_->join_game(host_ip, port, player_name);
    }
}



void Engine::network_leave_session() {
    if (network_manager_) {
        network_manager_->leave_session();
    }
}



void Engine::network_update(float dt) {
    if (network_manager_) {
        network_manager_->update(dt);
    }
}




} // namespace aom
