#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/renderer.h"
#include "assets/xmb_parser.h"
#include "assets/brg_model.h"
#include "data/game_data.h"
#include "data/unit_transform.h"
#include "data/proto_unit.h"
#include "ui/ui.h"
#include "ui/ui_layout.h"
#include "audio/audio.h"
#include "ai/ai_system.h"
#include "anim/animation_system.h"
#include "aom/campaign_select_system.h"
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
class PrtParticleSystem;

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
    MAIN_MENU,      ///< Menu pregame autentico (uipregamenew.xmb / The Titans)
    IN_GAME,
    OPTIONS,
    CREDITS,
    MULTIPLAYER_LAN,
    MULTIPLAYER_LOBBY,
    PAUSED,
    SHUTTING_DOWN,

    // ── Menus originais do Age of Mythology ──
    UI_AI_PROMPT,
    UI_ALTERNATES_DIALOG,
    UI_ANIMATION_DLG,
    UI_ARENA_SETUP,
    UI_ARMY_EDIT,
    UI_AUTOPATCHER_DIALOG,
    UI_CAMERA_EDITOR,
    UI_CAMPAIGN,
    UI_CAMPAIGN_EDITOR,
    UI_CAMPAIGN_NLOANDING,
    UI_CAMPAIGN_SETUP,
    UI_CHAT_DIALOG,
    UI_LIFF_DIALOG,
    UI_COMBAT_COMPARISON,
    UI_CUSTOMIZE_OPP,
    UI_DEFAULT_LOADING,
    UI_EDITOR_MENU,
    UI_EDITOR_PALETTES,
    UI_EDIT_UNIT_DLG,
    UI_EULA,
    UI_FAKE_TECHTREE,
    UI_FILE_MANAGER,
    UI_FRIENDS_DIALOGS,
    UI_GAME_LIST,
    UI_GAME_PALETTES,
    UI_GAME_SETTINGS_DIALOG,
    UI_GENERIC_ENTRY_DIALOG,
    UI_GOD_BUFFET_DIALOG,
    UI_GOD_PICKER,
    UI_HELP_DIALOG,
    UI_HOST_GAME_DIALOG,
    UI_HOTKEYS,
    UI_HOTKEY_SETUP,
    UI_IMAGE_DIALOG,
    UI_INGAME_MSG_1,
    UI_INGAME_MSG_2,
    UI_INGAME_MSG_3,
    UI_INGAME_MSG_DIALOG,
    UI_LAUNCH_SCREEN,
    UI_LAYOUT,
    UI_LOW_DETAIL_BACKGROUND,
    UI_MAIN,
    UI_MAP_PICKER,
    UI_MESSAGE_DIALOGS,
    UI_MOTD,
    UI_MP_CUSTOM,
    UI_MP_ESO,
    UI_MP_FRIENDS_LIST,
    UI_MP_HELP,
    UI_MP_PLANIP,
    UI_MP_LAUNCH_SCREEN,
    UI_MP_LOGIN,
    UI_MP_OTHER_SETTINGS,
    UI_MP_PASSPORT,
    UI_MP_PASSPORT_TAKEN,
    UI_MP_SETUP_FINAL,
    UI_MP_VOTE_DIALOG,
    UI_NEW_DIALOGS,
    UI_OBJECTIVES,
    UI_OBJECTIVES_DIALOG,
    UI_OPTIONS_MENU,
    UI_OTHER_SETTINGS,
    UI_PLAYER_LOGIN,
    UI_PLAYER_STATUS_DIALOG,
    UI_PLAYER_STATUS_DIALOG_2,
    UI_POST_GAME_SCREEN,
    UI_PREGAME,
    UI_PREGAME_MP_TEST,
    UI_PREGAME_MSG_1,
    UI_PREGAME_MSG_2,
    UI_PREGAME_MSG_DIALOG,
    UI_PREGAME_NEW,
    UI_PREGAME_STRESS,
    UI_PROFILE_SELECT,
    UI_RECEIVE,
    UI_RECEIVE_CONNECT,
    UI_RM_GENERATION,
    UI_ROOM_DIALOGS,
    UI_SAMPLE,
    UI_SCENARIO_SET_DLG,
    UI_SESSION_CONNECT,
    UI_SINGLE_PLAYER_SETUP,
    UI_SOUND_DLG,
    UI_SOUND_EDIT_DLG,
    UI_SOUND_SET_DLG,
    UI_SPRM_SETUP,
    UI_STATS_UPLOAD,
    UI_STATUS_MSG,
    UI_STRATEGIC_MAP,
    UI_TECHTREE,
    UI_TRIBUTE_DLG,
    UI_TRIGGER_EDITOR,
    UI_TRIG_GROUP_EDITOR,
    UI_WATER_DIALOG,
    UI_XS_DEBUGGER,
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

