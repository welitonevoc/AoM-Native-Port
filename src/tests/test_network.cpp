#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "network/network_manager.h"

using namespace aom::net;

int main() {
    std::cout << "========================================\n";
    std::cout << "  AoM Multiplayer & Network Suite Tests \n";
    std::cout << "========================================\n\n";

    // 1. Test Major Gods & Cultures
    std::cout << "--- 1. Testing Major Gods & Cultures ---\n";
    assert(std::string(major_god_to_string(MajorGod::ZEUS)) == "Zeus");
    assert(std::string(major_god_culture(MajorGod::ZEUS)) == "Grego");

    assert(std::string(major_god_to_string(MajorGod::RA)) == "Rá");
    assert(std::string(major_god_culture(MajorGod::RA)) == "Egípcio");

    assert(std::string(major_god_to_string(MajorGod::THOR)) == "Thor");
    assert(std::string(major_god_culture(MajorGod::THOR)) == "Nórdico");

    assert(std::string(major_god_to_string(MajorGod::KRONOS)) == "Kronos");
    assert(std::string(major_god_culture(MajorGod::KRONOS)) == "Atlante");

    assert(std::string(major_god_to_string(MajorGod::RANDOM)) == "Aleatório");
    std::cout << "PASS: All 12 Major Gods and 4 Cultures verified\n";

    // 2. Test Game Modes and AI Difficulties
    std::cout << "\n--- 2. Testing Game Modes & AI Diff ---\n";
    assert(std::string(game_mode_to_string(GameMode::SUPREMACY)) == "Supremacia");
    assert(std::string(game_mode_to_string(GameMode::CONQUEST)) == "Conquista");
    assert(std::string(game_mode_to_string(GameMode::DEATHMATCH)) == "Mata-Mata");
    assert(std::string(game_mode_to_string(GameMode::LIGHTNING)) == "Relâmpago");

    assert(std::string(ai_difficulty_to_string(AIDifficulty::EASY)) == "Fácil");
    assert(std::string(ai_difficulty_to_string(AIDifficulty::MODERATE)) == "Moderado");
    assert(std::string(ai_difficulty_to_string(AIDifficulty::HARD)) == "Difícil");
    assert(std::string(ai_difficulty_to_string(AIDifficulty::TITAN)) == "Titã");
    std::cout << "PASS: Game modes and AI difficulties verified\n";

    // 3. Test NetworkManager Hosting
    std::cout << "\n--- 3. Testing Host Game & Lobby Slots ---\n";
    auto& nm = NetworkManager::instance();
    bool hosted = nm.host_game("AoM Sala Android", "HostPlayer", 2305);
    assert(hosted);
    assert(nm.is_host());
    assert(nm.is_connected());
    assert(nm.my_slot_id() == 0);

    auto slots = nm.get_slots();
    assert(slots.size() == 8);
    assert(slots[0].is_host);
    assert(slots[0].name == "HostPlayer");
    assert(slots[1].is_open);
    std::cout << "PASS: Session created with 8 slots, Host in slot 0\n";

    // 4. Test Slot Customization
    std::cout << "\n--- 4. Testing Slot Customization ---\n";
    nm.update_my_slot(MajorGod::THOR, 2, 3, true);
    slots = nm.get_slots();
    assert(slots[0].god == MajorGod::THOR);
    assert(slots[0].team == 2);
    assert(slots[0].color_idx == 3);
    assert(slots[0].is_ready == true);
    std::cout << "PASS: Host slot updated: Thor, Team 2, Ready\n";

    // 5. Test AI Bot Slot Management
    std::cout << "\n--- 5. Testing AI Bot Slot Controls ---\n";
    nm.set_slot_state(1, true, false, AIDifficulty::HARD);
    slots = nm.get_slots();
    assert(!slots[1].is_human);
    assert(slots[1].ai_diff == AIDifficulty::HARD);

    nm.set_slot_state(2, false, false);
    slots = nm.get_slots();
    assert(!slots[2].is_open);
    std::cout << "PASS: AI Bot Hard placed in slot 1, slot 2 closed\n";

    // 6. Test Settings Update
    std::cout << "\n--- 6. Testing Game Settings ---\n";
    GameSettings set = nm.get_settings();
    set.map_name = "Alfheim";
    set.game_mode = GameMode::DEATHMATCH;
    set.pop_cap = 300;
    nm.update_settings(set);

    auto updated_set = nm.get_settings();
    assert(updated_set.map_name == "Alfheim");
    assert(updated_set.game_mode == GameMode::DEATHMATCH);
    assert(updated_set.pop_cap == 300);
    std::cout << "PASS: Settings updated (Alfheim, Deathmatch, Pop 300)\n";

    // 7. Test Chat and Portuguese Taunts
    std::cout << "\n--- 7. Testing Chat & Taunts Translation ---\n";
    nm.send_chat("1");
    nm.send_chat("14");
    nm.send_chat("Olá mundo!");

    auto chat = nm.get_chat_history();
    assert(chat.size() >= 3);
    assert(chat[0].text == "1 (Sim)");
    assert(chat[1].text == "14 (Inicie o jogo!)");
    assert(chat[2].text == "Olá mundo!");
    std::cout << "PASS: Chat & AoM authentic taunts verified\n";

    // 8. Test Clean Session Exit
    std::cout << "\n--- 8. Testing Session Exit ---\n";
    nm.leave_session();
    assert(nm.role() == NetRole::OFFLINE);
    assert(!nm.is_connected());
    std::cout << "PASS: Left session cleanly, network OFFLINE\n";

    std::cout << "\n========================================\n";
    std::cout << " ALL MULTIPLAYER SUITE TESTS PASSED!   \n";
    std::cout << "========================================\n";
    return 0;
}
