#include "network/network_manager.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <chrono>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AoM-Net"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) printf("[AoM-Net] " __VA_ARGS__); printf("\n")
#define LOGW(...) printf("[AoM-Net WARN] " __VA_ARGS__); printf("\n")
#define LOGE(...) fprintf(stderr, "[AoM-Net ERR] " __VA_ARGS__); fprintf(stderr, "\n")
#endif

namespace aom::net {

namespace {

enum PacketType : uint8_t {
    PKT_BEACON = 1,
    PKT_JOIN_REQ = 2,
    PKT_JOIN_RESP = 3,
    PKT_LOBBY_STATE = 4,
    PKT_SLOT_UPDATE = 5,
    PKT_SETTINGS_UPDATE = 6,
    PKT_CHAT = 7,
    PKT_LAUNCH = 8,
    PKT_LEAVE = 9,
    PKT_TECH_RESEARCH = 10,     // Nova: sincronização de tecnologia
    PKT_GOD_POWER = 11,         // Nova: sincronização de power divina
    PKT_VICTORY = 12            // Nova: notificação de vitória
};

void set_nonblocking(int fd) {
    if (fd < 0) return;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// AoM Classic Taunts in Portuguese
std::string translate_taunt(const std::string& msg) {
    if (msg == "1") return "1 (Sim)";
    if (msg == "2") return "2 (Não)";
    if (msg == "3") return "3 (Comida, por favor)";
    if (msg == "4") return "4 (Madeira, por favor)";
    if (msg == "5") return "5 (Ouro, por favor)";
    if (msg == "6") return "6 (Favor, por favor)";
    if (msg == "9") return "9 (Ah, que droga!)";
    if (msg == "11") return "11 (Hahaha!)";
    if (msg == "14") return "14 (Inicie o jogo!)";
    if (msg == "25") return "25 (Estou sob ataque!)";
    if (msg == "33") return "33 (Parabéns, bom jogo!)";
    return msg;
}

} // anonymous namespace

const char* major_god_to_string(MajorGod god) {
    switch (god) {
        case MajorGod::ZEUS: return "Zeus";
        case MajorGod::POSEIDON: return "Poseidon";
        case MajorGod::HADES: return "Hades";
        case MajorGod::RA: return "Rá";
        case MajorGod::ISIS: return "Ísis";
        case MajorGod::SET: return "Set";
        case MajorGod::THOR: return "Thor";
        case MajorGod::ODIN: return "Odin";
        case MajorGod::LOKI: return "Loki";
        case MajorGod::KRONOS: return "Kronos";
        case MajorGod::ORANOS: return "Oranos";
        case MajorGod::GAIA: return "Gaia";
        case MajorGod::RANDOM: default: return "Aleatório";
    }
}

const char* major_god_culture(MajorGod god) {
    switch (god) {
        case MajorGod::ZEUS:
        case MajorGod::POSEIDON:
        case MajorGod::HADES:
            return "Grego";
        case MajorGod::RA:
        case MajorGod::ISIS:
        case MajorGod::SET:
            return "Egípcio";
        case MajorGod::THOR:
        case MajorGod::ODIN:
        case MajorGod::LOKI:
            return "Nórdico";
        case MajorGod::KRONOS:
        case MajorGod::ORANOS:
        case MajorGod::GAIA:
            return "Atlante";
        case MajorGod::RANDOM: default:
            return "Qualquer";
    }
}

const char* game_mode_to_string(GameMode mode) {
    switch (mode) {
        case GameMode::SUPREMACY: return "Supremacia";
        case GameMode::CONQUEST: return "Conquista";
        case GameMode::DEATHMATCH: return "Mata-Mata";
        case GameMode::LIGHTNING: return "Relâmpago";
        default: return "Supremacia";
    }
}

const char* ai_difficulty_to_string(AIDifficulty diff) {
    switch (diff) {
        case AIDifficulty::EASY: return "Fácil";
        case AIDifficulty::MODERATE: return "Moderado";
        case AIDifficulty::HARD: return "Difícil";
        case AIDifficulty::TITAN: return "Titã";
        default: return "Moderado";
    }
}

NetworkManager::NetworkManager() {
    slots_.resize(MAX_LOBBY_SLOTS);
    for (size_t i = 0; i < slots_.size(); ++i) {
        slots_[i].slot_id = static_cast<uint8_t>(i);
        slots_[i].name = "Aberto";
        slots_[i].is_open = (i < 4); // First 4 open by default
        slots_[i].is_human = true;
        slots_[i].is_host = false;
        slots_[i].is_ready = false;
        slots_[i].team = static_cast<uint8_t>(1 + (i % 2));
        slots_[i].color_idx = static_cast<uint8_t>(i);
        slots_[i].god = static_cast<MajorGod>(i % 12 + 1);
    }
}

NetworkManager::~NetworkManager() {
    leave_session();
    stop_lan_discovery();
}

NetworkManager& NetworkManager::instance() {
    static NetworkManager s_instance;
    return s_instance;
}

void NetworkManager::start_lan_discovery() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_discovering_) return;

