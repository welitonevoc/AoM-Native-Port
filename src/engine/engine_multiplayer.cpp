#include "engine/engine.h"
#include "engine_internal.h"
#include "network/network_manager.h"
#include "ui/ui.h"
#include "ui/ui_layout.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>

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
    if (!load_ui_layout("uimplanip.xmb")) {
        LOGE("Failed to load uimplanip.xmb");
        return;
    }

    clear_ui();  // Limpa UI anterior (ex: dialog Host Game) antes de recriar menu LAN

    // Configurar callbacks do NetworkManager para room dialogs
    auto& net = net::NetworkManager::instance();
    net.on_room_invite_received = [this](const net::RoomInvite& invite) {
        pending_room_invite_ = {invite.inviter_name, invite.inviter_ip, invite.inviter_port, true};
        show_room_invite_dialog(invite.inviter_name);
    };
    net.on_room_join_request_received = [this](const net::RoomJoinRequest& req) {
        pending_room_join_request_ = {req.requester_name, req.requester_ip, req.requester_port, true};
        show_room_join_request_dialog(req.requester_name);
    };
    net.on_room_error = [this](const std::string& error) {
        show_room_error_dialog(error);
    };

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Mapa de ações para os botoes do XMB uimplanip
    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"MPLanIP-HostButton", [this]() {
            // Host button: abre dialog para criar sala (uihostgamedialog.xmb)
            show_host_game_dialog();
        }},
        {"MPLanIP-JoinLanButton", [this]() {
            // Join button: entra no jogo LAN selecionado na lista
            auto games = net::NetworkManager::instance().get_discovered_games();
            // Usar o jogo selecionado na lista, se houver
            int selected_idx = -1;
            if (ui_) {
                auto* lan_list = ui_->find_list_by_tag("MPLanIP-LanGamesList");
                if (lan_list) {
                    selected_idx = lan_list->selected_index;
                }
            }
            if (selected_idx >= 0 && selected_idx < static_cast<int>(games.size())) {
                const auto& g = games[selected_idx];
                net::NetworkManager::instance().join_game(g.ip_address, g.port, mp_player_name_);
                set_state(EngineState::MULTIPLAYER_LOBBY);
            } else if (!games.empty()) {
                // Fallback: primeiro jogo se nenhum selecionado
                const auto& g = games[0];
                net::NetworkManager::instance().join_game(g.ip_address, g.port, mp_player_name_);
                set_state(EngineState::MULTIPLAYER_LOBBY);
            } else {
                LOGW("Nenhum jogo LAN disponível para entrar");
            }
        }},
        {"MPLanIP-RefreshButton", [this]() {
            // Refresh button: atualiza lista de jogos LAN
            net::NetworkManager::instance().refresh_lan_games();
            // Re-renderiza o menu (recarrega XMB)
            create_multiplayer_lan_menu();
        }},
        {"MPLanIP-JoinIPButton", [this]() {
            // Connect button: conecta via IP direto
            // Ler o editBox MPLanIP-DirectIPEditBox
            std::string target_ip = mp_target_ip_;
            if (ui_) {
                auto* ip_btn = ui_->find_button_by_tag("MPLanIP-DirectIPEditBox");
                if (ip_btn && !ip_btn->label.empty()) {
                    target_ip = ip_btn->label;
                }
            }
            net::NetworkManager::instance().join_game(target_ip, net::DEFAULT_AOM_PORT, mp_player_name_);
            set_state(EngineState::MULTIPLAYER_LOBBY);
        }},
        {"MPLanIP-CancelButton", [this]() {
            // Cancel button: volta ao menu principal
            net::NetworkManager::instance().stop_lan_discovery();
            return_to_main_menu();
        }},
    };

    // Resolver de texturas (mesmo padrao do engine_menus.cpp)
    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }

        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            LOGI("UI texture resolved from authentic BAR: '%s' -> '%s' (handle=%u, %ux%u)",
                 name.c_str(), normalized.c_str(), h, rt.width, rt.height);
            return rt;
        }

        // Fallback PNG - tentar assets do jogo primeiro
        std::string png_name = name;
        if (png_name.rfind("ui\\", 0) == 0 || png_name.rfind("ui/", 0) == 0) {
            png_name = png_name.substr(3);
        }
        for (char& c : png_name) {
            if (c == ' ' || c == '\\' || c == '/') c = '_';
            else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (png_name.size() < 4 || png_name.substr(png_name.size() - 4) != ".png") {
            png_name += ".png";
        }
        
        std::string game_asset_path = "assets/game_textures/" + png_name;
        h = load_png_asset(game_asset_path);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            LOGI("UI texture resolution: asset do jogo encontrado: '%s'", game_asset_path.c_str());
            return rt;
        }

        // FALLBACK: asset UI
        std::string ui_asset_path = "assets/titans_menu_parts/" + png_name;
        h = load_png_asset(ui_asset_path);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            return rt;
        }
    };

    const auto* layout = ui_layout_manager_.get_layout("uimplanip.xmb");
    if (!layout) {
        LOGE("Layout uimplanip.xmb not found after load");
        return;
    }

    // Nao limpar UI - o XMB sera desenhado por cima
    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);

    // Resolver textos $$ID$$ usando as strings do Engine
    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    // Preencher lista de jogos LAN no list widget MPLanIP-LanGamesList
    auto games = net::NetworkManager::instance().get_discovered_games();
    if (ui_) {
        // Atualizar host game editBox com nome salvo
        auto* host_edit = ui_->find_button_by_tag("MPLanIP-HostGameEditBox");
        if (host_edit) {
            host_edit->label = mp_session_name_;
        }
        // Atualizar direct IP editBox com IP salvo
        auto* ip_edit = ui_->find_button_by_tag("MPLanIP-DirectIPEditBox");
        if (ip_edit) {
            ip_edit->label = mp_target_ip_;
        }
        // Popular lista de jogos LAN descobertos
        auto* lan_list = ui_->find_list_by_tag("MPLanIP-LanGamesList");
        if (lan_list) {
            std::vector<std::string> game_items;
            for (const auto& g : games) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s (%s) [%d/%d]",
                              g.name.c_str(), g.ip_address.c_str(), g.current_players, g.max_players);
                game_items.push_back(buf);
            }
            lan_list->items = game_items;
            lan_list->item_colors.assign(game_items.size(), 0xFFFFD000);
            lan_list->selected_index = -1;
            LOGI("Populated MPLanIP-LanGamesList with %zu games", game_items.size());
        }
    }

    LOGI("create_multiplayer_lan_menu: uimplanip.xmb exibido (%zu elementos, %zu jogos LAN)", created, games.size());
}

