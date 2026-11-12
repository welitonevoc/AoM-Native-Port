#include "campaign/campaign_mission.h"
#include "game/scenario_trigger.h"

#include <cmath>
#include <fstream>
#include <algorithm>

#define LOG_TAG "AoM-Mission"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

CampaignMissionManager::CampaignMissionManager() = default;
CampaignMissionManager::~CampaignMissionManager() = default;

bool CampaignMissionManager::load_scenario_data(BarArchive* scenarios_bar, const std::string& custom_scn_path) {
    std::vector<uint8_t> scn_data;

    // 1. Try extracting from scenarios.bar if available
    if (scenarios_bar && scenarios_bar->file_count() > 0) {
        std::vector<std::string> candidates;
        if (!custom_scn_path.empty()) {
            candidates.push_back(custom_scn_path);
            std::string alt = custom_scn_path;
            for (char& c : alt) { if (c == '/') c = '\\'; else if (c == '\\') c = '/'; }
            candidates.push_back(alt);
            size_t slash = custom_scn_path.find_last_of("/\\");
            if (slash != std::string::npos) {
                candidates.push_back(custom_scn_path.substr(slash + 1));
            }
        }
        candidates.push_back("spc\\aomsp04.scn");
        candidates.push_back("spc/aomsp04.scn");
        candidates.push_back("aomsp04.scn");

        for (const auto& c : candidates) {
            const BarEntry* entry = scenarios_bar->find(c);
            if (entry) {
                scn_data = scenarios_bar->extract(*entry);
                if (!scn_data.empty()) {
                    LOGI("Extracted scenario from scenarios.bar: %s (%zu bytes)", c.c_str(), scn_data.size());
                    break;
                }
            }
        }
    }

    // 2. Try loading from filesystem / assets
    if (scn_data.empty()) {
        std::vector<std::string> paths;
        if (!custom_scn_path.empty()) {
            paths.push_back(custom_scn_path);
            paths.push_back("assets/scenario/" + custom_scn_path);
            paths.push_back("/sdcard/Documents/AoM/scenario/" + custom_scn_path);
            paths.push_back("/sdcard/AoM/scenario/" + custom_scn_path);
        }
        paths.push_back("assets/scenario/spc/aomsp04.scn");
        paths.push_back("/storage/emulated/0/Documents/AoM-Native-Port/assets/scenario/spc/aomsp04.scn");
        paths.push_back("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/scenario/spc/aomsp04.scn");

        for (const auto& p : paths) {
            if (p.empty()) continue;
            std::ifstream file(p, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t sz = static_cast<size_t>(file.tellg());
                file.seekg(0, std::ios::beg);
                scn_data.resize(sz);
                file.read(reinterpret_cast<char*>(scn_data.data()), sz);
                LOGI("Loaded scenario from disk: %s (%zu bytes)", p.c_str(), sz);
                break;
            }
        }
    }

    if (!scn_data.empty()) {
        ScenarioHeader header = ScenarioTriggerSystem::parse_scenario_header(scn_data.data(), scn_data.size(), false);
        if (header.valid) {
            authentic_scn_loaded_ = true;
            if (!custom_scn_path.empty()) {
                scenario_title_ = custom_scn_path;
            } else {
                scenario_title_ = "Omens (Agouros)";
            }
            LOGI("Scenario valid: %s, creator: %s, format: %s",
                 scenario_title_.c_str(), header.creator_app.c_str(), header.format_magic.c_str());
            return true;
        }
    }

    LOGI("Scenario binary not found; utilizing built-in authentic Omens mission blueprint.");
    authentic_scn_loaded_ = false;
    scenario_title_ = "Omens (Agouros)";
    return true;
}