    bcast_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (bcast_sock_ >= 0) {
        int opt = 1;
        setsockopt(bcast_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(bcast_sock_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

        struct sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(DEFAULT_AOM_PORT);
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(bcast_sock_, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
            LOGW("Could not bind discovery socket to port %d", DEFAULT_AOM_PORT);
        }
        set_nonblocking(bcast_sock_);
        is_discovering_ = true;
        LOGI("LAN Discovery started on port %d", DEFAULT_AOM_PORT);
    }
}

void NetworkManager::stop_lan_discovery() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bcast_sock_ >= 0) {
        close(bcast_sock_);
        bcast_sock_ = -1;
    }
    is_discovering_ = false;
}

void NetworkManager::refresh_lan_games() {
    std::lock_guard<std::mutex> lock(mutex_);
    discovered_games_.clear();

    // Broadcast a discovery ping request
    if (bcast_sock_ >= 0) {
        struct sockaddr_in bcast_addr{};
        bcast_addr.sin_family = AF_INET;
        bcast_addr.sin_port = htons(DEFAULT_AOM_PORT);
        bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

        uint8_t ping_pkt[5];
        ping_pkt[0] = static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF);
        ping_pkt[1] = static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF);
        ping_pkt[2] = static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF);
        ping_pkt[3] = static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF);
        ping_pkt[4] = PKT_BEACON;

        sendto(bcast_sock_, ping_pkt, sizeof(ping_pkt), 0,
               reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
        LOGI("Sent LAN discovery ping");
    }
}

std::vector<DiscoveredGame> NetworkManager::get_discovered_games() {
    std::lock_guard<std::mutex> lock(mutex_);
    return discovered_games_;
}

bool NetworkManager::host_game(const std::string& game_name, const std::string& player_name, uint16_t port) {
    leave_session();

    std::lock_guard<std::mutex> lock(mutex_);
    udp_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock_ < 0) {
        LOGE("Failed to create UDP socket for hosting");
        return false;
    }

    int opt = 1;
    setsockopt(udp_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(udp_sock_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOGE("Failed to bind UDP socket to port %d", port);
        close(udp_sock_);
        udp_sock_ = -1;
        return false;
    }

    set_nonblocking(udp_sock_);

    port_ = port;
    role_ = NetRole::HOST;
    my_slot_id_ = 0;
    session_name_ = game_name.empty() ? "Partida de " + player_name : game_name;
    player_name_ = player_name.empty() ? "Jogador 1" : player_name;

    // Reset Slots
    for (size_t i = 0; i < slots_.size(); ++i) {
        slots_[i].slot_id = static_cast<uint8_t>(i);
        slots_[i].is_ready = false;
        if (i == 0) {
            slots_[i].name = player_name_;
            slots_[i].is_human = true;
            slots_[i].is_host = true;
            slots_[i].is_open = true;
            slots_[i].is_ready = true;
            slots_[i].team = 1;
            slots_[i].color_idx = 0;
            slots_[i].god = MajorGod::ZEUS;
        } else if (i == 1) {
            slots_[i].name = "Computador";
            slots_[i].is_human = false;
            slots_[i].is_host = false;
            slots_[i].is_open = true;
            slots_[i].is_ready = true;
            slots_[i].team = 2;
            slots_[i].color_idx = 1;
            slots_[i].god = MajorGod::KRONOS;
            slots_[i].ai_diff = AIDifficulty::MODERATE;
        } else {
            slots_[i].name = "Aberto";
            slots_[i].is_human = true;
            slots_[i].is_host = false;
            slots_[i].is_open = (i < 4);
            slots_[i].is_ready = false;
            slots_[i].team = static_cast<uint8_t>(1 + (i % 2));
            slots_[i].color_idx = static_cast<uint8_t>(i);
            slots_[i].god = MajorGod::RANDOM;
        }
    }

    chat_history_.clear();
    ChatMessage welcome;
    welcome.sender = "Sistema";
    welcome.text = "Sala criada: " + session_name_ + " (Porta " + std::to_string(port_) + ")";
    welcome.is_system = true;
    welcome.color = 0xFFFFD700;
    chat_history_.push_back(welcome);

    LOGI("Host created: %s on port %d", session_name_.c_str(), port_);
    return true;
}

