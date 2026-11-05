#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <utility>

#include "renderer/renderer.h"
#include "assets/xmb_parser.h"
#include "assets/brg_model.h"
#include "data/game_data.h"
#include "ui/ui.h"
#include "audio/audio.h"
#include "ai/ai_system.h"
#include "anim/animation_system.h"
#include "video/video_player.h"
#include "video/media_decoder.h"
#include "campaign/campaign.h"
#include "campaign/campaign_mission.h"
#include "game/save_load.h"
#include "game/victory.h"
#include "game/god_powers.h"
#include "assets/cube_map.h"

namespace aom {

class BarArchive;

struct EngineConfig {
    std::string data_path;
    int window_width = 1920;
    int window_height = 1080;
    bool use_vulkan = false;
    bool enable_audio = true;
    bool enable_touch_ui = true;
    float render_scale = 1.0f;
    int max_fps = 60;
    std::string language = "portuguese";
};

class GameplayWorld;
enum class ResourceType;

enum class EngineState {
    UNINITIALIZED,
    CINEMATIC,      ///< Playing opening cinematic (introcg.bik / introcg.mp4)
    SPLASH,
    LOADING,
    MAIN_MENU,
    IN_GAME,
    OPTIONS,
    CREDITS,
    MULTIPLAYER_LAN,
    MULTIPLAYER_LOBBY,
    PAUSED,
    SHUTTING_DOWN,
};

enum class PlacementMode {
    NONE,
    HOUSE,
    BARRACKS,
    TOWER
};

struct ActionBtn {
    glm::vec4 rect; // x, y, width, height
    std::string label;
    std::string cost;
    std::function<void()> action;
};

struct EngineCallbacks {
    std::function<void()> on_initialized;
    std::function<void(float progress, const std::string& stage)> on_loading_progress;
    std::function<void()> on_loading_complete;
    std::function<void(const std::string& error)> on_error;
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize(const EngineConfig& config, void* native_window);
    void shutdown();
    void update(float delta_time);
    void render();
    void resize(int width, int height);

    void set_asset_manager(void* asset_mgr) { asset_manager_ = asset_mgr; }
    void set_native_activity(void* activity) { native_activity_ = activity; }

    void handle_touch(const TouchEvent& touch);
    void adjust_camera_zoom(float delta);

    void set_state(EngineState new_state);

    void next_texture();
    void prev_texture();
    void next_model();
    void prev_model();

    void start_game();
    void start_campaign_mission(size_t scenario_idx = 1);

    /// Carrega (de forma limitada) os clips de animacao BRG de `models.bar`.
    ///
    /// O archive contem milhares de `.brg`; extrair e interpretar todos levava
    /// mais de 5 segundos na thread da UI e provocava ANR ao iniciar uma
    /// missao. O limite mantem o custo por partida na casa das dezenas de
    /// milissegundos, e o resultado e reaproveitado nas partidas seguintes.
    bool load_animation_clips(size_t max_clips = 8);

    /// Escolhe um clip ja carregado e resolve a textura correspondente.
    bool select_animation_clip(const aom::anim::BrgAnimationClip*& clip,
                               TextureHandle& texture) const;
    void create_campaign_menu();
    void save_game(const std::string& path = "");
    void load_game(const std::string& path = "");

    /// Reconstroi as entidades (unidades e edificios) gravadas no save.
    void restore_saved_entities(const SaveGameData& data);
    void open_options();
    void open_credits();
    void open_multiplayer();
    void return_to_main_menu();
    void quit_app();

    bool recreate_renderer(void* native_window);
    void release_renderer();

    [[nodiscard]] EngineState state() const { return state_; }
    void set_callbacks(const EngineCallbacks& callbacks) { callbacks_ = callbacks; }

    /// Abre, no Android, o seletor de pasta (SAF) para importar os dados do jogo.
    bool request_game_data_import();

