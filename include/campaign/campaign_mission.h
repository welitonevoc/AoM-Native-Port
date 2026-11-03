#pragma once

/// @file campaign_mission.h
/// @brief Authentic Mission and Cinematic Trigger Runtime for Age of Mythology campaigns
/// Implements Mission 1: Omens ("Agouros - Ataque a Atlântida"), cutscene tracks,
/// dialogue subtitles, reinforcement countdowns, and Kraken/Pirate combat logic.

#include "campaign/campaign.h"
#include "ecs/gameplay.h"
#include "renderer/renderer.h"
#include "renderer/camera_track.h"
#include "assets/bar_archive.h"

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace aom {

/// Represents an authentic spoken dialogue line in cutscene or gameplay
struct CinematicSubtitle {
    float start_time = 0.0f;
    float duration = 4.0f;
    std::string speaker;         ///< Character speaking ("Theocrat", "Arkantos", "Atlantean Army", "Atlantis Watch")
    std::string text;            ///< Authentic spoken dialogue text
    uint32_t color = 0xFFFFD700; ///< Speaker color (Cyan=Theocrat, Gold=Arkantos, Red=Danger, Green=Army)
    std::string audio_cue;       ///< Audio sound effect or speech filename
    bool audio_played = false;
};

/// Represents an active mission objective displayed on the HUD
struct MissionObjective {
    uint32_t id = 0;
    std::string description;
    bool completed = false;
    bool failed = false;
    bool primary = true;
};

/// Stage of the campaign mission
enum class MissionStage {
    CINEMATIC_INTRO,
    PIRATE_RAID,
    KRAKEN_ATTACK,
    REINFORCEMENTS_MARCH,
    REINFORCEMENTS_ARRIVED,
    VICTORY_DEFEAT
};

/// Victory / Defeat outcome
enum class MissionResult {
    IN_PROGRESS,
    VICTORY,
    DEFEAT
};

/// Manager and runner for authentic AoM campaign missions
class CampaignMissionManager {
public:
    CampaignMissionManager();
    ~CampaignMissionManager();

    /// Initialize Mission 1: Omens (Agouros) with authentic scenario data
    bool init_mission_omens(GameplayWorld* world,
                            BarArchive* scenarios_bar,
                            const std::string& custom_scn_path,
                            Camera* camera);

    /// Step simulation: updates cutscene tracks, triggers, reinforcements, and win/loss
    void update(float dt, GameplayWorld* world, Camera* camera);

    /// User skip: bypasses opening cutscene directly to tactical RTS gameplay
    void skip_cinematic(Camera* camera);

    /// Check if cinematic cutscene is currently playing
    [[nodiscard]] bool is_cinematic_active() const { return cinematic_active_; }

    /// Current elapsed mission time in seconds
    [[nodiscard]] float mission_time() const { return mission_time_; }

    /// Current mission progress stage
    [[nodiscard]] MissionStage stage() const { return stage_; }

    /// Current mission result
    [[nodiscard]] MissionResult result() const { return result_; }

    /// Reinforcement distance countdown in leagues (6 down to 0)
    [[nodiscard]] int army_leagues_away() const { return army_leagues_away_; }

    /// Currently active dialogue subtitle (or nullptr if none active)
    [[nodiscard]] const CinematicSubtitle* current_subtitle() const;

    /// Active scenario objectives
    [[nodiscard]] const std::vector<MissionObjective>& objectives() const { return objectives_; }

    /// Key entity accessors
    [[nodiscard]] Entity arkantos_entity() const { return arkantos_id_; }
    [[nodiscard]] Entity kraken_entity() const { return kraken_id_; }
    [[nodiscard]] Entity citadel_entity() const { return citadel_id_; }
    [[nodiscard]] const std::vector<Entity>& defender_entities() const { return defender_ids_; }
    [[nodiscard]] const std::vector<Entity>& pirate_entities() const { return pirate_ids_; }
    [[nodiscard]] const std::vector<Entity>& reinforcement_entities() const { return reinforcement_ids_; }

    /// Check if authentic .scn scenario data was parsed
    [[nodiscard]] bool is_authentic_scn_loaded() const { return authentic_scn_loaded_; }
    [[nodiscard]] const std::string& scenario_title() const { return scenario_title_; }

    /// Event callbacks
    std::function<void(const std::string& cue)> on_audio_cue;
    std::function<void(const MissionObjective& obj)> on_objective_updated;
    std::function<void(MissionResult res)> on_mission_completed;

private:
    bool load_scenario_data(BarArchive* scenarios_bar, const std::string& custom_scn_path);
    void setup_cinematic_track();
    void setup_dialogues();
    void setup_objectives();
    void evaluate_triggers(float dt, GameplayWorld* world);
    void spawn_pirate_raid(GameplayWorld* world);
    void spawn_kraken_attack(GameplayWorld* world);
    void spawn_reinforcements(GameplayWorld* world);

    bool initialized_ = false;
    bool cinematic_active_ = true;
    float cinematic_timer_ = 0.0f;
    float cinematic_duration_ = 20.0f;
    float mission_time_ = 0.0f;
    MissionStage stage_ = MissionStage::CINEMATIC_INTRO;
    MissionResult result_ = MissionResult::IN_PROGRESS;

    CameraTrack cinematic_track_;
    std::vector<CinematicSubtitle> subtitles_;
    std::vector<MissionObjective> objectives_;

    Entity arkantos_id_ = INVALID_ENTITY;
    Entity kraken_id_ = INVALID_ENTITY;
    Entity citadel_id_ = INVALID_ENTITY;
    std::vector<Entity> defender_ids_;
    std::vector<Entity> pirate_ids_;
    std::vector<Entity> reinforcement_ids_;

    // Reinforcement progression countdown (6 leagues -> 0 arrived)
    int army_leagues_away_ = 6;
    float reinforcement_timer_ = 0.0f;
    float reinforcement_interval_ = 14.0f; // advances one league every 14s in real-time
    bool reinforcements_spawned_ = false;

    // Trigger state flags
    bool pirate_wave_spawned_ = false;
    bool kraken_spawned_ = false;
    bool end_evaluated_ = false;

    // SCN file metadata
    std::string scenario_title_ = "Omens";
    bool authentic_scn_loaded_ = false;
};

} // namespace aom