struct ModelEntry {
    MeshHandle mesh = INVALID_MESH;
    TextureHandle texture = INVALID_TEXTURE;
    BrgModel* model = nullptr;
    const ProtoUnitData* unit_stats = nullptr;  ///< Estatísticas do proto.xmb (se houver match por nome)
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

    /// Inicia uma campanha a partir de um cenário específico (dados do .cpn/.cpx).
    /// Usado pelo menu dinâmico de campanha (Fase 1).
    void start_campaign_mission(const CampaignScenario* scenario);

    /// Inicia uma missão genérica (sem script específico) usando apenas o
    /// terrain do .scn e entidades básicas. Fallback para cenários sem
    /// CampaignMissionManager dedicado.
    void start_generic_mission(const std::string& scenario_filename, bool menu_background_mode = false);

    /// Carrega (de forma limitada) os clips de animacao BRG de `models.bar`.
    ///
    /// O archive contem milhares de `.brg`; extrair e interpretar todos levava
    /// mais de 5 segundos na thread da UI e provocava ANR ao iniciar uma
    /// missao. O limite mantem o custo por partida na casa das dezenas de
    /// milissegundos, e o resultado e reaproveitado nas partidas seguintes.
    bool load_animation_clips(size_t max_clips = 8);

    /// Carrega (de forma limitada) as animacoes esqueletais .grn de `anim.bar`.
    bool load_grn_clips(size_t max_clips = 8);

    /// Escolhe um clip ja carregado e resolve a textura correspondente.
    bool select_animation_clip(const aom::anim::BrgAnimationClip*& clip,
                               TextureHandle& texture) const;
    void create_campaign_menu();
    /// Abre a tela de selecao de cenario da campanha (lista de missoes).
    void create_campaign_selection_menu();
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

    // ── Layouts de UI do data.bar (Passo 4) ──────────────────────────────────

    /// Carrega um layout XMB do data.bar (ex: "uitechtree.xmb", "uitributedlg.xmb").
    /// O layout fica disponível para ser exibido via show_ui_layout().
    bool load_ui_layout(const std::string& xmb_name);

    /// Limpa a UI atual e aplica um layout previamente carregado.
    /// Retorna o número de elementos criados.
    size_t show_ui_layout(const std::string& layout_name);

    /// Exibe a árvore de tecnologias (uitechtree.xmb).
    bool show_techtree();

    /// Exibe a tela de tributo (uitributedlg.xmb).
    bool show_tribute();

    /// Exibe as configurações de som (uisoundsetdlg.xmb).
    bool show_sound_settings();

    /// Retorna o gerenciador de layouts para inspeção.
    [[nodiscard]] UILayoutManager& ui_layout_manager() { return ui_layout_manager_; }

    /// Transforma uma unidade existente em outro tipo (ex: "Wall Long" -> "Gate"),
    /// usando as regras do unittransform.xmb. Delegado ao GameplayWorld.
    [[nodiscard]] Entity transform_unit(Entity from_entity, const std::string& target_name);

