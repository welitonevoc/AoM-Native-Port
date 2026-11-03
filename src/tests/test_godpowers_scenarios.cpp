#include "game/god_powers.h"
#include "game/scenario_trigger.h"
#include "game/game_systems.h"
#include "assets/bar_archive.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            std::exit(1); \
        } else { \
            std::printf("PASS: %s\n", msg); \
        } \
    } while (0)

static std::string read_text_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    f.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::string s(sz, '\0');
    f.read(&s[0], sz);
    return s;
}

int main() {
    std::printf("========================================\n");
    std::printf(" AoM God Powers & Scenarios REA Tests   \n");
    std::printf("========================================\n\n");

    const std::string base_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology";

    // -------------------------------------------------------------
    // 1. Testing God Powers System (godpowers.bar & godpowers2.bar)
    // -------------------------------------------------------------
    std::printf("--- Testing God Powers Subsystem ---\n");
    aom::GodPowerSystem gp_system;

    aom::BarArchive gp_bar;
    bool opened_gp1 = gp_bar.open(base_dir + "/god powers/godpowers.bar");
    TEST_ASSERT(opened_gp1, "Opened godpowers.bar archive");
    TEST_ASSERT(gp_bar.file_count() == 44, "godpowers.bar contains exactly 44 god powers");

    bool loaded_gp1 = gp_system.load_from_bar(&gp_bar, false);
    TEST_ASSERT(loaded_gp1, "Loaded god powers from godpowers.bar");
    TEST_ASSERT(gp_system.power_count() == 44, "Loaded 44 base god powers");

    // Check Bolt
    const auto* bolt = gp_system.find_power("Bolt");
    TEST_ASSERT(bolt != nullptr, "Found 'Bolt' god power");
    TEST_ASSERT(bolt->target_type == aom::GodPowerTargetType::UNIT, "'Bolt' target type is UNIT");
    TEST_ASSERT(bolt->category == aom::GodPowerCategory::OFFENSIVE, "'Bolt' category is OFFENSIVE");
    TEST_ASSERT(bolt->match_type == "LogicalTypeValidBoltTarget", "'Bolt' match type is LogicalTypeValidBoltTarget");
    TEST_ASSERT(bolt->valid_target_type == "BoltGodPower", "'Bolt' targets BoltGodPower");
    TEST_ASSERT(bolt->active_time > 0.0f, "'Bolt' active time is > 0");

    // Check Rain
    const auto* rain = gp_system.find_power("Rain");
    TEST_ASSERT(rain != nullptr, "Found 'Rain' god power");
    TEST_ASSERT(rain->category == aom::GodPowerCategory::ECONOMIC, "'Rain' category is ECONOMIC");
    TEST_ASSERT(rain->active_time == 60.0f, "'Rain' active time is 60.0s");

    // Check Frost
    const auto* frost = gp_system.find_power("Frost");
    TEST_ASSERT(frost != nullptr, "Found 'Frost' god power");
    TEST_ASSERT(frost->category == aom::GodPowerCategory::DEFENSIVE, "'Frost' category is DEFENSIVE");
    TEST_ASSERT(frost->active_time == 50.0f, "'Frost' active time is 50.0s");

    // Load Expansion God Powers (godpowers2.bar)
    aom::BarArchive gp2_bar;
    bool opened_gp2 = gp2_bar.open(base_dir + "/god powers/godpowers2.bar");
    TEST_ASSERT(opened_gp2, "Opened godpowers2.bar archive");
    TEST_ASSERT(gp2_bar.file_count() == 71, "godpowers2.bar contains exactly 71 god powers");

    bool loaded_gp2 = gp_system.load_from_bar(&gp2_bar, true);
    TEST_ASSERT(loaded_gp2, "Loaded expansion god powers from godpowers2.bar");
    TEST_ASSERT(gp_system.power_count() == 71, "Total god powers pool reached 71 powers (44 base + 27 Titans)");

    // Check Titans Expansion Powers
    const auto* implode = gp_system.find_power("Implode");
    TEST_ASSERT(implode != nullptr, "Found Titans 'Implode' god power");
    TEST_ASSERT(implode->is_expansion == true, "'Implode' is marked as expansion power");

    const auto* vortex = gp_system.find_power("Vortex");
    TEST_ASSERT(vortex != nullptr, "Found Titans 'Vortex' god power");

    // Test Casting Runtime
    bool cast_cb_fired = false;
    gp_system.set_on_power_cast([&](const aom::ActiveGodPower& act) {
        cast_cb_fired = true;
        TEST_ASSERT(act.power_name == "Bolt", "Active power name in callback is Bolt");
    });

    bool casted = gp_system.cast_power("Bolt", 1, glm::vec3(50.0f, 0.0f, 50.0f), 101);
    TEST_ASSERT(casted, "Successfully casted 'Bolt'");
    TEST_ASSERT(cast_cb_fired, "on_power_cast callback fired");
    TEST_ASSERT(gp_system.active_count() == 1, "Active powers count is 1");

    gp_system.update(1.0f); // Bolt duration is ~0.5s minimum
    TEST_ASSERT(gp_system.active_count() == 0, "Active power expired and cleared after duration");

    // -------------------------------------------------------------
    // 2. Testing Scenario & Trigger System (typetest.xml & .scn)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Scenario & Trigger Subsystem ---\n");
    aom::ScenarioTriggerSystem trigger_system;

    std::string typetest_xml = read_text_file(base_dir + "/trigger/typetest.xml");
    TEST_ASSERT(!typetest_xml.empty(), "Read typetest.xml from trigger/");

    bool dict_loaded = trigger_system.load_trigger_dictionary(typetest_xml);
    TEST_ASSERT(dict_loaded, "Loaded trigger dictionary from typetest.xml");
    TEST_ASSERT(trigger_system.condition_count() > 30, "Parsed over 30 trigger conditions");
    TEST_ASSERT(trigger_system.effect_count() > 60, "Parsed over 60 trigger effects");

    const auto* cond_timer = trigger_system.find_condition("Timer");
    TEST_ASSERT(cond_timer != nullptr, "Found 'Timer' condition");
    TEST_ASSERT(!cond_timer->params.empty(), "'Timer' has parameter definition");

    const auto* cond_dist = trigger_system.find_condition("Distance to Unit");
    TEST_ASSERT(cond_dist != nullptr, "Found 'Distance to Unit' condition");
    TEST_ASSERT(cond_dist->params.size() >= 4, "'Distance to Unit' has 4 parameters (Src, Dst, Op, Dist)");

    const auto* eff_chat = trigger_system.find_effect("Send Chat");
    TEST_ASSERT(eff_chat != nullptr, "Found 'Send Chat' effect");

    // Test Scenario SCN Header
    aom::BarArchive scn_bar;
    bool opened_scn = scn_bar.open(base_dir + "/scenario/scenarios.bar");
    TEST_ASSERT(opened_scn, "Opened scenarios.bar archive");
    TEST_ASSERT(scn_bar.file_count() == 69, "scenarios.bar has 69 entries");

    const auto* scn_entry = scn_bar.find("spc\\aomsp01.scn");
    TEST_ASSERT(scn_entry != nullptr, "Found 'spc\\aomsp01.scn' in archive");

    std::vector<uint8_t> scn_bytes = scn_bar.extract(*scn_entry);
    TEST_ASSERT(!scn_bytes.empty(), "Extracted aomsp01.scn bytes");

    auto scn_header = aom::ScenarioTriggerSystem::parse_scenario_header(scn_bytes.data(), scn_bytes.size(), false);
    TEST_ASSERT(scn_header.valid == true, "Scenario header parsed as valid");
    TEST_ASSERT(scn_header.creator_app == "RTS3.exe", "Scenario creator app is 'RTS3.exe'");
    TEST_ASSERT(!scn_header.format_magic.empty(), "Found scenario format magic");

    // Test Diplomacy Matrix
    trigger_system.set_diplomacy(1, 2, aom::DiplomacyStatus::ALLY);
    trigger_system.set_diplomacy(2, 1, aom::DiplomacyStatus::ENEMY); // non-reciprocal
    TEST_ASSERT(trigger_system.get_diplomacy(1, 2) == aom::DiplomacyStatus::ALLY, "Player 1 to 2 is ALLY");
    TEST_ASSERT(trigger_system.get_diplomacy(2, 1) == aom::DiplomacyStatus::ENEMY, "Player 2 to 1 is ENEMY");

    // Test Victory Conditions (BasicVC1 & SuddenDeath)
    std::vector<uint32_t> alive_units = {0, 15, 0}; // Player 0 dummy, Player 1 (human) has 15, Player 2 has 0
    auto vc_conquest = trigger_system.evaluate_conquest_victory(1, alive_units);
    TEST_ASSERT(vc_conquest == aom::VictoryState::VICTORY, "Conquest evaluated as VICTORY when all enemies eliminated");

    alive_units[1] = 0;
    vc_conquest = trigger_system.evaluate_conquest_victory(1, alive_units);
    TEST_ASSERT(vc_conquest == aom::VictoryState::DEFEAT, "Conquest evaluated as DEFEAT when human player has 0 units");

    auto vc_sd = trigger_system.evaluate_sudden_death(1, true, {false});
    TEST_ASSERT(vc_sd == aom::VictoryState::VICTORY, "Sudden Death evaluated as VICTORY when enemy citadel destroyed");

    // -------------------------------------------------------------
    // 3. Testing Game Systems (History TOC, .con, .set, .lng)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Game Systems (History, Console, RM, Lightning) ---\n");
    aom::GameSystemsManager systems_mgr;

    // History Table of Contents
    std::string toc_raw = read_text_file(base_dir + "/history/table of contents.xmb");
    TEST_ASSERT(!toc_raw.empty(), "Read table of contents.xmb");

    bool toc_loaded = systems_mgr.load_history_toc(reinterpret_cast<const uint8_t*>(toc_raw.data()), toc_raw.size());
    TEST_ASSERT(toc_loaded, "Parsed table of contents.xmb");
    TEST_ASSERT(systems_mgr.history_topic_count() >= 10, "Found civilopedia history topics (27 categories)");

    const auto* food_topic = systems_mgr.find_topic("Food");
    TEST_ASSERT(food_topic != nullptr, "Found 'Food' history topic");
    TEST_ASSERT(!food_topic->icon.empty(), "Food topic has icon reference");

    // History Article Rich Links
    std::string sample_article = "Alimento e o recurso mais importante para treinar <link=\"proto:Villager Greek\">aldeoes</link>.";
    auto article = aom::GameSystemsManager::parse_history_article(sample_article);
    TEST_ASSERT(article.links.size() == 1, "Extracted 1 rich text link from article");
    TEST_ASSERT(article.links[0].link_type == "proto", "Link type is 'proto'");
    TEST_ASSERT(article.links[0].target == "Villager Greek", "Link target is 'Villager Greek'");
    TEST_ASSERT(article.links[0].anchor_text == "aldeoes", "Link anchor text is 'aldeoes'");

    // Console & Keybindings (.con)
    std::string game_con = read_text_file(base_dir + "/startup/game.con");
    TEST_ASSERT(!game_con.empty(), "Read game.con from startup/");

    bool con_loaded = systems_mgr.load_con_file(game_con);
    TEST_ASSERT(con_loaded, "Parsed keybindings from game.con");
    TEST_ASSERT(systems_mgr.keybinding_count() > 50, "Loaded over 50 keybindings");

    const auto* bind_pause = systems_mgr.find_binding("pause", "game");
    TEST_ASSERT(bind_pause != nullptr, "Found 'pause' keybinding");
    TEST_ASSERT(bind_pause->action == "pause", "'pause' maps to 'pause' action");

    const auto* bind_f4 = systems_mgr.find_binding("F4", "game");
    TEST_ASSERT(bind_f4 != nullptr, "Found 'F4' keybinding");
    TEST_ASSERT(bind_f4->action == "toggleScore", "'F4' maps to 'toggleScore'");

    // Random Map .set
    std::string rm_set = read_text_file(base_dir + "/rm/fastrandom.set");
    TEST_ASSERT(!rm_set.empty(), "Read fastrandom.set from rm/");

    auto parsed_set = aom::GameSystemsManager::parse_rm_set(rm_set, "Fast Random");
    TEST_ASSERT(parsed_set.maps.size() == 8, "fastrandom.set contains exactly 8 maps");
    TEST_ASSERT(parsed_set.maps[0] == "Alfheim", "First map in fastrandom is Alfheim");
    TEST_ASSERT(parsed_set.maps[1] == "Anatolia", "Second map in fastrandom is Anatolia");

    // Lightning VFX (.lng)
    std::string bolt_lng = read_text_file(base_dir + "/models/sfx a bolt.lng");
    TEST_ASSERT(!bolt_lng.empty(), "Read sfx a bolt.lng from models/");

    auto lng_def = aom::GameSystemsManager::parse_lng(bolt_lng);
    TEST_ASSERT(lng_def.width > 0.0f, "Lightning width is parsed");
    TEST_ASSERT(lng_def.branch_width > 0.0f, "Lightning branch width is parsed");
    TEST_ASSERT(lng_def.blend_mode == "additive", "Lightning blend mode is 'additive'");
    TEST_ASSERT(!lng_def.texture.empty(), "Lightning texture reference is parsed");

    auto branches = aom::GameSystemsManager::generate_lightning_bolt(
        lng_def, glm::vec3(0, 25, 0), glm::vec3(0, 0, 0), 12345);
    TEST_ASSERT(!branches.empty(), "Generated procedural lightning bolt");
    TEST_ASSERT(branches[0].points.size() > 5, "Main trunk has multiple jittered segments");
    TEST_ASSERT(branches[0].width == lng_def.width, "Main trunk width matches definition");

    std::printf("\n========================================\n");
    std::printf(" ALL GOD POWERS & SCENARIOS TESTS PASSED! (100%%)\n");
    std::printf("========================================\n");

    return 0;
}