bool NetworkManager::join_game(const std::string& host_ip, uint16_t port, const std::string& player_name) {
    leave_session();

    std::lock_guard<std::mutex> lock(mutex_);
    udp_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock_ < 0) {
        LOGE("Failed to create UDP socket for client");
        return false;
    }
    set_nonblocking(udp_sock_);

    host_ip_ = host_ip;
    port_ = port;
    role_ = NetRole::CLIENT;
    player_name_ = player_name.empty() ? "Jogador 2" : player_name;

    // Send Join Request to Host
    struct sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_ip_.c_str(), &target_addr.sin_addr);

    std::vector<uint8_t> req;
    req.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    req.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    req.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    req.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    req.push_back(PKT_JOIN_REQ);

    // Append player name
    uint8_t name_len = static_cast<uint8_t>(std::min(player_name_.size(), size_t(31)));
    req.push_back(name_len);
    req.insert(req.end(), player_name_.begin(), player_name_.begin() + name_len);

    sendto(udp_sock_, req.data(), req.size(), 0,
           reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr));

    chat_history_.clear();
    ChatMessage sys;
    sys.sender = "Sistema";
    sys.text = "Conectando ao anfitrião em " + host_ip_ + ":" + std::to_string(port_) + "...";
    sys.is_system = true;
    sys.color = 0xFFFFD700;
    chat_history_.push_back(sys);

    LOGI("Sent join request to %s:%d as %s", host_ip_.c_str(), port_, player_name_.c_str());
    return true;
}

void NetworkManager::leave_session() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::OFFLINE && udp_sock_ >= 0) {
        // Send Leave packet
        if (role_ == NetRole::CLIENT && !host_ip_.empty()) {
            struct sockaddr_in target_addr{};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(port_);
            inet_pton(AF_INET, host_ip_.c_str(), &target_addr.sin_addr);

            uint8_t leave_pkt[6];
            leave_pkt[0] = static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF);
            leave_pkt[1] = static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF);
            leave_pkt[2] = static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF);
            leave_pkt[3] = static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF);
            leave_pkt[4] = PKT_LEAVE;
            leave_pkt[5] = static_cast<uint8_t>(my_slot_id_);

            sendto(udp_sock_, leave_pkt, sizeof(leave_pkt), 0,
                   reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr));
        }
        close(udp_sock_);
        udp_sock_ = -1;
    }
    role_ = NetRole::OFFLINE;
    my_slot_id_ = 0;
    host_ip_ = "";
}

std::vector<PlayerSlot> NetworkManager::get_slots() {
    std::lock_guard<std::mutex> lock(mutex_);
    return slots_;
}

GameSettings NetworkManager::get_settings() {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

std::vector<ChatMessage> NetworkManager::get_chat_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    return chat_history_;
}