    /// Retorna as transformacoes disponiveis para uma entidade (do unittransform.xmb).
    [[nodiscard]] std::vector<UnitTransform> available_transforms(Entity entity) const;
    [[nodiscard]] MeshHandle model_mesh() const { return model_mesh_; }
    [[nodiscard]] TextureHandle model_texture() const { return model_texture_; }

    /// Recupera um modelo BRG carregado no cache por nome (ex: "hoplite.brg").
    /// Retorna nullptr se o modelo ainda não foi carregado.
    [[nodiscard]] ModelEntry* get_model_entry(const std::string& name);
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

    void create_main_menu();          ///< uipregamenew — menu pregame The Titans
    void populate_menu_background_scene();  ///< Popula cena 3D do MAIN_MENU com elementos do The Titans

    void create_ai_prompt_menu();
    void create_alternates_dialog_menu();
    void create_animation_dlg_menu();
    void create_arena_setup_menu();
    void create_army_edit_menu();
    void create_autopatcher_dialog_menu();
    void create_camera_editor_menu();
    void create_campaign_editor_menu();
    void create_campaign_nloanding_menu();
    void create_campaign_setup_menu();
    void create_chat_dialog_menu();
    void create_liff_dialog_menu();
    void create_combat_comparison_menu();
    void create_customize_opp_menu();
    void create_default_loading_menu();
    void create_editor_menu();
    void create_editor_palettes_menu();
    void create_edit_unit_dlg_menu();
    void create_eula_menu();
    void create_fake_techtree_menu();
    void create_file_manager_menu();
    void create_friends_dialogs_menu();
    void create_game_list_menu();
    void create_game_palettes_menu();
    void create_game_settings_dialog_menu();
    void create_generic_entry_dialog_menu();
    void create_god_buffet_dialog_menu();
    void create_god_picker_menu();
    void create_help_dialog_menu();
    void create_host_game_dialog_menu();
    void create_hotkeys_menu();
    void create_hotkey_setup_menu();
    void create_image_dialog_menu();
    void create_ingame_msg_1_menu();
    void create_ingame_msg_2_menu();
    void create_ingame_msg_3_menu();
    void create_ingame_msg_dialog_menu();
    void create_launch_screen_menu();
    void create_layout_menu();

    void create_skybox_mesh();
    void add_skybox_quad(std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                         const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                         const glm::vec3& p3, const glm::vec3& normal,
                         const glm::vec2& t0, const glm::vec2& t1,
                         const glm::vec2& t2, const glm::vec2& t3);
    void create_low_detail_background_menu();
    void create_main_alt_menu();
    void create_map_picker_menu();
    void create_message_dialogs_menu();
    void create_motd_menu();
    void create_mp_custom_menu();
    void create_mp_eso_menu();
    void create_mp_friends_list_menu();
    void create_mp_help_menu();
    void create_mp_planip_menu();
    void create_mp_launch_screen_menu();
    void create_mp_login_menu();
    void create_mp_other_settings_menu();
    void create_mp_passport_menu();
    void create_mp_passport_taken_menu();
    void create_mp_setup_final_menu();
    void create_mp_vote_dialog_menu();
    void create_new_dialogs_menu();
    void create_objectives_menu();
    void create_objectives_dialog_menu();
    void create_options_menu();
    void create_others_settings_menu();
    void create_player_login_menu();
    void create_player_status_dialog_menu();
    void create_player_status_dialog_2_menu();
    void create_post_game_screen_menu();
    void create_pregame_menu();
    void create_pregame_mp_test_menu();
    void create_pregame_msg_1_menu();
    void create_pregame_msg_2_menu();
    void create_pregame_msg_dialog_menu();
    void create_pregame_new_menu();
    void create_pregame_stress_menu();
    void create_profile_select_menu();
    void create_receive_menu();
    void create_receive_connect_menu();
    void create_rm_generation_menu();
    void create_room_dialogs_menu();
    void create_sample_menu();
    void create_scenario_set_dlg_menu();
    void create_session_connect_menu();
    void create_single_player_setup_menu();
    void create_sound_dlg_menu();
    void create_sound_edit_dlg_menu();
    void create_sound_set_dlg_menu();
    void create_sprm_setup_menu();
    void create_stats_upload_menu();
    void create_status_msg_menu();
    void create_strategic_map_menu();
    void create_techtree_menu();
    void create_tribute_dlg_menu();
    void create_trigger_editor_menu();
    void create_trig_group_editor_menu();
    void create_water_dialog_menu();
    void create_xs_debugger_menu();