void Engine::show_host_game_dialog() {
    if (!load_ui_layout("uihostgamedialog.xmb")) {
        LOGE("Failed to load uihostgamedialog.xmb");
        return;
    }

    clear_ui();  // Limpa menu LAN (z=9 background) para dialog modal (z=5) receber toques

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Mapa de ações para os botoes do XMB
    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"hostgame-dialog-LoginButton", [this]() {
            // OK button: ler valores do dialog XMB e criar sala
            std::string session_name = mp_session_name_;  // default
            int max_players = 8;  // default
            bool is_public = true;  // default

            // Ler editBox: hostgame-dialog-UserName-Text (nome da sala)
            // Nota: o editBox e renderizado como UIButton com tag, mas o texto esta no label
            // O apply_layout_to_ui cria o editBox como button, precisamos achar pelo tag
            if (ui_) {
                auto* name_btn = ui_->find_button_by_tag("hostgame-dialog-UserName-Text");
                if (name_btn && !name_btn->label.empty()) {
                    session_name = name_btn->label;
                }
                // Ler scrollList: hostgame-dialog--NumberPlayers (max players)
                auto* sl = ui_->find_scroll_list_by_tag("hostgame-dialog--NumberPlayers");
                if (sl && sl->selected_index >= 0 && sl->selected_index < (int)sl->items.size()) {
                    try {
                        max_players = std::stoi(sl->items[sl->selected_index]);
                    } catch (...) {}
                }
                // Ler radioset: hostgame-dialog-publicChoice (public/private)
                auto* rs = ui_->find_radio_set_by_tag("hostgame-dialog-publicChoice");
                if (rs) {
                    // index 0 = Public, 1 = Private
                    is_public = (rs->selected_index == 0);
                }
            }

            mp_session_name_ = session_name;  // salvar para proxima vez
            LOGI("Host Game: name='%s', max_players=%d, public=%d", session_name.c_str(), max_players, is_public);
            net::NetworkManager::instance().host_game(session_name, mp_player_name_, net::DEFAULT_AOM_PORT, is_public);
            set_state(EngineState::MULTIPLAYER_LOBBY);
        }},
        {"hostgame-dialog-CancelButton", [this]() {
            // Cancel: volta ao menu LAN
            create_multiplayer_lan_menu();
        }},
    };

    // Resolver de texturas (mesmo padrao do engine_menus.cpp)
    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }

        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            LOGI("UI texture resolved from authentic BAR: '%s' -> '%s' (handle=%u, %ux%u)",
                 name.c_str(), normalized.c_str(), h, rt.width, rt.height);
            return rt;
        }

        // Fallback PNG - tentar assets do jogo primeiro
        std::string png_name = name;
        if (png_name.rfind("ui\\", 0) == 0 || png_name.rfind("ui/", 0) == 0) {
            png_name = png_name.substr(3);
        }
        for (char& c : png_name) {
            if (c == ' ' || c == '\\' || c == '/') c = '_';
            else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (png_name.size() < 4 || png_name.substr(png_name.size() - 4) != ".png") {
            png_name += ".png";
        }
        
        std::string game_asset_path = "assets/game_textures/" + png_name;
        h = load_png_asset(game_asset_path);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            LOGI("UI texture resolution: asset do jogo encontrado: '%s'", game_asset_path.c_str());
            return rt;
        }

        // FALLBACK: asset UI
        std::string ui_asset_path = "assets/titans_menu_parts/" + png_name;
        h = load_png_asset(ui_asset_path);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            return rt;
        }
    };

    const auto* layout = ui_layout_manager_.get_layout("uihostgamedialog.xmb");
    if (!layout) {
        LOGE("Layout uihostgamedialog.xmb not found after load");
        return;
    }

    // Nao limpar UI - o dialog XMB sera desenhado por cima do menu LAN (Ja limpado acima)
    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);
    
    // Resolver textos $$ID$$ usando as strings do Engine
    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    // Preencher editBox do nome da sala com valor atual
    if (ui_) {
        auto* name_btn = ui_->find_button_by_tag("hostgame-dialog-UserName-Text");
        if (name_btn && name_btn->label.empty()) {
            name_btn->label = mp_session_name_;
            LOGI("Pre-filled host game dialog with session name: '%s'", mp_session_name_.c_str());
        }
        // Pre-selecionar scrollList de max players (default 8)
        auto* sl = ui_->find_scroll_list_by_tag("hostgame-dialog--NumberPlayers");
        if (sl && sl->selected_index < 0 && !sl->items.empty()) {
            // Procurar "8" na lista
            for (size_t i = 0; i < sl->items.size(); ++i) {
                if (sl->items[i] == "8") {
                    sl->selected_index = static_cast<int>(i);
                    break;
                }
            }
        }
        // Pre-selecionar radioset public/private (default Public = index 0)
        auto* rs = ui_->find_radio_set_by_tag("hostgame-dialog-publicChoice");
        if (rs && rs->selected_index < 0) {
            rs->selected_index = 0; // Public
        }
    }

    LOGI("show_host_game_dialog: uihostgamedialog.xmb exibido (%zu elementos)", created);
}

