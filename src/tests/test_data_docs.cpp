#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#include "graphics/lighting.h"
#include "campaign/campaign.h"
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"
#include "data/game_data.h"

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return 1; \
        } else { \
            std::cout << "PASS: " << msg << std::endl; \
        } \
    } while (0)

using namespace aom;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " AoM Data & Docs REA Test Suite        " << std::endl;
    std::cout << "========================================" << std::endl;

    const std::string data_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data";
    const std::string docs_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/docs";

    // ---------------------------------------------------------
    // TEST 1: Lighting System (.LGT Files)
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Lighting System (.LGT Files) ---" << std::endl;
    LightingManager light_mgr;

    std::vector<std::string> lgt_files = {
        "default.lgt", "alfheim.lgt", "anatolia.lgt", "dawn.lgt",
        "dusk.lgt", "eclipse.lgt", "erebus.lgt", "fimbulwinter.lgt",
        "ghost lake.lgt", "night.lgt", "olympus.lgt"
    };

    size_t loaded_lgt = 0;
    for (const auto& lgt_name : lgt_files) {
        std::string full_path = data_dir + "/" + lgt_name;
        if (light_mgr.load_file(full_path)) {
            ++loaded_lgt;
        }
    }
    TEST_ASSERT(loaded_lgt == 11, "Loaded all 11 .LGT lighting sets from data/");

    const LightingSet* def_light = light_mgr.get_set("Default");
    TEST_ASSERT(def_light != nullptr, "Found 'Default' lighting set");
    if (def_light) {
        TEST_ASSERT(def_light->name == "Default", "'Default' lighting set name matches");
        glm::vec3 sun_norm = def_light->get_sun_color_normalized();
        TEST_ASSERT(sun_norm.r >= 0.99f && sun_norm.g >= 0.99f && sun_norm.b >= 0.99f,
                    "Default sun color is pure white (255, 255, 255)");

        glm::vec3 sun_dir = def_light->calculate_sun_direction();
        float len = glm::length(sun_dir);
        TEST_ASSERT(std::abs(len - 1.0f) < 0.001f, "Sun direction vector is unit length 1.0");
    }

    const LightingSet* night_light = light_mgr.get_set("Night");
    TEST_ASSERT(night_light != nullptr, "Found 'Night' lighting set");
    if (night_light) {
        glm::vec3 amb_norm = night_light->get_ambient_normalized();
        TEST_ASSERT(amb_norm.r < 0.01f, "Night ambient red channel is 0 (dark blue palette)");
        TEST_ASSERT(amb_norm.b > 0.5f, "Night ambient blue channel is prominent");
    }

    // ---------------------------------------------------------
    // TEST 2: Campaign System (.CPN / .CPX Files)
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Campaign System (.CPN / .CPX Files) ---" << std::endl;
    CampaignManager camp_mgr;

    // Load main campaign
    bool main_camp_ok = camp_mgr.load_campaign(data_dir + "/main campaign.cpn");
    TEST_ASSERT(main_camp_ok, "Loaded 'main campaign.cpn' successfully");
    TEST_ASSERT(camp_mgr.current_campaign() != nullptr, "Campaign pointer is valid");
    TEST_ASSERT(camp_mgr.current_campaign()->name.find("Fall of the Trident") != std::string::npos,
                "Campaign name matches 'Fall of the Trident'");
    TEST_ASSERT(camp_mgr.scenario_count() == 60, "Main campaign contains exactly 60 scenarios");

    // Check first scenario (cineA_1)
    const CampaignScenario* sc0 = camp_mgr.get_scenario(0);
    TEST_ASSERT(sc0 != nullptr, "Scenario 0 is valid");
    if (sc0) {
        TEST_ASSERT(sc0->display_name.find("Troubled Sleep") != std::string::npos,
                    "Scenario 0 title contains 'Troubled Sleep'");
        TEST_ASSERT(sc0->filename == "spc\\cinematics\\cineA_1",
                    "Scenario 0 filename matches cinematic 'spc\\cinematics\\cineA_1'");
        TEST_ASSERT(sc0->background_set == "Atlantis", "Scenario 0 background is 'Atlantis'");
    }

    // Check second scenario (Omens) with overlays
    const CampaignScenario* sc1 = camp_mgr.get_scenario(1);
    TEST_ASSERT(sc1 != nullptr, "Scenario 1 is valid");
    if (sc1) {
        TEST_ASSERT(sc1->display_name.find("Omens") != std::string::npos,
                    "Scenario 1 title contains 'Omens'");
        TEST_ASSERT(!sc1->overlays.empty(), "Scenario 1 contains campaign map overlay markers");
    }

    // Test campaign progression
    TEST_ASSERT(camp_mgr.current_index() == 0, "Initial scenario index is 0");
    camp_mgr.advance_scenario();
    TEST_ASSERT(camp_mgr.current_index() == 1, "Advanced to scenario index 1");
    camp_mgr.set_scenario_index(59);
    TEST_ASSERT(camp_mgr.current_index() == 59, "Jumped to final scenario index 59");

    // Load expansion campaign
    CampaignManager xpack_mgr;
    bool xpack_ok = xpack_mgr.load_campaign(data_dir + "/xpack campaign.cpx");
    TEST_ASSERT(xpack_ok, "Loaded 'xpack campaign.cpx' successfully");
    TEST_ASSERT(xpack_mgr.current_campaign()->name.find("The New Atlantis") != std::string::npos,
                "XPack campaign name matches 'The New Atlantis'");

    // ---------------------------------------------------------
    // TEST 3: XMB Parser Transparent l33t Decompression
    // ---------------------------------------------------------
    std::cout << "\n--- Testing XMB Parser Transparent l33t Decompression ---" << std::endl;
    std::ifstream uimain_in(data_dir + "/uimain.xmb", std::ios::binary);
    TEST_ASSERT(uimain_in.is_open(), "Opened data/uimain.xmb on disk");
    std::vector<uint8_t> uimain_bytes((std::istreambuf_iterator<char>(uimain_in)),
                                       std::istreambuf_iterator<char>());
    TEST_ASSERT(!uimain_bytes.empty(), "Read uimain.xmb bytes");

    auto uimain_root = XmbParser::parse(uimain_bytes);
    TEST_ASSERT(uimain_root != nullptr, "XmbParser decompressed and parsed uimain.xmb successfully");
    if (uimain_root) {
        TEST_ASSERT(uimain_root->name == "layout", "uimain.xmb root element is 'layout'");
    }

    // ---------------------------------------------------------
    // TEST 4: Data.bar Extraction & GameDataManager Registry
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Data.bar Extraction & GameDataManager ---" << std::endl;
    BarArchive data_bar;
    bool bar_ok = data_bar.open(data_dir + "/data.bar");
    TEST_ASSERT(bar_ok, "Opened data/data.bar archive");
    TEST_ASSERT(data_bar.file_count() == 126, "data.bar has exactly 126 entries");

    GameDataManager game_data;
    bool loaded_game_data = game_data.load_from_bar(data_bar);
    TEST_ASSERT(loaded_game_data, "Loaded proto units, major gods and techtree from data.bar");
    TEST_ASSERT(game_data.unit_count() >= 600, "Loaded over 600 proto units from proto.xmb");

    // Verify Hoplite
    const ProtoUnitData* hoplite = game_data.find_unit("Hoplite");
    TEST_ASSERT(hoplite != nullptr, "Found 'Hoplite' in proto database");
    if (hoplite) {
        TEST_ASSERT(std::abs(hoplite->max_hitpoints - 115.0f) < 0.01f, "Hoplite Max HP is exactly 115.0");
        TEST_ASSERT(std::abs(hoplite->los - 16.0f) < 0.01f, "Hoplite LOS is exactly 16.0");
        TEST_ASSERT(std::abs(hoplite->max_velocity - 4.2f) < 0.01f, "Hoplite velocity is exactly 4.2");
        TEST_ASSERT(std::abs(hoplite->cost_food - 60.0f) < 0.01f, "Hoplite cost Food is exactly 60.0");
        TEST_ASSERT(std::abs(hoplite->cost_gold - 35.0f) < 0.01f, "Hoplite cost Gold is exactly 35.0");
    }

    // Verify Toxotes
    const ProtoUnitData* toxotes = game_data.find_unit("Toxotes");
    TEST_ASSERT(toxotes != nullptr, "Found 'Toxotes' in proto database");
    if (toxotes) {
        TEST_ASSERT(std::abs(toxotes->max_hitpoints - 60.0f) < 0.01f, "Toxotes Max HP is exactly 60.0");
        TEST_ASSERT(std::abs(toxotes->los - 19.0f) < 0.01f, "Toxotes LOS is exactly 19.0");
        TEST_ASSERT(std::abs(toxotes->cost_wood - 55.0f) < 0.01f, "Toxotes cost Wood is exactly 55.0");
        TEST_ASSERT(std::abs(toxotes->cost_gold - 35.0f) < 0.01f, "Toxotes cost Gold is exactly 35.0");
    }

    // Verify Minotaur
    const ProtoUnitData* minotaur = game_data.find_unit("Minotaur");
    TEST_ASSERT(minotaur != nullptr, "Found 'Minotaur' myth unit in proto database");
    if (minotaur) {
        TEST_ASSERT(std::abs(minotaur->max_hitpoints - 300.0f) < 0.01f, "Minotaur Max HP is exactly 300.0");
        TEST_ASSERT(std::abs(minotaur->cost_food - 200.0f) < 0.01f, "Minotaur cost Food is exactly 200.0");
        TEST_ASSERT(std::abs(minotaur->cost_favor - 16.0f) < 0.01f, "Minotaur cost Favor is exactly 16.0");
    }

    // Verify Major Gods
    TEST_ASSERT(game_data.god_count() >= 9, "Loaded Major Gods from majorgods.xmb");
    const MajorGodData* zeus = game_data.find_god("Age 1 Zeus");
    TEST_ASSERT(zeus != nullptr, "Found 'Age 1 Zeus'");
    if (zeus) {
        TEST_ASSERT(zeus->minor_gods.size() == 6, "Zeus has exactly 6 Minor Gods assigned");
    }
    const MajorGodData* poseidon = game_data.find_god("Age 1 Poseidon");
    TEST_ASSERT(poseidon != nullptr, "Found 'Age 1 Poseidon'");
    const MajorGodData* hades = game_data.find_god("Age 1 Hades");
    TEST_ASSERT(hades != nullptr, "Found 'Age 1 Hades'");

    // Verify Technologies
    TEST_ASSERT(game_data.tech_count() >= 300, "Loaded over 300 techs from techtree.xmb");
    const TechData* age2 = game_data.find_tech("Age 2");
    TEST_ASSERT(age2 != nullptr, "Found 'Age 2' technology in techtree");

    // ---------------------------------------------------------
    // TEST 5: Cross-Validation with Ensemble Studios Documentation
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Cross-Validation with Documentation in docs/ ---" << std::endl;
    std::ifstream ai_doc_in(docs_dir + "/aom ai script help file.rtf");
    TEST_ASSERT(ai_doc_in.is_open(), "Official 'aom ai script help file.rtf' verified present in docs/");

    std::ifstream rm_doc_in(docs_dir + "/aom random map help file.rtf");
    TEST_ASSERT(rm_doc_in.is_open(), "Official 'aom random map help file.rtf' verified present in docs/");

    std::cout << "========================================" << std::endl;
    std::cout << " ALL DATA & DOCS TESTS PASSED! (100%)   " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