    [[nodiscard]] IRenderer* renderer() { return renderer_.get(); }
    [[nodiscard]] UIManager* ui() { return ui_.get(); }
    [[nodiscard]] AudioEngine* audio() { return audio_.get(); }
    [[nodiscard]] BarArchive* data_archive() { return data_bar_.get(); }
    [[nodiscard]] BarArchive* texture_archive() { return textures_bar_.get(); }
    [[nodiscard]] BarArchive* model_archive() { return models_bar_.get(); }
    [[nodiscard]] BarArchive* scenario_archive() { return scenarios_bar_.get(); }
    [[nodiscard]] BarArchive* sound_archive() { return sound_bar_.get(); }
    [[nodiscard]] BarArchive* animation_archive() { return anim_bar_.get(); }
    [[nodiscard]] BarArchive* godpower_archive() { return godpowers_bar_.get(); }
    [[nodiscard]] CampaignManager* campaign_manager() { return campaign_mgr_.get(); }
    /// Campanha da expansão The Titans ("The New Atlantis"), se disponível.
    [[nodiscard]] CampaignManager* expansion_campaign_manager() { return campaign_mgr_xpack_.get(); }
    /// Registro de unidades, deuses e tecnologias (proto.xmb + protox.xmb).
    [[nodiscard]] GameDataManager& game_data() { return game_data_; }
    [[nodiscard]] GodPowerSystem& god_powers() { return god_powers_; }
    [[nodiscard]] CampaignMissionManager* mission_manager() { return mission_mgr_.get(); }
    [[nodiscard]] MeshHandle model_mesh() const { return model_mesh_; }
    [[nodiscard]] TextureHandle model_texture() const { return model_texture_; }
    [[nodiscard]] const XmbElement* game_config() const { return game_config_root_.get(); }
    [[nodiscard]] bool archives_loaded() const { return archives_loaded_; }

    /// Read asset binary content from APK assets or fallback filesystem
    std::vector<uint8_t> read_asset(const std::string& name);

    // 3D Interactive Particle System
    void spawn_particle(const glm::vec3& pos, const glm::vec3& vel, float life, uint32_t color, float size = 0.15f);
    void spawn_construction_sparks(const glm::vec3& pos, int count = 8);
    void spawn_combat_hit(const glm::vec3& pos, int count = 10);
    void spawn_harvest_chips(ResourceType type, const glm::vec3& pos, int count = 6);
    void spawn_death_smoke(const glm::vec3& pos, int count = 12);
    void update_particles(float dt);
    void render_particles();
    size_t particle_count() const { return particles_.size(); }

    struct Particle {
        glm::vec3 pos{0.0f};
        glm::vec3 vel{0.0f};
        float life = 0.0f;
        float max_life = 1.0f;
        uint32_t color = 0xFFFFD000;
        float size = 0.15f;
    };
    const std::vector<Particle>& particles() const { return particles_; }

private:
    bool init_renderer(const EngineConfig& config, void* native_window);
    bool load_core_ui_textures();
    bool load_archives(const std::string& data_path);
    bool load_textures();
    bool load_models();
    TextureHandle load_ddt_texture(const std::string& material_name);
    bool load_game_config();
    bool load_lighting(const std::string& path);
    bool load_strings(const std::string& path);
    std::string tr(const std::string& key) const;

    void create_main_menu();
    void create_single_player_menu();
    void create_random_map_menu();
    void create_load_game_menu();
    void create_multiplayer_menu();
    void create_multiplayer_lan_menu();
    void create_multiplayer_lobby_menu();
    void create_cinematics_menu();
    void create_history_menu();
    void create_editor_menu();
    void create_menu_screen(
        const std::string& title,
        const std::string& subtitle,
        const std::vector<std::pair<std::string, std::function<void()>>>& actions,
        const std::function<void()>& back_action);
    void replay_opening_cinematics();
    void play_titans_cinematics();
    void play_outtakes_cinematic();
    void play_credits_cinematic();
    void play_titans_credits_cinematic();
    void play_named_cinematic(const char* method_name);
    void create_options_menu();
    bool generate_ui_from_xmb();
    void create_credits_menu();
    void create_in_game_hud();
    void update_in_game_hud(float dt);
    void clear_ui();

    void load_options();
    void save_options();
    std::string save_game_path() const;

    EngineState state_ = EngineState::UNINITIALIZED;
    EngineConfig config_;
    EngineCallbacks callbacks_;

