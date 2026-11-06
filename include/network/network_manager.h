#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include <glm/glm.hpp>

#include "game/victory.h"

namespace aom::net {

constexpr uint16_t DEFAULT_AOM_PORT = 2300;
constexpr uint32_t AOM_NET_MAGIC = 0x4D504F41; // "AOPM"
constexpr int MAX_LOBBY_SLOTS = 8;

enum class MajorGod : uint8_t {
    RANDOM = 0,
    ZEUS,
    POSEIDON,
    HADES,
    RA,
    ISIS,
    SET,
    THOR,
    ODIN,
    LOKI,
    KRONOS,
    ORANOS,
    GAIA
};

const char* major_god_to_string(MajorGod god);
const char* major_god_culture(MajorGod god);

enum class GameMode : uint8_t {
    SUPREMACY = 0,
    CONQUEST,
    DEATHMATCH,
    LIGHTNING
};

const char* game_mode_to_string(GameMode mode);

enum class AIDifficulty : uint8_t {
    EASY = 0,
    MODERATE,
    HARD,
    TITAN
};

const char* ai_difficulty_to_string(AIDifficulty diff);

struct DiscoveredGame {
    std::string name;
    std::string host_player;
    std::string map_name;
    std::string ip_address;
    uint16_t port = DEFAULT_AOM_PORT;
    int current_players = 1;
    int max_players = MAX_LOBBY_SLOTS;
    int ping_ms = 0;
    float last_seen_time = 0.0f;
};

struct PlayerSlot {
    uint8_t slot_id = 0;
    std::string name;
    bool is_human = true;
    bool is_host = false;
    bool is_open = true;
    bool is_ready = false;
    MajorGod god = MajorGod::ZEUS;
    uint8_t team = 1;      // 1 to 4
    uint8_t color_idx = 0; // 0: Blue, 1: Red, 2: Yellow, 3: Green, etc.
    AIDifficulty ai_diff = AIDifficulty::MODERATE;
    int ping_ms = 10;
};

struct GameSettings {
    std::string map_name = "Random Map";
    std::string map_size = "Normal";
    GameMode game_mode = GameMode::SUPREMACY;
    std::string starting_resources = "Standard";
    std::string starting_age = "Archaic";
    int pop_cap = 150;
};

struct ChatMessage {
    std::string sender;
    std::string text;
    bool is_system = false;
    uint32_t color = 0xFFFFFFFF;
};

enum class NetRole {
    OFFLINE,
    HOST,
    CLIENT
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // Singleton / Global Access
    static NetworkManager& instance();

    // LAN Discovery
    void start_lan_discovery();
    void stop_lan_discovery();
    void refresh_lan_games();
    std::vector<DiscoveredGame> get_discovered_games();

    // Session Management
    bool host_game(const std::string& game_name, const std::string& player_name, uint16_t port = DEFAULT_AOM_PORT);
    bool join_game(const std::string& host_ip, uint16_t port, const std::string& player_name);
    void leave_session();

    // State Queries
    NetRole role() const { return role_; }
    bool is_host() const { return role_ == NetRole::HOST; }
    bool is_connected() const { return role_ != NetRole::OFFLINE; }
    int my_slot_id() const { return my_slot_id_; }

    // Engine replication access
    Engine* engine() const { return engine_; }
    void set_engine(Engine* engine) { engine_ = engine; }

    // Lobby Interactions
    std::vector<PlayerSlot> get_slots();
    GameSettings get_settings();
    std::vector<ChatMessage> get_chat_history();

    void update_my_slot(MajorGod god, uint8_t team, uint8_t color_idx, bool is_ready);
    void set_slot_state(uint8_t slot_id, bool is_open, bool is_human, AIDifficulty diff = AIDifficulty::MODERATE);
    void update_settings(const GameSettings& settings); // Host only
    void send_chat(const std::string& message);
    bool launch_game(); // Host only

    // Network replication functions
    void send_tech_research(uint8_t slot_id, const std::string& tech_name);
    void send_god_power_cast(uint8_t slot_id, const std::string& power_name, int target_type, const glm::vec3& target_pos);
    void send_victory_notification(uint8_t winner_slot, VictoryResult result);

    // Frame update (called in Engine::update)
    void update(float dt);

    // Callbacks
    std::function<void()> on_lobby_updated;
    std::function<void(const ChatMessage&)> on_chat_received;
    std::function<void()> on_game_launched;
    std::function<void(const std::string& reason)> on_disconnected;

private:
    void send_broadcast_beacon();
    void process_incoming_packets();
    void broadcast_lobby_state();
    void send_to_host(const std::vector<uint8_t>& data);

    int udp_sock_ = -1;
    int bcast_sock_ = -1;
    uint16_t port_ = DEFAULT_AOM_PORT;

    NetRole role_ = NetRole::OFFLINE;
    int my_slot_id_ = 0;
    std::string player_name_ = "Player";
    std::string session_name_ = "AoM LAN Game";
    std::string host_ip_ = "";

    mutable std::mutex mutex_;
    std::vector<DiscoveredGame> discovered_games_;
    std::vector<PlayerSlot> slots_;
    GameSettings settings_;
    std::vector<ChatMessage> chat_history_;

    float beacon_timer_ = 0.0f;
    bool is_discovering_ = false;
    Engine* engine_ = nullptr;
};

} // namespace aom::net