void CampaignMissionManager::setup_cinematic_track() {
    cinematic_track_.clear();
    cinematic_track_.set_name("CI_OmensIntro");

    // Waypoint 1 (0.0s): High panoramic view of Poseidon's Cliff and Temple
    CameraWaypoint wp1;
    wp1.time = 0.0f;
    wp1.position = glm::vec3(0.0f, 16.0f, -22.0f);
    wp1.target = glm::vec3(0.0f, 2.0f, 0.0f);
    wp1.fov = 52.0f;
    wp1.tension = 0.0f;
    cinematic_track_.add_waypoint(wp1);

    // Waypoint 2 (4.5s): Smooth sweeping dolly down towards the city terrace
    CameraWaypoint wp2;
    wp2.time = 4.5f;
    wp2.position = glm::vec3(-4.0f, 7.0f, -9.0f);
    wp2.target = glm::vec3(0.0f, 1.5f, 0.0f);
    wp2.fov = 46.0f;
    wp2.tension = 0.2f;
    cinematic_track_.add_waypoint(wp2);

    // Waypoint 3 (10.0s): Close conversation view of Arkantos and the Theocrat at the shrine
    CameraWaypoint wp3;
    wp3.time = 10.0f;
    wp3.position = glm::vec3(2.5f, 4.5f, -5.5f);
    wp3.target = glm::vec3(0.0f, 1.2f, 0.0f);
    wp3.fov = 40.0f;
    wp3.tension = 0.1f;
    cinematic_track_.add_waypoint(wp3);

    // Waypoint 4 (15.0s): Rapid cut & pan toward harbor coastline and ocean horizon
    CameraWaypoint wp4;
    wp4.time = 15.0f;
    wp4.position = glm::vec3(0.0f, 9.0f, 12.0f);
    wp4.target = glm::vec3(0.0f, 0.5f, 28.0f);
    wp4.fov = 56.0f;
    wp4.tension = 0.3f;
    cinematic_track_.add_waypoint(wp4);

    // Waypoint 5 (19.5s): Dramatic re-focusing on Arkantos readying his weapon
    CameraWaypoint wp5;
    wp5.time = 19.5f;
    wp5.position = glm::vec3(0.0f, 8.0f, 13.0f);
    wp5.target = glm::vec3(0.0f, 0.8f, 0.0f);
    wp5.fov = 45.0f;
    wp5.tension = 0.0f;
    cinematic_track_.add_waypoint(wp5);

    cinematic_duration_ = 20.0f;
    cinematic_track_.set_duration(cinematic_duration_);
}

void CampaignMissionManager::setup_dialogues() {
    subtitles_.clear();

    // 1. Theocrat opening line (CI_03TheocratTalksMore)
    subtitles_.push_back({
        0.5f, 4.5f,
        "Theocrat",
        "We must help break the siege at Troy.",
        0xFF00E5FF, // Cyan
        ""
    });

    // 2. Arkantos response (CI_03HeroBoyTalks)
    subtitles_.push_back({
        5.2f, 5.8f,
        "Arkantos",
        "Troops we send now will be lucky to arrive before the fight is over. Even if they do, their only task will be to show the banner of Atlantis - not a task for an Admiral.",
        0xFFFFD700, // Gold
        ""
    });

    // 3. Theocrat omens warning (CI_10TheocratYaps)
    subtitles_.push_back({
        11.2f, 4.2f,
        "Theocrat",
        "You cannot ignore the omens, Arkantos. Lord Poseidon will not be denied.",
        0xFF00E5FF, // Cyan
        ""
    });

    // 4. Arkantos alarm (CI_15PanFromCliff)
    subtitles_.push_back({
        15.6f, 3.5f,
        "Arkantos",
        "What in the name of - Kastor, stay here!",
        0xFFFFD700, // Gold
        ""
    });

    // 5. Watch shouting pirates (CI_12CutToHarbor1)
    subtitles_.push_back({
        19.2f, 4.0f,
        "Atlantis Watch",
        "Black Sails! Pirates are attacking the harbor!",
        0xFFFF3333, // Red
        "new_objective.wav"
    });
}

