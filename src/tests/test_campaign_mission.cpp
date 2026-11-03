#include "campaign/campaign.h"
#include "campaign/campaign_mission.h"
#include "game/scenario_trigger.h"
#include "assets/bar_archive.h"
#include "ecs/gameplay.h"

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

static std::string read_file_string(const std::string& path) {
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
    std::printf("====================================================\n");
    std::printf(" AoM Sprint 5: Campaign & Cinematic Mission Tests    \n");
    std::printf("====================================================\n\n");

    const std::string base_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology";

    // -------------------------------------------------------------
    // TEST 1: Campaign XML & Structure Loading (main campaign.cpn)
    // -------------------------------------------------------------
    std::printf("--- 1. Testing Campaign XML & Structure Loading ---\n");
    aom::CampaignManager campaign_mgr;

    std::string cpn_path = "assets/main campaign.cpn";
    if (!std::ifstream(cpn_path).good()) {
        cpn_path = base_dir + "/data/main campaign.cpn";
    }

    bool loaded_cpn = campaign_mgr.load_campaign(cpn_path);
    TEST_ASSERT(loaded_cpn, "Loaded main campaign.cpn successfully");
    TEST_ASSERT(campaign_mgr.scenario_count() == 60, "Campaign contains all 60 scenarios (cinematics + missions)");

    const auto* camp = campaign_mgr.current_campaign();
    TEST_ASSERT(camp != nullptr, "Current campaign pointer is valid");
    TEST_ASSERT(camp->name.find("Fall of the Trident") != std::string::npos, "Campaign title is 'Fall of the Trident'");

    // Check Scenario 0: Prologue (cineA_1)
    const auto* sc0 = campaign_mgr.get_scenario(0);
    TEST_ASSERT(sc0 != nullptr, "Scenario 0 is valid");
    TEST_ASSERT(sc0->display_name.find("Troubled Sleep") != std::string::npos, "Scenario 0 is 'Troubled Sleep'");
    TEST_ASSERT(sc0->filename == "spc\\cinematics\\cineA_1", "Scenario 0 filename matches 'spc\\cinematics\\cineA_1'");

    // Check Scenario 1: Mission 1: Omens (spc\aomsp04)
    const auto* sc1 = campaign_mgr.get_scenario(1);
    TEST_ASSERT(sc1 != nullptr, "Scenario 1 is valid");
    TEST_ASSERT(sc1->display_name.find("Omens") != std::string::npos, "Scenario 1 is 'Omens'");
    TEST_ASSERT(sc1->filename == "spc\\aomsp04", "Scenario 1 filename matches 'spc\\aomsp04'");
    TEST_ASSERT(sc1->background_set == "Atlantis", "Scenario 1 background set is 'Atlantis'");

    // Scenario Lookups
    const auto* found_sc = campaign_mgr.find_scenario_by_filename("spc\\aomsp04");
    TEST_ASSERT(found_sc != nullptr, "Lookup by filename 'spc\\aomsp04' succeeded");
    TEST_ASSERT(found_sc->display_name.find("Omens") != std::string::npos, "Lookup result matches 'Omens'");

    const auto* found_by_name = campaign_mgr.find_scenario_by_name("Omens");
    TEST_ASSERT(found_by_name != nullptr, "Lookup by name 'Omens' succeeded");

    // -------------------------------------------------------------
    // TEST 2: Authentic Scenario Binary Header & Triggers (.scn)
    // -------------------------------------------------------------
    std::printf("\n--- 2. Testing Authentic Scenario Header (.scn) ---\n");
    std::vector<uint8_t> scn_bytes;

    // Check scenarios.bar
    aom::BarArchive scn_bar;
    std::string bar_path = base_dir + "/scenario/scenarios.bar";
    if (scn_bar.open(bar_path)) {
        const auto* entry = scn_bar.find("spc\\aomsp04.scn");
        if (entry) {
            scn_bytes = scn_bar.extract(*entry);
        }
    }

    if (scn_bytes.empty()) {
        std::ifstream scn_f("assets/scenario/spc/aomsp04.scn", std::ios::binary);
        if (scn_f) {
            scn_f.seekg(0, std::ios::end);
            size_t sz = static_cast<size_t>(scn_f.tellg());
            scn_f.seekg(0, std::ios::beg);
            scn_bytes.resize(sz);
            scn_f.read(reinterpret_cast<char*>(scn_bytes.data()), sz);
        }
    }

    TEST_ASSERT(!scn_bytes.empty(), "Obtained aomsp04.scn data from archive or assets");
    auto header = aom::ScenarioTriggerSystem::parse_scenario_header(scn_bytes.data(), scn_bytes.size(), false);
    TEST_ASSERT(header.valid, "Scenario header parsed as authentic and valid");
    TEST_ASSERT(header.creator_app == "RTS3.exe", "Header creator app is 'RTS3.exe'");
    TEST_ASSERT(!header.format_magic.empty(), "Scenario format magic verified (R3SG/SM4)");

    // -------------------------------------------------------------
    // TEST 3: CampaignMissionManager Omens Setup & Entity Spawning
    // -------------------------------------------------------------
    std::printf("\n--- 3. Testing Mission 1: Omens Initialization ---\n");
    aom::GameplayWorld world;
    aom::Camera camera;
    aom::CampaignMissionManager mission_mgr;

    bool init_ok = mission_mgr.init_mission_omens(&world, &scn_bar, "assets/scenario/spc/aomsp04.scn", &camera);
    TEST_ASSERT(init_ok, "Initialized Mission 1: Omens in CampaignMissionManager");
    TEST_ASSERT(mission_mgr.is_cinematic_active(), "Cinematic intro is active upon mission start");
    TEST_ASSERT(mission_mgr.stage() == aom::MissionStage::CINEMATIC_INTRO, "Stage is CINEMATIC_INTRO");
    TEST_ASSERT(mission_mgr.result() == aom::MissionResult::IN_PROGRESS, "Result is IN_PROGRESS");

    // Arkantos Hero verification
    aom::Entity arkantos = mission_mgr.arkantos_entity();
    TEST_ASSERT(arkantos != aom::INVALID_ENTITY, "Arkantos hero entity spawned");
    const auto* ark_unit = world.get_unit(arkantos);
    const auto* ark_hp = world.get_health(arkantos);
    TEST_ASSERT(ark_unit != nullptr && ark_unit->unit_type_name == "Arkantos", "Hero unit name is 'Arkantos'");
    TEST_ASSERT(ark_unit->team == aom::Team::FRIENDLY, "Arkantos belongs to Team::FRIENDLY");
    TEST_ASSERT(ark_hp != nullptr && ark_hp->max_hp >= 400, "Arkantos has Hero HP (>= 400)");

    // Citadel (Town Center) verification
    aom::Entity citadel = mission_mgr.citadel_entity();
    TEST_ASSERT(citadel != aom::INVALID_ENTITY, "Citadel / Town Center spawned");
    const auto* cit_bld = world.get_building(citadel);
    TEST_ASSERT(cit_bld != nullptr && cit_bld->type == aom::BuildingType::TOWN_CENTER, "Citadel is flagged as town center");

    // Atlantean Defenders
    TEST_ASSERT(mission_mgr.defender_entities().size() == 4, "4 Atlantean defenders spawned (2 Hoplites + 2 Toxotes)");

    // Objectives verification
    const auto& objs = mission_mgr.objectives();
    TEST_ASSERT(objs.size() == 4, "4 Scenario objectives registered");
    TEST_ASSERT(objs[0].description.find("Kraken") != std::string::npos, "Objective 1 requires killing Kraken");
    TEST_ASSERT(objs[1].description.find("pirate") != std::string::npos, "Objective 2 requires defeating pirates");
    TEST_ASSERT(objs[2].description.find("Arkantos") != std::string::npos, "Objective 3 requires Arkantos survival");

    // -------------------------------------------------------------
    // TEST 4: Cinematic Dialogue & Subtitle Sequencing
    // -------------------------------------------------------------
    std::printf("\n--- 4. Testing Cinematic Dialogue & Subtitles ---\n");

    // Check Theocrat dialogue at t=1.0s
    mission_mgr.update(1.0f, &world, &camera);
    const auto* sub1 = mission_mgr.current_subtitle();
    TEST_ASSERT(sub1 != nullptr, "Subtitle active at t=1.0s");
    TEST_ASSERT(sub1->speaker == "Theocrat", "Speaker is 'Theocrat'");
    TEST_ASSERT(sub1->text.find("break the siege at Troy") != std::string::npos, "Text contains 'break the siege at Troy'");
    TEST_ASSERT(sub1->color == 0xFF00E5FF, "Theocrat text color is signature Cyan (0xFF00E5FF)");

    // Advance to Arkantos response at t=6.0s
    mission_mgr.update(5.0f, &world, &camera);
    const auto* sub2 = mission_mgr.current_subtitle();
    TEST_ASSERT(sub2 != nullptr, "Subtitle active at t=6.0s");
    TEST_ASSERT(sub2->speaker == "Arkantos", "Speaker is 'Arkantos'");
    TEST_ASSERT(sub2->text.find("Troops we send now") != std::string::npos, "Text contains 'Troops we send now'");
    TEST_ASSERT(sub2->color == 0xFFFFD700, "Arkantos text color is signature Gold (0xFFFFD700)");

    // -------------------------------------------------------------
    // TEST 5: Cinematic Skip & Transition to RTS Gameplay
    // -------------------------------------------------------------
    std::printf("\n--- 5. Testing Cinematic Skip & RTS Camera Mode ---\n");
    mission_mgr.skip_cinematic(&camera);
    TEST_ASSERT(!mission_mgr.is_cinematic_active(), "Cinematic cutscene inactive after skip");
    TEST_ASSERT(mission_mgr.stage() == aom::MissionStage::PIRATE_RAID, "Mission progressed to PIRATE_RAID stage");
    TEST_ASSERT(camera.fov <= glm::radians(50.0f), "Camera smoothly reverted to tactical RTS FOV");

    // -------------------------------------------------------------
    // TEST 6: Live Triggers (Pirates, Kraken & Reinforcements)
    // -------------------------------------------------------------
    std::printf("\n--- 6. Testing Live Triggers & Combat Spawning ---\n");

    // Step to trigger pirate and kraken spawns
    mission_mgr.update(25.0f, &world, &camera);
    TEST_ASSERT(mission_mgr.pirate_entities().size() == 3, "Spawned 3 Pirate Raiders on the coastline");
    aom::Entity kraken = mission_mgr.kraken_entity();
    TEST_ASSERT(kraken != aom::INVALID_ENTITY, "Mythological enemy KRAKEN spawned");
    const auto* kraken_hp = world.get_health(kraken);
    TEST_ASSERT(kraken_hp != nullptr && kraken_hp->max_hp >= 800, "Kraken spawned with massive mythic HP (>= 800)");

    // Check Reinforcement progression countdown (6 leagues -> 0)
    int initial_leagues = mission_mgr.army_leagues_away();
    TEST_ASSERT(initial_leagues <= 6, "Army starts 6 leagues away");

    // Step simulation across reinforcement intervals until arrival
    for (int step = 0; step < 7; ++step) {
        mission_mgr.update(15.0f, &world, &camera);
    }
    TEST_ASSERT(mission_mgr.army_leagues_away() == 0, "Reinforcement countdown reached 0 leagues");
    TEST_ASSERT(mission_mgr.reinforcement_entities().size() == 4, "4 Atlantean reinforcement veterans joined player");
    TEST_ASSERT(mission_mgr.objectives()[3].completed, "Secondary objective (army arrival) marked completed");

    // -------------------------------------------------------------
    // TEST 7: Victory Condition Simulation
    // -------------------------------------------------------------
    std::printf("\n--- 7. Testing Victory & Defeat Outcomes ---\n");

    // Defeat test: Arkantos falls
    {
        aom::GameplayWorld defeat_world;
        aom::Camera cam;
        aom::CampaignMissionManager mgr;
        mgr.init_mission_omens(&defeat_world, &scn_bar, "assets/scenario/spc/aomsp04.scn", &cam);
        mgr.skip_cinematic(&cam);

        // Inflict lethal damage to Arkantos
        auto* ark_h = const_cast<aom::HealthComponent*>(defeat_world.get_health(mgr.arkantos_entity()));
        TEST_ASSERT(ark_h != nullptr, "Got Arkantos health component");
        ark_h->current_hp = 0;

        mgr.update(1.0f, &defeat_world, &cam);
        TEST_ASSERT(mgr.result() == aom::MissionResult::DEFEAT, "Mission ends in DEFEAT when Arkantos dies");
        TEST_ASSERT(mgr.objectives()[2].failed, "Objective 3 (Arkantos survive) marked failed");
    }

    // Victory test: Kraken slain and all pirates eliminated
    {
        aom::GameplayWorld vic_world;
        aom::Camera cam;
        aom::CampaignMissionManager mgr;
        mgr.init_mission_omens(&vic_world, &scn_bar, "assets/scenario/spc/aomsp04.scn", &cam);
        mgr.skip_cinematic(&cam);
        mgr.update(25.0f, &vic_world, &cam); // Spawns kraken and pirates

        // Slay Kraken
        auto* k_h = const_cast<aom::HealthComponent*>(vic_world.get_health(mgr.kraken_entity()));
        TEST_ASSERT(k_h != nullptr, "Got Kraken health component");
        k_h->current_hp = 0;

        // Eliminate all pirates
        for (aom::Entity p : mgr.pirate_entities()) {
            auto* p_h = const_cast<aom::HealthComponent*>(vic_world.get_health(p));
            if (p_h) p_h->current_hp = 0;
        }

        mgr.update(1.0f, &vic_world, &cam);
        TEST_ASSERT(mgr.result() == aom::MissionResult::VICTORY, "Mission ends in VICTORY when Kraken & Pirates slain");
        TEST_ASSERT(mgr.objectives()[0].completed, "Objective 1 (Kraken slain) marked completed");
        TEST_ASSERT(mgr.objectives()[1].completed, "Objective 2 (Pirates repelled) marked completed");

        const auto* vic_sub = mgr.current_subtitle();
        TEST_ASSERT(vic_sub != nullptr, "Victory dialogue subtitle triggered");
        TEST_ASSERT(vic_sub->text.find("Black Sails are fleeing") != std::string::npos,
                    "Victory dialogue matches authentic AoM quote");
    }

    // -------------------------------------------------------------
    // TEST 8: Campaign Progression to Mission 2 (Consequences)
    // -------------------------------------------------------------
    std::printf("\n--- 8. Testing Campaign Advance to Scenario 2 ---\n");
    campaign_mgr.set_scenario_index(1); // At Omens
    TEST_ASSERT(campaign_mgr.current_index() == 1, "Current scenario is Mission 1 (Omens)");

    bool advanced = campaign_mgr.advance_scenario();
    TEST_ASSERT(advanced, "Advanced campaign scenario successfully");
    TEST_ASSERT(campaign_mgr.current_index() == 2, "Current scenario index is now 2");

    const auto* sc2 = campaign_mgr.current_scenario();
    TEST_ASSERT(sc2 != nullptr, "Scenario 2 is valid");
    TEST_ASSERT(sc2->filename.find("cine04_2") != std::string::npos, "Scenario 2 is post-mission cinematic 'cine04_2' (Stolen Trident)");

    // Advance to next playable scenario (Mission 2: Consequences)
    campaign_mgr.advance_scenario();
    TEST_ASSERT(campaign_mgr.current_index() == 3, "Advanced to scenario index 3");
    const auto* sc3 = campaign_mgr.current_scenario();
    TEST_ASSERT(sc3 != nullptr, "Scenario 3 is valid");
    TEST_ASSERT(sc3->filename.find("aomsp05") != std::string::npos, "Scenario 3 filename matches 'aomsp05' (Consequences)");

    std::printf("\n====================================================\n");
    std::printf(" ALL SPRINT 5 CAMPAIGN & MISSION TESTS PASSED! (100%%)\n");
    std::printf("====================================================\n");

    return 0;
}