void NetworkManager::update_my_slot(MajorGod god, uint8_t team, uint8_t color_idx, bool is_ready) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (my_slot_id_ < 0 || my_slot_id_ >= static_cast<int>(slots_.size())) return;

    slots_[my_slot_id_].god = god;
    slots_[my_slot_id_].team = team;
    slots_[my_slot_id_].color_idx = color_idx;
    slots_[my_slot_id_].is_ready = is_ready;

    if (role_ == NetRole::HOST) {
        broadcast_lobby_state();
    } else if (role_ == NetRole::CLIENT) {
        // Sincronizar estado do jogador para o host
        std::vector<uint8_t> pkt;
        pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
        pkt.push_back(PKT_SLOT_UPDATE);
        pkt.push_back(static_cast<uint8_t>(my_slot_id_));
        pkt.push_back(static_cast<uint8_t>(god));
        pkt.push_back(team);
        pkt.push_back(color_idx);
        pkt.push_back(is_ready ? 1 : 0);
        send_to_host(pkt);
    }
    if (on_lobby_updated) on_lobby_updated();
}

void NetworkManager::send_tech_research(uint8_t slot_id, const std::string& tech_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::CLIENT || udp_sock_ < 0 || host_ip_.empty()) return;

    std::vector<uint8_t> pkt;
    pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    pkt.push_back(PKT_TECH_RESEARCH);

    // Append tech name (max 31 chars)
    uint8_t tlen = static_cast<uint8_t>(std::min(tech_name.size(), size_t(31)));
    pkt.push_back(tlen);
    pkt.insert(pkt.end(), tech_name.begin(), tech_name.begin() + tlen);

    send_to_host(pkt);
}

void NetworkManager::send_god_power_cast(uint8_t slot_id, const std::string& power_name, int target_type, const glm::vec3& target_pos) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::CLIENT || udp_sock_ < 0 || host_ip_.empty()) return;

    std::vector<uint8_t> pkt;
    pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    pkt.push_back(PKT_GOD_POWER);

    // Slot ID
    pkt.push_back(slot_id);

    // Power name (max 31 chars)
    uint8_t plen = static_cast<uint8_t>(std::min(power_name.size(), size_t(31)));
    pkt.push_back(plen);
    pkt.insert(pkt.end(), power_name.begin(), power_name.begin() + plen);

    // Target type
    pkt.push_back(static_cast<uint8_t>(target_type));

    // Target position (3 floats)
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.x));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.x + 1)); // simplificado
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.x + 2));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.y));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.y + 1));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.y + 2));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.z));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.z + 1));
    pkt.push_back(*reinterpret_cast<const uint8_t*>(&target_pos.z + 2));

    send_to_host(pkt);
}

void NetworkManager::send_victory_notification(uint8_t winner_slot, VictoryResult result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::HOST || udp_sock_ < 0) return;

    std::vector<uint8_t> pkt;
    pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    pkt.push_back(PKT_VICTORY);

    pkt.push_back(winner_slot);
    pkt.push_back(static_cast<uint8_t>(result));

    // Broadcast to all clients
    struct sockaddr_in bcast_addr{};
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(port_);
    bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(udp_sock_, pkt.data(), pkt.size(), 0,
           reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
}

void NetworkManager::update_settings(const GameSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::HOST) return;
    settings_ = settings;
    broadcast_lobby_state();
    if (on_lobby_updated) on_lobby_updated();
}