    void create_single_player_menu();
    void create_random_map_menu();
    void create_load_game_menu();
    void create_multiplayer_menu();
    void create_multiplayer_lan_menu();
    void create_multiplayer_lobby_menu();
    void create_cinematics_menu();
    void create_history_menu();
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
    bool generate_ui_from_xmb();
    void create_credits_menu();
    void create_in_game_hud();
    void update_in_game_hud(float dt);
    void clear_ui();

    void load_options();
    void save_options();
    std::string save_game_path() const;

    // --- Saved Game Browser ---
    void open_saved_game_browser();
    void load_selected_save();
    void delete_selected_save();
    void refresh_saved_game_list();
    std::vector<std::string> list_saved_games() const;
    void open_save_game_menu();
    void save_game_slot(const std::string& slot_name);

    EngineState state_ = EngineState::UNINITIALIZED;
    int selected_save_index_ = -1;
    EngineConfig config_;
    EngineCallbacks callbacks_;

    void* asset_manager_ = nullptr;
    void* native_activity_ = nullptr;
    bool archives_loaded_ = false;
    bool resources_loaded_ = false;   ///< Recursos de jogo (modelos, partículas, etc.) já carregados — evita recarregar a cada volta ao MAIN_MENU

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
    std::unique_ptr<CampaignSelectSystem> campaign_select_system_;
    std::unique_ptr<CampaignMissionManager> mission_mgr_;
    GodPowerSystem god_powers_;

    std::unique_ptr<IRenderer> renderer_;
    std::unique_ptr<UIManager> ui_;
    std::unique_ptr<AudioEngine> audio_;

    /// Gerenciador de layouts de UI do data.bar (XMB: uitechtree.xml, uitributedlg.xml, etc.)
    UILayoutManager ui_layout_manager_;

    std::unordered_map<std::string, aom::TextureHandle> texture_cache_;
    /// Dimensões (largura x altura) de cada textura carregada (para 9-slice)
    std::unordered_map<std::string, glm::ivec2> texture_dimensions_;
    // Índice tolerante de nomes de textura (chave = só [a-z0-9] do nome-base).
    // Resolve "ui\\ui pregame button" <-> "ui_pregame_button.ddt" e variações de pasta/caixa.
    std::unordered_map<std::string, std::string> texture_key_index_;
    bool texture_key_index_built_ = false;
    std::unordered_set<std::string> texture_miss_logged_;
    void build_texture_key_index();
    void log_bar_names_containing(const std::string& needle, size_t max_lines = 40) const;
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

    /// —— Skybox completo (P3.2) ——
    TextureHandle skybox_faces_[6] = {INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE,
                                      INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE};
    TextureHandle skybox_cubemap_ = INVALID_TEXTURE;  // GL_TEXTURE_CUBE_MAP
    MeshHandle skybox_mesh_ = INVALID_MESH;
    bool load_skybox_faces(const std::string& cub_name);

    /// Define a cor da água com base em um corpo d'água do waterbodies.xmb
    void set_water_color_from_body(const std::string& body_name);

    /// Carrega texturas de beira (bottom/bank/outer_bank) de um corpo d'água
    /// do waterbodies.xmb a partir do textures.bar
    void load_water_body_textures(const std::string& body_name);

    /// Spawna clusters de objetos (rochas, árvores) na beira de um corpo d'água,
    /// usando as regras de objectplacement do waterbodies.xmb.
    void spawn_water_body_clusters(const std::string& body_name, const glm::vec3& center);