// ── Room Dialogs (uiroomdialogs.xmb) ──────────────────────────────────────

void Engine::show_room_invite_dialog(const std::string& inviter_name) {
    if (!load_ui_layout("uiroomdialogs.xmb")) {
        LOGE("Failed to load uiroomdialogs.xmb");
        return;
    }

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    std::string prompt_text = inviter_name + " has invited you to their room. Accept?";

    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"mproom-roominviterequestdialog-Yesbutton", [this]() {
            LOGI("Room invite accepted from %s", pending_room_invite_.inviter_name.c_str());
            if (pending_room_invite_.valid) {
                auto& net = net::NetworkManager::instance();
                net.send_room_invite_response(pending_room_invite_.inviter_ip, pending_room_invite_.inviter_port, true);
                // Join the host's game
                net.join_game(pending_room_invite_.inviter_ip, pending_room_invite_.inviter_port, mp_player_name_);
                set_state(EngineState::MULTIPLAYER_LOBBY);
            }
            pending_room_invite_.valid = false;
        }},
        {"mproom-roominviterequestdialog-Nobutton", [this]() {
            LOGI("Room invite declined");
            if (pending_room_invite_.valid) {
                auto& net = net::NetworkManager::instance();
                net.send_room_invite_response(pending_room_invite_.inviter_ip, pending_room_invite_.inviter_port, false);
            }
            pending_room_invite_.valid = false;
            create_multiplayer_lan_menu();
        }},
    };

    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }
        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            return rt;
        }
        return rt;
    };

    auto* layout = ui_layout_manager_.get_layout_mut("uiroomdialogs.xmb");
    if (!layout) {
        LOGE("Layout uiroomdialogs.xmb not found after load");
        return;
    }

    for (auto& root : layout->root_gadgets) {
        if (root.name == "mproom-roominviterequestdialog") {
            root.hidden = false;
            for (auto& child : root.children) {
                if (child.name == "mproom-roominviterequestdialog-prompt") {
                    child.text = prompt_text;
                }
            }
            break;
        }
    }

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);

    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    LOGI("show_room_invite_dialog: displayed for %s (%zu elementos)", inviter_name.c_str(), created);
}

void Engine::show_room_join_request_dialog(const std::string& requester_name) {
    if (!load_ui_layout("uiroomdialogs.xmb")) {
        LOGE("Failed to load uiroomdialogs.xmb");
        return;
    }

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    std::string prompt_text = requester_name + " would like to join your room. Accept?";

    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"mproom-roomjoinrequestdialog-Yesbutton", [this]() {
            LOGI("Room join request accepted from %s", pending_room_join_request_.requester_name.c_str());
            if (pending_room_join_request_.valid) {
                auto& net = net::NetworkManager::instance();
                // Find a free slot
                int free_slot = -1;
                auto slots = net.get_slots();
                for (size_t i = 1; i < slots.size(); ++i) {
                    if (slots[i].is_open && slots[i].name == "Aberto") {
                        free_slot = static_cast<int>(i);
                        break;
                    }
                }
                if (free_slot >= 0) {
                    net.send_room_join_response(pending_room_join_request_.requester_ip, pending_room_join_request_.requester_port, true, free_slot);
                    // Update local slot
                    net.set_slot_state(free_slot, false, true, net::AIDifficulty::MODERATE);
                    net.update_settings(net.get_settings());
                } else {
                    net.send_room_join_response(pending_room_join_request_.requester_ip, pending_room_join_request_.requester_port, false, -1);
                }
            }
            pending_room_join_request_.valid = false;
            create_multiplayer_lobby_menu();
        }},
        {"mproom-roomjoinrequestdialog-Nobutton", [this]() {
            LOGI("Room join request declined");
            if (pending_room_join_request_.valid) {
                auto& net = net::NetworkManager::instance();
                net.send_room_join_response(pending_room_join_request_.requester_ip, pending_room_join_request_.requester_port, false, -1);
            }
            pending_room_join_request_.valid = false;
        }},
    };

    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }
        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            return rt;
        }
        return rt;
    };

    auto* layout = ui_layout_manager_.get_layout_mut("uiroomdialogs.xmb");
    if (!layout) {
        LOGE("Layout uiroomdialogs.xmb not found after load");
        return;
    }

    for (auto& root : layout->root_gadgets) {
        if (root.name == "mproom-roomjoinrequestdialog") {
            root.hidden = false;
            for (auto& child : root.children) {
                if (child.name == "mproom-roomjoinrequestdialog-prompt") {
                    child.text = prompt_text;
                }
            }
            break;
        }
    }

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);

    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    LOGI("show_room_join_request_dialog: displayed for %s (%zu elementos)", requester_name.c_str(), created);
}