void NetworkManager::send_chat(const std::string& message) {
    if (message.empty()) return;
    std::string text = translate_taunt(message);

    std::lock_guard<std::mutex> lock(mutex_);
    ChatMessage cm;
    cm.sender = player_name_;
    cm.text = text;
    cm.is_system = false;
    cm.color = 0xFFFFFFFF;
    chat_history_.push_back(cm);

    if (on_chat_received) on_chat_received(cm);

    // Broadcast or send to host
    std::vector<uint8_t> pkt;
    pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    pkt.push_back(PKT_CHAT);
    uint8_t slen = static_cast<uint8_t>(std::min(player_name_.size(), size_t(31)));
    pkt.push_back(slen);
    pkt.insert(pkt.end(), player_name_.begin(), player_name_.begin() + slen);
    uint8_t mlen = static_cast<uint8_t>(std::min(text.size(), size_t(127)));
    pkt.push_back(mlen);
    pkt.insert(pkt.end(), text.begin(), text.begin() + mlen);

    if (role_ == NetRole::CLIENT) {
        send_to_host(pkt);
    } else if (role_ == NetRole::HOST && udp_sock_ >= 0) {
        // Broadcast chat to all clients
        struct sockaddr_in bcast_addr{};
        bcast_addr.sin_family = AF_INET;
        bcast_addr.sin_port = htons(port_);
        bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
        sendto(udp_sock_, pkt.data(), pkt.size(), 0,
               reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
    }
}

bool NetworkManager::launch_game() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (role_ != NetRole::HOST) return false;

    // Check if open slots are ready
    for (const auto& slot : slots_) {
        if (slot.is_open && !slot.is_ready) {
            ChatMessage err;
            err.sender = "Sistema";
            err.text = "Aguardando todos os jogadores estarem prontos!";
            err.is_system = true;
            err.color = 0xFFFF5555;
            chat_history_.push_back(err);
            if (on_chat_received) on_chat_received(err);
            return false;
        }
    }

    // Broadcast launch packet
    if (udp_sock_ >= 0) {
        std::vector<uint8_t> pkt;
        pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
        pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
        pkt.push_back(PKT_LAUNCH);

        struct sockaddr_in bcast_addr{};
        bcast_addr.sin_family = AF_INET;
        bcast_addr.sin_port = htons(port_);
        bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
        sendto(udp_sock_, pkt.data(), pkt.size(), 0,
               reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
    }

    if (on_game_launched) on_game_launched();
    return true;
}

void NetworkManager::send_broadcast_beacon() {
    if (role_ != NetRole::HOST || udp_sock_ < 0) return;

    // Beacon format:
    // Magic (4B) | PKT_BEACON (1B) | Port (2B) | Players (1B) | Max (1B) | NameLen (1B) | Name | HostLen (1B) | Host | MapLen (1B) | Map
    std::vector<uint8_t> bcast;
    bcast.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    bcast.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    bcast.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    bcast.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    bcast.push_back(PKT_BEACON);
    bcast.push_back(static_cast<uint8_t>(port_ & 0xFF));
    bcast.push_back(static_cast<uint8_t>((port_ >> 8) & 0xFF));

    int current_count = 0;
    for (const auto& s : slots_) {
        if (s.is_open && s.name != "Aberto") ++current_count;
    }
    bcast.push_back(static_cast<uint8_t>(std::max(1, current_count)));
    bcast.push_back(static_cast<uint8_t>(MAX_LOBBY_SLOTS));

    uint8_t nlen = static_cast<uint8_t>(std::min(session_name_.size(), size_t(31)));
    bcast.push_back(nlen);
    bcast.insert(bcast.end(), session_name_.begin(), session_name_.begin() + nlen);

    uint8_t hlen = static_cast<uint8_t>(std::min(player_name_.size(), size_t(31)));
    bcast.push_back(hlen);
    bcast.insert(bcast.end(), player_name_.begin(), player_name_.begin() + hlen);

    uint8_t mlen = static_cast<uint8_t>(std::min(settings_.map_name.size(), size_t(31)));
    bcast.push_back(mlen);
    bcast.insert(bcast.end(), settings_.map_name.begin(), settings_.map_name.begin() + mlen);

    struct sockaddr_in bcast_addr{};
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(DEFAULT_AOM_PORT);
    bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(udp_sock_, bcast.data(), bcast.size(), 0,
           reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
}

void NetworkManager::broadcast_lobby_state() {
    if (role_ != NetRole::HOST || udp_sock_ < 0) return;

    // Send complete lobby slots + settings to clients
    std::vector<uint8_t> pkt;
    pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
    pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
    pkt.push_back(PKT_LOBBY_STATE);

    pkt.push_back(static_cast<uint8_t>(slots_.size()));
    for (const auto& s : slots_) {
        pkt.push_back(s.slot_id);
        pkt.push_back(s.is_human ? 1 : 0);
        pkt.push_back(s.is_host ? 1 : 0);
        pkt.push_back(s.is_open ? 1 : 0);
        pkt.push_back(s.is_ready ? 1 : 0);
        pkt.push_back(static_cast<uint8_t>(s.god));
        pkt.push_back(s.team);
        pkt.push_back(s.color_idx);
        pkt.push_back(static_cast<uint8_t>(s.ai_diff));
        uint8_t nlen = static_cast<uint8_t>(std::min(s.name.size(), size_t(23)));
        pkt.push_back(nlen);
        pkt.insert(pkt.end(), s.name.begin(), s.name.begin() + nlen);
    }

    struct sockaddr_in bcast_addr{};
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(port_);
    bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(udp_sock_, pkt.data(), pkt.size(), 0,
           reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
}

void NetworkManager::send_to_host(const std::vector<uint8_t>& data) {
    if (role_ != NetRole::CLIENT || udp_sock_ < 0 || host_ip_.empty()) return;
    struct sockaddr_in host_addr{};
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_ip_.c_str(), &host_addr.sin_addr);
    sendto(udp_sock_, data.data(), data.size(), 0,
           reinterpret_cast<struct sockaddr*>(&host_addr), sizeof(host_addr));
}

void NetworkManager::process_incoming_packets() {
    int active_sock = (udp_sock_ >= 0) ? udp_sock_ : bcast_sock_;
    if (active_sock < 0) return;

    uint8_t buf[1024];
    struct sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    while (true) {
        ssize_t rec = recvfrom(active_sock, buf, sizeof(buf), 0,
                               reinterpret_cast<struct sockaddr*>(&from), &from_len);
        if (rec < 5) break;

        uint32_t magic = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        if (magic != AOM_NET_MAGIC) continue;

        uint8_t type = buf[4];
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));

        if (type == PKT_BEACON && is_discovering_) {
            // Discovery Beacon received
            if (rec >= 12) {
                uint16_t b_port = buf[5] | (buf[6] << 8);
                int cur_p = buf[7];
                int max_p = buf[8];
                size_t p = 9;

                uint8_t nlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
                std::string sname = (p + nlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), nlen) : "AoM Game";
                p += nlen;

                uint8_t hlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
                std::string hname = (p + hlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), hlen) : "Host";
                p += hlen;

                uint8_t mlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
                std::string mname = (p + mlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), mlen) : "Random Map";

                DiscoveredGame dg;
                dg.name = sname;
                dg.host_player = hname;
                dg.map_name = mname;
                dg.ip_address = ip_str;
                dg.port = b_port;
                dg.current_players = cur_p;
                dg.max_players = max_p;
                dg.ping_ms = 15;
                dg.last_seen_time = 0.0f;

                // Add or update
                auto it = std::find_if(discovered_games_.begin(), discovered_games_.end(),
                    [&](const DiscoveredGame& g) { return g.ip_address == dg.ip_address && g.port == dg.port; });
                if (it != discovered_games_.end()) {
                    *it = dg;
                } else {
                    discovered_games_.push_back(dg);
                    LOGI("Discovered LAN Game: %s (%s:%d)", dg.name.c_str(), dg.ip_address.c_str(), dg.port);
                }
            }
        } else if (type == PKT_JOIN_REQ && role_ == NetRole::HOST) {
            // New player wants to join!
            size_t p = 5;
            uint8_t nlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
            std::string joiner_name = (p + nlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), nlen) : "Jogador";

            // Find free open slot
            int assigned = -1;
            for (size_t i = 1; i < slots_.size(); ++i) {
                if (slots_[i].is_open && slots_[i].name == "Aberto") {
                    assigned = static_cast<int>(i);
                    slots_[i].name = joiner_name;
                    slots_[i].is_human = true;
                    slots_[i].is_ready = false;
                    break;
                }
            }

            if (assigned >= 0) {
                ChatMessage join_msg;
                join_msg.sender = "Sistema";
                join_msg.text = joiner_name + " entrou na sala!";
                join_msg.is_system = true;
                join_msg.color = 0xFF88FF88;
                chat_history_.push_back(join_msg);
                if (on_chat_received) on_chat_received(join_msg);

                // Send accept response
                uint8_t resp[7];
                resp[0] = static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF);
                resp[1] = static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF);
                resp[2] = static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF);
                resp[3] = static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF);
                resp[4] = PKT_JOIN_RESP;
                resp[5] = 1; // Success
                resp[6] = static_cast<uint8_t>(assigned);

                sendto(udp_sock_, resp, sizeof(resp), 0,
                       reinterpret_cast<struct sockaddr*>(&from), from_len);

                broadcast_lobby_state();
                if (on_lobby_updated) on_lobby_updated();
            }
        } else if (type == PKT_JOIN_RESP && role_ == NetRole::CLIENT) {
            if (rec >= 7 && buf[5] == 1) {
                my_slot_id_ = buf[6];
                ChatMessage sys;
                sys.sender = "Sistema";
                sys.text = "Conexão aceita! Atribuído ao Slot #" + std::to_string(my_slot_id_ + 1);
                sys.is_system = true;
                sys.color = 0xFF88FF88;
                chat_history_.push_back(sys);
                if (on_chat_received) on_chat_received(sys);
                if (on_lobby_updated) on_lobby_updated();
            }
        } else if (type == PKT_LOBBY_STATE && role_ == NetRole::CLIENT) {
            // Unpack full lobby state
            if (rec >= 6) {
                uint8_t num_s = buf[5];
                size_t p = 6;
                slots_.resize(num_s);
                for (uint8_t i = 0; i < num_s && p + 10 <= static_cast<size_t>(rec); ++i) {
                    slots_[i].slot_id = buf[p++];
                    slots_[i].is_human = (buf[p++] == 1);
                    slots_[i].is_host = (buf[p++] == 1);
                    slots_[i].is_open = (buf[p++] == 1);
                    slots_[i].is_ready = (buf[p++] == 1);
                    slots_[i].god = static_cast<MajorGod>(buf[p++]);
                    slots_[i].team = buf[p++];
                    slots_[i].color_idx = buf[p++];
                    slots_[i].ai_diff = static_cast<AIDifficulty>(buf[p++]);
                    uint8_t nlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
                    if (p + nlen <= static_cast<size_t>(rec)) {
                        slots_[i].name = std::string(reinterpret_cast<char*>(buf + p), nlen);
                        p += nlen;
                    }
                }
                if (on_lobby_updated) on_lobby_updated();
            }
        } else if (type == PKT_SLOT_UPDATE && role_ == NetRole::HOST) {
            if (rec >= 10) {
                uint8_t sid = buf[5];
                if (sid < slots_.size()) {
                    slots_[sid].god = static_cast<MajorGod>(buf[6]);
                    slots_[sid].team = buf[7];
                    slots_[sid].color_idx = buf[8];
                    slots_[sid].is_ready = (buf[9] == 1);
                    broadcast_lobby_state();
                    if (on_lobby_updated) on_lobby_updated();
                }
            }
        } else if (type == PKT_CHAT) {
            if (rec >= 7) {
                size_t p = 5;
                uint8_t slen = buf[p++];
                std::string sname = (p + slen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), slen) : "Jogador";
                p += slen;
                uint8_t mlen = (p < static_cast<size_t>(rec)) ? buf[p++] : 0;
                std::string mtext = (p + mlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), mlen) : "";

                if (sname != player_name_) {
                    ChatMessage cm;
                    cm.sender = sname;
                    cm.text = mtext;
                    cm.is_system = false;
                    cm.color = 0xFFFFFFFF;
                    chat_history_.push_back(cm);
                    if (on_chat_received) on_chat_received(cm);
                }
            }
        } else if (type == PKT_LAUNCH && role_ == NetRole::CLIENT) {
            LOGI("Received game launch signal from host!");
            if (on_game_launched) on_game_launched();
        } else if (type == PKT_LEAVE) {
            if (rec >= 6 && role_ == NetRole::HOST) {
                uint8_t sid = buf[5];
                if (sid > 0 && sid < slots_.size()) {
                    std::string leaver = slots_[sid].name;
                    slots_[sid].name = "Aberto";
                    slots_[sid].is_ready = false;
                    ChatMessage sys;
                    sys.sender = "Sistema";
                    sys.text = leaver + " saiu da sala.";
                    sys.is_system = true;
                    sys.color = 0xFFFFAA33;
                    chat_history_.push_back(sys);
                    if (on_chat_received) on_chat_received(sys);
                    broadcast_lobby_state();
                    if (on_lobby_updated) on_lobby_updated();
                }
            }
        } else if (type == PKT_TECH_RESEARCH && role_ == NetRole::HOST) {
            // Cliente pesquisou uma tecnologia
            if (rec >= 6) {
                size_t p = 5;
                uint8_t tlen = buf[p++];
                std::string tech_name = (p + tlen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), tlen) : "";
                p += tlen;
                
                LOGI("Host received tech research: %s", tech_name.c_str());
                // Chamar engine para pesquisar tecnologia
                if (engine_) {
                    engine_->research_tech(tech_name);
                }
            }
        } else if (type == PKT_GOD_POWER && role_ == NetRole::HOST) {
            // Cliente castou uma power divina
            if (rec >= 8) {
                size_t p = 5;
                uint8_t slot_id = buf[p++];
                uint8_t plen = buf[p++];
                std::string power_name = (p + plen <= static_cast<size_t>(rec)) ? std::string(reinterpret_cast<char*>(buf + p), plen) : "";
                p += plen;
                uint8_t target_type = buf[p++];
                
                // Read target position (3 floats = 12 bytes)
                float target_x, target_y, target_z;
                // Simplified - ler bytes como floats
                if (p + 12 <= static_cast<size_t>(rec)) {
                    memcpy(&target_x, buf.data() + p, 4);
                    memcpy(&target_y, buf.data() + p + 4, 4);
                    memcpy(&target_z, buf.data() + p + 8, 4);
                    p += 12;
                }
                
                LOGI("Host received god power cast: %s slot=%d target_type=%d pos=(%.1f, %.1f, %.1f)", 
                     power_name.c_str(), slot_id, target_type, target_x, target_y, target_z);
                // Chamar engine para castar power divina
                if (engine_) {
                    engine_->cast_god_power(power_name, target_type, glm::vec3(target_x, target_y, target_z));
                }
            }
        } else if (type == PKT_VICTORY && role_ == NetRole::HOST) {
            // Host received victory notification - broadcast to all clients
            if (rec >= 3) {
                uint8_t winner_slot = buf[5];
                uint8_t result = buf[6];
                
                LOGI("Host received victory: slot=%d result=%d", winner_slot, result);
                // Chamar engine para definir estado de vitória
                if (engine_) {
                    engine_->set_victory_result(result);
                }
            }
            // Broadcast victory to all connected clients
            if (udp_sock_ >= 0) {
                struct sockaddr_in bcast_addr{};
                bcast_addr.sin_family = AF_INET;
                bcast_addr.sin_port = htons(port_);
                bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
                
                std::vector<uint8_t> pkt;
                pkt.push_back(static_cast<uint8_t>(AOM_NET_MAGIC & 0xFF));
                pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 8) & 0xFF));
                pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 16) & 0xFF));
                pkt.push_back(static_cast<uint8_t>((AOM_NET_MAGIC >> 24) & 0xFF));
                pkt.push_back(PKT_VICTORY);
                pkt.push_back(winner_slot);
                pkt.push_back(result);
                
                sendto(udp_sock_, pkt.data(), pkt.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));
            }
        }
    }
}

void NetworkManager::update(float dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Process network packets
    process_incoming_packets();

    // 2. Periodically broadcast discovery beacon if hosting
    if (role_ == NetRole::HOST) {
        beacon_timer_ += dt;
        if (beacon_timer_ >= 1.5f) {
            beacon_timer_ = 0.0f;
            send_broadcast_beacon();
        }
    }

    // 3. Remove stale discovered games (> 6 seconds without beacon)
    if (is_discovering_) {
        for (auto& g : discovered_games_) {
            g.last_seen_time += dt;
        }
        discovered_games_.erase(
            std::remove_if(discovered_games_.begin(), discovered_games_.end(),
                           [](const DiscoveredGame& g) { return g.last_seen_time > 6.0f; }),
            discovered_games_.end()
        );
    }
}

} // namespace aom::net