    MeshHandle terrain_mesh_ = INVALID_MESH;
    void generate_skydome();
    void generate_terrain_from_heightmap(const std::vector<float>& heightmap, int width, int height, float scale);
    bool load_scenario_heightmap(const std::string& scenario_path);
    TextureHandle terrain_texture_ = INVALID_TEXTURE;
    TextureHandle terrain_blend_tex_ = INVALID_TEXTURE;
    TextureHandle terrain_bank_tex_ = INVALID_TEXTURE;      ///< Textura da beira interna (waterbodies.xmb bank)
    TextureHandle terrain_outer_bank_tex_ = INVALID_TEXTURE;///< Textura da beira externa (waterbodies.xmb outer_bank)
    TextureHandle water_normal_tex_ = INVALID_TEXTURE;
    TextureHandle water_tex_ = INVALID_TEXTURE;         ///< Textura de água (water_*.ddt do textures.bar)
    glm::vec4 water_plane_{0.0f, 1.0f, 0.0f, 0.4f}; ///< Plane equation Ax+By+Cz+D = 0, y = -0.4f
    float terrain_height_ = -0.4f;                      ///< Altura do terreno sob a água (para depth/foam)
    glm::vec3 water_color_{0, 50, 170};             ///< Cor da água do waterbodies.xmb (RGB 0-255)
    int heightmap_width_ = 0;
    int heightmap_height_ = 0;
    float heightmap_scale_ = 1.0f;
    std::vector<float> heightmap_data_; ///< Elevation data loaded from .scn height-map
    std::unique_ptr<GameplayWorld> gameplay_world_;
    std::unique_ptr<ai::AiSystem> ai_system_;
    std::unique_ptr<aom::anim::AnimationSystem> anim_system_;
    std::vector<std::string> anim_clip_names_; ///< Nomes dos .brg já interpretados
    bool anim_clips_ready_ = false;            ///< Clips já carregados (reaproveitados)
    std::unordered_map<std::string, std::unique_ptr<aom::anim::GrnAnimation>> grn_clips_; ///< Nomes dos .grn já interpretados

    /// Retorna um clip .grn previamente carregado por nome (ou nullptr).
    [[nodiscard]] const aom::anim::GrnAnimation* find_grn_clip(const std::string& name) const {
        auto it = grn_clips_.find(name);
        return (it != grn_clips_.end()) ? it->second.get() : nullptr;
    }

    std::unique_ptr<aom::video::VideoPlayer> video_player_;
    std::unique_ptr<aom::video::MediaDecoder> media_decoder_;

    // Cinematic state
    TextureHandle cinematic_frame_tex_ = INVALID_TEXTURE; ///< GPU texture updated each video frame
    bool cinematic_skip_requested_ = false;
    bool cinematic_new_frame_      = false; ///< flag: novo frame disponível para upload
    uint32_t cinematic_tex_w_ = 0;          ///< dimensões atuais da textura do frame
    uint32_t cinematic_tex_h_ = 0;

    // Menu background video (menu background video playing behind the menu)
    std::unique_ptr<aom::video::MediaDecoder> menu_bg_;
    TextureHandle menu_bg_tex_ = INVALID_TEXTURE;
    uint32_t menu_bg_w_ = 0;
    uint32_t menu_bg_h_ = 0;
    bool menu_bg_restart_ = false;

    void start_menu_background(); ///< Abre e inicia a decodificação do vídeo de fundo do menu

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

    /// —— P3.5: Sistema de partículas baseado em .prt ——
    std::unique_ptr<aom::PrtParticleSystem> prt_particles_;
    TextureHandle prt_particle_tex_ = INVALID_TEXTURE;
    bool load_prt_particles(const std::string& prt_name);

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
    std::unordered_map<std::string, ModelEntry> model_cache_;
    std::unordered_map<std::string, std::unique_ptr<BrgModel>> model_cache_models_;

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

    // God Picker state (after campaign)
    std::string god_picker_civilization_ = "Grego";
    std::string god_picker_minor_god_ = "Zeus";
    bool god_picker_selection_made_ = false;

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