void Engine::show_room_error_dialog(const std::string& error_message) {
    if (!load_ui_layout("uiroomdialogs.xmb")) {
        LOGE("Failed to load uiroomdialogs.xmb");
        return;
    }

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"mproom-roomerrordialog-OKbutton", [this]() {
            create_multiplayer_lan_menu();
        }},
    };

    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }
        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            return rt;
        }
        return rt;
    };

    auto* layout = ui_layout_manager_.get_layout_mut("uiroomdialogs.xmb");
    if (!layout) {
        LOGE("Layout uiroomdialogs.xmb not found after load");
        return;
    }

    for (auto& root : layout->root_gadgets) {
        if (root.name == "mproom-roomerrordialog") {
            root.hidden = false;
            for (auto& child : root.children) {
                if (child.name == "mproom-roomerrordialog-prompt") {
                    child.text = error_message;
                }
            }
            break;
        }
    }

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);

    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    LOGI("show_room_error_dialog: displayed (%zu elementos)", created);
}

// ── Session Connect Dialog (uisessionconnect.xmb) ──────────────────────────

void Engine::show_session_connect_dialog() {
    if (!load_ui_layout("uisessionconnect.xmb")) {
        LOGE("Failed to load uisessionconnect.xmb");
        return;
    }

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    std::unordered_map<std::string, std::function<void()>> action_callbacks = {
        {"session-connect-Cancelbutton", [this]() {
            LOGI("Session connect cancelled by user");
            net::NetworkManager::instance().leave_session();
            create_multiplayer_lan_menu();
        }},
    };

    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }
        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            return rt;
        }
        return rt;
    };

    const auto* layout = ui_layout_manager_.get_layout("uisessionconnect.xmb");
    if (!layout) {
        LOGE("Layout uisessionconnect.xmb not found after load");
        return;
    }

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                        static_cast<int>(vw), static_cast<int>(vh),
                                        action_callbacks, texture_resolver,
                                        &button_set_manager_);

    ui_->resolve_text([this](const std::string& key) { return tr(key); });

    LOGI("show_session_connect_dialog: displayed (%zu elementos)", created);
}

void Engine::hide_session_connect_dialog() {
    // Volta ao menu LAN ou lobby dependendo do estado
    if (state_ == EngineState::MULTIPLAYER_LAN) {
        create_multiplayer_lan_menu();
    } else if (state_ == EngineState::MULTIPLAYER_LOBBY) {
        create_multiplayer_lobby_menu();
    }
}