void CampaignMissionManager::setup_objectives() {
    objectives_.clear();

    MissionObjective obj1;
    obj1.id = 1;
    obj1.description = "Protect Atlantis by killing the Kraken.";
    obj1.primary = true;
    objectives_.push_back(obj1);

    MissionObjective obj2;
    obj2.id = 2;
    obj2.description = "Defeat the Black Sails pirate raiders.";
    obj2.primary = true;
    objectives_.push_back(obj2);

    MissionObjective obj3;
    obj3.id = 3;
    obj3.description = "Arkantos must survive.";
    obj3.primary = true;
    objectives_.push_back(obj3);

    MissionObjective obj4;
    obj4.id = 4;
    obj4.description = "Hold out until the Atlantean army arrives.";
    obj4.primary = false;
    objectives_.push_back(obj4);
}

bool CampaignMissionManager::init_mission_omens(GameplayWorld* world,
                                                BarArchive* scenarios_bar,
                                                const std::string& custom_scn_path,
                                                Camera* camera) {
    if (!world) return false;

    // Reset state
    mission_time_ = 0.0f;
    cinematic_timer_ = 0.0f;
    cinematic_active_ = true;
    stage_ = MissionStage::CINEMATIC_INTRO;
    result_ = MissionResult::IN_PROGRESS;
    army_leagues_away_ = 6;
    reinforcement_timer_ = 0.0f;
    reinforcements_spawned_ = false;
    pirate_wave_spawned_ = false;
    kraken_spawned_ = false;
    end_evaluated_ = false;

    defender_ids_.clear();
    pirate_ids_.clear();
    reinforcement_ids_.clear();

    load_scenario_data(scenarios_bar, custom_scn_path);
    setup_cinematic_track();
    setup_dialogues();
    setup_objectives();

    // Clear previous entities and spawn authentic Mission 1 entities
    world->clear_all_entities();

    // 1. Friendly Atlantean Citadel (Town Center)
    citadel_id_ = world->place_town_center(Team::FRIENDLY, glm::vec3(-6.0f, 0.0f, 0.0f), true);

    // 2. Arkantos (Hero of Atlantis)
    UnitStats arkantos_stats;
    arkantos_stats.name = "Arkantos";
    arkantos_stats.max_hp = 450.0f;
    arkantos_stats.attack_damage = 26.0f;
    arkantos_stats.move_speed = 4.2f;
    arkantos_stats.attack_range = 2.4f;
    arkantos_stats.attack_cooldown = 0.9f;
    arkantos_stats.los = 20.0f;
    arkantos_id_ = world->spawn_unit(Team::FRIENDLY, glm::vec3(0.0f, 0.0f, 0.0f), arkantos_stats);

    // 3. Atlantean Guards (2 Hoplites, 2 Toxotes, 1 Worker)
    UnitStats hoplite_stats;
    hoplite_stats.name = "Hoplite";
    hoplite_stats.max_hp = 135.0f;
    hoplite_stats.attack_damage = 13.0f;
    hoplite_stats.move_speed = 3.8f;
    hoplite_stats.attack_range = 2.0f;

    defender_ids_.push_back(world->spawn_unit(Team::FRIENDLY, glm::vec3(-1.6f, 0.0f, 1.4f), hoplite_stats));
    defender_ids_.push_back(world->spawn_unit(Team::FRIENDLY, glm::vec3(-1.6f, 0.0f, -1.4f), hoplite_stats));

    UnitStats toxotes_stats;
    toxotes_stats.name = "Toxotes";
    toxotes_stats.max_hp = 95.0f;
    toxotes_stats.attack_damage = 11.0f;
    toxotes_stats.move_speed = 4.0f;
    toxotes_stats.attack_range = 14.0f;

    defender_ids_.push_back(world->spawn_unit(Team::FRIENDLY, glm::vec3(-3.0f, 0.0f, 0.8f), toxotes_stats));
    defender_ids_.push_back(world->spawn_unit(Team::FRIENDLY, glm::vec3(-3.0f, 0.0f, -0.8f), toxotes_stats));

    world->spawn_worker(glm::vec3(-4.5f, 0.0f, 2.0f));

    // 4. Defensive Guard Tower
    Entity tower_id = world->place_building(Team::FRIENDLY, glm::vec3(-2.0f, 0.0f, -4.5f), 5.0f, BuildingType::TOWER, "Torre de Guarda");
    auto* tower_b = const_cast<BuildingComponent*>(world->get_building(tower_id));
    if (tower_b) tower_b->complete = true;

    // 5. Atlantean Coastline Resource Nodes
    world->add_resource_node(ResourceType::FOOD, glm::vec3(-4.0f, 0.0f, 4.0f), 1500);
    world->add_resource_node(ResourceType::WOOD, glm::vec3(-7.5f, 0.0f, 4.0f), 1800);
    world->add_resource_node(ResourceType::GOLD, glm::vec3(-5.5f, 0.0f, -3.5f), 2400);

    // 6. Start initial cinematic camera waypoint
    if (camera) {
        camera->position = glm::vec3(0.0f, 16.0f, -22.0f);
        camera->target = glm::vec3(0.0f, 2.0f, 0.0f);
        camera->fov = glm::radians(52.0f);
        camera->up = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    cinematic_track_.play();
    initialized_ = true;
    LOGI("Mission 1: Omens successfully initialized! Stage: CINEMATIC_INTRO");
    return true;
}

const CinematicSubtitle* CampaignMissionManager::current_subtitle() const {
    for (auto it = subtitles_.rbegin(); it != subtitles_.rend(); ++it) {
        if (mission_time_ >= it->start_time && mission_time_ < (it->start_time + it->duration)) {
            return &(*it);
        }
    }
    return nullptr;
}

void CampaignMissionManager::skip_cinematic(Camera* camera) {
    if (!cinematic_active_) return;

    cinematic_active_ = false;
    cinematic_timer_ = cinematic_duration_;
    stage_ = MissionStage::PIRATE_RAID;

    if (camera) {
        camera->target = glm::vec3(0.0f, 0.0f, 0.0f);
        camera->position = glm::vec3(0.0f, 8.0f, 13.0f);
        camera->fov = glm::radians(45.0f);
    }

    if (on_audio_cue) {
        on_audio_cue("new_objective.wav");
    }

    LOGI("Cinematic cutscene ended/skipped. Transitioned to active RTS gameplay.");
}

void CampaignMissionManager::spawn_pirate_raid(GameplayWorld* world) {
    if (!world || pirate_wave_spawned_) return;

    pirate_wave_spawned_ = true;
    stage_ = MissionStage::PIRATE_RAID;

    UnitStats pirate_stats;
    pirate_stats.name = "Pirate Raider";
    pirate_stats.max_hp = 160.0f;
    pirate_stats.attack_damage = 14.0f;
    pirate_stats.move_speed = 3.9f;
    pirate_stats.attack_range = 2.2f;

    // Spawn 3 pirates landing from the coastline
    const glm::vec3 spawn_pos[3] = {
        glm::vec3(4.0f, 0.0f, 6.0f),
        glm::vec3(5.5f, 0.0f, 7.5f),
        glm::vec3(3.0f, 0.0f, 8.0f)
    };

    for (int i = 0; i < 3; ++i) {
        Entity p = world->spawn_unit(Team::ENEMY, spawn_pos[i], pirate_stats);
        pirate_ids_.push_back(p);
        // Order pirates to attack Arkantos / Citadel
        world->order_attack(p, arkantos_id_);
    }

    LOGI("Spawned Pirate Raiding Party (%zu raiders) advancing on harbor", pirate_ids_.size());
}

void CampaignMissionManager::spawn_kraken_attack(GameplayWorld* world) {
    if (!world || kraken_spawned_) return;

    kraken_spawned_ = true;
    stage_ = MissionStage::KRAKEN_ATTACK;

    UnitStats kraken_stats;
    kraken_stats.name = "Kraken";
    kraken_stats.max_hp = 850.0f;
    kraken_stats.attack_damage = 50.0f;
    kraken_stats.move_speed = 2.7f;
    kraken_stats.attack_range = 3.2f;
    kraken_stats.attack_cooldown = 1.6f;

    // Kraken rises from the deep waters
    kraken_id_ = world->spawn_unit(Team::ENEMY, glm::vec3(6.5f, 0.0f, 12.0f), kraken_stats);
    world->order_attack(kraken_id_, arkantos_id_);

    // Subtitle & roar cue
    subtitles_.push_back({
        mission_time_, 4.5f,
        "Arkantos",
        "By the gods... a Kraken! Protect Atlantis by slaying the beast!",
        0xFFFFD700,
        "krakenacknowledge1.wav"
    });

    if (on_audio_cue) {
        on_audio_cue("krakenacknowledge1.wav");
    }

    LOGI("Spawned Mythological Enemy: KRAKEN (HP: %.0f) at harbor entrance", kraken_stats.max_hp);
}

void CampaignMissionManager::spawn_reinforcements(GameplayWorld* world) {
    if (!world || reinforcements_spawned_) return;

    reinforcements_spawned_ = true;
    stage_ = MissionStage::REINFORCEMENTS_ARRIVED;

    UnitStats army_hoplite;
    army_hoplite.name = "Atlantean Veteran";
    army_hoplite.max_hp = 160.0f;
    army_hoplite.attack_damage = 16.0f;
    army_hoplite.move_speed = 4.0f;

    const glm::vec3 march_pos[4] = {
        glm::vec3(-8.0f, 0.0f, 2.0f),
        glm::vec3(-8.0f, 0.0f, -2.0f),
        glm::vec3(-9.5f, 0.0f, 1.0f),
        glm::vec3(-9.5f, 0.0f, -1.0f)
    };

    for (int i = 0; i < 4; ++i) {
        Entity r = world->spawn_unit(Team::FRIENDLY, march_pos[i], army_hoplite);
        reinforcement_ids_.push_back(r);
        world->order_move(r, glm::vec3(-1.0f, 0.0f, 0.0f));
    }

    // Mark reinforcement objective complete
    for (auto& obj : objectives_) {
        if (obj.id == 4) obj.completed = true;
    }

    subtitles_.push_back({
        mission_time_, 5.0f,
        "Atlantean Army",
        "Arkantos! The Atlantean army has arrived! Command us, Arkantos!",
        0xFF76FF03, // Bright Green
        "new_objective.wav"
    });

    if (on_audio_cue) {
        on_audio_cue("new_objective.wav");
    }

    LOGI("Atlantean Reinforcements Arrived! (%zu units joined player)", reinforcement_ids_.size());
}

void CampaignMissionManager::evaluate_triggers(float dt, GameplayWorld* world) {
    if (!world) return;

    // Trigger 1: Pirate wave at t >= 20.5s or after intro cutscene
    if (!pirate_wave_spawned_ && (mission_time_ >= 20.5f || !cinematic_active_)) {
        spawn_pirate_raid(world);
    }

    // Trigger 2: Kraken rises at t >= 24.0s (or shortly after raid starts)
    if (!kraken_spawned_ && pirate_wave_spawned_ && (mission_time_ >= 24.0f || !cinematic_active_)) {
        spawn_kraken_attack(world);
    }

    // Trigger 3: Reinforcement march countdown (6 leagues -> 0 leagues)
    if (!reinforcements_spawned_) {
        reinforcement_timer_ += dt;
        if (reinforcement_timer_ >= reinforcement_interval_) {
            reinforcement_timer_ = 0.0f;
            if (army_leagues_away_ > 0) {
                --army_leagues_away_;
                const char* league_names[] = {
                    "zero leagues", "one league", "two leagues",
                    "three leagues", "four leagues", "five leagues", "six leagues"
                };
                char buf[128];
                std::snprintf(buf, sizeof(buf), "Atlantean Army: %s away", league_names[army_leagues_away_]);
                subtitles_.push_back({
                    mission_time_, 3.0f,
                    "Messenger",
                    buf,
                    0xFF88DDFF,
                    ""
                });
                LOGI("Reinforcements update: %s", buf);

                if (army_leagues_away_ == 0) {
                    spawn_reinforcements(world);
                }
            }
        }
    }
}

void CampaignMissionManager::update(float dt, GameplayWorld* world, Camera* camera) {
    if (!initialized_ || result_ != MissionResult::IN_PROGRESS) return;

    mission_time_ += dt;

    // 1. Cinematic Camera Update
    if (cinematic_active_) {
        cinematic_timer_ += dt;
        cinematic_track_.set_current_time(cinematic_timer_);

        if (camera) {
            Camera cam_eval = cinematic_track_.evaluate(cinematic_timer_);
            camera->position = cam_eval.position;
            camera->target = cam_eval.target;
            camera->fov = cam_eval.fov;
        }

        if (cinematic_timer_ >= cinematic_duration_) {
            skip_cinematic(camera);
        }
    }

    // 2. Play Audio Cues for newly triggered Subtitles
    for (auto& sub : subtitles_) {
        if (!sub.audio_played && !sub.audio_cue.empty() &&
            mission_time_ >= sub.start_time && mission_time_ < (sub.start_time + sub.duration)) {
            sub.audio_played = true;
            if (on_audio_cue) {
                on_audio_cue(sub.audio_cue);
            }
        }
    }

    // 3. Evaluate Live Scenario Triggers
    evaluate_triggers(dt, world);

    // 4. Check Victory / Defeat Conditions
    if (!end_evaluated_ && world) {
        // Condition A: Arkantos died -> DEFEAT
        const auto* arkantos_hp = world->get_health(arkantos_id_);
        if (!arkantos_hp || arkantos_hp->current_hp <= 0) {
            result_ = MissionResult::DEFEAT;
            end_evaluated_ = true;
            stage_ = MissionStage::VICTORY_DEFEAT;
            for (auto& obj : objectives_) {
                if (obj.id == 3) obj.failed = true;
            }
            subtitles_.push_back({
                mission_time_, 6.0f,
                "Arkantos",
                "Atlantis has fallen... all is lost!",
                0xFFFF3333,
                "lose.wav"
            });
            if (on_audio_cue) on_audio_cue("lose.wav");
            if (on_mission_completed) on_mission_completed(result_);
            LOGI("MISSION 1 RESULT: DEFEAT (Arkantos has fallen)");
            return;
        }

        // Condition B: Citadel destroyed -> DEFEAT
        const auto* citadel_hp = world->get_health(citadel_id_);
        if (!citadel_hp || citadel_hp->current_hp <= 0) {
            result_ = MissionResult::DEFEAT;
            end_evaluated_ = true;
            stage_ = MissionStage::VICTORY_DEFEAT;
            if (on_audio_cue) on_audio_cue("lose.wav");
            if (on_mission_completed) on_mission_completed(result_);
            LOGI("MISSION 1 RESULT: DEFEAT (Citadel destroyed)");
            return;
        }

        // Condition C: Kraken slain and Pirates repelled -> VICTORY
        if (kraken_spawned_ && pirate_wave_spawned_) {
            const auto* kraken_hp = world->get_health(kraken_id_);
            bool kraken_dead = (!kraken_hp || kraken_hp->current_hp <= 0);

            bool all_pirates_dead = true;
            for (Entity p : pirate_ids_) {
                const auto* p_hp = world->get_health(p);
                if (p_hp && p_hp->current_hp > 0) {
                    all_pirates_dead = false;
                    break;
                }
            }

            if (kraken_dead) {
                for (auto& obj : objectives_) {
                    if (obj.id == 1) obj.completed = true;
                }
            }
            if (all_pirates_dead) {
                for (auto& obj : objectives_) {
                    if (obj.id == 2) obj.completed = true;
                }
            }

            if (kraken_dead && all_pirates_dead) {
                result_ = MissionResult::VICTORY;
                end_evaluated_ = true;
                stage_ = MissionStage::VICTORY_DEFEAT;
                for (auto& obj : objectives_) {
                    if (!obj.failed) obj.completed = true;
                }
                subtitles_.push_back({
                    mission_time_, 6.0f,
                    "Arkantos",
                    "The Black Sails are fleeing - it will take more than pirates to overrun Atlantis!",
                    0xFFFFD700,
                    "win.wav"
                });
                if (on_audio_cue) on_audio_cue("win.wav");
                if (on_mission_completed) on_mission_completed(result_);
                LOGI("MISSION 1 RESULT: VICTORY! (Kraken slain and Pirates defeated)");
            }
        }
    }
}

} // namespace aom