    void* asset_manager_ = nullptr;
    void* native_activity_ = nullptr;
    bool archives_loaded_ = false;

    // Archives base do jogo; cada um pode ter a expansão The Titans
    // montada por cima (data2.bar, textures2.bar, models2.bar, ...).
    std::unique_ptr<BarArchive> data_bar_;
    std::unique_ptr<BarArchive> textures_bar_;
    std::unique_ptr<BarArchive> models_bar_;
    std::unique_ptr<BarArchive> scenarios_bar_;
    std::unique_ptr<BarArchive> sound_bar_;
    std::unique_ptr<BarArchive> anim_bar_;
    std::unique_ptr<BarArchive> godpowers_bar_;
    std::unique_ptr<CampaignManager> campaign_mgr_;
    std::unique_ptr<CampaignManager> campaign_mgr_xpack_;
    std::unique_ptr<CampaignMissionManager> mission_mgr_;
    GodPowerSystem god_powers_;

    std::unique_ptr<IRenderer> renderer_;
    std::unique_ptr<UIManager> ui_;
    std::unique_ptr<AudioEngine> audio_;

    std::unordered_map<std::string, aom::TextureHandle> texture_cache_;
    std::unordered_map<std::string, std::string> strings_;

    // Core Authentic AoM UI Textures
    TextureHandle splash_tex_ = INVALID_TEXTURE;
    TextureHandle loading_tex_ = INVALID_TEXTURE;
    TextureHandle main_menu_tex_ = INVALID_TEXTURE;
    TextureHandle button_main_tex_ = INVALID_TEXTURE;
    TextureHandle button_clicked_tex_ = INVALID_TEXTURE;
    TextureHandle minimap_tex_ = INVALID_TEXTURE;
    glm::vec4 minimap_rect_{0.0f}; // x, y, width, height

    MeshHandle model_mesh_ = INVALID_MESH;
    TextureHandle model_texture_ = INVALID_TEXTURE;
    MeshHandle hoplite_mesh_ = INVALID_MESH;
    TextureHandle hoplite_texture_ = INVALID_TEXTURE;

    MeshHandle skydome_mesh_ = INVALID_MESH;
    TextureHandle skydome_texture_ = INVALID_TEXTURE;
    CubeMapDef current_cubemap_;
    bool load_skybox_cube(const std::string& cub_name);
    MeshHandle terrain_mesh_ = INVALID_MESH;
    void generate_skydome();
    void generate_terrain_from_heightmap(const std::vector<float>& heightmap, int width, int height, float scale);
    bool load_scenario_heightmap(const std::string& scenario_path);
    TextureHandle terrain_texture_ = INVALID_TEXTURE;
    TextureHandle terrain_blend_tex_ = INVALID_TEXTURE;
    TextureHandle water_normal_tex_ = INVALID_TEXTURE;
    glm::vec4 water_plane_{0.0f, 1.0f, 0.0f, 0.4f}; ///< Plane equation Ax+By+Cz+D = 0, y = -0.4f
    int heightmap_width_ = 0;
    int heightmap_height_ = 0;
    float heightmap_scale_ = 1.0f;
    std::vector<float> heightmap_data_; ///< Elevation data loaded from .scn height-map
    std::unique_ptr<GameplayWorld> gameplay_world_;
    std::unique_ptr<ai::AiSystem> ai_system_;
    std::unique_ptr<aom::anim::AnimationSystem> anim_system_;
    std::vector<std::string> anim_clip_names_; ///< Nomes dos .brg já interpretados
    bool anim_clips_ready_ = false;            ///< Clips já carregados (reaproveitados)
    std::unique_ptr<aom::video::VideoPlayer> video_player_;
    std::unique_ptr<aom::video::MediaDecoder> media_decoder_;

    // Cinematic state
    TextureHandle cinematic_frame_tex_ = INVALID_TEXTURE; ///< GPU texture updated each video frame
    bool cinematic_skip_requested_ = false;
    bool cinematic_new_frame_      = false; ///< flag: novo frame disponível para upload
    uint32_t cinematic_tex_w_ = 0;          ///< dimensões atuais da textura do frame
    uint32_t cinematic_tex_h_ = 0;