void Engine::create_multiplayer_lobby_menu() {
    clear_ui();

    // Configurar callbacks do NetworkManager para room dialogs (no lobby tambem)
    auto& net = net::NetworkManager::instance();
    net.on_room_join_request_received = [this](const net::RoomJoinRequest& req) {
        pending_room_join_request_ = {req.requester_name, req.requester_ip, req.requester_port, true};
        show_room_join_request_dialog(req.requester_name);
    };
    net.on_room_error = [this](const std::string& error) {
        show_room_error_dialog(error);
    };

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    auto& nm = net::NetworkManager::instance();
    auto slots = nm.get_slots();
    auto settings = nm.get_settings();
    auto chat = nm.get_chat_history();
    bool is_host = nm.is_host();
    int my_slot = nm.my_slot_id();

    // ── Resolver de texturas autenticas (mesmo padrao dos menus XMB) ──────
    auto texture_resolver = [this](const std::string& name) -> ResolvedTexture {
        ResolvedTexture rt;
        auto it = texture_cache_.find(name);
        if (it != texture_cache_.end()) {
            rt.handle = it->second;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            return rt;
        }
        std::string normalized = name;
        if (normalized.size() < 4 || normalized.substr(normalized.size() - 4) != ".ddt") {
            normalized += ".ddt";
        }
        TextureHandle h = load_ddt_texture(normalized);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(normalized);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            texture_dimensions_[name] = texture_dimensions_[normalized];
            return rt;
        }
        // Fallback PNG
        std::string png_name = name;
        if (png_name.rfind("ui\\", 0) == 0 || png_name.rfind("ui/", 0) == 0) {
            png_name = png_name.substr(3);
        }
        for (char& c : png_name) {
            if (c == ' ' || c == '\\' || c == '/') c = '_';
            else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (png_name.size() < 4 || png_name.substr(png_name.size() - 4) != ".png") {
            png_name += ".png";
        }
        std::string asset_path = "assets/titans_menu_parts/" + png_name;
        h = load_png_asset(asset_path);
        if (h != INVALID_TEXTURE) {
            rt.handle = h;
            auto dit = texture_dimensions_.find(name);
            if (dit != texture_dimensions_.end()) {
                rt.width = static_cast<uint32_t>(dit->second.x);
                rt.height = static_cast<uint32_t>(dit->second.y);
            }
            texture_cache_[name] = h;
            return rt;
        }
        return rt;
    };

    // ── Cores autenticas AoM ──────────────────────────────────────────────
    const uint32_t COLOR_GOLD = 0xFFFFD000;
    const uint32_t COLOR_GOLD_SHADOW = 0xFF181005;
    const uint32_t COLOR_STONE_DARK = 0xFF1A1208;
    const uint32_t COLOR_STONE_MED = 0xFF2A2018;
    const uint32_t COLOR_STONE_LIGHT = 0xFF3A2A18;
    const uint32_t COLOR_BORDER = 0xFFFFD000;
    const uint32_t COLOR_TEXT_WHITE = 0xFFFFFFFF;
    const uint32_t COLOR_TEXT_GRAY = 0xFFCCCCCC;
    const uint32_t COLOR_TEXT_DIM = 0xFF888888;
    const uint32_t COLOR_GREEN = 0xFF00FF88;
    const uint32_t COLOR_RED = 0xFFFF4444;
    const uint32_t COLOR_BUTTON_NORMAL = 0xAA2A2010;
    const uint32_t COLOR_BUTTON_PRESSED = 0xAA504020;
    const uint32_t COLOR_BUTTON_HOVER = 0xAA3A3018;

    // ── Carregar texturas de fundo autenticas ─────────────────────────────
    ResolvedTexture bg_panel = texture_resolver("ui\\ui main background.ddt");
    ResolvedTexture btn_normal = texture_resolver("ui\\ui button normal.ddt");
    ResolvedTexture btn_pressed = texture_resolver("ui\\ui button pressed.ddt");
    ResolvedTexture btn_rollover = texture_resolver("ui\\ui button rollover.ddt");
    ResolvedTexture list_bg = texture_resolver("ui\\ui list background.ddt");
    ResolvedTexture list_sel = texture_resolver("ui\\ui list selection.ddt");

    // ── Fundo do Lobby (painel de pedra estilo AoM) ───────────────────────
    if (bg_panel.handle != INVALID_TEXTURE) {
        renderer_->draw_ui_quad({0, 0}, {vw, vh}, bg_panel.handle, 0xFFFFFFFF);
    } else {
        renderer_->draw_ui_quad({0, 0}, {vw, vh}, INVALID_TEXTURE, 0xFF0A0806);
    }

    // ── Cabecalho do Lobby com estilo autentico ───────────────────────────
    std::string role_str = is_host ? "[HOST / COORDENADOR]" : "[CLIENTE]";
    char title_buf[128];
    std::snprintf(title_buf, sizeof(title_buf), "SALA MULTIJOGADOR: %s - %s",
                  role_str.c_str(), settings.map_name.c_str());
    
    float title_h = 40.0f;
    renderer_->draw_ui_quad({10.0f, 10.0f}, {vw - 20.0f, title_h}, INVALID_TEXTURE, COLOR_STONE_DARK);
    renderer_->draw_ui_quad({10.0f, 10.0f}, {vw - 20.0f, 2.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({10.0f, 10.0f + title_h - 2.0f}, {vw - 20.0f, 2.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({10.0f, 10.0f}, {2.0f, title_h}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({10.0f + vw - 22.0f, 10.0f}, {2.0f, title_h}, INVALID_TEXTURE, COLOR_BORDER);
    
    renderer_->draw_ui_text(title_buf, {20.0f, 18.0f}, 22.0f, COLOR_GOLD_SHADOW);
    renderer_->draw_ui_text(title_buf, {21.0f, 19.0f}, 22.0f, COLOR_GOLD);

    char sub_buf[128];
    std::snprintf(sub_buf, sizeof(sub_buf), "Porta UDP: %u | Seu Slot: #%d (%s)",
                  net::DEFAULT_AOM_PORT, my_slot + 1, is_host ? "Host" : "Jogador");
    renderer_->draw_ui_text(sub_buf, {20.0f, 42.0f}, 13.0f, COLOR_GOLD_SHADOW);
    renderer_->draw_ui_text(sub_buf, {21.0f, 43.0f}, 13.0f, COLOR_TEXT_GRAY);

    // ── Tabela de Slots (Painel principal estilo pedra) ───────────────────
    const float slot_x = vw * 0.03f;
    const float slot_w = vw * 0.60f;
    const float slot_h = 40.0f;
    const float gap = 4.0f;
    float y = vh * 0.14f;

    float table_w = slot_w;
    float table_h = 28.0f + 8 * (slot_h + gap);
    renderer_->draw_ui_quad({slot_x, y - 30.0f}, {table_w, table_h}, INVALID_TEXTURE, COLOR_STONE_DARK);
    renderer_->draw_ui_quad({slot_x, y - 30.0f}, {table_w, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({slot_x, y - 30.0f + table_h - 1.0f}, {table_w, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({slot_x, y - 30.0f}, {1.0f, table_h}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({slot_x + table_w - 1.0f, y - 30.0f}, {1.0f, table_h}, INVALID_TEXTURE, COLOR_BORDER);

    renderer_->draw_ui_text("SLOT | JOGADOR | DEUS PRINCIPAL | EQUIPE | COR | STATUS", 
                          {slot_x + 10.0f, y - 24.0f}, 13.0f, COLOR_BORDER);
    y += 2.0f;

    for (size_t i = 0; i < slots.size() && i < 8; ++i) {
        const auto& s = slots[i];
        float row_y = y + i * (slot_h + gap);
        bool is_my_slot = (static_cast<int>(i) == my_slot);
        bool is_ready = s.is_ready;
        bool is_human = s.is_human;
        bool is_open = s.is_open;

        uint32_t row_bg = (i % 2 == 0) ? COLOR_STONE_MED : COLOR_STONE_LIGHT;
        if (is_my_slot) row_bg = 0xAA2A3A20;
        renderer_->draw_ui_quad({slot_x + 2.0f, row_y}, {table_w - 4.0f, slot_h}, INVALID_TEXTURE, row_bg);

        char slot_num[16];
        std::snprintf(slot_num, sizeof(slot_num), "#%zu", i + 1);
        renderer_->draw_ui_text(slot_num, {slot_x + 12.0f, row_y + 10.0f}, 14.0f, COLOR_GOLD);

        if (is_my_slot) {
            renderer_->draw_ui_text(s.name.empty() ? "(Sem nome)" : s.name.c_str(), 
                                  {slot_x + 50.0f, row_y + 10.0f}, 14.0f, COLOR_GREEN);

            float god_x = slot_x + 220.0f;
            float god_w = 140.0f;
            std::string god_txt = std::string(net::major_god_to_string(s.god)) + " \u25B6";
            auto* god_btn = ui_->add_button({god_x, row_y + 2.0f}, {god_w, slot_h - 4.0f}, god_txt, 
                [this, &nm, s]() {
                    uint8_t next_god = (static_cast<uint8_t>(s.god) + 1) % 13;
                    nm.update_my_slot(static_cast<net::MajorGod>(next_god), s.team, s.color_idx, s.is_ready);
                    create_multiplayer_lobby_menu();
                });
            if (god_btn) {
                god_btn->normal_tex = btn_normal.handle;
                god_btn->pressed_tex = btn_pressed.handle;
                god_btn->rollover_tex = btn_rollover.handle;
                god_btn->text_color = COLOR_GOLD;
                god_btn->text_size = 13.0f;
            }

            float team_x = god_x + god_w + 8.0f;
            float team_w = 70.0f;
            std::string team_txt = "Eq " + std::to_string(s.team);
            auto* team_btn = ui_->add_button({team_x, row_y + 2.0f}, {team_w, slot_h - 4.0f}, team_txt,
                [this, &nm, s]() {
                    uint8_t next_team = (s.team % 4) + 1;
                    nm.update_my_slot(s.god, next_team, s.color_idx, s.is_ready);
                    create_multiplayer_lobby_menu();
                });
            if (team_btn) {
                team_btn->normal_tex = btn_normal.handle;
                team_btn->pressed_tex = btn_pressed.handle;
                team_btn->rollover_tex = btn_rollover.handle;
                team_btn->text_color = COLOR_GOLD;
                team_btn->text_size = 13.0f;
            }

            float ready_x = team_x + team_w + 8.0f;
            float ready_w = 110.0f;
            std::string ready_txt = s.is_ready ? "PRONTO \u2713" : "AGUARDAR";
            uint32_t ready_bg = s.is_ready ? 0xAA184818 : 0xAA481818;
            auto* ready_btn = ui_->add_button({ready_x, row_y + 2.0f}, {ready_w, slot_h - 4.0f}, ready_txt,
                [this, &nm, s]() {
                    nm.update_my_slot(s.god, s.team, s.color_idx, !s.is_ready);
                    create_multiplayer_lobby_menu();
                }, ready_bg, 0xAA386838);
            if (ready_btn) {
                ready_btn->text_color = COLOR_GOLD;
                ready_btn->text_size = 13.0f;
            }

        } else if (is_host) {
            if (is_open && is_human && !s.name.empty()) {
                renderer_->draw_ui_text(s.name.c_str(), {slot_x + 50.0f, row_y + 10.0f}, 14.0f, 
                                      is_ready ? COLOR_TEXT_WHITE : COLOR_TEXT_GRAY);
                renderer_->draw_ui_text(net::major_god_to_string(s.god), {slot_x + 220.0f, row_y + 10.0f}, 13.0f, COLOR_TEXT_GRAY);
                char team_buf[16];
                std::snprintf(team_buf, sizeof(team_buf), "Eq %d", s.team);
                renderer_->draw_ui_text(team_buf, {slot_x + 370.0f, row_y + 10.0f}, 13.0f, COLOR_TEXT_GRAY);
                renderer_->draw_ui_text(is_ready ? "PRONTO" : "AGUARDANDO", {slot_x + 440.0f, row_y + 10.0f}, 13.0f, 
                                      is_ready ? COLOR_GREEN : COLOR_TEXT_DIM);
                char ping_buf[32];
                std::snprintf(ping_buf, sizeof(ping_buf), "%dms", s.ping_ms);
                renderer_->draw_ui_text(ping_buf, {slot_x + 520.0f, row_y + 10.0f}, 12.0f, COLOR_TEXT_DIM);
            } else {
                std::string slot_desc;
                if (!is_open) slot_desc = "#" + std::to_string(i + 1) + " [FECHADO]";
                else if (!is_human) slot_desc = "#" + std::to_string(i + 1) + " BOT " + net::ai_difficulty_to_string(s.ai_diff) + " - " + net::major_god_to_string(s.god);
                else slot_desc = "#" + std::to_string(i + 1) + " [ABERTO]";

                auto* slot_btn = ui_->add_button({slot_x + 2.0f, row_y}, {table_w - 4.0f, slot_h}, slot_desc,
                    [this, &nm, i, s]() {
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
                if (slot_btn) {
                    slot_btn->normal_tex = btn_normal.handle;
                    slot_btn->pressed_tex = btn_pressed.handle;
                    slot_btn->text_color = is_open ? COLOR_GOLD : COLOR_TEXT_DIM;
                    slot_btn->text_size = 13.0f;
                }
            }
        } else {
            char buf[128];
            if (!is_open) {
                std::snprintf(buf, sizeof(buf), "#%zu [FECHADO]", i + 1);
                renderer_->draw_ui_text(buf, {slot_x + 12.0f, row_y + 10.0f}, 14.0f, COLOR_TEXT_DIM);
            } else if (!is_human) {
                std::snprintf(buf, sizeof(buf), "#%zu BOT %s | %s | Eq %d",
                              i + 1, net::ai_difficulty_to_string(s.ai_diff),
                              net::major_god_to_string(s.god), s.team);
                renderer_->draw_ui_text(buf, {slot_x + 12.0f, row_y + 10.0f}, 13.0f, COLOR_TEXT_GRAY);
            } else if (s.name.empty()) {
                std::snprintf(buf, sizeof(buf), "#%zu [AGUARDANDO JOGADOR...]", i + 1);
                renderer_->draw_ui_text(buf, {slot_x + 12.0f, row_y + 10.0f}, 13.0f, COLOR_TEXT_DIM);
            } else {
                std::snprintf(buf, sizeof(buf), "#%zu %s | %s | Eq %d | %s",
                              i + 1, s.name.c_str(), net::major_god_to_string(s.god),
                              s.team, is_ready ? "PRONTO" : "AGUARDANDO");
                renderer_->draw_ui_text(buf, {slot_x + 12.0f, row_y + 10.0f}, 13.0f, 
                                      is_ready ? COLOR_TEXT_WHITE : COLOR_TEXT_GRAY);
            }
        }
    }

    // ── Coluna Direita: Configuracoes & Bate-Papo ─────────────────────────
    const float right_x = vw * 0.66f;
    const float right_w = vw * 0.30f;
    float right_y = vh * 0.14f;

    float right_panel_h = vh * 0.72f;
    renderer_->draw_ui_quad({right_x, right_y - 8.0f}, {right_w, right_panel_h}, INVALID_TEXTURE, COLOR_STONE_DARK);
    renderer_->draw_ui_quad({right_x, right_y - 8.0f}, {right_w, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x, right_y - 8.0f + right_panel_h - 1.0f}, {right_w, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x, right_y - 8.0f}, {1.0f, right_panel_h}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x + right_w - 1.0f, right_y - 8.0f}, {1.0f, right_panel_h}, INVALID_TEXTURE, COLOR_BORDER);

    renderer_->draw_ui_text("CONFIGURACOES DA SALA:", {right_x + 10.0f, right_y + 2.0f}, 16.0f, COLOR_GOLD);
    right_y += 30.0f;

    auto add_setting_row = [&](const std::string& label, const std::string& value, std::function<void()> on_click, bool clickable) {
        renderer_->draw_ui_quad({right_x + 4.0f, right_y}, {right_w - 8.0f, 36.0f}, INVALID_TEXTURE, COLOR_STONE_MED);
        renderer_->draw_ui_text(label, {right_x + 10.0f, right_y + 8.0f}, 14.0f, COLOR_BORDER);
        
        if (clickable) {
            auto* btn = ui_->add_button({right_x + 4.0f, right_y}, {right_w - 8.0f, 36.0f}, value + " \u25B6", on_click);
            if (btn) {
                btn->normal_tex = btn_normal.handle;
                btn->pressed_tex = btn_pressed.handle;
                btn->rollover_tex = btn_rollover.handle;
                btn->text_color = COLOR_GOLD;
                btn->text_size = 13.0f;
            }
        } else {
            renderer_->draw_ui_text(value, {right_x + right_w - 10.0f - value.size() * 8.0f, right_y + 8.0f}, 13.0f, COLOR_TEXT_WHITE);
        }
        right_y += 42.0f;
    };

    add_setting_row("Mapa:", settings.map_name, [this, &nm, settings]() mutable {
        static const char* MAPS[] = { "Mapa Aleatorio", "Alfheim", "Oasis", "Mediterraneo", "Lago Fantasma", "Savana", "Acropole" };
        static int map_idx = 0;
        map_idx = (map_idx + 1) % 7;
        settings.map_name = MAPS[map_idx];
        nm.update_settings(settings);
        create_multiplayer_lobby_menu();
    }, is_host);

    add_setting_row("Modo:", net::game_mode_to_string(settings.game_mode), [this, &nm, settings]() mutable {
        uint8_t next_mode = (static_cast<uint8_t>(settings.game_mode) + 1) % 4;
        settings.game_mode = static_cast<net::GameMode>(next_mode);
        nm.update_settings(settings);
        create_multiplayer_lobby_menu();
    }, is_host);

    add_setting_row("Populacao:", std::to_string(settings.pop_cap), [this, &nm, settings]() mutable {
        settings.pop_cap = (settings.pop_cap == 150) ? 200 : (settings.pop_cap == 200 ? 300 : 150);
        nm.update_settings(settings);
        create_multiplayer_lobby_menu();
    }, is_host);

    right_y += 10.0f;

    renderer_->draw_ui_text("BATE-PAPO / PROVOCACOES:", {right_x + 10.0f, right_y}, 15.0f, COLOR_GOLD);
    right_y += 24.0f;

    float chat_h = 100.0f;
    renderer_->draw_ui_quad({right_x + 4.0f, right_y}, {right_w - 8.0f, chat_h}, INVALID_TEXTURE, COLOR_STONE_DARK);
    renderer_->draw_ui_quad({right_x + 4.0f, right_y}, {right_w - 8.0f, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x + 4.0f, right_y + chat_h - 1.0f}, {right_w - 8.0f, 1.0f}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x + 4.0f, right_y}, {1.0f, chat_h}, INVALID_TEXTURE, COLOR_BORDER);
    renderer_->draw_ui_quad({right_x + right_w - 5.0f, right_y}, {1.0f, chat_h}, INVALID_TEXTURE, COLOR_BORDER);

    size_t start_msg = chat.size() > 5 ? chat.size() - 5 : 0;
    for (size_t i = start_msg; i < chat.size(); ++i) {
        std::string line = chat[i].sender + ": " + chat[i].text;
        float line_y = right_y + 4.0f + (i - start_msg) * 18.0f;
        renderer_->draw_ui_text(line, {right_x + 8.0f, line_y}, 12.0f, chat[i].color);
    }
    right_y += chat_h + 10.0f;

    renderer_->draw_ui_text("TAUNTS RAPIDOS:", {right_x + 10.0f, right_y}, 14.0f, COLOR_GOLD);
    right_y += 22.0f;

    const char* taunts[6][2] = {
        {"1 (Sim)", "1"}, {"2 (Nao)", "2"}, {"11 (Risos)", "11"},
        {"14 (Inicie!)", "14"}, {"25 (Ataque)", "25"}, {"33 (GG)", "33"}
    };
    float tw = (right_w - 20.0f) / 3.0f;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = row * 3 + col;
            float bx = right_x + 8.0f + col * (tw + 4.0f);
            float by = right_y + row * 36.0f;
            const char* taunt_cmd = taunts[idx][1]; // Capture by value
            auto* tbtn = ui_->add_button({bx, by}, {tw, 32.0f}, taunts[idx][0], [&nm, taunt_cmd]() { nm.send_chat(taunt_cmd); });
            if (tbtn) {
                tbtn->normal_tex = btn_normal.handle;
                tbtn->pressed_tex = btn_pressed.handle;
                tbtn->rollover_tex = btn_rollover.handle;
                tbtn->text_color = COLOR_GOLD;
                tbtn->text_size = 12.0f;
            }
        }
    }
    right_y += 76.0f;

    // ── Barra Inferior: Acoes ─────────────────────────────────────────────
    float bottom_y = vh - 60.0f;
    
    if (is_host) {
        float launch_w = 280.0f;
        auto* launch_btn = ui_->add_button({(vw - launch_w) * 0.5f, bottom_y}, {launch_w, 48.0f},
                           "INICIAR PARTIDA (LAUNCH)", [this, &nm]() {
                if (nm.launch_game()) {
                    start_game();
                }
            });
        if (launch_btn) {
            launch_btn->normal_tex = btn_normal.handle;
            launch_btn->pressed_tex = btn_pressed.handle;
            launch_btn->rollover_tex = btn_rollover.handle;
            launch_btn->text_color = COLOR_GOLD;
            launch_btn->text_size = 18.0f;
            launch_btn->is_big_button = true;
        }
    }

    auto* leave_btn = ui_->add_button({slot_x, bottom_y}, {180.0f, 48.0f}, "SAIR DA SALA", [this, &nm]() {
        nm.leave_session();
        set_state(EngineState::MULTIPLAYER_LAN);
    });
    if (leave_btn) {
        leave_btn->normal_tex = btn_normal.handle;
        leave_btn->pressed_tex = btn_pressed.handle;
        leave_btn->rollover_tex = btn_rollover.handle;
        leave_btn->text_color = COLOR_GOLD;
        leave_btn->text_size = 14.0f;
    }
}



void Engine::network_host_game(const std::string& game_name, const std::string& player_name, uint16_t port, bool is_public) {
    LOGI("Iniciando host multiplayer...");
    net::NetworkManager::instance().host_game(game_name, player_name, port, is_public);
}

void Engine::network_join_game(const std::string& host_ip, uint16_t port, const std::string& player_name) {
    LOGI("Tentando conectar em: %s:%d", host_ip.c_str(), port);
    net::NetworkManager::instance().join_game(host_ip, port, player_name);
}

void Engine::network_leave_session() {
    net::NetworkManager::instance().leave_session();
}

void Engine::network_update(float dt) {
    net::NetworkManager::instance().update(dt);
}

} // namespace aom