    // Playlist de abertura fiel ao original (playlist.aom: AOM LOGO → introcg)
    std::vector<std::string> cinematic_queue_;
    size_t cinematic_queue_idx_ = 0;
    bool try_start_cinematic_queue();

    int last_ui_width_  = -1; ///< última resolução para a qual a UI foi criada
    int last_ui_height_ = -1;
    void generate_terrain();

    std::vector<std::string> available_textures_;
    std::vector<std::string> available_models_;
    size_t current_texture_idx_ = 0;
    size_t current_model_idx_ = 0;

    std::vector<Particle> particles_;
    Camera camera_;
    DirectionalLight dir_light_;

    // Camera orbit controls
    bool dragging_ = false;
    glm::vec2 last_touch_ = {0.0f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.2f;
    float radius_ = 12.0f;

    float accumulated_time_ = 0.0f;
    float state_timer_ = 0.0f;
    float loading_progress_ = 0.0f;
    uint64_t frame_count_ = 0;
    float current_fps_ = 60.0f;
    UILabel* fps_label_ = nullptr;

    std::unique_ptr<XmbElement> game_config_root_;

    // Game data loaded from proto.xmb / majorgods.xmb / techtree.xmb
    GameDataManager game_data_;

    // Cache de modelos BRG carregados
    struct ModelEntry {
        MeshHandle mesh = INVALID_MESH;
        TextureHandle texture = INVALID_TEXTURE;
        BrgModel* model = nullptr;
    };
    std::unordered_map<std::string, ModelEntry> model_cache_;

    // Options state
    float master_volume_ = 0.8f;
    float music_volume_ = 0.7f;
    float sfx_volume_ = 0.9f;
    bool music_enabled_ = true;
    bool sfx_enabled_ = true;
    bool show_fps_ = true;
    float graphics_quality_ = 1.0f;

    // RTS Construction & Training Placement
    PlacementMode placement_mode_ = PlacementMode::NONE;
    glm::vec3 placement_world_pos_{0.0f};
    bool placement_valid_ = false;
    std::vector<ActionBtn> action_buttons_;

    // Fog of War & Drag-Box Multi-Selection
    TextureHandle fog_tex_ = INVALID_TEXTURE;
    bool box_selecting_ = false;
    glm::vec2 box_start_{0.0f};
    glm::vec2 box_current_{0.0f};
    bool box_select_mode_ = false;

    // Multiplayer LAN / Direct IP settings
    std::string mp_player_name_ = "Jogador Android";
    std::string mp_session_name_ = "AoM Sala LAN";
    std::string mp_target_ip_ = "192.168.1.100";
    int mp_ip_last_octet_ = 100;
    int mp_ip_subnet_ = 1;

    // In-game HUD references
    UILabel* hud_food_label_ = nullptr;
    UILabel* hud_wood_label_ = nullptr;
    UILabel* hud_gold_label_ = nullptr;
    UILabel* hud_favor_label_ = nullptr;
    UILabel* hud_pop_label_ = nullptr;
    UILabel* hud_sel_title_ = nullptr;
    UILabel* hud_sel_hp_ = nullptr;
    UILabel* hud_sel_info_ = nullptr;
    UIButton* hud_action_btn1_ = nullptr;
    UIButton* hud_action_btn2_ = nullptr;
    UIButton* hud_action_btn3_ = nullptr;
    UIButton* hud_action_btn4_ = nullptr;
    glm::vec2 touch_down_pos_{0.0f, 0.0f};
    Entity last_selected_entity_ = 0;

    // Campaign & Mission HUD references
    UILabel* hud_obj_title_ = nullptr;
    UILabel* hud_subtitle_speaker_ = nullptr;
    UILabel* hud_subtitle_text_ = nullptr;
    UILabel* hud_reinforce_label_ = nullptr;
    UIButton* hud_skip_btn_ = nullptr;

    // Condicoes de vitoria em Skirmish / Mapa Aleatorio
    VictoryCondition skirmish_victory_condition_ = VictoryCondition::CONQUEST;
    VictoryResult    skirmish_victory_result_    = VictoryResult::NONE;
    float            victory_display_timer_      = 0.0f;
};

} // namespace aom
