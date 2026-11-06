#include "engine/engine.h"
#include "assets/brg_model.h"
#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"
#include "assets/xmb_parser.h"
#include "ui/ui.h"
#include "audio/audio.h"
#include "ecs/gameplay.h"
#include "ai/ai_system.h"
#include "anim/animation_system.h"
#include "video/video_player.h"
#include "video/media_decoder.h"
#include "renderer/camera_track.h"
#include "network/network_manager.h"
#include "game/save_load.h"
#include "game/victory.h"
#include "assets/cube_map.h"

extern "C" {
#include "stb_image.h"
}

#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>

#define LOG_TAG "AoM-Engine"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

[[maybe_unused]] static std::vector<uint8_t> read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    file.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(sz);
    file.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

} // anonymous namespace

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

bool Engine::initialize(const EngineConfig& config, void* native_window) {
    config_ = config;

    LOGI("=== Initializing AoM Native Engine ===");
    LOGI("Resolution: %dx%d, Data Path: %s", config.window_width, config.window_height, config.data_path.c_str());

    // 1. Initialize Graphic Renderer FIRST (Zero-ANR Rule)
    if (!init_renderer(config, native_window)) {
        LOGE("Renderer initialization failed!");
        if (callbacks_.on_error) callbacks_.on_error("Renderer initialization failed");
        return false;
    }

    // 2. Initialize Audio Engine
    audio_ = create_audio_engine();
    if (audio_) {
        audio_->initialize(static_cast<AAssetManager*>(asset_manager_));
    }

    // 3. Initialize UI Subsystem
    ui_ = std::make_unique<aom::UIManager>();
    if (!ui_->initialize(renderer_.get(), config_.window_width, config_.window_height)) {
        LOGE("UI initialization failed - continuing without UI overlay");
    } else {
        if (audio_) ui_->set_audio_engine(audio_.get());
    }

    // 4. Load Core UI Textures (Splash, Loading, Stone Buttons)
    load_core_ui_textures();
    if (ui_ && button_main_tex_ != INVALID_TEXTURE) {
        ui_->set_default_button_textures(button_main_tex_, button_clicked_tex_);
    }

    // 5. Load Options
    load_options();

    // 6. Initialize VideoPlayer (fallback timer) + MediaDecoder (real frames)
    video_player_ = std::make_unique<aom::video::VideoPlayer>();
    video_player_->on_playlist_completed = [this]() {
        set_state(EngineState::MAIN_MENU);
    };

    media_decoder_ = std::make_unique<aom::video::MediaDecoder>();

    // on_frame: chamado pelo update() — faz upload do frame YUV→RGBA para GPU.
    // Recria a textura quando as dimensões do vídeo mudam (playlist com vídeos
    // de resolução diferente), evitando glTexSubImage2D inválido.
    media_decoder_->on_frame = [this](const aom::video::VideoFrame& frame) {
        if (!renderer_ || frame.width == 0 || frame.height == 0) return;
        if (cinematic_frame_tex_ != INVALID_TEXTURE &&
            (frame.width != cinematic_tex_w_ || frame.height != cinematic_tex_h_)) {
            renderer_->destroy_texture(cinematic_frame_tex_);
            cinematic_frame_tex_ = INVALID_TEXTURE;
        }
        if (cinematic_frame_tex_ == INVALID_TEXTURE) {
            cinematic_frame_tex_ = renderer_->create_texture(
                frame.width, frame.height, frame.rgba.data());
            cinematic_tex_w_ = frame.width;
            cinematic_tex_h_ = frame.height;
        } else {
            renderer_->update_texture(cinematic_frame_tex_,
                frame.width, frame.height, frame.rgba.data());
        }
        cinematic_new_frame_ = true;
    };

    media_decoder_->on_eos = [this]() {
        // Fim de um vídeo da playlist: avança para o próximo ou vai ao menu
        if (state_ != EngineState::CINEMATIC) return;
        ++cinematic_queue_idx_;
        if (try_start_cinematic_queue()) return;
        LOGI("Cinematic playlist finished → MAIN_MENU");
        set_state(EngineState::MAIN_MENU);
    };

    // 7. Load archives in background / step
    load_archives(config.data_path);
    load_textures();
    load_models();
    load_game_config();

    // 8. Playlist de abertura fiel ao original (playlist.aom: AOM LOGO → introcg).
    //    Os MP4s convertidos são empacotados no APK em assets/avi/.
    cinematic_queue_.clear();
    cinematic_queue_idx_ = 0;
    cinematic_queue_.push_back("assets/avi/aom-logo.mp4");
    cinematic_queue_.push_back("assets/avi/introcg.mp4");
    if (!try_start_cinematic_queue()) {
        LOGI("No opening cinematic available — going straight to MAIN_MENU");
        set_state(EngineState::MAIN_MENU);
    }

    if (callbacks_.on_initialized) callbacks_.on_initialized();
    return true;
}

// ── Playlist de abertura: abre o próximo vídeo da fila ──────────────────────
bool Engine::try_start_cinematic_queue() {
    if (!media_decoder_) return false;

    // Diretório gravável para fallback de cópia de assets comprimidos
    std::string cache_dir;
    if (native_activity_) {
        auto* act = static_cast<ANativeActivity*>(native_activity_);
        if (act && act->internalDataPath) cache_dir = act->internalDataPath;
    }
    if (cache_dir.empty() && !config_.data_path.empty()) {
        cache_dir = config_.data_path + "/cinematic";
    }
    if (cache_dir.empty()) cache_dir = "/data/local/tmp";

    namespace fs = std::filesystem;

    while (cinematic_queue_idx_ < cinematic_queue_.size()) {
        const std::string& item = cinematic_queue_[cinematic_queue_idx_];

        // 1) Asset do APK: aapt armazena MP4 sem compressão → fd direto
        if (asset_manager_ && media_decoder_->open_asset(asset_manager_, item, cache_dir)) {
            LOGI("Opening cinematic from APK asset: %s", item.c_str());
            set_state(EngineState::CINEMATIC);
            return true;
        }

        // 2) Caminhos do filesystem (vídeos instalados ao lado dos .bar)
        const std::string base_name = item.substr(item.find_last_of('/') + 1);
        const std::vector<std::string> fs_candidates = {
            (fs::path(config_.data_path) / base_name).string(),
            item,
        };
        for (const auto& p : fs_candidates) {
            std::error_code ec;
            if (!p.empty() && fs::exists(p, ec) && media_decoder_->open(p)) {
                LOGI("Opening cinematic from filesystem: %s", p.c_str());
                set_state(EngineState::CINEMATIC);
                return true;
            }
        }

        LOGI("Cinematic unavailable, skipping item: %s", item.c_str());
        ++cinematic_queue_idx_;
    }
    return false;
}

std::vector<uint8_t> Engine::read_asset(const std::string& name) {
#ifdef __ANDROID__
    if (asset_manager_) {
        auto* mgr = static_cast<AAssetManager*>(asset_manager_);

        std::string asset_name = name;
        if (asset_name.rfind("assets/", 0) == 0) {
            asset_name = asset_name.substr(7);
        }
        AAsset* asset = AAssetManager_open(mgr, asset_name.c_str(), AASSET_MODE_BUFFER);
        if (!asset && asset_name != name) {
            asset = AAssetManager_open(mgr, name.c_str(), AASSET_MODE_BUFFER);
        }
        if (asset) {
            off_t len = AAsset_getLength(asset);
            std::vector<uint8_t> buf(static_cast<size_t>(len));
            AAsset_read(asset, buf.data(), len);
            AAsset_close(asset);
            LOGI("read_asset loaded from APK: %s (%zu bytes)", asset_name.c_str(), buf.size());
            return buf;
        }
    }
#endif

    const std::string candidates[] = {
        name,
        "assets/" + name,
        config_.data_path + "/" + name,
        config_.data_path + "/assets/" + name,
        "/storage/emulated/0/Documents/AoM-Native-Port/assets/" + name,
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/" + name
    };
    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (file) {
            file.seekg(0, std::ios::end);
            size_t sz = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(sz);
            file.read(reinterpret_cast<char*>(buf.data()), sz);
            LOGI("read_asset loaded from FS: %s (%zu bytes)", path.c_str(), buf.size());
            return buf;
        }
    }
    LOGE("read_asset could not find: %s", name.c_str());
    return {};
}

bool Engine::load_core_ui_textures() {
    if (!renderer_) return false;

    // Helper: load PNG from asset, decode with stb_image
    auto load_png_asset = [&](const std::string& name) -> TextureHandle {
        auto bytes = read_asset(name);
        if (bytes.empty()) return INVALID_TEXTURE;
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load_from_memory(
            bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
        if (!data) return INVALID_TEXTURE;
        TextureHandle t = renderer_->create_texture(w, h, data);
        stbi_image_free(data);
        LOGI("Loaded PNG asset '%s' (%dx%d)", name.c_str(), w, h);
        return t;
    };

    // Helper: load DDT from textures.bar by name
    auto load_ddt_bar = [&](const std::string& name) -> TextureHandle {
        if (!textures_bar_ || textures_bar_->file_count() == 0) return INVALID_TEXTURE;
        const std::vector<uint8_t> raw = textures_bar_->extract(name);
        if (raw.empty()) return INVALID_TEXTURE;
        auto ddt = DdtLoader::load(raw);
        if (!ddt) return INVALID_TEXTURE;
        auto rgba = ddt->decode_rgba(0);
        if (rgba.empty()) return INVALID_TEXTURE;
        TextureHandle t = renderer_->create_texture(ddt->width, ddt->height, rgba.data());
        LOGI("Loaded DDT from bar '%s' (%dx%d)", name.c_str(), ddt->width, ddt->height);
        return t;
    };

    // --- 1. Splash / Intro background ---
    splash_tex_ = load_png_asset("splash.png");
    if (splash_tex_ == INVALID_TEXTURE)
        splash_tex_ = load_ddt_bar("loading screen.ddt");
    if (splash_tex_ == INVALID_TEXTURE)
        splash_tex_ = load_ddt_bar("loadingscreen.ddt");

    // --- 2. Loading screen background (1024x768 gods art) ---
    loading_tex_ = load_png_asset("loading.png");
    if (loading_tex_ == INVALID_TEXTURE)
        loading_tex_ = splash_tex_;

    // --- 3. Main Menu background (authentic stone frame + background) ---
    main_menu_tex_ = load_png_asset("main_menu.png");
    if (main_menu_tex_ == INVALID_TEXTURE)
        main_menu_tex_ = load_png_asset("mainmenu.png");
    if (main_menu_tex_ == INVALID_TEXTURE)
        main_menu_tex_ = loading_tex_;

    // --- 4. Authentic AoM Stone Buttons ---
    button_main_tex_ = load_png_asset("button_main.png");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("greek button background.ddt");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("ui\\button main.ddt");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("icons\\ui_generic_button.ddt");

    button_clicked_tex_ = load_png_asset("button_clicked.png");
    if (button_clicked_tex_ == INVALID_TEXTURE)
        button_clicked_tex_ = load_ddt_bar("ui\\button main_clicked.ddt");
    if (button_clicked_tex_ == INVALID_TEXTURE)
        button_clicked_tex_ = button_main_tex_;

    if (ui_ && button_main_tex_ != INVALID_TEXTURE) {
        ui_->set_default_button_textures(button_main_tex_, button_clicked_tex_);
    }

    // --- 5. Font ---
    auto font_bytes = read_asset("font.ttf");
    if (!font_bytes.empty()) {
        if (renderer_->set_font_data(font_bytes.data(), font_bytes.size())) {
            LOGI("Font loaded (%zu bytes)", font_bytes.size());
        } else {
            LOGE("Renderer rejected font data");
        }
    } else {
        LOGE("font.ttf not found — UI text will use built-in fallback");
    }

    LOGI("UI textures: splash=%u menu_bg=%u main_menu=%u btn=%u btn_pressed=%u",
         splash_tex_, loading_tex_, main_menu_tex_, button_main_tex_, button_clicked_tex_);
    return true;
}

void Engine::set_state(EngineState new_state) {
    state_ = new_state;
    state_timer_ = 0.0f;
    cinematic_skip_requested_ = false;

    switch (new_state) {
        case EngineState::CINEMATIC:
            LOGI("Entered state: CINEMATIC");
            clear_ui();
            // Allocate a placeholder texture that will be updated each frame.
            // 640x480 RGBA matches the BIK native resolution.
            if (cinematic_frame_tex_ == INVALID_TEXTURE) {
                std::vector<uint8_t> black(640 * 480 * 4, 0);
                cinematic_frame_tex_ = renderer_->create_texture(640, 480, black.data());
            }
            break;
        case EngineState::SPLASH:
            LOGI("Entered state: SPLASH");
            clear_ui();
            break;
        case EngineState::LOADING:
            LOGI("Entered state: LOADING");
            clear_ui();
            loading_progress_ = 0.0f;
            break;
        case EngineState::MAIN_MENU:
            LOGI("Entered state: MAIN_MENU (uipregamenew)");
            if (audio_) {
                audio_->play_music("menu_music.mp3", true);
            }
            create_main_menu();
            break;
        case EngineState::GAME_LIST:
            LOGI("Entered state: GAME_LIST (uigamelist)");
            create_game_list_menu();
            break;
        case EngineState::GOD_PICKER:
            LOGI("Entered state: GOD_PICKER (uigodpicker)");
            create_god_picker_menu();
            break;

        // ── Novos menus (90+) ──
        case EngineState::UI_AI_PROMPT:
            LOGI("Entered state: UI_AI_PROMPT");
            create_ai_prompt_menu();
            break;
        case EngineState::UI_ALTERNATES_DIALOG:
            LOGI("Entered state: UI_ALTERNATES_DIALOG");
            create_alternates_dialog_menu();
            break;
        case EngineState::UI_ANIMATION_DLG:
            LOGI("Entered state: UI_ANIMATION_DLG");
            create_animation_dlg_menu();
            break;
        case EngineState::UI_ARENA_SETUP:
            LOGI("Entered state: UI_ARENA_SETUP");
            create_arena_setup_menu();
            break;
        case EngineState::UI_ARMY_EDIT:
            LOGI("Entered state: UI_ARMY_EDIT");
            create_army_edit_menu();
            break;
        case EngineState::UI_AUTOPATCHER_DIALOG:
            LOGI("Entered state: UI_AUTOPATCHER_DIALOG");
            create_autopatcher_dialog_menu();
            break;
        case EngineState::UI_CAMERA_EDITOR:
            LOGI("Entered state: UI_CAMERA_EDITOR");
            create_camera_editor_menu();
            break;
        case EngineState::UI_CAMPAIGN:
            LOGI("Entered state: UI_CAMPAIGN");
            create_campaign_menu();
            break;
        case EngineState::UI_CAMPAIGN_EDITOR:
            LOGI("Entered state: UI_CAMPAIGN_EDITOR");
            create_campaign_editor_menu();
            break;
        case EngineState::UI_CAMPAIGN_NLOANDING:
            LOGI("Entered state: UI_CAMPAIGN_NLOANDING");
            create_campaign_nloanding_menu();
            break;
        case EngineState::UI_CAMPAIGN_SETUP:
            LOGI("Entered state: UI_CAMPAIGN_SETUP");
            create_campaign_setup_menu();
            break;
        case EngineState::UI_CHAT_DIALOG:
            LOGI("Entered state: UI_CHAT_DIALOG");
            create_chat_dialog_menu();
            break;
        case EngineState::UI_LIFF_DIALOG:
            LOGI("Entered state: UI_LIFF_DIALOG");
            create_liff_dialog_menu();
            break;
        case EngineState::UI_COMBAT_COMPARISON:
            LOGI("Entered state: UI_COMBAT_COMPARISON");
            create_combat_comparison_menu();
            break;
        case EngineState::UI_CUSTOMIZE_OPP:
            LOGI("Entered state: UI_CUSTOMIZE_OPP");
            create_customize_opp_menu();
            break;
        case EngineState::UI_DEFAULT_LOADING:
            LOGI("Entered state: UI_DEFAULT_LOADING");
            create_default_loading_menu();
            break;
        case EngineState::UI_EDITOR_MENU:
            LOGI("Entered state: UI_EDITOR_MENU");
            create_editor_menu();
            break;
        case EngineState::UI_EDITOR_PALETTES:
            LOGI("Entered state: UI_EDITOR_PALETTES");
            create_editor_palettes_menu();
            break;
        case EngineState::UI_EDIT_UNIT_DLG:
            LOGI("Entered state: UI_EDIT_UNIT_DLG");
            create_edit_unit_dlg_menu();
            break;
        case EngineState::UI_EULA:
            LOGI("Entered state: UI_EULA");
            create_eula_menu();
            break;
        case EngineState::UI_FAKE_TECHTREE:
            LOGI("Entered state: UI_FAKE_TECHTREE");
            create_fake_techtree_menu();
            break;
        case EngineState::UI_FILE_MANAGER:
            LOGI("Entered state: UI_FILE_MANAGER");
            create_file_manager_menu();
            break;
        case EngineState::UI_FRIENDS_DIALOGS:
            LOGI("Entered state: UI_FRIENDS_DIALOGS");
            create_friends_dialogs_menu();
            break;
        case EngineState::UI_GAME_PALETTES:
            LOGI("Entered state: UI_GAME_PALETTES");
            create_game_palettes_menu();
            break;
        case EngineState::UI_GAME_SETTINGS_DIALOG:
            LOGI("Entered state: UI_GAME_SETTINGS_DIALOG");
            create_game_settings_dialog_menu();
            break;
        case EngineState::UI_GENERIC_ENTRY_DIALOG:
            LOGI("Entered state: UI_GENERIC_ENTRY_DIALOG");
            create_generic_entry_dialog_menu();
            break;
        case EngineState::UI_GOD_BUFFET_DIALOG:
            LOGI("Entered state: UI_GOD_BUFFET_DIALOG");
            create_god_buffet_dialog_menu();
            break;
        case EngineState::UI_HELP_DIALOG:
            LOGI("Entered state: UI_HELP_DIALOG");
            create_help_dialog_menu();
            break;
        case EngineState::UI_HOST_GAME_DIALOG:
            LOGI("Entered state: UI_HOST_GAME_DIALOG");
            create_host_game_dialog_menu();
            break;
        case EngineState::UI_HOTKEYS:
            LOGI("Entered state: UI_HOTKEYS");
            create_hotkeys_menu();
            break;
        case EngineState::UI_HOTKEY_SETUP:
            LOGI("Entered state: UI_HOTKEY_SETUP");
            create_hotkey_setup_menu();
            break;
        case EngineState::UI_IMAGE_DIALOG:
            LOGI("Entered state: UI_IMAGE_DIALOG");
            create_image_dialog_menu();
            break;
        case EngineState::UI_INGAME_MSG_1:
            LOGI("Entered state: UI_INGAME_MSG_1");
            create_ingame_msg_1_menu();
            break;
        case EngineState::UI_INGAME_MSG_2:
            LOGI("Entered state: UI_INGAME_MSG_2");
            create_ingame_msg_2_menu();
            break;
        case EngineState::UI_INGAME_MSG_3:
            LOGI("Entered state: UI_INGAME_MSG_3");
            create_ingame_msg_3_menu();
            break;
        case EngineState::UI_INGAME_MSG_DIALOG:
            LOGI("Entered state: UI_INGAME_MSG_DIALOG");
            create_ingame_msg_dialog_menu();
            break;
        case EngineState::UI_LAUNCH_SCREEN:
            LOGI("Entered state: UI_LAUNCH_SCREEN");
            create_launch_screen_menu();
            break;
        case EngineState::UI_LAYOUT:
            LOGI("Entered state: UI_LAYOUT");
            create_layout_menu();
            break;
        case EngineState::UI_LOW_DETAIL_BACKGROUND:
            LOGI("Entered state: UI_LOW_DETAIL_BACKGROUND");
            create_low_detail_background_menu();
            break;
        case EngineState::UI_MAIN:
            LOGI("Entered state: UI_MAIN");
            create_main_alt_menu();
            break;
        case EngineState::UI_MAP_PICKER:
            LOGI("Entered state: UI_MAP_PICKER");
            create_map_picker_menu();
            break;
        case EngineState::UI_MESSAGE_DIALOGS:
            LOGI("Entered state: UI_MESSAGE_DIALOGS");
            create_message_dialogs_menu();
            break;
        case EngineState::UI_MOTD:
            LOGI("Entered state: UI_MOTD");
            create_motd_menu();
            break;
        case EngineState::UI_MP_CUSTOM:
            LOGI("Entered state: UI_MP_CUSTOM");
            create_mp_custom_menu();
            break;
        case EngineState::UI_MP_ESO:
            LOGI("Entered state: UI_MP_ESO");
            create_mp_eso_menu();
            break;
        case EngineState::UI_MP_FRIENDS_LIST:
            LOGI("Entered state: UI_MP_FRIENDS_LIST");
            create_mp_friends_list_menu();
            break;
        case EngineState::UI_MP_HELP:
            LOGI("Entered state: UI_MP_HELP");
            create_mp_help_menu();
            break;
        case EngineState::UI_MP_PLANIP:
            LOGI("Entered state: UI_MP_PLANIP");
            create_mp_planip_menu();
            break;
        case EngineState::UI_MP_LAUNCH_SCREEN:
            LOGI("Entered state: UI_MP_LAUNCH_SCREEN");
            create_mp_launch_screen_menu();
            break;
        case EngineState::UI_MP_LOGIN:
            LOGI("Entered state: UI_MP_LOGIN");
            create_mp_login_menu();
            break;
        case EngineState::UI_MP_OTHER_SETTINGS:
            LOGI("Entered state: UI_MP_OTHER_SETTINGS");
            create_mp_other_settings_menu();
            break;
        case EngineState::UI_MP_PASSPORT:
            LOGI("Entered state: UI_MP_PASSPORT");
            create_mp_passport_menu();
            break;
        case EngineState::UI_MP_PASSPORT_TAKEN:
            LOGI("Entered state: UI_MP_PASSPORT_TAKEN");
            create_mp_passport_taken_menu();
            break;
        case EngineState::UI_MP_SETUP_FINAL:
            LOGI("Entered state: UI_MP_SETUP_FINAL");
            create_mp_setup_final_menu();
            break;
        case EngineState::UI_MP_VOTE_DIALOG:
            LOGI("Entered state: UI_MP_VOTE_DIALOG");
            create_mp_vote_dialog_menu();
            break;
        case EngineState::UI_NEW_DIALOGS:
            LOGI("Entered state: UI_NEW_DIALOGS");
            create_new_dialogs_menu();
            break;
        case EngineState::UI_OBJECTIVES:
            LOGI("Entered state: UI_OBJECTIVES");
            create_objectives_menu();
            break;
        case EngineState::UI_OBJECTIVES_DIALOG:
            LOGI("Entered state: UI_OBJECTIVES_DIALOG");
            create_objectives_dialog_menu();
            break;
        case EngineState::UI_OPTIONS:
            LOGI("Entered state: OPTIONS");
            create_options_menu();
            break;
        case EngineState::CREDITS:
            LOGI("Entered state: CREDITS");
            create_credits_menu();
            break;
        case EngineState::MULTIPLAYER_LAN:
            LOGI("Entered state: MULTIPLAYER_LAN");
            create_multiplayer_lan_menu();
            break;
        case EngineState::MULTIPLAYER_LOBBY:
            LOGI("Entered state: MULTIPLAYER_LOBBY");
            create_multiplayer_lobby_menu();
            break;
        case EngineState::IN_GAME:
            LOGI("Entered state: IN_GAME");
            create_in_game_hud();
            break;
        default:
            break;
    }
}

void Engine::create_main_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // ── Layout Autêntico Age of Mythology: The Titans (uipregamenew.xmb) ──────
    // A arte do menu The Titans é desenhada na resolução padrão 1024x768.
    // Os 7 botões de jade verde estão perfeitamente posicionados dentro do portal:
    // Left: 417, Right: 591 -> Largura de 174px (margem de toque ampliada)
    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    const float btn_w = 190.0f * sx;
    const float btn_h = 36.0f * sy;
    const float btn_x = (417.0f - 8.0f) * sx;

    // Ordem exata dos 7 botões do menu The Titans:
    // 1. Learn To Play (Y=325)
    // 2. Campaign      (Y=365)
    // 3. Single Player (Y=405)
    // 4. Multiplayer   (Y=445)
    // 5. Options       (Y=485)
    // 6. More          (Y=525)
    // 7. Exit          (Y=573)
    struct TitansBtn {
        float y;
        std::function<void()> action;
    };

    const TitansBtn btns[] = {
        { 325.0f, [this]() { start_game(); }    }, // Learn To Play
        { 365.0f, [this]() { create_campaign_menu(); } }, // Campaign
        { 405.0f, [this]() { create_campaign_menu(); } }, // Single Player - abre menu de seleção de cenário
        { 445.0f, [this]() { open_multiplayer(); } }, // Multiplayer
        { 485.0f, [this]() { open_options(); } }, // Options
        { 525.0f, [this]() { open_credits(); } }, // More
        { 573.0f, [this]() { quit_app(); }     }, // Exit
    };

    for (const auto& b : btns) {
        auto* btn = ui_->add_button(
            {btn_x, (b.y - 2.0f) * sy},
            {btn_w, btn_h},
            "",
            b.action,
            0x00000000,   // Transparente em repouso: exibe o botão original de jade perfeitamente
            0x55FFD700    // Brilho dourado ao toque (feedback tátil)
        );
        btn->normal_tex = INVALID_TEXTURE;
        btn->pressed_tex = INVALID_TEXTURE;
    }

    // ── Botoes extras do menu pregame (uipregamenew) ────────────────────────
    // uigamelist e uigodpicker nao estao no portal original dos 7 botoes de jade,
    // entao recebem um estilo visível (fundo dourado) para diferenciar.
    const float extra_btn_w = 220.0f * sx;
    const float extra_btn_h = 34.0f * sy;
    const float extra_btn_x = (vw - extra_btn_w) * 0.5f;
    float extra_y = (573.0f + 44.0f) * sy;

    ui_->add_button({extra_btn_x, extra_y}, {extra_btn_w, extra_btn_h},
                    "LISTA DE JOGOS (uigamelist)",
                    [this]() { set_state(EngineState::GAME_LIST); },
                    0xFF8B4513, 0xFFCD853F);
    extra_y += extra_btn_h + 8.0f * sy;

    ui_->add_button({extra_btn_x, extra_y}, {extra_btn_w, extra_btn_h},
                    "ESCOLHA DE DEUS (uigodpicker)",
                    [this]() { set_state(EngineState::GOD_PICKER); },
                    0xFF8B4513, 0xFFCD853F);

    // ── Aviso de dados originais ausentes ────────────────────────────────────
    if (!archives_loaded_) {
        ui_->add_label("DADOS DO JOGO NAO ENCONTRADOS",
            {24.0f, vh - 64.0f}, 16.0f, 0xFFFF6666);
        ui_->add_label("OPCOES > IMPORTAR DADOS DO JOGO",
            {24.0f, vh - 42.0f}, 13.0f, 0xFFFFD000);
    }

    // ── FPS discreto no canto inferior direito ───────────────────────────────
    if (show_fps_) {
        fps_label_ = ui_->add_label("FPS: 60",
            {vw - 90.0f, vh - 24.0f}, 14.0f, 0x88FFFFFF);
    }
}

// ── uigamelist — lista de jogos salvos, campanhas e cenarios ─────────────────
void Engine::create_game_list_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    ui_->add_label("LISTA DE JOGOS", {vw * 0.5f - 130.0f, vh * 0.08f}, 32.0f,
                   0xFFFFD000);

    // ── Jogos salvos ────────────────────────────────────────────────────────
    ui_->add_label("JOGOS SALVOS", {vw * 0.5f - 200.0f, vh * 0.18f}, 22.0f,
                   0xFFCCCCCC);

    const std::string sp = save_game_path();
    const bool found = has_valid_save(sp);

    float y = vh * 0.26f;
    const float btn_w = 340.0f * sx;
    const float btn_h = 44.0f * sy;
    const float btn_x = (vw - btn_w) * 0.5f;
    const float gap = 12.0f * sy;

    if (found) {
        ui_->add_button({btn_x, y}, {btn_w, btn_h}, "CONTINUAR PARTIDA SALVA",
                        [this]() { load_game(); });
        y += btn_h + gap;
    } else {
        ui_->add_label("Nenhum jogo salvo encontrado.",
                       {vw * 0.5f - 150.0f, y}, 16.0f, 0xFFCCCCCC);
        y += 30.0f * sy;
    }

    // ── Campanhas disponiveis ───────────────────────────────────────────────
    y += 8.0f * sy;
    ui_->add_label("CAMPANHAS", {vw * 0.5f - 200.0f, y}, 22.0f, 0xFFCCCCCC);
    y += 32.0f * sy;

    const Campaign* camp = campaign_mgr_ ? campaign_mgr_->current_campaign() : nullptr;
    if (campaign_mgr_ && campaign_mgr_->scenario_count() > 0) {
        const size_t scenario_count = campaign_mgr_->scenario_count();
        const size_t show_count = std::min<size_t>(scenario_count, 6);
        std::string camp_name = camp ? camp->name : "Campanha";
        ui_->add_label("Campanha: " + camp_name + "  (" +
                           std::to_string(scenario_count) + " cenarios)",
                       {vw * 0.5f - 240.0f, y - 8.0f * sy}, 16.0f, 0xFFCCCCCC);
        for (size_t i = 0; i < show_count; ++i) {
            std::string label = "Cenario " + std::to_string(i + 1);
            ui_->add_button({btn_x, y}, {btn_w, btn_h}, label,
                            [this, i]() { start_campaign_mission(i); });
            y += btn_h + gap;
        }
    } else {
        ui_->add_label("Nenhuma campanha carregada.",
                       {vw * 0.5f - 150.0f, y}, 16.0f, 0xFFCCCCCC);
        y += 30.0f * sy;
    }

    // ── Cenarios aleatorios / skirmish ──────────────────────────────────────
    y += 8.0f * sy;
    ui_->add_label("MAPA ALEATORIO / SKIRMISH", {vw * 0.5f - 220.0f, y},
                   22.0f, 0xFFCCCCCC);
    y += 32.0f * sy;

    ui_->add_button({btn_x, y}, {btn_w, btn_h}, "NOVO JOGO SKIRMISH",
                    [this]() { set_state(EngineState::IN_GAME); start_game(); });
    y += btn_h + gap;

    ui_->add_button({btn_x, y}, {btn_w, btn_h}, "CAMPANHA (selecionar)",
                    [this]() { create_campaign_menu(); });
    y += btn_h + gap;

    // ── Voltar ──────────────────────────────────────────────────────────────
    float back_w = 220.0f * sx;
    float back_h = 44.0f * sy;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, back_h},
                    "VOLTAR", [this]() { return_to_main_menu(); });
}

// ── uigodpicker — escolha de civilizacao / deus menor ────────────────────────
void Engine::create_god_picker_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    ui_->add_label("ESCOLHA DE DEUS / CIVILIZACAO",
                   {vw * 0.5f - 230.0f, vh * 0.08f}, 30.0f, 0xFFFFD000);

    // Mostra selecao atual (se ja houver)
    if (god_picker_selection_made_) {
        std::string sel = "Selecionado: " + god_picker_civilization_ +
                          " / " + god_picker_minor_god_;
        ui_->add_label(sel, {vw * 0.5f - 200.0f, vh * 0.13f}, 16.0f,
                       0xFF88FF88);
    }

    // Lista de civilizacoes / deuses menores do Age of Mythology
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    const float sx = vw / 1024.0f;
    const float sy = vh / 768.0f;

    ui_->add_label("ESCOLHA DE DEUS / CIVILIZACAO",
                   {vw * 0.5f - 230.0f, vh * 0.08f}, 30.0f, 0xFFFFD000);

    // Mostra selecao atual (se ja houver)
    if (god_picker_selection_made_) {
        std::string sel = "Selecionado: " + god_picker_civilization_ +
                          " / " + god_picker_minor_god_;
        ui_->add_label(sel, {vw * 0.5f - 200.0f, vh * 0.13f}, 16.0f,
                       0xFF88FF88);
    }

    // Lista de civilizacoes / deuses menores do Age of Mythology
    struct GodEntry {
        const char* civilization;
        const char* major_god;
        const char** minor_gods;
        int minor_count;
    };

    static const char* greek_minors[] = {"Afrodite", "Apolo", "Ares", "Artemis",
                                         "Atena", "Dioniso", "Hades", "Hefesto",
                                         "Hermes", "Hestia", "Poseidon", "Zeus"};
    static const char* egyptian_minors[] = {"Anubis", "Bastet", "Hórus", "Isis",
                                            "Nefti", "Osíris", "Ptah", "Ra",
                                            "Set", "Toth"};
    static const char* norse_minors[] = {"Balder", "Freyja", "Freyr", "Heimdall",
                                         "Hermod", "Loki", "Njord", "Odin",
                                         "Sif", "Skadi", "Thor", "Tyr", "Uller"};
    static const char* atlantean_minors[] = {"Atlas", "Cronos", "Hécate", "Helios",
                                             "Leto", "Okeano", "Prometeu", "Reia",
                                             "Téia"};

    const GodEntry gods[] = {
        {"Gregos", "Zeus", greek_minors, 12},
        {"Egípcios", "Rá", egyptian_minors, 10},
        {"Nórdicos", "Odin", norse_minors, 13},
        {"Atlantes", "Cronos", atlantean_minors, 9},
    };

    const float col_w = 300.0f * sx;
    const float col_h = 42.0f * sy;
    const float gap = 10.0f * sy;
    const float start_x = vw * 0.08f;
    const float start_y = vh * 0.18f;

    float x = start_x;
    float y = start_y;
    const float max_col_h = vh * 0.62f;

    for (const auto& g : gods) {
        // Título da civilização
        ui_->add_label(g.civilization, {x, y}, 20.0f, 0xFFFFD000);
        y += 28.0f * sy;

        // Deus maior
        ui_->add_label("Maior: " + std::string(g.major_god), {x, y}, 16.0f,
                       0xFFCCCCCC);
        y += 24.0f * sy;

        // Deuses menores
        for (int i = 0; i < g.minor_count; ++i) {
            if (y + col_h > max_col_h) {
                // Nova coluna
                x += col_w + gap;
                y = start_y;
            }
            std::string btn_label = std::string(g.minor_gods[i]) +
                                    " (" + g.civilization + ")";
            ui_->add_button({x, y}, {col_w, col_h}, btn_label,
                            [this, civ = std::string(g.civilization),
                             god = std::string(g.minor_gods[i])]() {
                                god_picker_civilization_ = civ;
                                god_picker_minor_god_    = god;
                                god_picker_selection_made_ = true;
                                LOGI("Deus selecionado: %s — %s", civ.c_str(),
                                     god.c_str());
                                set_state(EngineState::IN_GAME);
                                start_game();
                            });
            y += col_h + gap;
        }

        x += col_w + gap;
        y = start_y;
    }

    // Voltar
    float back_w = 200.0f * sx;
    float back_h = 44.0f * sy;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, back_h},
                    "VOLTAR", [this]() { return_to_main_menu(); });
}

// ── uigamelist — lista de jogos salvos, campanhas e cenarios ──
void Engine::create_game_list_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("LISTA DE JOGOS", {vw * 0.5f - 130.0f, vh * 0.08f}, 32.0f,
                   0xFFFFD000);

    const std::string sp = save_game_path();
    const bool found = has_valid_save(sp);

    float y = vh * 0.26f;
    const float btn_w = 340.0f;
    const float btn_h = 44.0f;
    const float btn_x = (vw - btn_w) * 0.5f;
    const float gap = 12.0f;

    if (found) {
        ui_->add_button({btn_x, y}, {btn_w, btn_h}, "CONTINUAR PARTIDA SALVA",
                        [this]() { load_game(); });
        y += btn_h + gap;
    } else {
        ui_->add_label("Nenhum jogo salvo encontrado.",
                       {vw * 0.5f - 150.0f, y}, 16.0f, 0xFFCCCCCC);
        y += 30.0f;
    }

    y += 8.0f;
    ui_->add_label("CAMPANHAS", {vw * 0.5f - 200.0f, y}, 22.0f, 0xFFCCCCCC);
    y += 32.0f;

    if (campaign_mgr_ && campaign_mgr_->scenario_count() > 0) {
        const size_t scenario_count = campaign_mgr_->scenario_count();
        const size_t show_count = std::min<size_t>(scenario_count, 6);
        std::string camp_name = campaign_mgr_->current_campaign() ? campaign_mgr_->current_campaign()->name : "Campanha";
        ui_->add_label("Campanha: " + camp_name + " (" + std::to_string(scenario_count) + " cenarios)",
                       {vw * 0.5f - 240.0f, y - 8.0f}, 16.0f, 0xFFCCCCCC);
        for (size_t i = 0; i < show_count; ++i) {
            ui_->add_button({btn_x, y}, {btn_w, btn_h}, "Cenario " + std::to_string(i + 1),
                            [this, i]() { start_campaign_mission(i); });
            y += btn_h + gap;
        }
    } else {
        ui_->add_label("Nenhuma campanha carregada.",
                       {vw * 0.5f - 150.0f, y}, 16.0f, 0xFFCCCCCC);
        y += 30.0f;
    }

    y += 8.0f;
    ui_->add_label("MAPA ALEATORIO / SKIRMISH", {vw * 0.5f - 220.0f, y}, 22.0f,
                   0xFFCCCCCC);
    y += 32.0f;

    ui_->add_button({btn_x, y}, {btn_w, btn_h}, "NOVO JOGO SKIRMISH",
                    [this]() { set_state(EngineState::IN_GAME); start_game(); });
    y += btn_h + gap;

    ui_->add_button({btn_x, y}, {btn_w, btn_h}, "CAMPANHA (selecionar)",
                    [this]() { create_campaign_menu(); });

    float back_w = 220.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, back_h},
                    "VOLTAR", [this]() { return_to_main_menu(); });
}

// ── uialternates_dialog ──
void Engine::create_alternates_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ALTERNATES DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uianimation_dlg ──
void Engine::create_animation_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ANIMATION DLG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiarena_setup ──
void Engine::create_arena_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ARENA SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiarmy_edit ──
void Engine::create_army_edit_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ARMY EDIT", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiautopatcher_dialog ──
void Engine::create_autopatcher_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("AUTOPATCHER DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucamera_editor ──
void Engine::create_camera_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMERA EDITOR", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign ──
void Engine::create_campaign_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_editor ──
void Engine::create_campaign_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN EDITOR", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_nloanding ──
void Engine::create_campaign_nloanding_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN NLOANDING", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── ucampaign_setup ──
void Engine::create_campaign_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CAMPAIGN SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uichat_dialog ──
void Engine::create_chat_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CHAT DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiliff_dialog ──
void Engine::create_liff_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LIFF DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uicombat_comparison ──
void Engine::create_combat_comparison_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("COMBAT COMPARISON", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uicustomize_opp ──
void Engine::create_customize_opp_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("CUSTOMIZE OPPONENT", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uidefault_loading ──
void Engine::create_default_loading_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("DEFAULT LOADING", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieditor_menu ──
void Engine::create_editor_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDITOR MENU", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieditor_palettes ──
void Engine::create_editor_palettes_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDITOR PALETTES", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiedit_unit_dlg ──
void Engine::create_edit_unit_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EDIT UNIT DLG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uieula ──
void Engine::create_eula_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("EULA", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uifake_techtree ──
void Engine::create_fake_techtree_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FAKE TECHTREE", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uifile_manager ──
void Engine::create_file_manager_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FILE MANAGER", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uifriends_dialogs ──
void Engine::create_friends_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("FRIENDS DIALOGS", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigame_palettes ──
void Engine::create_game_palettes_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GAME PALETTES", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigame_settings_dialog ──
void Engine::create_game_settings_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GAME SETTINGS DIALOG", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigeneric_entry_dialog ──
void Engine::create_generic_entry_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GENERIC ENTRY DIALOG", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uigod_buffet_dialog ──
void Engine::create_god_buffet_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("GOD BUFFET DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihelp_dialog ──
void Engine::create_help_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HELP DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihost_game_dialog ──
void Engine::create_host_game_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOST GAME DIALOG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihotkeys ──
void Engine::create_hotkeys_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOTKEYS", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uihotkey_setup ──
void Engine::create_hotkey_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("HOTKEY SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiimage_dialog ──
void Engine::create_image_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("IMAGE DIALOG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingame_msg_1 ──
void Engine::create_ingame_msg_1_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 1", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingame_msg_2 ──
void Engine::create_ingame_msg_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 2", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingame_msg_3 ──
void Engine::create_ingame_msg_3_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG 3", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiingame_msg_dialog ──
void Engine::create_ingame_msg_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("INGAME MSG DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilaunch_screen ──
void Engine::create_launch_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LAUNCH SCREEN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilayout ──
void Engine::create_layout_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LAYOUT", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uilow_detail_background ──
void Engine::create_low_detail_background_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("LOW DETAIL BACKGROUND", {vw * 0.5f - 170.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimain ──
void Engine::create_main_alt_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MAIN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimap_picker ──
void Engine::create_map_picker_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MAP PICKER", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimessage_dialogs ──
void Engine::create_message_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MESSAGE DIALOGS", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimotd ──
void Engine::create_motd_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MOTD", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_custom ──
void Engine::create_mp_custom_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP CUSTOM", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_eso ──
void Engine::create_mp_eso_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP ESO", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_friends_list ──
void Engine::create_mp_friends_list_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP FRIENDS LIST", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_help ──
void Engine::create_mp_help_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP HELP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_planip ──
void Engine::create_mp_planip_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PLANIP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_launch_screen ──
void Engine::create_mp_launch_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP LAUNCH SCREEN", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_login ──
void Engine::create_mp_login_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP LOGIN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_other_settings ──
void Engine::create_mp_other_settings_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP OTHER SETTINGS", {vw * 0.5f - 170.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_passport ──
void Engine::create_mp_passport_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PASSPORT", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_passport_taken ──
void Engine::create_mp_passport_taken_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP PASSPORT TAKEN", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_setup_final ──
void Engine::create_mp_setup_final_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP SETUP FINAL", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uimp_vote_dialog ──
void Engine::create_mp_vote_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("MP VOTE DIALOG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uinew_dialogs ──
void Engine::create_new_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("NEW DIALOGS", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiobjectives ──
void Engine::create_objectives_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OBJECTIVES", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiobjectives_dialog ──
void Engine::create_objectives_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OBJECTIVES DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiothers_settings ──
void Engine::create_others_settings_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("OTHER SETTINGS", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uip layer_login ──
void Engine::create_player_login_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER LOGIN", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uip layer_status_dialog ──
void Engine::create_player_status_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER STATUS DIALOG", {vw * 0.5f - 170.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uip layer_status_dialog_2 ──
void Engine::create_player_status_dialog_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PLAYER STATUS DIALOG 2", {vw * 0.5f - 180.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipostgamescreen ──
void Engine::create_post_game_screen_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("POST GAME SCREEN", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregame ──
void Engine::create_pregame_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemptest ──
void Engine::create_pregame_mp_test_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MP TEST", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsg1 ──
void Engine::create_pregame_msg_1_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG 1", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsg2 ──
void Engine::create_pregame_msg_2_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG 2", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamemsgdialog ──
void Engine::create_pregame_msg_dialog_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME MSG DIALOG", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamenew ──
void Engine::create_pregame_new_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME NEW", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipregamestress ──
void Engine::create_pregame_stress_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PREGAME STRESS", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uipfilelect ──
void Engine::create_profile_select_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("PROFILE SELECT", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uireceive ──
void Engine::create_receive_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RECEIVE", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uireceiveconnect ──
void Engine::create_receive_connect_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RECEIVE CONNECT", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uirmgeneration ──
void Engine::create_rm_generation_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("RM GENERATION", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiroomdialogs ──
void Engine::create_room_dialogs_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("ROOM DIALOGS", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisample ──
void Engine::create_sample_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SAMPLE", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uiscensetdlg ──
void Engine::create_scenario_set_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SCENARIO SET DLG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisessionconnect ──
void Engine::create_session_connect_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SESSION CONNECT", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisingleplayersetup ──
void Engine::create_single_player_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SINGLE PLAYER SETUP", {vw * 0.5f - 160.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisounddlg ──
void Engine::create_sound_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND DLG", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisoundeditdlg ──
void Engine::create_sound_edit_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND EDIT DLG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisoundsetdlg ──
void Engine::create_sound_set_dlg_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SOUND SET DLG", {vw * 0.5f - 150.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uisprmsetup ──
void Engine::create_sprm_setup_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("SPRM SETUP", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

// ── uistatsupload ──
void Engine::create_stats_upload_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);
    ui_->add_label("STATS UPLOAD", {vw * 0.5f - 140.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);
    ui_->add_button({(vw - 200.0f) * 0.5f, vh * 0.88f}, {200.0f, 44.0f}, "VOLTAR",
                    [this]() { return_to_main_menu(); });
}

void Engine::create_options_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CONFIGURACOES", {24.0f, vh * 0.08f}, 32.0f, 0xFFFFD000);

    // ── Importação dos dados originais do jogo (Android SAF) ────────────────
    const float import_w = 280.0f;
    const float import_h = 40.0f;
    const float import_x = vw - import_w - 24.0f;
    const float import_y = vh * 0.08f;

    ui_->add_button({import_x, import_y}, {import_w, import_h},
                    "IMPORTAR DADOS DO JOGO",
                    [this]() {
                        if (!request_game_data_import()) {
                            LOGW("Nao foi possivel abrir o seletor de pasta do jogo");
                        }
                    });

    ui_->add_label(archives_loaded_ ? "Dados do jogo: carregados"
                                    : "Dados do jogo: nao encontrados",
                   {import_x, import_y + import_h + 6.0f}, 14.0f,
                   archives_loaded_ ? 0xFF88FF88 : 0xFFFF7777);
    ui_->add_label("Selecione a pasta que contem data/ e textures/.",
                   {import_x, import_y + import_h + 26.0f}, 12.0f, 0xFFCCCCCC);

    const float col_w = 340.0f;
    const float col1_x = vw * 0.5f - col_w - 20.0f;
    const float col2_x = vw * 0.5f + 20.0f;
    const float content_top = std::max(vh * 0.24f, import_y + import_h + 70.0f);
    float y1 = content_top;
    float y2 = content_top;
    const float ctrl_h = 44.0f;
    const float gap = (vh < 600.0f) ? 12.0f : 16.0f;

    // --- Coluna 1: AUDIO ---
    ui_->add_label("AUDIO", {col1_x, y1}, 20.0f, 0xFFFFD000);
    y1 += 32.0f;

    ui_->add_slider({col1_x, y1}, {col_w, ctrl_h}, "Volume Geral", master_volume_,
                    [this](float val) { master_volume_ = val; save_options(); });
    y1 += ctrl_h + gap;

    ui_->add_slider({col1_x, y1}, {col_w, ctrl_h}, "Volume da Musica", music_volume_,
                    [this](float val) {
                        music_volume_ = val;
                        if (audio_) audio_->set_music_volume(val);
                        save_options();
                    });
    y1 += ctrl_h + gap;

    ui_->add_slider({col1_x, y1}, {col_w, ctrl_h}, "Volume dos Efeitos", sfx_volume_,
                    [this](float val) {
                        sfx_volume_ = val;
                        if (audio_) audio_->set_sfx_volume(val);
                        save_options();
                    });
    y1 += ctrl_h + gap;

    ui_->add_checkbox({col1_x, y1}, {col_w, 36.0f}, "Musica Ativada", music_enabled_,
                      [this](bool val) {
                          music_enabled_ = val;
                          if (audio_) audio_->set_music_enabled(val);
                          save_options();
                      });
    y1 += 36.0f + gap;

    ui_->add_checkbox({col1_x, y1}, {col_w, 36.0f}, "Efeitos Sonoros Ativados", sfx_enabled_,
                      [this](bool val) {
                          sfx_enabled_ = val;
                          if (audio_) audio_->set_sfx_enabled(val);
                          save_options();
                      });

    // --- Coluna 2: GRAFICOS & SISTEMA ---
    ui_->add_label("VIDEO & SISTEMA", {col2_x, y2}, 20.0f, 0xFFFFD000);
    y2 += 32.0f;

    ui_->add_slider({col2_x, y2}, {col_w, ctrl_h}, "Qualidade Grafica", graphics_quality_,
                    [this](float val) { graphics_quality_ = val; save_options(); });
    y2 += ctrl_h + gap;

    ui_->add_checkbox({col2_x, y2}, {col_w, 36.0f}, "Exibir Contador de FPS", show_fps_,
                      [this](bool val) { show_fps_ = val; save_options(); });
    y2 += 36.0f + gap;

    ui_->add_checkbox({col2_x, y2}, {col_w, 36.0f}, "Filtragem Trilinear Mipmap", true,
                      [](bool) {});
    y2 += 36.0f + gap;

    ui_->add_checkbox({col2_x, y2}, {col_w, 36.0f}, "Iluminacao Direcional Phong", true,
                      [](bool) {});

    // Botao Salvar Partida (se mundo de jogo ativo)
    if (gameplay_world_) {
        float save_w = 220.0f, save_h = 44.0f;
        ui_->add_button({vw * 0.5f - save_w - 15.0f, vh * 0.84f}, {save_w, save_h}, "SALVAR PARTIDA",
                       [this]() { save_game(); });
        float back_w = 220.0f, back_h = 44.0f;
        ui_->add_button({vw * 0.5f + 15.0f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                       [this]() { return_to_main_menu(); });
    } else {
        float back_w = 220.0f, back_h = 44.0f;
        ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                       [this]() { return_to_main_menu(); });
    }
}

void Engine::create_credits_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CREDITOS", {vw * 0.5f - 80.0f, vh * 0.12f}, 32.0f, 0xFFFFD000);

    float text_y = vh * 0.25f;
    const float step = 28.0f;
    ui_->add_label("Age of Mythology Original por Ensemble Studios (2002)", {vw * 0.5f - 240.0f, text_y}, 18.0f, 0xFFFFFFFF);
    text_y += step;
    ui_->add_label("Porte Nativo Android ARM64 por Antigravity Studio", {vw * 0.5f - 220.0f, text_y}, 18.0f, 0xFFFFD000);
    text_y += step * 1.5f;
    ui_->add_label("Lead Architecture: Technical Director", {vw * 0.5f - 160.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("OpenGL ES 3 Shaders & Pipeline: Graphics Lead", {vw * 0.5f - 190.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("BAR, DDT & BRG Parsers: Asset Pipeline Lead", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("OpenSL ES Low-Latency Audio: Audio Specialist", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);
    text_y += step;
    ui_->add_label("Reverse Engineering & XMB UI: REA Specialist", {vw * 0.5f - 180.0f, text_y}, 16.0f, 0xFFCCCCCC);

    float back_w = 200.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                   [this]() { return_to_main_menu(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// Clips de animação BRG
//
// `models.bar` (jogo base + expansão The Titans) contém milhares de arquivos
// `.brg`. Extrair e interpretar TODOS eles dentro de `start_game()` e
// `start_campaign_mission()` bloqueava a thread da UI do NativeActivity por
// mais de 5 segundos — o Android encerrava o app com ANR ("Input dispatching
// timed out") ao iniciar uma missão.
//
// Aqui a carga é limitada (`max_clips`) e o resultado fica em cache, de modo
// que as partidas seguintes reaproveitam os clips já interpretados.
// ─────────────────────────────────────────────────────────────────────────────
bool Engine::load_animation_clips(size_t max_clips) {
    if (anim_clips_ready_ && anim_system_ && anim_system_->clip_count() > 0) {
        return true;  // já carregados em uma partida anterior
    }

    if (!anim_system_) {
        anim_system_ = std::make_unique<aom::anim::AnimationSystem>();
    }
    if (!models_bar_ || models_bar_->file_count() == 0) return false;

    const auto t0 = std::chrono::steady_clock::now();

    // Os nomes das entradas são baratos; só a extração dos dados é cara.
    // Prioriza os clips das unidades jogáveis e completa com os demais.
    static const char* kPreferred[] = {"hoplite", "villager", "toxotes", "minotaur",
                                        "archer",   "cavalry",  "infantry", "soldier"};

    std::vector<std::string> preferred;
    std::vector<std::string> fallback;
    for (const BarEntry& entry : models_bar_->entries()) {
        const std::string low = to_lower(entry.filename);
        if (!ends_with(low, ".brg")) continue;
        bool is_preferred = false;
        for (const char* pref : kPreferred) {
            if (low.find(pref) != std::string::npos) { is_preferred = true; break; }
        }
        if (is_preferred) {
            if (preferred.size() < max_clips) preferred.push_back(entry.filename);
        } else if (fallback.size() < max_clips) {
            fallback.push_back(entry.filename);
        }
        if (preferred.size() >= max_clips && fallback.size() >= max_clips) break;
    }

    std::vector<std::string> candidates = preferred;
    for (const std::string& name : fallback) {
        if (candidates.size() >= max_clips) break;
        candidates.push_back(name);
    }

    size_t loaded = 0;
    anim_clip_names_.clear();
    for (const std::string& name : candidates) {
        const BarEntry* entry = models_bar_->find(name);
        if (!entry) continue;
        const std::vector<uint8_t> data = models_bar_->extract(*entry);
        if (data.empty()) continue;
        if (anim_system_->load_brg_clip_from_memory(name, data.data(), data.size())) {
            anim_clip_names_.push_back(name);
            ++loaded;
        }
    }

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
    LOGI("BRG clips: %zu carregados (de %zu candidatos) em %lld ms",
         loaded, candidates.size(), static_cast<long long>(elapsed_ms));

    anim_clips_ready_ = true;
    return loaded > 0;
}

bool Engine::select_animation_clip(const aom::anim::BrgAnimationClip*& clip,
                                   TextureHandle& texture) const {
    clip = nullptr;
    texture = INVALID_TEXTURE;
    if (!anim_system_ || anim_clip_names_.empty()) return false;

    for (const std::string& name : anim_clip_names_) {
        const aom::anim::BrgAnimationClip* candidate = anim_system_->find_clip(name);
        if (!candidate || !candidate->is_valid()) continue;
        clip = candidate;

        if (renderer_ && textures_bar_ && textures_bar_->file_count() > 0) {
            const std::string base = name.substr(0, name.size() - 4);
            for (const char* ext : {".ddt", " diffuse.ddt"}) {
                const std::string tex_name = base + ext;
                const std::vector<uint8_t> tex_data = textures_bar_->extract(tex_name);
                if (tex_data.empty()) continue;
                auto ddt = DdtLoader::load(tex_data);
                if (!ddt) break;
                const auto rgba = ddt->decode_rgba(0);
                if (rgba.empty()) break;
                texture = renderer_->create_texture(ddt->width, ddt->height, rgba.data());
                LOGI("Loaded anim texture: %s", tex_name.c_str());
                break;
            }
        }
        break;
    }
    return clip != nullptr;
}

void Engine::start_game() {
    const auto setup_start = std::chrono::steady_clock::now();
    LOGI("Starting Game...");
    skirmish_victory_result_ = VictoryResult::NONE;
    victory_display_timer_   = 0.0f;
    skirmish_victory_condition_ = VictoryCondition::CONQUEST;
    set_state(EngineState::IN_GAME);

    // ── Aplicar seleção do uigodpicker (civilização / deus menor) ──
    if (god_picker_selection_made_) {
        LOGI("God picker aplicado: civilização=%s deus_menor=%s",
             god_picker_civilization_.c_str(), god_picker_minor_god_.c_str());
        // TODO: aplicar civilização e deus menor às unidades/estatísticas
        // (ex: selecionar mesh/textura da unidade, bônus de deus, etc.)
        god_picker_selection_made_ = false;  // consome a seleção
    }

    radius_ = 12.0f;
    yaw_ = 0.0f;
    pitch_ = 0.3f;
    camera_.position = glm::vec3(0.0f, 4.0f, radius_);
    camera_.target = glm::vec3(0.0f, 0.0f, 0.0f);
    camera_.up = glm::vec3(0.0f, 1.0f, 0.0f);
    camera_.fov = glm::radians(45.0f);
    camera_.aspect = static_cast<float>(config_.window_width) / static_cast<float>(config_.window_height);

    accumulated_time_ = 0.0f;
    frame_count_ = 0;

    // --- Initialize GameplayWorld ---
    gameplay_world_ = std::make_unique<GameplayWorld>(game_config_root_.get());
    gameplay_world_->set_anim_renderer(renderer_.get());
    gameplay_world_->set_unit_transform_manager(&game_data_.unit_transforms());

    // --- Load BRG animation clips from models.bar (carga limitada + cache) ---
    load_animation_clips();

    // --- Spawn unidades com tipos distintos e mesh apropriada ---
    // Cada unidade recebe UnitStats extraídos do proto.xmb (se disponível) e
    // a mesh/textura correspondente do model_cache_ (por nome de arquivo .brg).
    struct UnitSpawn {
        glm::vec3 pos;
        Team team;
        const char* unit_name;
        const char* brg_name;   // nome do .brg no models.bar, ou nullptr para fallback
    };

    const std::vector<UnitSpawn> spawns = {
        // Friendly base (esquerda)
        {{-3.0f, 0.0f, 0.0f},  Team::FRIENDLY, "Hoplite",      "infantry g hoplite_idlea.brg"},
        {{-1.5f, 0.0f,  1.0f}, Team::FRIENDLY, "Toxotes",      "archer g toxotes_idlea.brg"},
        {{-1.5f, 0.0f, -1.0f}, Team::FRIENDLY, "Villager Greek", nullptr},
        // Enemy base (direita)
        {{ 3.0f, 0.0f, 0.0f},  Team::ENEMY,    "Hoplite",      "infantry g hoplite_idlea.brg"},
        {{ 1.5f, 0.0f,  1.0f},  Team::ENEMY,    "Toxotes",      "archer g toxotes_idlea.brg"},
        {{ 1.5f, 0.0f, -1.0f},  Team::ENEMY,    "Hoplite",      "infantry g hoplite_idlea.brg"},
    };

    const aom::anim::BrgAnimationClip* clip = nullptr;
    TextureHandle clip_tex = INVALID_TEXTURE;
    select_animation_clip(clip, clip_tex);

    for (const auto& s : spawns) {
        // Extrair stats do proto.xmb (se disponível), senão defaults por tipo
        UnitStats stats = extract_unit_stats_from_xmb(game_config_root_.get(), s.unit_name);
        if (stats.name == "Unit") {
            // Fallback se proto.xmb não tiver esta unidade
            stats.name = s.unit_name;
            stats.max_hp = 120.0f;
            stats.move_speed = 3.8f;
            stats.attack_damage = 12.0f;
            stats.attack_range = 2.2f;
        }

        Entity e = gameplay_world_->spawn_unit(s.team, s.pos, stats);

        // Associar mesh/textura do cache, se disponível
        if (s.brg_name) {
            if (auto* me = get_model_entry(s.brg_name)) {
                if (me->mesh != INVALID_MESH) {
                    // Armazenar mesh/textura na AnimationComponent via attach_animation
                    // ou usar diretamente no render. Aqui usamos o default_clip para
                    // animação; a mesh específica será usada pelo render se houver.
                    if (clip && clip->is_valid()) {
                        gameplay_world_->attach_animation(e, clip, clip_tex, renderer_.get());
                    }
                    // TODO: associar mesh específica por entidade (pode exigir
                    // render_animated por entidade com mesh individual).
                    LOGI("Spawned %s (%s) with cached mesh %s",
                         s.unit_name, s.team == Team::FRIENDLY ? "friendly" : "enemy", s.brg_name);
                }
            }
        } else if (clip && clip->is_valid()) {
            gameplay_world_->attach_animation(e, clip, clip_tex, renderer_.get());
        }
    }

    if (clip && clip->is_valid()) {
        gameplay_world_->set_default_animation(clip, clip_tex);
    }

    // Wire gameplay world callbacks to interactive particle spawners & 3D audio triggers
    // P3.5: usa PrtParticleSystem (.prt) quando disponível, fallback para partículas antigas
    gameplay_world_->on_resource_harvested = [this](ResourceType type, const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(6, pos);  // emite partículas de colheita
        } else {
            spawn_harvest_chips(type, pos);
        }
        if (audio_) {
            std::string res_name = (type == ResourceType::WOOD) ? "wood" : ((type == ResourceType::GOLD) ? "gold" : "food");
            audio_->play_harvest_sound(pos, res_name);
        }
    };
    gameplay_world_->on_combat_hit = [this](Entity, Entity, const glm::vec3& hit_pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(10, hit_pos);  // emite partículas de impacto
        } else {
            spawn_combat_hit(hit_pos);
        }
        if (audio_) audio_->play_combat_hit(hit_pos, true);
    };
    gameplay_world_->on_building_sparks = [this](const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(4, pos);  // emite faíscas de construção
        } else {
            spawn_construction_sparks(pos, 3);
        }
        if (audio_) audio_->play_building_work(pos);
    };
    gameplay_world_->on_building_completed = [this](Entity, const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(16, pos);  // emite partículas de conclusão
        } else {
            spawn_construction_sparks(pos, 16);
        }
        if (audio_) audio_->play_sfx_3d("assets/sound/new_objective.wav", pos, 1.0f);
    };
    gameplay_world_->on_unit_death = [this](const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(12, pos);  // emite fumaça de morte
        } else {
            spawn_death_smoke(pos);
        }
        if (audio_) audio_->play_unit_death(pos, false);
    };

    // Spawn Town Centers for Friendly and Enemy bases
    gameplay_world_->place_town_center(Team::FRIENDLY, glm::vec3(-6.0f, 0.0f, 0.0f), true);
    gameplay_world_->place_town_center(Team::ENEMY, glm::vec3(6.0f, 0.0f, 0.0f), true);

    // Spawn 1 Friendly Greek Villager near friendly Town Center
    gameplay_world_->spawn_worker(glm::vec3(-4.0f, 0.0f, 1.5f));

    // Spawn Resource Nodes (Food, Wood, Gold) around Friendly and Enemy bases
    gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(-4.0f, 0.0f, 3.5f), 1200);
    gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(-7.5f, 0.0f, 3.5f), 1500);
    gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(-5.5f, 0.0f, -3.5f), 2000);

    gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(4.0f, 0.0f, 3.5f), 1200);
    gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(7.5f, 0.0f, 3.5f), 1500);
    gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(5.5f, 0.0f, -3.5f), 2000);

    // Generate 3D terrain: try loading real height-map from .scn first,
    // fall back to procedural terrain if no scenario file is available.
    if (terrain_mesh_ == INVALID_MESH) {
        bool heightmap_loaded = false;
        if (scenarios_bar_ && scenarios_bar_->file_count() > 0) {
            // Limite de tentativas: cada .scn e varrido byte a byte na busca
            // do height-map, e o archive do AoM tem dezenas de cenários.
            int attempts = 0;
            for (const BarEntry& entry : scenarios_bar_->entries()) {
                if (!ends_with(to_lower(entry.filename), ".scn")) continue;
                if (++attempts > 6) break;
                heightmap_loaded = load_scenario_heightmap(entry.filename);
                if (heightmap_loaded) break;
            }
        }
        if (!heightmap_loaded) {
            LOGI("No .scn height-map available — using procedural terrain");
            generate_terrain();
        }
    }
    if (skydome_mesh_ == INVALID_MESH) generate_skydome();

    create_in_game_hud();

    const auto setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - setup_start)
                              .count();
    LOGI("start_game: %zu units spawned, Town Centers established, anim clips=%zu (setup: %lld ms)",
         spawn_positions.size(), anim_system_->clip_count(), static_cast<long long>(setup_ms));
}

void Engine::spawn_particle(const glm::vec3& pos, const glm::vec3& vel, float life, uint32_t color, float size) {
    if (particles_.size() >= 1000) return;
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.life = 0.0f;
    p.max_life = (life > 0.05f) ? life : 0.6f;
    p.color = color;
    p.size = size;
    particles_.push_back(p);
}

void Engine::spawn_construction_sparks(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 1.0f + static_cast<float>(rand() % 100) * 0.02f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 1.5f + static_cast<float>(rand() % 100) * 0.025f;
        uint32_t colors[] = {0xFFFFEE44, 0xFFFFCC00, 0xFFFF9900, 0xFFEEDD88};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos + glm::vec3(vx * 0.2f, 0.2f, vz * 0.2f),
                       glm::vec3(vx, vy, vz),
                       0.5f + static_cast<float>(rand() % 50) * 0.01f,
                       col, 0.12f);
    }
}

void Engine::spawn_combat_hit(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float theta = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float phi = static_cast<float>(rand() % 180 - 90) * (3.14159f / 180.0f);
        float speed = 2.0f + static_cast<float>(rand() % 100) * 0.03f;
        float vx = std::cos(phi) * std::cos(theta) * speed;
        float vy = std::abs(std::sin(phi)) * speed + 0.8f;
        float vz = std::cos(phi) * std::sin(theta) * speed;
        uint32_t colors[] = {0xFFFFFFFF, 0xFFFFDD33, 0xFFFF6622, 0xFFFF2222};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos, glm::vec3(vx, vy, vz),
                       0.35f + static_cast<float>(rand() % 40) * 0.01f,
                       col, 0.14f);
    }
}

void Engine::spawn_harvest_chips(ResourceType type, const glm::vec3& pos, int count) {
    uint32_t palette[3] = {0xFFFFD700, 0xFFFFE066, 0xFFFFF0AA};
    if (type == ResourceType::WOOD) {
        palette[0] = 0xFF8B5A2B; palette[1] = 0xFFCD853F; palette[2] = 0xFFD2B48C;
    } else if (type == ResourceType::FOOD) {
        palette[0] = 0xFF66BB33; palette[1] = 0xFF88DD44; palette[2] = 0xFFCCEE55;
    }

    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 0.8f + static_cast<float>(rand() % 100) * 0.015f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 1.2f + static_cast<float>(rand() % 100) * 0.02f;
        spawn_particle(pos, glm::vec3(vx, vy, vz),
                       0.6f + static_cast<float>(rand() % 40) * 0.01f,
                       palette[rand() % 3], 0.10f);
    }
}

void Engine::spawn_death_smoke(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 0.4f + static_cast<float>(rand() % 60) * 0.01f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 0.8f + static_cast<float>(rand() % 80) * 0.015f;
        uint32_t colors[] = {0xAAEEEEFF, 0x88CCCCCC, 0x7799BBAA, 0x55888888};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos + glm::vec3(0.0f, 0.2f, 0.0f),
                       glm::vec3(vx, vy, vz),
                       1.2f + static_cast<float>(rand() % 60) * 0.01f,
                       col, 0.20f);
    }
}

void Engine::update_particles(float dt) {
    for (auto& p : particles_) {
        p.life += dt;
        p.pos += p.vel * dt;
        p.vel.y -= 9.8f * dt; // Gravity
        if (p.pos.y < 0.02f) {
            p.pos.y = 0.02f;
            p.vel.y = -p.vel.y * 0.35f; // Ground bounce
            p.vel.x *= 0.75f;           // Surface friction
            p.vel.z *= 0.75f;
        }
    }

    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& p) { return p.life >= p.max_life; }),
        particles_.end());
}

void Engine::render_particles() {
    if (!renderer_ || particles_.empty()) return;
    for (const auto& p : particles_) {
        float progress = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        uint32_t alpha = static_cast<uint32_t>((1.0f - progress) * 255.0f);
        if (alpha == 0) continue;
        uint32_t col = (p.color & 0x00FFFFFF) | (alpha << 24);
        glm::vec3 tail = p.pos - p.vel * 0.04f;
        renderer_->draw_debug_line(tail, p.pos, col);
    }
}

void Engine::open_options() {
    set_state(EngineState::OPTIONS);
}

bool Engine::request_game_data_import() {
#ifdef __ANDROID__
    auto* activity = static_cast<ANativeActivity*>(native_activity_);
    if (!activity || !activity->vm || !activity->clazz) {
        LOGW("Data import requested without a NativeActivity/JVM");
        return false;
    }

    JNIEnv* env = nullptr;
    bool attached_here = false;
    const jint env_status = activity->vm->GetEnv(
        reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (env_status == JNI_EDETACHED) {
        if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
            LOGE("Failed to attach the game thread to the JVM");
            return false;
        }
        attached_here = true;
    } else if (env_status != JNI_OK || !env) {
        LOGE("Failed to obtain JNIEnv for data import");
        return false;
    }

    bool ok = false;
    jclass activity_class = env->GetObjectClass(activity->clazz);
    if (activity_class) {
        jmethodID import_method = env->GetMethodID(
            activity_class, "requestGameDataImport", "()V");
        if (import_method) {
            env->CallVoidMethod(activity->clazz, import_method);
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
            } else {
                ok = true;
            }
        } else {
            LOGE("Java method MyNativeActivity.requestGameDataImport() not found");
            env->ExceptionClear();
        }
        env->DeleteLocalRef(activity_class);
    }

    if (attached_here) {
        activity->vm->DetachCurrentThread();
    }
    LOGI("Data import picker requested: %s", ok ? "ok" : "failed");
    return ok;
#else
    LOGW("Data import is only available on Android");
    return false;
#endif
}

void Engine::open_credits() {
    set_state(EngineState::CREDITS);
}

void Engine::return_to_main_menu() {
    save_options();
    set_state(EngineState::MAIN_MENU);
}

void Engine::quit_app() {
    LOGI("Quitting app...");
    shutdown();
    std::exit(0);
}

void Engine::clear_ui() {
    if (ui_) ui_->clear();
    fps_label_ = nullptr;
}

void Engine::load_options() {
    std::string path = config_.data_path + "/options.cfg";
    std::ifstream in(path);
    if (!in) {
        in.open("/storage/emulated/0/Documents/AoM-Native-Port/options.cfg");
    }
    if (!in) return;

    std::string key;
    float val;
    while (in >> key >> val) {
        if (key == "master_volume") master_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "music_volume") music_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "sfx_volume") sfx_volume_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "graphics_quality") graphics_quality_ = std::clamp(val, 0.0f, 1.0f);
        else if (key == "show_fps") show_fps_ = (val > 0.5f);
        else if (key == "music_enabled") music_enabled_ = (val > 0.5f);
        else if (key == "sfx_enabled") sfx_enabled_ = (val > 0.5f);
    }
    LOGI("Loaded options: master=%.2f, music=%.2f, sfx=%.2f", master_volume_, music_volume_, sfx_volume_);
}

void Engine::save_options() {
    std::string path = config_.data_path + "/options.cfg";
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        out.open("/storage/emulated/0/Documents/AoM-Native-Port/options.cfg", std::ios::trunc);
    }
    if (!out) return;

    out << "master_volume " << master_volume_ << "\n";
    out << "music_volume " << music_volume_ << "\n";
    out << "sfx_volume " << sfx_volume_ << "\n";
    out << "graphics_quality " << graphics_quality_ << "\n";
    out << "show_fps " << (show_fps_ ? 1.0f : 0.0f) << "\n";
    out << "music_enabled " << (music_enabled_ ? 1.0f : 0.0f) << "\n";
    out << "sfx_enabled " << (sfx_enabled_ ? 1.0f : 0.0f) << "\n";
}

void Engine::shutdown() {
    if (audio_) audio_->shutdown();
    audio_.reset();
    if (renderer_) renderer_->shutdown();
    renderer_.reset();
    terrain_mesh_ = INVALID_MESH;
    terrain_texture_ = INVALID_TEXTURE;
    terrain_blend_tex_ = INVALID_TEXTURE;
    terrain_bank_tex_ = INVALID_TEXTURE;
    terrain_outer_bank_tex_ = INVALID_TEXTURE;
    water_normal_tex_ = INVALID_TEXTURE;
    skydome_mesh_ = INVALID_MESH;
    model_mesh_ = INVALID_MESH;
    ui_.reset();
    state_ = EngineState::SHUTTING_DOWN;
}

void Engine::update(float delta_time) {
    accumulated_time_ += delta_time;
    state_timer_ += delta_time;

    // State Machine Transitions
    if (state_ == EngineState::CINEMATIC) {
        // Avança o decoder real (entrega frames via on_frame; on_eos avança a fila)
        if (media_decoder_ && media_decoder_->is_playing()) {
            media_decoder_->update(delta_time);
        } else if (video_player_) {
            // Fallback: sem decoder ativo, usa o temporizador do VideoPlayer
            video_player_->update(delta_time);
        }
        // Skip solicitado pelo usuário ou timeout sem frames decodificados → vai ao menu principal
        if (state_ == EngineState::CINEMATIC && (cinematic_skip_requested_ || (!cinematic_new_frame_ && state_timer_ > 2.5f))) {
            if (media_decoder_) media_decoder_->stop();
            if (video_player_)  video_player_->stop();
            LOGI("Cinematic ended or skipped (timer=%.2fs) -> transitioning to MAIN_MENU", state_timer_);
            set_state(EngineState::MAIN_MENU);
        }
    } else if (state_ == EngineState::SPLASH) {
        if (state_timer_ >= 2.5f) {
            set_state(EngineState::LOADING);
        }
    } else if (state_ == EngineState::LOADING) {
        loading_progress_ = std::min(1.0f, loading_progress_ + delta_time * 0.75f);
        if (loading_progress_ >= 1.0f && state_timer_ >= 1.8f) {
            set_state(EngineState::MAIN_MENU);
        }
    } else if (state_ == EngineState::IN_GAME) {
        if (mission_mgr_) {
            mission_mgr_->update(delta_time, gameplay_world_.get(), &camera_);
            if (!mission_mgr_->is_cinematic_active()) {
                // Orbit camera calculations centered at camera_.target
                const float cx = camera_.target.x + std::sin(yaw_) * std::cos(pitch_) * radius_;
                const float cy = camera_.target.y + std::sin(pitch_) * radius_ + 2.0f;
                const float cz = camera_.target.z + std::cos(yaw_) * std::cos(pitch_) * radius_;
                camera_.position = glm::vec3(cx, cy, cz);
            }
        } else {
            // Orbit camera calculations centered at camera_.target
            const float cx = camera_.target.x + std::sin(yaw_) * std::cos(pitch_) * radius_;
            const float cy = camera_.target.y + std::sin(pitch_) * radius_ + 2.0f;
            const float cz = camera_.target.z + std::cos(yaw_) * std::cos(pitch_) * radius_;
            camera_.position = glm::vec3(cx, cy, cz);
        }

        // Update gameplay world (movement, combat, fog, animation morph+upload)
        if (gameplay_world_) gameplay_world_->update(delta_time);

        // Update 3D interactive particles
        update_particles(delta_time);

        // P3.5: atualizar partículas .prt
        if (prt_particles_) {
            prt_particles_->update(delta_time);
        }

        // Update real-time in-game HUD (resources, population, selection)
        update_in_game_hud(delta_time);

        // Update 3D positional audio listener from camera orientation
        if (audio_) {
            glm::vec3 diff = camera_.target - camera_.position;
            glm::vec3 forward = (glm::length(diff) > 0.001f) ? glm::normalize(diff) : glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            audio_->set_listener(camera_.target, forward, right);
        }

        // Verificacao de condicoes de vitoria (Skirmish / Mapa Aleatorio)
        if (!mission_mgr_ && gameplay_world_ && skirmish_victory_result_ == VictoryResult::NONE) {
            skirmish_victory_result_ = check_victory(*gameplay_world_, skirmish_victory_condition_);
            if (skirmish_victory_result_ != VictoryResult::NONE) {
                victory_display_timer_ = 0.0f;
                if (hud_sel_title_) {
                    if (skirmish_victory_result_ == VictoryResult::WIN) {
                        hud_sel_title_->text  = "VITORIA! O IMPERIO INIMIGO FOI DERROTADO!";
                        hud_sel_title_->color = 0xFF76FF03;
                    } else {
                        hud_sel_title_->text  = "DERROTA! SEU CENTRO DA CIDADE FOI DESTRUIDO!";
                        hud_sel_title_->color = 0xFFFF3333;
                    }
                }
            }
        }

        // Exibir botoes de fim de jogo apos 2.5s de vitoria/derrota
        if (skirmish_victory_result_ != VictoryResult::NONE && gameplay_world_) {
            victory_display_timer_ += delta_time;
            if (victory_display_timer_ >= 2.5f && victory_display_timer_ < 2.5f + delta_time) {
                const float vw = static_cast<float>(config_.window_width);
                const float vh = static_cast<float>(config_.window_height);
                float btn_w = 220.0f, btn_h = 48.0f;
                float cy    = vh * 0.58f;
                ui_->add_button({(vw - btn_w) * 0.5f - 120.0f, cy}, {btn_w, btn_h}, "MENU PRINCIPAL",
                               [this]() {
                                   skirmish_victory_result_ = VictoryResult::NONE;
                                   victory_display_timer_   = 0.0f;
                                   return_to_main_menu();
                               });
                ui_->add_button({(vw - btn_w) * 0.5f + 120.0f, cy}, {btn_w, btn_h}, "JOGAR NOVAMENTE",
                               [this]() {
                                   skirmish_victory_result_ = VictoryResult::NONE;
                                   victory_display_timer_   = 0.0f;
                                   start_game();
                               });
            }
        }
    }

    // Atualizar subsistema de rede multiplayer
    net::NetworkManager::instance().update(delta_time);

    // Update UI elements
    if (ui_) ui_->update(delta_time);

    // Update FPS calculation
    if (fps_label_ && show_fps_) {
        float instant_fps = (delta_time > 0.0001f) ? (1.0f / delta_time) : 60.0f;
        current_fps_ = current_fps_ * 0.9f + instant_fps * 0.1f;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "FPS: %.0f", current_fps_);
        fps_label_->text = buf;
    }
}

void Engine::render() {
    if (!renderer_) return;

    renderer_->begin_frame();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    if (state_ == EngineState::CINEMATIC) {
        // Fundo preto atrás do vídeo (letterbox)
        renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xFF000000);
        if (cinematic_frame_tex_ != INVALID_TEXTURE) {
            // Centralizar o frame mantendo o aspect ratio REAL do decoder
            float src_w = 640.0f, src_h = 480.0f;
            if (media_decoder_ && media_decoder_->video_width() > 0 &&
                media_decoder_->video_height() > 0) {
                src_w = static_cast<float>(media_decoder_->video_width());
                src_h = static_cast<float>(media_decoder_->video_height());
            }
            const float aspect_src = src_w / src_h;
            const float aspect_dst = vw / vh;
            float fw = vw, fh = vh;
            if (aspect_dst > aspect_src) { fw = vh * aspect_src; }
            else                         { fh = vw / aspect_src; }
            renderer_->draw_ui_quad({(vw - fw) * 0.5f, (vh - fh) * 0.5f},
                                    {fw, fh}, cinematic_frame_tex_, 0xFFFFFFFF);
        }
        // Barra de progresso: decoder real com fallback para o temporizador
        float prog = (media_decoder_) ? media_decoder_->progress() : 0.0f;
        if (prog <= 0.0f && video_player_) prog = video_player_->progress();
        const float bar_w = vw * 0.4f;
        renderer_->draw_ui_quad({(vw - bar_w) * 0.5f, vh - 8.0f},
                                {bar_w * prog, 4.0f}, INVALID_TEXTURE, 0x66FFFFFF);
        if (state_timer_ > 2.0f) {
            renderer_->draw_ui_text("Tap to skip", {vw - 160.0f, vh - 32.0f}, 16.0f, 0x66FFFFFF);
        }
    } else if (state_ == EngineState::SPLASH) {
        // Draw Splash Art fullscreen
        if (splash_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, splash_tex_, 0xFFFFFFFF);
        } else {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xFF111111);
            renderer_->draw_ui_text("ENSEMBLE STUDIOS", {vw * 0.5f - 140.0f, vh * 0.45f}, 28.0f, 0xFFFFD000);
        }
        renderer_->draw_ui_text("Toque na tela para pular...", {vw * 0.5f - 110.0f, vh - 50.0f}, 16.0f, 0x88FFFFFF);
    } else if (state_ == EngineState::LOADING) {
        // Draw Loading Temple Art fullscreen
        if (loading_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, loading_tex_, 0xFFFFFFFF);
        } else if (splash_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, splash_tex_, 0xFFFFFFFF);
        } else {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xFF1A1208);
        }

        // Ornate AoM Golden Progress Bar
        const float bar_w = vw * 0.60f;
        const float bar_h = 16.0f;
        const float bar_x = (vw - bar_w) * 0.5f;
        const float bar_y = vh * 0.88f;

        // Background stone track
        renderer_->draw_ui_quad({bar_x - 3.0f, bar_y - 3.0f}, {bar_w + 6.0f, bar_h + 6.0f}, INVALID_TEXTURE, 0xFF000000);
        renderer_->draw_ui_quad({bar_x, bar_y}, {bar_w, bar_h}, INVALID_TEXTURE, 0xFF332211);
        // Golden fill
        renderer_->draw_ui_quad({bar_x, bar_y}, {bar_w * loading_progress_, bar_h}, INVALID_TEXTURE, 0xFFFFD000);

        char prog_buf[64];
        std::snprintf(prog_buf, sizeof(prog_buf), "Carregando a Era dos Deuses... %.0f%%", loading_progress_ * 100.0f);
        renderer_->draw_ui_text(prog_buf, {bar_x, bar_y - 28.0f}, 18.0f, 0xFFFFD000);
    } else if (state_ == EngineState::MAIN_MENU) {
        // ── Background fullscreen (arte autêntica Age of Mythology: The Titans) ──
        if (main_menu_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, main_menu_tex_, 0xFFFFFFFF);
        } else if (loading_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, loading_tex_, 0xFFFFFFFF);
        } else if (splash_tex_ != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, splash_tex_, 0xFFFFFFFF);
        } else {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xFF0A0806);
        }

        // ── Botões e labels (feedback de toque e elementos interativos) ───────
        if (ui_) ui_->render();
    } else if (state_ == EngineState::OPTIONS || state_ == EngineState::CREDITS ||
               state_ == EngineState::MULTIPLAYER_LAN || state_ == EngineState::MULTIPLAYER_LOBBY) {
        // Background with darkened stone modal
        TextureHandle bg = (main_menu_tex_ != INVALID_TEXTURE) ? main_menu_tex_ : loading_tex_;
        if (bg != INVALID_TEXTURE) {
            renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, bg, 0xFFFFFFFF);
        }
        renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, INVALID_TEXTURE, 0xCC110802);

        // Render UI
        if (ui_) ui_->render();
    } else if (state_ == EngineState::IN_GAME) {
        // 3D Scene Rendering
        renderer_->set_camera(camera_);
        renderer_->set_light(dir_light_);

        // 1. Celestial Skybox (P3.2: 6 faces do cube map) ou Skydome procedural (fallback)
        if (skybox_mesh_ != INVALID_MESH) {
            glm::mat4 sky_transform = glm::translate(glm::mat4(1.0f), camera_.target);
            renderer_->draw_skybox(skybox_faces_, 65.0f);
        } else if (skydome_mesh_ != INVALID_MESH) {
            glm::mat4 sky_transform = glm::translate(glm::mat4(1.0f), camera_.target);
            renderer_->draw_mesh(skydome_mesh_, sky_transform, skydome_texture_, BlendMode::OPAQUE);
        }

        // 2. Authentic AoM 3D Terrain & Water Plane
        if (terrain_mesh_ != INVALID_MESH) {
            renderer_->draw_terrain(terrain_mesh_, terrain_texture_, terrain_blend_tex_,
                                 terrain_bank_tex_, terrain_outer_bank_tex_);
        }
        renderer_->draw_water(water_plane_, water_normal_tex_, accumulated_time_, water_color_);

        if (model_mesh_ != INVALID_MESH) {
            glm::mat4 model_transform = glm::mat4(1.0f);
            float angle = static_cast<float>(frame_count_) * 0.015f;
            model_transform = glm::rotate(model_transform, angle, glm::vec3(0, 1, 0));
            renderer_->draw_mesh(model_mesh_, model_transform, model_texture_, BlendMode::OPAQUE);
        }

        // 3. RTS Entities, Buildings, Health Bars, Selection Rings, Tactical Vectors
        if (gameplay_world_) {
            gameplay_world_->render(renderer_.get(), model_mesh_, model_texture_, hoplite_mesh_, hoplite_texture_);
            gameplay_world_->render_animated(renderer_.get());
        }

        // Render 3D particles (combat hits, sparks, harvesting chips, death smoke)
        render_particles();

        // P3.5: renderizar partículas .prt (cruz de debug visível sem textura)
        if (prt_particles_) {
            prt_particles_->render(renderer_.get(), prt_particle_tex_);
        }

        // 4. In-Game HUD: Top Banner background & Golden divider
        renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, 36.0f}, INVALID_TEXTURE, 0xEE120A04);
        renderer_->draw_ui_quad({0.0f, 36.0f}, {vw, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);

        // 5. In-Game HUD: Bottom Selection Panel background & Golden divider
        const float panel_h = 96.0f;
        const float panel_y = vh - panel_h;
        renderer_->draw_ui_quad({0.0f, panel_y - 2.0f}, {vw, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({0.0f, panel_y}, {vw, panel_h}, INVALID_TEXTURE, 0xEE140C06);

        // 6. Minimap Radar at bottom-left corner
        const float mm_w = 130.0f;
        const float mm_h = 130.0f;
        const float mm_x = 16.0f;
        const float mm_y = vh - mm_h - 12.0f;

        renderer_->draw_ui_quad({mm_x - 2.0f, mm_y - 2.0f}, {mm_w + 4.0f, mm_h + 4.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({mm_x, mm_y}, {mm_w, mm_h}, INVALID_TEXTURE, 0xEE12180A);

        if (gameplay_world_) {
            auto blips = gameplay_world_->get_minimap_blips();
            for (const auto& blip : blips) {
                float nx = (blip.position.x + 20.0f) / 40.0f;
                float ny = (blip.position.z + 20.0f) / 40.0f;
                if (nx >= 0.0f && nx <= 1.0f && ny >= 0.0f && ny <= 1.0f) {
                    float bx = mm_x + nx * mm_w - blip.size * 0.5f;
                    float by = mm_y + ny * mm_h - blip.size * 0.5f;
                    renderer_->draw_ui_quad({bx, by}, {blip.size, blip.size}, INVALID_TEXTURE, blip.color);
                }
            }
        }

        // 7. Box Selection Overlay (translucent rectangle during drag-select)
        if (box_selecting_) {
            float bx = std::min(box_start_.x, box_current_.x);
            float by = std::min(box_start_.y, box_current_.y);
            float bw = std::abs(box_current_.x - box_start_.x);
            float bh = std::abs(box_current_.y - box_start_.y);
            if (bw > 2.0f && bh > 2.0f) {
                // Translucent fill
                renderer_->draw_ui_quad({bx, by}, {bw, bh}, INVALID_TEXTURE, 0x440000FF);
                // Golden border (top)
                renderer_->draw_ui_quad({bx, by}, {bw, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
                // Golden border (bottom)
                renderer_->draw_ui_quad({bx, by + bh - 2.0f}, {bw, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
                // Golden border (left)
                renderer_->draw_ui_quad({bx, by}, {2.0f, bh}, INVALID_TEXTURE, 0xFFFFD000);
                // Golden border (right)
                renderer_->draw_ui_quad({bx + bw - 2.0f, by}, {2.0f, bh}, INVALID_TEXTURE, 0xFFFFD000);
            }
        }

        // 8. Render HUD UI labels, buttons, and action commands
        if (ui_) ui_->render();
    }

    renderer_->end_frame();
    ++frame_count_;
}

void Engine::handle_touch(const TouchEvent& touch) {
    // Cinematic: qualquer toque pula diretamente para o menu
    if (state_ == EngineState::CINEMATIC) {
        if (touch.state == TouchEvent::State::DOWN) {
            cinematic_skip_requested_ = true;
        }
        return;
    }

    if (state_ == EngineState::SPLASH) {
        if (touch.state == TouchEvent::State::DOWN) {
            set_state(EngineState::LOADING);
        }
        return;
    }

    // Forward to UI first
    if (ui_ && ui_->handle_touch(touch)) {
        return;
    }

    // In-game touch handling (Unit selection, movement orders, placement, camera pan)
    if (state_ == EngineState::IN_GAME) {
        const float vw = static_cast<float>(config_.window_width);
        const float vh = static_cast<float>(config_.window_height);

        // Ignore 3D world raycast if tapping inside HUD top banner or bottom selection panel
        if (touch.y <= 38.0f || touch.y >= vh - 96.0f) {
            return;
        }

        // Raycast touch coordinates to 3D ground plane (y = 0)
        float ndc_x = (2.0f * touch.x) / vw - 1.0f;
        float ndc_y = 1.0f - (2.0f * touch.y) / vh;
        glm::mat4 inv_vp = glm::inverse(camera_.projection_matrix() * camera_.view_matrix());
        glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
        near_pt /= (std::abs(near_pt.w) > 0.0001f ? near_pt.w : 1.0f);
        glm::vec4 far_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
        far_pt /= (std::abs(far_pt.w) > 0.0001f ? far_pt.w : 1.0f);

        glm::vec3 ray_origin = glm::vec3(near_pt);
        glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt - near_pt));

        glm::vec3 ground_hit{0.0f};
        bool hit_ground = false;
        if (std::abs(ray_dir.y) > 0.0001f) {
            float t = -ray_origin.y / ray_dir.y;
            if (t > 0.0f) {
                ground_hit = ray_origin + ray_dir * t;
                hit_ground = true;
            }
        }

        if (touch.state == TouchEvent::State::DOWN) {
            dragging_ = false;
            box_selecting_ = false;
            box_start_ = {touch.x, touch.y};
            box_current_ = {touch.x, touch.y};
            last_touch_ = {touch.x, touch.y};
            touch_down_pos_ = {touch.x, touch.y};
        } else if (touch.state == TouchEvent::State::MOVE) {
            float dx = touch.x - last_touch_.x;
            float dy = touch.y - last_touch_.y;
            float total_dx = touch.x - touch_down_pos_.x;
            float total_dy = touch.y - touch_down_pos_.y;
            float dist = std::sqrt(total_dx * total_dx + total_dy * total_dy);
            box_current_ = {touch.x, touch.y};

            if (dist > 14.0f) {
                dragging_ = true;
                box_selecting_ = true;
            }

            if (dragging_ && box_selecting_) {
                // Box selection: update the second corner of the selection rectangle
                // (no camera pan while box-selecting)
                last_touch_ = {touch.x, touch.y};
            } else if (dragging_) {
                // Pan RTS camera along ground plane
                camera_.target.x -= dx * 0.02f;
                camera_.target.z -= dy * 0.02f;
                camera_.target.x = std::clamp(camera_.target.x, -20.0f, 20.0f);
                camera_.target.z = std::clamp(camera_.target.z, -20.0f, 20.0f);
                last_touch_ = {touch.x, touch.y};
            }
        } else if (touch.state == TouchEvent::State::UP) {
            if (box_selecting_ && hit_ground && gameplay_world_) {
                // Box selection complete: convert screen corners to world coordinates
                // and select all friendly units inside the rectangle.
                auto raycast_ground = [&](float sx, float sy) -> glm::vec3 {
                    float ndc_x = (2.0f * sx) / vw - 1.0f;
                    float ndc_y = 1.0f - (2.0f * sy) / vh;
                    glm::mat4 inv_vp = glm::inverse(camera_.projection_matrix() * camera_.view_matrix());
                    glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
                    near_pt /= (std::abs(near_pt.w) > 0.0001f ? near_pt.w : 1.0f);
                    glm::vec4 far_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);
                    far_pt /= (std::abs(far_pt.w) > 0.0001f ? far_pt.w : 1.0f);
                    glm::vec3 ro = glm::vec3(near_pt);
                    glm::vec3 rd = glm::normalize(glm::vec3(far_pt - near_pt));
                    if (std::abs(rd.y) > 0.0001f) {
                        float t = -ro.y / rd.y;
                        if (t > 0.0f) return ro + rd * t;
                    }
                    return glm::vec3(0.0f);
                };

                glm::vec3 p1 = raycast_ground(box_start_.x, box_start_.y);
                glm::vec3 p2 = raycast_ground(box_current_.x, box_current_.y);
                gameplay_world_->select_box(p1, p2);
                LOGI("Box selection: %zu units selected", gameplay_world_->selected_count());
                box_selecting_ = false;
                dragging_ = false;
            } else if (!dragging_ && hit_ground && gameplay_world_) {
                if (placement_mode_ != PlacementMode::NONE) {
                    Entity sel = gameplay_world_->selected_entity();
                    if (placement_mode_ == PlacementMode::HOUSE) {
                        if (sel != INVALID_ENTITY) {
                            gameplay_world_->order_worker_build(sel, BuildingType::HOUSE, ground_hit);
                        } else {
                            if (gameplay_world_->deduct_resources(0, 100, 0, 0)) {
                                gameplay_world_->place_building(Team::FRIENDLY, ground_hit, 5.0f, BuildingType::HOUSE, "Casa");
                            }
                        }
                    } else if (placement_mode_ == PlacementMode::BARRACKS) {
                        if (sel != INVALID_ENTITY) {
                            gameplay_world_->order_worker_build(sel, BuildingType::BARRACKS, ground_hit);
                        } else {
                            if (gameplay_world_->deduct_resources(0, 100, 50, 0)) {
                                gameplay_world_->place_building(Team::FRIENDLY, ground_hit, 8.0f, BuildingType::BARRACKS, "Quartel Militar");
                            }
                        }
                    }
                    placement_mode_ = PlacementMode::NONE;
                } else {
                    // RTS Command Dispatch
                    Entity enemy = gameplay_world_->enemy_at(ground_hit, 2.2f);
                    if (enemy != INVALID_ENTITY && gameplay_world_->selected_count() > 0) {
                        gameplay_world_->order_attack_selected(enemy);
                        LOGI("Ordered attack on enemy %u", enemy);
                        if (audio_ && !gameplay_world_->selected_entities().empty()) {
                            Entity fsel = *gameplay_world_->selected_entities().begin();
                            const auto* u = gameplay_world_->get_unit(fsel);
                            const auto* t = gameplay_world_->get_transform(fsel);
                            glm::vec3 pos = t ? t->position : ground_hit;
                            std::string uname = u ? u->unit_type_name : "greek";
                            audio_->play_unit_voice_3d("attack", pos, uname, u ? u->worker : false);
                        }
                    } else {
                        // Check if a worker is selected and tapped a resource or construction site
                        bool worker_ordered = false;
                        for (Entity e : gameplay_world_->selected_entities()) {
                            const auto* u = gameplay_world_->get_unit(e);
                            if (u && u->worker) {
                                if (gameplay_world_->order_gather(e, ground_hit)) {
                                    LOGI("Worker %u ordered to gather resource", e);
                                    if (audio_) {
                                        const auto* t = gameplay_world_->get_transform(e);
                                        glm::vec3 pos = t ? t->position : ground_hit;
                                        audio_->play_unit_voice_3d("move", pos, "greek", true);
                                    }
                                    worker_ordered = true;
                                    break;
                                }
                                Entity bld = gameplay_world_->building_at(ground_hit, 2.5f);
                                if (bld != INVALID_ENTITY) {
                                    const auto* b_comp = gameplay_world_->get_building(bld);
                                    if (b_comp && !b_comp->complete) {
                                        gameplay_world_->order_build(e, bld);
                                        LOGI("Worker %u ordered to construct building %u", e, bld);
                                        if (audio_) {
                                            const auto* t = gameplay_world_->get_transform(e);
                                            glm::vec3 pos = t ? t->position : ground_hit;
                                            audio_->play_unit_voice_3d("move", pos, "greek", true);
                                        }
                                        worker_ordered = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!worker_ordered) {
                            // Try selecting friendly unit or building
                            Entity tapped = gameplay_world_->select_at(ground_hit);
                            if (tapped != INVALID_ENTITY) {
                                LOGI("Selected entity %u at (%.1f, %.1f)", tapped, ground_hit.x, ground_hit.z);
                                if (audio_) {
                                    const auto* u = gameplay_world_->get_unit(tapped);
                                    const auto* t = gameplay_world_->get_transform(tapped);
                                    glm::vec3 pos = t ? t->position : ground_hit;
                                    if (u && u->team == Team::FRIENDLY) {
                                        audio_->play_unit_voice_3d("select", pos, u->unit_type_name, u->worker);
                                    }
                                }
                            } else if (gameplay_world_->selected_count() > 0) {
                                // Move order on empty ground
                                gameplay_world_->order_move_selected(ground_hit);
                                LOGI("Ordered move to (%.1f, %.1f)", ground_hit.x, ground_hit.z);
                                if (audio_) {
                                    Entity fsel = *gameplay_world_->selected_entities().begin();
                                    const auto* u = gameplay_world_->get_unit(fsel);
                                    const auto* t = gameplay_world_->get_transform(fsel);
                                    glm::vec3 pos = t ? t->position : ground_hit;
                                    std::string uname = u ? u->unit_type_name : "greek";
                                    audio_->play_unit_voice_3d("move", pos, uname, u ? u->worker : false);
                                }
                            }
                        }
                    }
                }
            }
            dragging_ = false;
        }
    }
}

void Engine::adjust_camera_zoom(float delta) {
    radius_ = std::clamp(radius_ - delta, 3.0f, 40.0f);
}

void Engine::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == config_.window_width && height == config_.window_height) return;

    if (renderer_) renderer_->resize(width, height);
    config_.window_width = width;
    config_.window_height = height;
    camera_.aspect = static_cast<float>(width) / static_cast<float>(height);

    if (ui_) ui_->initialize(renderer_.get(), width, height);

    // Recria a UI do estado atual para manter o layout centralizado e correto
    // na nova resolução (evita botões cortados/deslocados após rotação).
    switch (state_) {
        case EngineState::MAIN_MENU:
            create_main_menu();
            break;
        case EngineState::OPTIONS:
            create_options_menu();
            break;
        case EngineState::CREDITS:
            create_credits_menu();
            break;
        case EngineState::MULTIPLAYER_LAN:
            create_multiplayer_lan_menu();
            break;
        case EngineState::MULTIPLAYER_LOBBY:
            create_multiplayer_lobby_menu();
            break;
        case EngineState::IN_GAME: {
            create_in_game_hud();
            break;
        }
        default:
            break;
    }
}

bool Engine::recreate_renderer(void* native_window) {
    if (!renderer_) return init_renderer(config_, native_window);
    renderer_->shutdown();
    if (!renderer_->initialize(native_window, config_.window_width, config_.window_height)) {
        LOGE("Failed to recreate renderer");
        return false;
    }
    load_core_ui_textures();
    return true;
}

void Engine::release_renderer() {
    if (renderer_) renderer_->shutdown();
}

bool Engine::init_renderer(const EngineConfig& config, void* native_window) {
    renderer_ = create_gles_renderer();
    if (!renderer_) return false;
    renderer_->set_asset_manager(asset_manager_);
    return renderer_->initialize(native_window, config.window_width, config.window_height);
}

bool Engine::load_archives(const std::string& data_path) {
    namespace fs = std::filesystem;
    const fs::path base(data_path);

    data_bar_ = std::make_unique<BarArchive>();
    textures_bar_ = std::make_unique<BarArchive>();
    models_bar_ = std::make_unique<BarArchive>();
    scenarios_bar_ = std::make_unique<BarArchive>();
    sound_bar_ = std::make_unique<BarArchive>();
    anim_bar_ = std::make_unique<BarArchive>();
    godpowers_bar_ = std::make_unique<BarArchive>();

    auto find_archive = [](const fs::path& base, const std::string& sub, const std::string& name) -> fs::path {
        std::error_code ec;
        fs::path p1 = base / sub / name;
        if (fs::exists(p1, ec)) return p1;
        fs::path p2 = base / name;
        if (fs::exists(p2, ec)) return p2;
        return p1;
    };

    // Abre o arquivo base e monta a expansão The Titans (`*2.bar`) por cima.
    // O overlay sobrescreve as entradas homônimas e adiciona as novas, que é
    // exatamente como o jogo original sobrepõe a expansão ao jogo base.
    auto open_pair = [](BarArchive& bar, const fs::path& base_file, const fs::path& expansion_file) -> bool {
        std::error_code ec;
        bool ok = false;
        if (fs::exists(base_file, ec)) {
            ok = bar.open(base_file.string());
        }
        if (fs::exists(expansion_file, ec)) {
            if (bar.mount(expansion_file.string())) {
                ok = true;
            }
        }
        return ok;
    };

    open_pair(*data_bar_,
              find_archive(base, "data", "data.bar"),
              find_archive(base, "data", "data2.bar"));
    open_pair(*textures_bar_,
              find_archive(base, "textures", "textures.bar"),
              find_archive(base, "textures", "textures2.bar"));
    open_pair(*models_bar_,
              find_archive(base, "models", "models.bar"),
              find_archive(base, "models", "models2.bar"));
    open_pair(*scenarios_bar_,
              find_archive(base, "scenario", "scenarios.bar"),
              find_archive(base, "scenario", "scenarios2.bar"));
    open_pair(*sound_bar_,
              find_archive(base, "sound", "sounds.bar"),
              find_archive(base, "sound", "sounds2.bar"));
    open_pair(*anim_bar_,
              find_archive(base, "anim", "anim.bar"),
              find_archive(base, "anim", "anim2.bar"));
    open_pair(*godpowers_bar_,
              find_archive(base, "god powers", "godpowers.bar"),
              find_archive(base, "god powers", "godpowers2.bar"));

    if (audio_ && sound_bar_->is_open()) {
        audio_->set_sound_archive(sound_bar_.get());
    }

    // Poderes dos deuses: base e expansão são lidos separadamente para que
    // cada poder saiba se é exclusivo dos Titãs.
    {
        const fs::path gp_base = find_archive(base, "god powers", "godpowers.bar");
        const fs::path gp_exp = find_archive(base, "god powers", "godpowers2.bar");
        std::error_code ec;
        if (fs::exists(gp_base, ec)) {
            BarArchive gp;
            if (gp.open(gp_base.string())) {
                god_powers_.load_from_bar(&gp, false);
            }
        }
        if (fs::exists(gp_exp, ec)) {
            BarArchive gp;
            if (gp.open(gp_exp.string())) {
                god_powers_.load_expansion_from_bar(&gp);
            }
        }
    }

    // Initialize CampaignManager and load campaign XML
    campaign_mgr_ = std::make_unique<CampaignManager>();

    // A instalação do usuário tem prioridade sobre o asset do APK.
    bool base_campaign_loaded = false;
    const std::string cpn_candidates[] = {
        data_path + "/data/main campaign.cpn",
        data_path + "/main campaign.cpn",
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data/main campaign.cpn"
    };
    for (const auto& c : cpn_candidates) {
        if (campaign_mgr_->load_campaign(c)) {
            LOGI("Loaded campaign from disk '%s': %zu scenarios", c.c_str(), campaign_mgr_->scenario_count());
            base_campaign_loaded = true;
            break;
        }
    }
    if (!base_campaign_loaded) {
        auto cpn_bytes = read_asset("main campaign.cpn");
        if (!cpn_bytes.empty()) {
            campaign_mgr_->load_campaign_from_memory(cpn_bytes.data(), cpn_bytes.size());
            LOGI("Loaded campaign from APK assets: %zu scenarios", campaign_mgr_->scenario_count());
        }
    }

    // Campanha da expansão The Titans ("The New Atlantis")
    campaign_mgr_xpack_ = std::make_unique<CampaignManager>();
    const std::string cpx_candidates[] = {
        data_path + "/data/xpack campaign.cpx",
        data_path + "/xpack campaign.cpx",
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data/xpack campaign.cpx"
    };
    for (const auto& c : cpx_candidates) {
        if (campaign_mgr_xpack_->load_campaign(c)) {
            LOGI("Loaded Titans campaign '%s': %zu scenarios", c.c_str(),
                 campaign_mgr_xpack_->scenario_count());
            break;
        }
    }

    archives_loaded_ = (textures_bar_->file_count() > 0 || data_bar_->file_count() > 0);
    LOGI("Loaded archives: data=%zu, textures=%zu, models=%zu, scenarios=%zu, sounds=%zu, "
         "anim=%zu, godpowers=%zu (valid=%d)",
         data_bar_->file_count(), textures_bar_->file_count(), models_bar_->file_count(),
         scenarios_bar_->file_count(), sound_bar_->file_count(), anim_bar_->file_count(),
         godpowers_bar_->file_count(), archives_loaded_ ? 1 : 0);
    return true;
}

bool Engine::load_game_config() {
    if (data_bar_ && data_bar_->file_count() > 0) {
        // Registro de unidades, deuses e tecnologias: proto.xmb + protox.xmb
        // (expansão The Titans) e techtree.xmb + techtreex.xmb.
        if (game_data_.load_from_bar(*data_bar_)) {
            LOGI("Game data registry: %zu units, %zu gods, %zu techs",
                 game_data_.unit_count(), game_data_.god_count(), game_data_.tech_count());
        }

        const BarEntry* entry = data_bar_->find("proto.xmb");
        if (entry) {
            const std::vector<uint8_t> raw = data_bar_->extract(*entry);
            if (!raw.empty()) {
                auto root = XmbParser::parse(raw);
                if (root) {
                    LOGI("Game config loaded from BAR: root <%s>, %zu entities", root->name.c_str(), root->children.size());
                    game_config_root_ = std::move(root);
                    return true;
                }
            }
        }
    }

    // Fallback: load proto.xmb packaged in APK assets!
    auto proto_bytes = read_asset("proto.xmb");
    if (!proto_bytes.empty()) {
        auto root = XmbParser::parse(proto_bytes);
        if (root) {
            LOGI("Game config loaded from APK asset proto.xmb: root <%s>, %zu entities", root->name.c_str(), root->children.size());
            game_config_root_ = std::move(root);
            return true;
        }
    }
    return false;
}

namespace {

TextureHandle create_procedural_grass(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float n = std::sin(x * 0.4f) * std::cos(y * 0.4f) * 18.0f;
            float n2 = std::sin(x * 0.8f + y * 0.8f) * 10.0f;
            int r = std::clamp(static_cast<int>(60 + n * 0.5f), 0, 255);
            int g = std::clamp(static_cast<int>(130 + n + n2), 0, 255);
            int b = std::clamp(static_cast<int>(45 + n * 0.4f), 0, 255);
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(r);
            pixels[idx + 1] = static_cast<uint8_t>(g);
            pixels[idx + 2] = static_cast<uint8_t>(b);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

TextureHandle create_procedural_cliff(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float n = std::sin(x * 0.6f) * std::cos(y * 0.3f) * 25.0f;
            float str = std::sin(y * 0.8f) * 15.0f;
            int v = std::clamp(static_cast<int>(115 + n + str), 0, 255);
            int r = std::clamp(static_cast<int>(v * 1.05f), 0, 255);
            int g = std::clamp(static_cast<int>(v * 0.95f), 0, 255);
            int b = std::clamp(static_cast<int>(v * 0.88f), 0, 255);
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(r);
            pixels[idx + 1] = static_cast<uint8_t>(g);
            pixels[idx + 2] = static_cast<uint8_t>(b);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

TextureHandle create_procedural_water_normal(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float nx = std::sin(x * 0.35f) * 0.3f;
            float ny = std::cos(y * 0.35f) * 0.3f;
            glm::vec3 n = glm::normalize(glm::vec3(nx, ny, 1.0f));
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>((n.x * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>((n.y * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>((n.z * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

} // anonymous namespace

bool Engine::load_textures() {
    if (!renderer_) return false;

    if (textures_bar_ && textures_bar_->file_count() > 0) {
        if (terrain_texture_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/grassa.ddt");
            if (data.empty()) data = textures_bar_->extract("terrain/dirta.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    terrain_texture_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded terrain base texture from BAR (handle %u)", terrain_texture_);
                }
            }
        }

        if (terrain_blend_tex_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/cliffgreeka.ddt");
            if (data.empty()) data = textures_bar_->extract("terrain/cliffa.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    terrain_blend_tex_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded terrain blend/cliff texture from BAR (handle %u)", terrain_blend_tex_);
                }
            }
        }

        if (water_normal_tex_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/waterbump.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    water_normal_tex_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded water normal map from BAR (handle %u)", water_normal_tex_);
                }
            }
        }
    }

    if (terrain_texture_ == INVALID_TEXTURE) {
        terrain_texture_ = create_procedural_grass(renderer_.get());
        LOGI("Created procedural terrain grass texture (handle %u)", terrain_texture_);
    }
    if (terrain_blend_tex_ == INVALID_TEXTURE) {
        terrain_blend_tex_ = create_procedural_cliff(renderer_.get());
        LOGI("Created procedural terrain cliff texture (handle %u)", terrain_blend_tex_);
    }
    if (water_normal_tex_ == INVALID_TEXTURE) {
        water_normal_tex_ = create_procedural_water_normal(renderer_.get());
        LOGI("Created procedural water normal texture (handle %u)", water_normal_tex_);
    }

    // Carregar Skybox / Cube Map (.cub) — P3.2: 6 faces completas
    if (skybox_mesh_ == INVALID_MESH && textures_bar_) {
        for (const char* cub_name : {"sfx a shiny water.cub", "sfx shinygold map.cub", "sfx shinyice map.cub"}) {
            if (load_skybox_faces(cub_name)) break;
        }
    }

    // Fallback: skybox com 1 textura (procedural skydome)
    if (skydome_texture_ == INVALID_TEXTURE && textures_bar_ && skybox_mesh_ == INVALID_MESH) {
        for (const char* cub_name : {"sfx a shiny water.cub", "sfx shinygold map.cub", "sfx shinyice map.cub"}) {
            if (load_skybox_cube(cub_name)) break;
        }
    }

    // Carregar partículas .prt (P3.5)
    if (textures_bar_) {
        load_prt_particles("particles/fire_torch.prt");
        load_prt_particles("particles/combat_hit.prt");
        load_prt_particles("particles/construction_sparks.prt");
        load_prt_particles("particles/death_smoke.prt");
        load_prt_particles("particles/harvest_chips.prt");
    }

    return true;
}

bool Engine::load_skybox_cube(const std::string& cub_name) {
    if (!renderer_) return false;
    current_cubemap_ = load_cub_definition(cub_name, textures_bar_.get());
    if (!current_cubemap_.is_valid()) {
        LOGW("load_skybox_cube: definicao de cubemap '%s' nao encontrada ou invalida", cub_name.c_str());
        return false;
    }

    LOGI("load_skybox_cube: carregando cubemap '%s' (Up='%s', Front='%s')",
         cub_name.c_str(), current_cubemap_.up.c_str(), current_cubemap_.front.c_str());

    std::string tex_name = !current_cubemap_.up.empty() ? current_cubemap_.up : current_cubemap_.front;
    if (textures_bar_ && !tex_name.empty()) {
        for (const std::string& candidate : {tex_name + ".ddt", tex_name + " diffuse.ddt", tex_name}) {
            std::vector<uint8_t> data = textures_bar_->extract(candidate);
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    skydome_texture_ = renderer_->create_texture(*ddt);
                    LOGI("load_skybox_cube: textura do skydome '%s' carregada com sucesso (handle %u)",
                         candidate.c_str(), skydome_texture_);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Engine::load_skybox_faces(const std::string& cub_name) {
    if (!renderer_ || !textures_bar_) return false;

    current_cubemap_ = load_cub_definition(cub_name, textures_bar_.get());
    if (!current_cubemap_.is_valid()) {
        LOGW("load_skybox_faces: definicao de cubemap '%s' nao encontrada ou invalida", cub_name.c_str());
        return false;
    }

    LOGI("load_skybox_faces: carregando cubemap '%s' com 6 faces",
         cub_name.c_str());

    // Mapeamento das faces do .cub para nomes de arquivo .ddt no textures.bar
    struct FaceEntry {
        const std::string& face_name;
        TextureHandle& handle;
    };

    const std::array<FaceEntry, 6> faces = {{
        {current_cubemap_.up,    skybox_faces_[0]},   // UP
        {current_cubemap_.down,  skybox_faces_[1]},   // DOWN
        {current_cubemap_.front, skybox_faces_[2]},   // FRONT
        {current_cubemap_.back,  skybox_faces_[3]},   // BACK
        {current_cubemap_.left,  skybox_faces_[4]},   // LEFT
        {current_cubemap_.right, skybox_faces_[5]},   // RIGHT
    }};

    const char* face_names[6] = {"up", "down", "front", "back", "left", "right"};
    int loaded_count = 0;

    for (int i = 0; i < 6; ++i) {
        const auto& fe = faces[i];
        if (fe.face_name.empty()) {
            LOGW("load_skybox_faces: face '%s' sem nome definido no .cub", face_names[i]);
            continue;
        }

        // Tenta candidatos de arquivo de textura
        bool loaded = false;
        for (const std::string& cand : {
                 fe.face_name + ".ddt",
                 fe.face_name + " diffuse.ddt",
                 fe.face_name + "_diffuse.ddt",
                 fe.face_name,
             }) {
            std::vector<uint8_t> data = textures_bar_->extract(cand);
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    fe.handle = renderer_->create_texture(*ddt);
                    LOGI("load_skybox_faces: face '%s' = '%s' (handle %u)",
                         face_names[i], cand.c_str(), fe.handle);
                    loaded = true;
                    break;
                }
            }
        }
        if (!loaded) {
            LOGW("load_skybox_faces: nao foi possivel carregar textura para face '%s'", face_names[i]);
        } else {
            ++loaded_count;
        }
    }

    if (loaded_count == 0) {
        LOGE("load_skybox_faces: nenhuma face de skybox carregada para '%s'", cub_name.c_str());
        return false;
    }

    // Criar malha do skybox (cubo invertido com normais apontando para dentro)
    create_skybox_mesh();
    LOGI("load_skybox_faces: %d/6 faces carregadas, skybox mesh criado", loaded_count);
    return true;
}

void Engine::set_water_color_from_body(const std::string& body_name) {
    const auto& wb = game_data_.water_bodies();
    glm::vec3 color{0, 50, 170}; // default

    if (const auto* river = wb.find_river(body_name); river) {
        color = river->color / 255.0f;
    } else if (const auto* ocean = wb.find_ocean(body_name); ocean) {
        color = ocean->color / 255.0f;
    } else if (const auto* lake = wb.find_lake(body_name); lake) {
        color = lake->color / 255.0f;
    }

    water_color_ = color;
    LOGI("set_water_color_from_body: '%s' -> RGB(%.0f, %.0f, %.0f)",
         body_name.c_str(), water_color_.r * 255.0f, water_color_.g * 255.0f, water_color_.b * 255.0f);
}

void Engine::load_water_body_textures(const std::string& body_name) {
    if (!textures_bar_) return;

    const auto& wb = game_data().water_bodies();
    std::string bank_name, outer_bank_name;

    if (const auto* river = wb.find_river(body_name); river) {
        bank_name = river->bank;
        outer_bank_name = river->outer_bank;
        LOGI("load_water_body_textures: rio '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else if (const auto* ocean = wb.find_ocean(body_name); ocean) {
        bank_name = ocean->bank;
        outer_bank_name = ocean->outer_bank;
        LOGI("load_water_body_textures: oceano '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else if (const auto* lake = wb.find_lake(body_name); lake) {
        bank_name = lake->bank;
        outer_bank_name = lake->outer_bank;
        LOGI("load_water_body_textures: lago '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else {
        LOGW("load_water_body_textures: corpo d'agua '%s' nao encontrado no waterbodies.xmb",
             body_name.c_str());
        return;
    }

    if (!bank_name.empty()) {
        terrain_bank_tex_ = load_ddt_texture(bank_name);
        if (terrain_bank_tex_ != INVALID_TEXTURE) {
            LOGI("load_water_body_textures: bank texture '%s' -> handle %u",
                 bank_name.c_str(), terrain_bank_tex_);
        }
    }
    if (!outer_bank_name.empty()) {
        terrain_outer_bank_tex_ = load_ddt_texture(outer_bank_name);
        if (terrain_outer_bank_tex_ != INVALID_TEXTURE) {
            LOGI("load_water_body_textures: outer_bank texture '%s' -> handle %u",
                 outer_bank_name.c_str(), terrain_outer_bank_tex_);
        }
    }
}

void Engine::spawn_water_body_clusters(const std::string& body_name, const glm::vec3& center) {
    if (!gameplay_world_ || !textures_bar_) return;

    const auto& wb = game_data().water_bodies();
    const RiverDef* river = wb.find_river(body_name);
    const OceanDef* ocean = wb.find_ocean(body_name);
    const LakeDef* lake = wb.find_lake(body_name);

    const std::vector<ObjectCluster>* clusters = nullptr;
    float curve_angle = 15.0f;
    float distance_between = 30.0f;
    float distance_variation = 10.0f;
    bool lock_to_water_edge = false;

    if (river) {
        clusters = &river->object_clusters;
        curve_angle = river->curve_angle;
        distance_between = river->distance_between;
        distance_variation = river->distance_variation;
        lock_to_water_edge = river->lock_to_water_edge;
    } else if (ocean) {
        clusters = &ocean->object_clusters;
        curve_angle = 0.0f; // oceanos não têm objectplacement no XML de referência
        distance_between = 30.0f;
        distance_variation = 10.0f;
    } else if (lake) {
        clusters = &lake->object_clusters;
        curve_angle = lake->curve_angle;
        distance_between = lake->distance_between;
        distance_variation = lake->distance_variation;
        lock_to_water_edge = lake->lock_to_water_edge;
    } else {
        LOGW("spawn_water_body_clusters: corpo d'agua '%s' nao encontrado", body_name.c_str());
        return;
    }

    if (clusters->empty()) return;

    // Número de posições ao redor do centro: baseado no perímetro e distance_between.
    // Usamos um raio de referência de ~distance_between * 2 para clusters próximos à beira.
    const float radius = distance_between * 1.5f;
    const int num_positions = std::max(4, static_cast<int>(6.28318f * radius / distance_between));

    // Semente determinística baseada no nome do corpo d'água
    uint32_t seed = 0;
    for (char c : body_name) seed = seed * 31u + static_cast<uint8_t>(c);

    int spawned = 0;
    for (const auto& cluster : *clusters) {
        for (int i = 0; i < num_positions; ++i) {
            // Ângulo distribuído ao redor do centro, com variação baseada em curve_angle
            float angle = (6.28318f * i) / static_cast<float>(num_positions);
            if (curve_angle > 0.0f) {
                angle += glm::radians(curve_angle * 0.5f);
            }

            // Variação de distância determinística
            seed = seed * 1103515245u + 12345u;
            float dist_var = (static_cast<float>(seed % 1000) / 1000.0f - 0.5f) * 2.0f * distance_variation;
            float dist = radius + dist_var;

            // Posição no mundo (Y = 0, o terreno vai ajustar via heightmap se necessário)
            glm::vec3 pos = center + glm::vec3(
                std::cos(angle) * dist,
                0.0f,
                std::sin(angle) * dist
            );

            // Limita ao mundo jogável (aprox. ±16 unidades conforme is_walkable)
            if (std::abs(pos.x) > 15.0f || std::abs(pos.z) > 15.0f) continue;

            for (const auto& proto_name : cluster.protounits) {
                // Tenta carregar a textura do protounit do textures.bar
                TextureHandle tex = load_ddt_texture(proto_name);
                if (tex == INVALID_TEXTURE) {
                    // Fallback: tenta com sufixo .ddt
                    tex = load_ddt_texture(proto_name + ".ddt");
                }

                gameplay_world_->spawn_scenery_object(proto_name, pos, 1.0f, tex);
                ++spawned;
            }
        }
    }

    LOGI("spawn_water_body_clusters: '%s' -> %d objetos spawnados (%d clusters, %d posicoes)",
         body_name.c_str(), spawned, static_cast<int>(clusters->size()), num_positions);
}

void Engine::create_skybox_mesh() {
    if (!renderer_) return;
    if (skybox_mesh_ != INVALID_MESH) return;

    const float R = 65.0f;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    // Cubo com 6 faces, normais apontando para dentro (para vista de dentro para fora)
    // Face: UP (y = +R)
    {
        glm::vec3 corners[4] = {{-R, R, -R}, {R, R, -R}, {R, R, R}, {-R, R, R}};
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        // Ordem invertida para normais para dentro (vista interna)
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: DOWN (y = -R)
    {
        glm::vec3 corners[4] = {{-R, -R, R}, {R, -R, R}, {R, -R, -R}, {-R, -R, -R}};
        glm::vec3 normal(0.0f, -1.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: FRONT (z = +R)
    {
        glm::vec3 corners[4] = {{-R, -R, R}, {R, -R, R}, {R, R, R}, {-R, R, R}};
        glm::vec3 normal(0.0f, 0.0f, 1.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: BACK (z = -R)
    {
        glm::vec3 corners[4] = {{R, -R, -R}, {-R, -R, -R}, {-R, R, -R}, {R, R, -R}};
        glm::vec3 normal(0.0f, 0.0f, -1.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: LEFT (x = -R)
    {
        glm::vec3 corners[4] = {{-R, -R, -R}, {-R, -R, R}, {-R, R, R}, {-R, R, -R}};
        glm::vec3 normal(-1.0f, 0.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: RIGHT (x = +R)
    {
        glm::vec3 corners[4] = {{R, -R, R}, {R, -R, -R}, {R, R, -R}, {R, R, R}};
        glm::vec3 normal(1.0f, 0.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }

    skybox_mesh_ = renderer_->create_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                          indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("create_skybox_mesh: %zu verts, %zu indices", verts.size(), indices.size());
}

void Engine::add_skybox_quad(std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                             const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                             const glm::vec3& p3, const glm::vec3& normal,
                             const glm::vec2& t0, const glm::vec2& t1,
                             const glm::vec2& t2, const glm::vec2& t3) {
    uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back({p0, normal, t0, glm::vec4(1.0f)});
    verts.push_back({p1, normal, t1, glm::vec4(1.0f)});
    verts.push_back({p2, normal, t2, glm::vec4(1.0f)});
    verts.push_back({p3, normal, t3, glm::vec4(1.0f)});
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

bool Engine::load_prt_particles(const std::string& prt_name) {
    if (!renderer_ || !textures_bar_) return false;

    prt_particles_ = std::make_unique<aom::PrtParticleSystem>();
    if (!prt_particles_->load_definition(prt_name, textures_bar_.get())) {
        LOGW("load_prt_particles: falha ao carregar '%s'", prt_name.c_str());
        prt_particles_.reset();
        return false;
    }

    // Tentar carregar a textura de partícula definida no .prt
    const auto& def = prt_particles_->definition();
    if (!def.texture_name.empty()) {
        for (const std::string& cand : {
                 def.texture_name + ".ddt",
                 def.texture_name + " diffuse.ddt",
                 def.texture_name + "_diffuse.ddt",
                 def.texture_name,
             }) {
            prt_particle_tex_ = load_ddt_texture(cand);
            if (prt_particle_tex_ != INVALID_TEXTURE) break;
        }
    }

    LOGI("load_prt_particles: '%s' carregado (textura=%u)",
         prt_name.c_str(), prt_particle_tex_);
    return true;
}

bool Engine::load_models() {
    if (!renderer_ || !models_bar_) return false;

    int loaded = 0;
    for (const BarEntry& entry : models_bar_->entries()) {
        if (!ends_with(to_lower(entry.filename), ".brg")) continue;

        // Carregar dados do modelo
        auto model = load_brg_model_data(models_bar_.get(), entry.filename);
        if (!model || model->meshes.empty()) continue;

        // Upload da malha para o renderer
        const auto& m = model->meshes[0];
        MeshHandle mesh = renderer_->create_mesh(
            m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
            m.indices.data(), static_cast<uint32_t>(m.indices.size()));
        if (mesh == INVALID_MESH) continue;

        // Tentar carregar textura difusa com mesmo nome base (.ddt)
        TextureHandle tex = INVALID_TEXTURE;
        std::string base = entry.filename;
        if (ends_with(to_lower(base), ".brg")) {
            base = base.substr(0, base.size() - 4);
        }
        // Tenta vários candidatos de textura
        for (const std::string& cand : {
                 base + ".ddt",
                 base + " diffuse.ddt",
                 base + "_diffuse.ddt",
                 base + ".bti",
             }) {
            tex = load_ddt_texture(cand);
            if (tex != INVALID_TEXTURE) break;
        }

        // Associar com dados do proto.xmb (se houver uma unidade com esse nome base)
        const ProtoUnitData* unit_stats = nullptr;
        std::string base_name = entry.filename;
        if (ends_with(to_lower(base_name), ".brg")) {
            base_name = base_name.substr(0, base_name.size() - 4);
        }
        unit_stats = game_data_.proto_units().find(base_name);

        ModelEntry& entry_cache = model_cache_[entry.filename];
        entry_cache.mesh = mesh;
        entry_cache.texture = tex;
        entry_cache.model = model.get();
        entry_cache.unit_stats = unit_stats;
        // Guardar ponteiro estável (o unique_ptr do modelo é descartado, mas
        // precisamos dos dados para referência; o BrgModel é copiado por valor)
        model_cache_models_[entry.filename] = std::move(model);

        if (loaded == 0) {
            model_mesh_ = mesh;
            model_texture_ = tex;
        }
        ++loaded;
        LOGI("Cached BRG model: %s (mesh=%u, texture=%u)",
             entry.filename.c_str(), mesh, tex);
    }

    LOGI("load_models: %d modelos BRG em cache", loaded);
    return loaded > 0;
}

void Engine::next_texture() {}
void Engine::prev_texture() {}
void Engine::next_model() {}
void Engine::prev_model() {}

void Engine::load_game(const std::string& path) {
    const std::string p = path.empty() ? save_game_path() : path;
    LOGI("load_game: carregando de '%s'", p.c_str());
    SaveGameData data;
    if (!load_game_state(p, data)) {
        LOGW("load_game: save nao encontrado ou invalido — iniciando nova partida");
        start_game();
        return;
    }
    start_game();
    if (gameplay_world_) {
        ResourcePool& pool_f = gameplay_world_->resource_pool_mutable(Team::FRIENDLY);
        pool_f.food  = data.food[0];
        pool_f.wood  = data.wood[0];
        pool_f.gold  = data.gold[0];
        pool_f.favor = data.favor[0];
        ResourcePool& pool_e = gameplay_world_->resource_pool_mutable(Team::ENEMY);
        pool_e.food  = data.food[1];
        pool_e.wood  = data.wood[1];
        pool_e.gold  = data.gold[1];
        pool_e.favor = data.favor[1];
        accumulated_time_ = data.game_time_seconds;
        restore_saved_entities(data);
        LOGI("load_game: recursos e estado restaurados com sucesso de '%s'", p.c_str());
    }
}

void Engine::restore_saved_entities(const SaveGameData& data) {
    if (!gameplay_world_) return;
    GameplayWorld& world = *gameplay_world_;

    // Remove as entidades padrao criadas por start_game(), preservando os nos
    // de recursos e a grade de navegacao ja montada.
    world.clear_units_and_buildings();

    // Animacao reaproveitada da partida (mesmo clip usado no start_game).
    const aom::anim::BrgAnimationClip* clip = nullptr;
    TextureHandle clip_tex = INVALID_TEXTURE;
    select_animation_clip(clip, clip_tex);

    size_t restored_units = 0;
    for (const SavedUnit& su : data.units) {
        UnitStats stats = extract_unit_stats_from_xmb(game_config_root_.get(), su.unit_type);
        if (su.max_hp > 0) stats.max_hp = static_cast<float>(su.max_hp);
        stats.worker   = su.worker;
        stats.pop_cost = (su.pop_cost > 0) ? su.pop_cost : 1;

        const Team team = (su.team == 0) ? Team::FRIENDLY : Team::ENEMY;
        Entity e = world.spawn_unit(team, glm::vec3(su.pos_x, su.pos_y, su.pos_z), stats);
        if (su.current_hp > 0) world.set_entity_hp(e, su.current_hp);
        if (clip && clip->is_valid() && renderer_) {
            world.attach_animation(e, clip, clip_tex, renderer_.get());
        }

        // —— Restaurar estado de ordem da unidade (v2) ——
        if (auto* u = world.unit_component_mutable(e)) {
            u->has_move_order   = su.has_move_order;
            u->move_target      = glm::vec3(su.move_target_x, 0.0f, su.move_target_z);
            u->attack_target    = (su.attack_target != 0) ? static_cast<Entity>(su.attack_target) : INVALID_ENTITY;
            u->build_target     = (su.build_target != 0) ? static_cast<Entity>(su.build_target) : INVALID_ENTITY;
            u->gather_node      = su.gather_node;
            u->returning_cargo  = su.returning_cargo;
            u->carried_resource_amount = su.carried_resource_amount;
            u->carried_resource_type = static_cast<ResourceType>(su.carried_resource_type);
            // Reconstruir waypoints se houver ordem de movimento ativa
            if (u->has_move_order) {
                auto t_it = world.get_transform(e);
                auto move_t = glm::vec3(su.move_target_x, 0.0f, su.move_target_z);
                if (t_it) {
                    u->waypoints = world.navigation_find_path(t_it->position, move_t);
                    u->waypoint_index = 0;
                }
            }
        }
        ++restored_units;
    }

    size_t restored_buildings = 0;
    for (const SavedBuilding& sb : data.buildings) {
        const Team team = (sb.team == 0) ? Team::FRIENDLY : Team::ENEMY;
        const BuildingType type = static_cast<BuildingType>(sb.building_type);
        Entity e = world.place_building(team, glm::vec3(sb.pos_x, sb.pos_y, sb.pos_z),
                                        1.0f, type, sb.name.empty() ? "Edificio" : sb.name);
        if (BuildingComponent* bc = world.building_component_mutable(e)) {
            bc->complete = sb.complete;
            bc->progress = sb.progress;
            bc->training_unit = sb.training_unit;
            bc->train_timer = sb.train_timer;
            // Restaurar fila de treinamento (v2)
            for (const auto& unit_type : sb.train_queue) {
                bc->train_queue.push(unit_type);
            }
        }
        if (sb.current_hp > 0) world.set_entity_hp(e, sb.current_hp);
        ++restored_buildings;
    }

    LOGI("load_game: %zu unidades e %zu edificios restaurados (v2)", restored_units, restored_buildings);
}

void Engine::save_game(const std::string& path) {
    const std::string p = path.empty() ? save_game_path() : path;
    LOGI("save_game: salvando em '%s'", p.c_str());
    if (!gameplay_world_) {
        LOGW("save_game: mundo de jogo nao inicializado, nada a salvar");
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(p).parent_path(), ec);
    if (save_game_state(p, *gameplay_world_, accumulated_time_)) {
        LOGI("save_game: gravado com sucesso em '%s'", p.c_str());
    } else {
        LOGE("save_game: falha ao gravar '%s'", p.c_str());
    }
}

std::string Engine::save_game_path() const {
    return config_.data_path + "/savegame.dat";
}

void Engine::create_load_game_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CARREGAR JOGO", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);

    const std::string sp = save_game_path();
    const bool found = has_valid_save(sp);
    if (found) {
        ui_->add_label("Jogo salvo encontrado!", {vw * 0.5f - 110.0f, vh * 0.38f}, 20.0f, 0xFF76FF03);
        ui_->add_label(sp, {vw * 0.5f - 180.0f, vh * 0.44f}, 14.0f, 0xFF88DDFF);
        float btn_w = 240.0f, btn_h = 48.0f;
        ui_->add_button({(vw - btn_w) * 0.5f, vh * 0.54f}, {btn_w, btn_h}, "CONTINUAR PARTIDA",
                       [this]() { load_game(); });
    } else {
        ui_->add_label("Nenhum jogo salvo encontrado.", {vw * 0.5f - 140.0f, vh * 0.45f}, 20.0f, 0xFFCCCCCC);
    }

    float back_w = 200.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                   [this]() { return_to_main_menu(); });
}

void Engine::create_single_player_menu() {
    start_game();
}

void Engine::create_random_map_menu() {
    start_game();
}

void Engine::create_campaign_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Header
    const Campaign* camp = campaign_mgr_ ? campaign_mgr_->current_campaign() : nullptr;
    std::string camp_name = camp ? camp->name : "Campanha";
    ui_->add_label("CAMPANHA: " + camp_name, {vw * 0.5f - 240.0f, vh * 0.05f}, 26.0f, 0xFFFFD700);
    ui_->add_label("Total de " + std::to_string(campaign_mgr_ ? campaign_mgr_->scenario_count() : 0) +
                   " cenarios",
                   {vw * 0.5f - 160.0f, vh * 0.09f}, 14.0f, 0xFFCCCCCC);

    // Lista dinamica de cenarios jogáveis (pula cutscenes/unlisted)
    const float box_w = vw * 0.90f;
    const float box_x = (vw - box_w) * 0.5f;
    const float box_y = vh * 0.13f;
    const float row_h = 42.0f;
    const float max_rows = 10.0f;  // numero de linhas visíveis antes de rolagem (simplificado)

    float y = box_y;
    int shown = 0;
    std::vector<const CampaignScenario*> playable;
    if (camp) {
        for (size_t i = 0; i < camp->scenarios.size(); ++i) {
            const auto* sc = &camp->scenarios[i];
            if (sc->unlisted) continue;
            // Considera jogável se tem filename .scn ou .scx (não cinemática pura)
            if (sc->filename.empty()) continue;
            playable.push_back(sc);
        }
    }

    for (size_t i = 0; i < playable.size() && shown < static_cast<size_t>(max_rows); ++i, ++shown) {
        const auto* sc = playable[i];
        std::string label = sc->display_name.empty() ? ("Cenario " + std::to_string(i + 1))
                                                      : sc->display_name;
        std::string file_info = sc->filename;
        // Trunca nome do arquivo para caber na linha
        if (file_info.size() > 38) file_info = file_info.substr(0, 35) + "...";

        ui_->add_label(label + "  |  " + file_info,
                       {box_x + 20.0f, y}, 15.0f, 0xFFFFFFFF);

        // Botão "JOGAR" para cada cenário
        float btn_w = 120.0f, btn_h = 30.0f;
        float btn_x = box_x + box_w - btn_w - 20.0f;
        ui_->add_button({btn_x, y - 6.0f}, {btn_w, btn_h}, "JOGAR",
                        [this, sc]() { start_campaign_mission(sc); },
                        0xDD2A5A1A, 0xEE408525);

        y += row_h;
    }

    if (playable.size() > max_rows) {
        ui_->add_label("... e mais " + std::to_string(playable.size() - max_rows) +
                       " cenario(ns)",
                       {box_x + 20.0f, y}, 12.0f, 0xFF888888);
        y += row_h;
    }

    // Buttons fixos no rodapé
    const float btn_w = 200.0f;
    const float btn_h = 44.0f;
    const float btn_y = vh * 0.88f;

    ui_->add_button({(vw - btn_w) * 0.5f, btn_y}, {btn_w, btn_h}, "VOLTAR",
                    [this]() { return_to_main_menu(); }, 0xAA221105, 0xAA44220A);
}

// ── Layouts de UI do data.bar (Passo 4) ──────────────────────────────────

bool Engine::load_ui_layout(const std::string& xmb_name) {
    if (!data_bar_) {
        LOGW("load_ui_layout: data_bar_ nao disponível");
        return false;
    }
    auto result = ui_layout_manager_.load_layout(*data_bar_, xmb_name);
    if (!result.success) {
        LOGE("load_ui_layout: %s", result.error_message.c_str());
        return false;
    }
    LOGI("load_ui_layout: '%s' carregado (%zu gadgets)", xmb_name.c_str(),
         result.root_gadgets.size());
    return true;
}

size_t Engine::show_ui_layout(const std::string& layout_name) {
    if (!ui_) return 0;

    const auto* layout = ui_layout_manager_.get_layout(layout_name);
    if (!layout) {
        LOGW("show_ui_layout: layout '%s' não encontrado — tente load_ui_layout() primeiro",
             layout_name.c_str());
        return 0;
    }

    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    size_t created = apply_layout_to_ui(*layout, ui_.get(),
                                         static_cast<int>(vw), static_cast<int>(vh));

    LOGI("show_ui_layout: '%s' exibido (%zu elementos)", layout_name.c_str(), created);
    return created;
}

bool Engine::show_techtree() {
    if (!load_ui_layout("uitechtree.xmb")) return false;
    show_ui_layout("uitechtree.xmb");
    return true;
}

bool Engine::show_tribute() {
    if (!load_ui_layout("uitributedlg.xmb")) return false;
    show_ui_layout("uitributedlg.xmb");
    return true;
}

bool Engine::show_sound_settings() {
    if (!load_ui_layout("uisoundsetdlg.xmb")) return false;
    show_ui_layout("uisoundsetdlg.xmb");
    return true;
}

void Engine::start_campaign_mission(size_t scenario_idx) {
    if (!campaign_mgr_) {
        LOGW("start_campaign_mission: campaign_mgr_ nao inicializado");
        return;
    }
    const CampaignScenario* sc = campaign_mgr_->get_scenario(scenario_idx);
    if (!sc) {
        LOGW("start_campaign_mission: indice %zu fora do alcance", scenario_idx);
        return;
    }
    start_campaign_mission(sc);
}

void Engine::start_campaign_mission(const CampaignScenario* scenario) {
    if (!scenario) {
        LOGW("start_campaign_mission: scenario nulo");
        return;
    }

    const auto setup_start = std::chrono::steady_clock::now();
    LOGI("Starting Campaign Mission: '%s' (%s)", scenario->display_name.c_str(),
         scenario->filename.c_str());
    set_state(EngineState::IN_GAME);

    radius_ = 14.0f;
    yaw_ = 0.0f;
    pitch_ = 0.35f;
    camera_.up = glm::vec3(0.0f, 1.0f, 0.0f);
    camera_.aspect = static_cast<float>(config_.window_width) / static_cast<float>(config_.window_height);

    accumulated_time_ = 0.0f;
    frame_count_ = 0;

    // --- Initialize GameplayWorld ---
    gameplay_world_ = std::make_unique<GameplayWorld>(game_config_root_.get());
    gameplay_world_->set_anim_renderer(renderer_.get());
    gameplay_world_->set_unit_transform_manager(&game_data_.unit_transforms());

    // --- Load BRG animation clips from models.bar (carga limitada + cache) ---
    load_animation_clips();

    const aom::anim::BrgAnimationClip* clip = nullptr;
    TextureHandle clip_tex = INVALID_TEXTURE;
    select_animation_clip(clip, clip_tex);

    if (clip && clip->is_valid()) {
        gameplay_world_->set_default_animation(clip, clip_tex);
    }

    // Wire gameplay world callbacks to interactive particle spawners & 3D audio triggers
    // P3.5: usa PrtParticleSystem (.prt) quando disponível, fallback para partículas antigas
    gameplay_world_->on_resource_harvested = [this](ResourceType type, const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(6, pos);  // emite partículas de colheita
        } else {
            spawn_harvest_chips(type, pos);
        }
        if (audio_) {
            std::string res_name = (type == ResourceType::WOOD) ? "wood" : ((type == ResourceType::GOLD) ? "gold" : "food");
            audio_->play_harvest_sound(pos, res_name);
        }
    };
    gameplay_world_->on_combat_hit = [this](Entity, Entity, const glm::vec3& hit_pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(10, hit_pos);  // emite partículas de impacto
        } else {
            spawn_combat_hit(hit_pos);
        }
        if (audio_) audio_->play_combat_hit(hit_pos, true);
    };
    gameplay_world_->on_building_sparks = [this](const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(4, pos);  // emite faíscas de construção
        } else {
            spawn_construction_sparks(pos, 3);
        }
        if (audio_) audio_->play_building_work(pos);
    };
    gameplay_world_->on_building_completed = [this](Entity, const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(16, pos);  // emite partículas de conclusão
        } else {
            spawn_construction_sparks(pos, 16);
        }
        if (audio_) audio_->play_sfx_3d("assets/sound/new_objective.wav", pos, 1.0f);
    };
    gameplay_world_->on_unit_death = [this](const glm::vec3& pos) {
        if (prt_particles_) {
            prt_particles_->burst_emit(12, pos);  // emite fumaça de morte
        } else {
            spawn_death_smoke(pos);
        }
        if (audio_) audio_->play_unit_death(pos, false);
    };

    // Initialize CampaignMissionManager
    mission_mgr_ = std::make_unique<CampaignMissionManager>();
    mission_mgr_->on_audio_cue = [this](const std::string& cue) {
        if (audio_) {
            audio_->play_sfx(cue);
        }
    };
    mission_mgr_->on_mission_completed = [this](MissionResult res) {
        if (res == MissionResult::VICTORY) {
            LOGI("Campaign Mission '%s' COMPLETED with VICTORY!", scenario->display_name.c_str());
        } else if (res == MissionResult::DEFEAT) {
            LOGI("Campaign Mission '%s' FAILED with DEFEAT!", scenario->display_name.c_str());
        }
        // Fase 2: aqui vai a transição para o próximo cenário
    };

    // Montar caminho do .scn a partir do filename do cenário
    // filename do .cpn vem como "spc\\aomsp04" ou "spc/aomsp04" (sem .scn)
    std::string scn_base = scenario->filename;
    // Remove extensão se houver
    if (ends_with(to_lower(scn_base), ".scn")) scn_base = scn_base.substr(0, scn_base.size() - 4);
    if (ends_with(to_lower(scn_base), ".scx")) scn_base = scn_base.substr(0, scn_base.size() - 4);

    // Normaliza separadores para / e adiciona .scn
    std::string scn_path = scn_base;
    for (char& c : scn_path) {
        if (c == '\\') c = '/';
    }
    if (!ends_with(to_lower(scn_path), ".scn")) scn_path += ".scn";

    // Tenta usar Mission 1: Omens se for esse cenário (script dedicado)
    bool used_dedicated_mission = false;
    if (scn_path.find("aomsp04") != std::string::npos ||
        scenario->display_name.find("Omens") != std::string::npos) {
        mission_mgr_->init_mission_omens(gameplay_world_.get(), scenarios_bar_.get(),
                                         scn_path, &camera_);
        used_dedicated_mission = true;
    }

    // Para outros cenários, usa missão genérica (terrain + entidades básicas)
    if (!used_dedicated_mission) {
        start_generic_mission(scn_path);
    }

    // Terreno 3D da campanha: o height-map vem do próprio cenário da missão
    if (terrain_mesh_ == INVALID_MESH) {
        bool heightmap_loaded = false;
        static const char* kMissionScn[] = {
            scn_path.c_str(),
            "spc\\aomsp04.scn", "spc/aomsp04.scn", "aomsp04.scn"
        };
        for (const char* scn : kMissionScn) {
            if (load_scenario_heightmap(scn)) {
                heightmap_loaded = true;
                break;
            }
        }
        if (!heightmap_loaded) {
            LOGI("Cenario da missao sem height-map — usando terreno procedural");
            generate_terrain();
        }
    }

    // Passo 1: aplicar cor e texturas de beira do waterbodies.xmb ao terreno/agua
    // Passo 2: spawnar clusters de objetos (rochas, árvores) na beira da água
    const auto& wb = game_data().water_bodies();
    if (wb.ocean_count() > 0 || wb.river_count() > 0 || wb.lake_count() > 0) {
        // Tenta Mediterrâneo primeiro (cenário Omens/Atena), depois qualquer outro
        if (wb.find_ocean("Mediterranean Sea")) {
            set_water_color_from_body("Mediterranean Sea");
            load_water_body_textures("Mediterranean Sea");
            spawn_water_body_clusters("Mediterranean Sea", glm::vec3(0.0f));
        } else if (wb.find_river("Greek River")) {
            set_water_color_from_body("Greek River");
            load_water_body_textures("Greek River");
            spawn_water_body_clusters("Greek River", glm::vec3(0.0f));
        } else {
            const auto& oceans = wb.all_oceans();
            if (!oceans.empty()) {
                set_water_color_from_body(oceans[0].name);
                load_water_body_textures(oceans[0].name);
                spawn_water_body_clusters(oceans[0].name, glm::vec3(0.0f));
            } else {
                const auto& rivers = wb.all_rivers();
                if (!rivers.empty()) {
                    set_water_color_from_body(rivers[0].name);
                    load_water_body_textures(rivers[0].name);
                    spawn_water_body_clusters(rivers[0].name, glm::vec3(0.0f));
                }
            }
        }
    }

    if (skydome_mesh_ == INVALID_MESH) generate_skydome();

    create_in_game_hud();
    const auto setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - setup_start)
                              .count();
    LOGI("Campaign Mission '%s' running! (setup: %lld ms)", scenario->display_name.c_str(),
         static_cast<long long>(setup_ms));
}

void Engine::start_generic_mission(const std::string& scenario_filename) {
    LOGI("Starting generic mission: '%s'", scenario_filename.c_str());

    // Carrega dados do cenário .scn (apenas header/terrain por enquanto)
    std::vector<uint8_t> scn_data;
    if (scenarios_bar_ && scenarios_bar_->file_count() > 0) {
        // Tenta o caminho exato
        const BarEntry* entry = scenarios_bar_->find(scenario_filename);
        if (entry) {
            scn_data = scenarios_bar_->extract(*entry);
        }
        // Fallback: primeiro .scn disponível
        if (scn_data.empty()) {
            for (const BarEntry& e : scenarios_bar_->entries()) {
                if (ends_with(to_lower(e.filename), ".scn")) {
                    scn_data = scenarios_bar_->extract(e);
                    if (!scn_data.empty()) {
                        LOGW("Generic mission: '%s' não encontrado, usando '%s'",
                             scenario_filename.c_str(), e.filename.c_str());
                        break;
                    }
                }
            }
        }
    }

    // Parse do header para saber se é válido
    bool scn_valid = false;
    if (!scn_data.empty()) {
        auto header = ScenarioTriggerSystem::parse_scenario_header(scn_data.data(),
                                                                   scn_data.size(), false);
        scn_valid = header.valid;
        LOGI("Generic mission: scenario '%s' — valid=%d, magic=%s, size=%zu bytes",
             scenario_filename.c_str(), scn_valid,
             header.format_magic.empty() ? "?" : header.format_magic.c_str(),
             scn_data.size());
    }

    if (!scn_valid) {
        LOGW("Generic mission: scenario '%s' inválido ou não encontrado — missão sem terreno real",
             scenario_filename.c_str());
    }

    // Spawn entidades básicas para missão genérica (Town Center + unidades iniciais)
    gameplay_world_->place_town_center(Team::FRIENDLY, glm::vec3(-6.0f, 0.0f, 0.0f), true);
    gameplay_world_->place_town_center(Team::ENEMY, glm::vec3(6.0f, 0.0f, 0.0f), true);

    // Unidades friendly iniciais
    UnitStats hoplite_stats;
    hoplite_stats.name = "Hoplite";
    hoplite_stats.max_hp = 120.0f;
    hoplite_stats.move_speed = 3.8f;
    hoplite_stats.attack_damage = 12.0f;
    hoplite_stats.attack_range = 2.2f;

    gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-3.0f, 0.0f, 0.0f), hoplite_stats);
    gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-1.5f, 0.0f, 1.0f), hoplite_stats);
    gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-1.5f, 0.0f, -1.0f), hoplite_stats);

    // Worker friendly
    gameplay_world_->spawn_worker(glm::vec3(-4.0f, 0.0f, 1.5f));

    // Unidades enemy iniciais
    UnitStats enemy_hoplite = hoplite_stats;
    enemy_hoplite.name = "Enemy Hoplite";
    gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(3.0f, 0.0f, 0.0f), enemy_hoplite);
    gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(1.5f, 0.0f, 1.0f), enemy_hoplite);
    gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(1.5f, 0.0f, -1.0f), enemy_hoplite);

    // Nós de recursos
    gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(-4.0f, 0.0f, 3.5f), 1200);
    gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(-7.5f, 0.0f, 3.5f), 1500);
    gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(-5.5f, 0.0f, -3.5f), 2000);

    gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(4.0f, 0.0f, 3.5f), 1200);
    gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(7.5f, 0.0f, 3.5f), 1500);
    gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(5.5f, 0.0f, -3.5f), 2000);

    LOGI("Generic mission: entidades básicas spawnadas");
}

void Engine::open_multiplayer() {
    LOGI("Opening Multiplayer...");
    auto& nm = net::NetworkManager::instance();
    nm.on_lobby_updated = [this]() {
        if (state_ == EngineState::MULTIPLAYER_LOBBY) {
            create_multiplayer_lobby_menu();
        } else if (state_ == EngineState::MULTIPLAYER_LAN) {
            create_multiplayer_lan_menu();
        }
    };
    nm.on_chat_received = [this](const net::ChatMessage&) {
        if (state_ == EngineState::MULTIPLAYER_LOBBY) {
            create_multiplayer_lobby_menu();
        }
    };
    nm.on_game_launched = [this]() {
        LOGI("Multiplayer game launched by host! Transitioning to game...");
        start_game();
    };
    nm.on_disconnected = [this](const std::string& reason) {
        LOGW("Multiplayer disconnected: %s", reason.c_str());
        set_state(EngineState::MULTIPLAYER_LAN);
    };

    nm.start_lan_discovery();
    set_state(EngineState::MULTIPLAYER_LAN);
}

void Engine::create_multiplayer_menu() {
    open_multiplayer();
}

void Engine::create_multiplayer_lan_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // Titulo Autentico AoM
    ui_->add_label("MULTIJOGADOR - REDE LOCAL (LAN) / IP DIRETO", {vw * 0.5f - 300.0f, vh * 0.07f}, 26.0f, 0xFFFFD000);
    ui_->add_label("Jogue contra outros dispositivos Android ou PC na mesma rede Wi-Fi", {vw * 0.5f - 270.0f, vh * 0.12f}, 15.0f, 0xFFCCCCCC);

    const float col_w = vw * 0.42f;
    const float col1_x = vw * 0.06f;
    const float col2_x = vw * 0.52f;
    const float btn_h = 42.0f;
    const float gap = 12.0f;

    // ── Coluna 1: Hospedar & Conexao Direta ────────────────────────────
    float y1 = vh * 0.18f;
    ui_->add_label("COORDENAR PARTIDA (HOST):", {col1_x, y1}, 20.0f, 0xFFFFD000);
    y1 += 32.0f;

    ui_->add_button({col1_x, y1}, {col_w, btn_h}, "CRIAR SALA LAN (HOST)", [this]() {
        net::NetworkManager::instance().host_game(mp_session_name_, mp_player_name_, net::DEFAULT_AOM_PORT);
        set_state(EngineState::MULTIPLAYER_LOBBY);
    }, 0xAA2A1808, 0xAA553311);
    y1 += btn_h + gap * 1.5f;

    ui_->add_label("CONEXAO DIRETA VIA IP:", {col1_x, y1}, 20.0f, 0xFFFFD000);
    y1 += 30.0f;

    // Mostrador de IP selecionado
    std::string ip_label = "IP Alvo: " + mp_target_ip_;
    ui_->add_label(ip_label, {col1_x, y1 + 6.0f}, 17.0f, 0xFFFFFFFF);
    y1 += 34.0f;

    // Botao Conectar ao IP alvo
    ui_->add_button({col1_x, y1}, {col_w, btn_h}, "CONECTAR AO IP ALVO", [this]() {
        net::NetworkManager::instance().join_game(mp_target_ip_, net::DEFAULT_AOM_PORT, mp_player_name_);
        set_state(EngineState::MULTIPLAYER_LOBBY);
    }, 0xAA183818, 0xAA286028);
    y1 += btn_h + gap;

    // Botoes de ajuste de IP octeto (+1, +10, -1, -10)
    float mini_w = (col_w - 3.0f * 8.0f) / 4.0f;
    ui_->add_button({col1_x, y1}, {mini_w, 36.0f}, "-10", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ - 10, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + mini_w + 8.0f, y1}, {mini_w, 36.0f}, "-1", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ - 1, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + (mini_w + 8.0f) * 2.0f, y1}, {mini_w, 36.0f}, "+1", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ + 1, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    ui_->add_button({col1_x + (mini_w + 8.0f) * 3.0f, y1}, {mini_w, 36.0f}, "+10", [this]() {
        mp_ip_last_octet_ = std::clamp(mp_ip_last_octet_ + 10, 1, 254);
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });
    y1 += 36.0f + gap;

    // Presets de IP rapidos (Localhost 127.0.0.1 e Subnet 192.168.0.x / 192.168.1.x)
    float half_w = (col_w - 8.0f) * 0.5f;
    ui_->add_button({col1_x, y1}, {half_w, 36.0f}, "127.0.0.1 (Local)", [this]() {
        mp_target_ip_ = "127.0.0.1";
        create_multiplayer_lan_menu();
    });
    std::string subnet_text = (mp_ip_subnet_ == 1) ? "Sub-rede: .1.x" : "Sub-rede: .0.x";
    ui_->add_button({col1_x + half_w + 8.0f, y1}, {half_w, 36.0f}, subnet_text, [this]() {
        mp_ip_subnet_ = (mp_ip_subnet_ == 1) ? 0 : 1;
        mp_target_ip_ = "192.168." + std::to_string(mp_ip_subnet_) + "." + std::to_string(mp_ip_last_octet_);
        create_multiplayer_lan_menu();
    });

    // ── Coluna 2: Jogos em LAN Descobertos ──────────────────────────────
    float y2 = vh * 0.18f;
    ui_->add_label("JOGOS EM LAN DETECTADOS (UDP 2300):", {col2_x, y2}, 20.0f, 0xFFFFD000);
    y2 += 32.0f;

    auto games = net::NetworkManager::instance().get_discovered_games();
    if (games.empty()) {
        ui_->add_label("(Nenhum host encontrado na rede local ainda...)", {col2_x, y2}, 16.0f, 0xFFAAAAAA);
        y2 += 26.0f;
        ui_->add_label("Inicie o Host em outro aparelho ou use Conexao Direta.", {col2_x, y2}, 14.0f, 0xFF888888);
        y2 += 36.0f;
    } else {
        for (size_t i = 0; i < games.size() && i < 4; ++i) {
            const auto& g = games[i];
            char btn_txt[128];
            std::snprintf(btn_txt, sizeof(btn_txt), "%s (%s) [%d/%d]",
                          g.name.c_str(), g.ip_address.c_str(), g.current_players, g.max_players);
            std::string ip = g.ip_address;
            uint16_t port = g.port;
            ui_->add_button({col2_x, y2}, {col_w, btn_h}, btn_txt, [this, ip, port]() {
                net::NetworkManager::instance().join_game(ip, port, mp_player_name_);
                set_state(EngineState::MULTIPLAYER_LOBBY);
            }, 0xAA204060, 0xAA306090);
            y2 += btn_h + 8.0f;
        }
    }

    ui_->add_button({col2_x, vh * 0.68f}, {col_w, btn_h}, "ATUALIZAR LISTA (REFRESH)", [this]() {
        net::NetworkManager::instance().refresh_lan_games();
        create_multiplayer_lan_menu();
    });

    // ── Barra Inferior: Voltar ─────────────────────────────────────────
    float back_w = 260.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.86f}, {back_w, 46.0f}, "VOLTAR AO MENU", [this]() {
        net::NetworkManager::instance().stop_lan_discovery();
        return_to_main_menu();
    });
}

void Engine::create_multiplayer_lobby_menu() {
    clear_ui();

    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    auto& nm = net::NetworkManager::instance();
    auto slots = nm.get_slots();
    auto settings = nm.get_settings();
    auto chat = nm.get_chat_history();
    bool is_host = nm.is_host();
    int my_slot = nm.my_slot_id();

    // ── Cabecalho do Lobby ─────────────────────────────────────────────
    std::string role_str = is_host ? "[HOST / COORDENADOR]" : "[CLIENTE]";
    char title_buf[128];
    std::snprintf(title_buf, sizeof(title_buf), "SALA MULTIJOGADOR: %s - %s",
                  role_str.c_str(), settings.map_name.c_str());
    ui_->add_label(title_buf, {vw * 0.05f, vh * 0.05f}, 24.0f, 0xFFFFD000);

    char sub_buf[128];
    std::snprintf(sub_buf, sizeof(sub_buf), "Porta UDP: %u | Seu Slot: #%d (%s)",
                  net::DEFAULT_AOM_PORT, my_slot + 1, is_host ? "Host" : "Jogador");
    ui_->add_label(sub_buf, {vw * 0.05f, vh * 0.09f}, 14.0f, 0xFFCCCCCC);

    // ── Tabela de Slots (Esquerda / Centro) ────────────────────────────
    const float slot_x = vw * 0.05f;
    const float slot_w = vw * 0.58f;
    const float slot_h = 36.0f;
    const float gap = 6.0f;
    float y = vh * 0.14f;

    ui_->add_label("SLOT | JOGADOR | DEUS PRINCIPAL | EQUIPE | COR | STATUS", {slot_x, y}, 14.0f, 0xFFFFD000);
    y += 22.0f;

    for (size_t i = 0; i < slots.size() && i < 8; ++i) {
        const auto& s = slots[i];
        float row_y = y + i * (slot_h + gap);

        if (static_cast<int>(i) == my_slot) {
            // Meu slot: botoes interativos para alterar Deus, Equipe e Pronto
            char info_buf[64];
            std::snprintf(info_buf, sizeof(info_buf), "#%zu %s (VOCE)", i + 1, s.name.c_str());
            ui_->add_label(info_buf, {slot_x, row_y + 8.0f}, 15.0f, 0xFF00FF88);

            // Botao do Deus Principal
            float god_x = slot_x + slot_w * 0.32f;
            float god_w = slot_w * 0.28f;
            std::string god_txt = std::string(net::major_god_to_string(s.god)) + " >";
            ui_->add_button({god_x, row_y}, {god_w, slot_h}, god_txt, [this, &nm, s]() {
                uint8_t next_god = (static_cast<uint8_t>(s.god) + 1) % 13;
                nm.update_my_slot(static_cast<net::MajorGod>(next_god), s.team, s.color_idx, s.is_ready);
                create_multiplayer_lobby_menu();
            }, 0xAA2A2010, 0xAA504020);

            // Botao da Equipe
            float team_x = god_x + god_w + 6.0f;
            float team_w = slot_w * 0.16f;
            std::string team_txt = "Eq " + std::to_string(s.team);
            ui_->add_button({team_x, row_y}, {team_w, slot_h}, team_txt, [this, &nm, s]() {
                uint8_t next_team = (s.team % 4) + 1;
                nm.update_my_slot(s.god, next_team, s.color_idx, s.is_ready);
                create_multiplayer_lobby_menu();
            }, 0xAA182838, 0xAA284868);

            // Botao Pronto
            float ready_x = team_x + team_w + 6.0f;
            float ready_w = slot_w * 0.20f;
            std::string ready_txt = s.is_ready ? "[X] PRONTO" : "[ ] ESPERA";
            uint32_t ready_col = s.is_ready ? 0xAA184818 : 0xAA481818;
            ui_->add_button({ready_x, row_y}, {ready_w, slot_h}, ready_txt, [this, &nm, s]() {
                nm.update_my_slot(s.god, s.team, s.color_idx, !s.is_ready);
                create_multiplayer_lobby_menu();
            }, ready_col, 0xAA386838);

        } else if (is_host) {
            // Sou host gerenciando outro slot
            if (s.is_open && s.is_human && !s.name.empty()) {
                // Outro jogador humano conectado
                char buf[128];
                std::snprintf(buf, sizeof(buf), "#%zu %s | %s | Eq %d | %s (%dms)",
                              i + 1, s.name.c_str(), net::major_god_to_string(s.god),
                              s.team, s.is_ready ? "Pronto" : "Aguardando", s.ping_ms);
                ui_->add_label(buf, {slot_x, row_y + 8.0f}, 14.0f, s.is_ready ? 0xFFFFFFFF : 0xFFAAAAAA);
            } else {
                // Slot vazio ou Bot de IA gerenciavel pelo Host
                std::string slot_desc;
                if (!s.is_open) slot_desc = "#" + std::to_string(i + 1) + " [Slot Fechado]";
                else if (!s.is_human) slot_desc = "#" + std::to_string(i + 1) + " Bot IA (" + net::ai_difficulty_to_string(s.ai_diff) + ") - " + net::major_god_to_string(s.god);
                else slot_desc = "#" + std::to_string(i + 1) + " [Slot Aberto para Jogador]";

                ui_->add_button({slot_x, row_y}, {slot_w, slot_h}, slot_desc, [this, &nm, i, s]() {
                    // Ciclo: Aberto Humano -> Bot Moderado -> Bot Dificil -> Fechado -> Aberto
                    if (s.is_open && s.is_human) {
                        nm.set_slot_state(i, true, false, net::AIDifficulty::MODERATE);
                    } else if (s.is_open && !s.is_human && s.ai_diff == net::AIDifficulty::MODERATE) {
                        nm.set_slot_state(i, true, false, net::AIDifficulty::HARD);
                    } else if (s.is_open && !s.is_human) {
                        nm.set_slot_state(i, false, false);
                    } else {
                        nm.set_slot_state(i, true, true);
                    }
                    create_multiplayer_lobby_menu();
                }, 0x88202020, 0x88404040);
            }
        } else {
            // Sou cliente visualizando slot de outro jogador / bot / fechado
            char buf[128];
            if (!s.is_open) {
                std::snprintf(buf, sizeof(buf), "#%zu [Slot Fechado]", i + 1);
            } else if (!s.is_human) {
                std::snprintf(buf, sizeof(buf), "#%zu Bot IA (%s) | %s | Eq %d",
                              i + 1, net::ai_difficulty_to_string(s.ai_diff),
                              net::major_god_to_string(s.god), s.team);
            } else if (s.name.empty()) {
                std::snprintf(buf, sizeof(buf), "#%zu [Aguardando Jogador...]", i + 1);
            } else {
                std::snprintf(buf, sizeof(buf), "#%zu %s | %s | Eq %d | %s",
                              i + 1, s.name.c_str(), net::major_god_to_string(s.god),
                              s.team, s.is_ready ? "Pronto" : "Aguardando");
            }
            ui_->add_label(buf, {slot_x, row_y + 8.0f}, 14.0f, s.is_ready ? 0xFFFFFFFF : 0xFF999999);
        }
    }

    // ── Coluna Direita: Configuracoes & Bate-Papo (Taunts) ─────────────
    const float right_x = vw * 0.66f;
    const float right_w = vw * 0.29f;
    float right_y = vh * 0.14f;

    ui_->add_label("CONFIGURACOES DA SALA:", {right_x, right_y}, 18.0f, 0xFFFFD000);
    right_y += 28.0f;

    // Mapa
    std::string map_txt = "Mapa: " + settings.map_name + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, map_txt, [this, &nm, settings]() mutable {
            static const char* MAPS[] = { "Mapa Aleatorio", "Alfheim", "Oasis", "Mediterraneo", "Lago Fantasma", "Savana", "Acropole" };
            static int map_idx = 0;
            map_idx = (map_idx + 1) % 7;
            settings.map_name = MAPS[map_idx];
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(map_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 42.0f;

    // Modo de Jogo
    std::string mode_txt = "Modo: " + std::string(net::game_mode_to_string(settings.game_mode)) + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, mode_txt, [this, &nm, settings]() mutable {
            uint8_t next_mode = (static_cast<uint8_t>(settings.game_mode) + 1) % 4;
            settings.game_mode = static_cast<net::GameMode>(next_mode);
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(mode_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 42.0f;

    // Limite de Populacao
    std::string pop_txt = "Populacao Max: " + std::to_string(settings.pop_cap) + (is_host ? " >" : "");
    if (is_host) {
        ui_->add_button({right_x, right_y}, {right_w, 36.0f}, pop_txt, [this, &nm, settings]() mutable {
            settings.pop_cap = (settings.pop_cap == 150) ? 200 : (settings.pop_cap == 200 ? 300 : 150);
            nm.update_settings(settings);
            create_multiplayer_lobby_menu();
        });
    } else {
        ui_->add_label(pop_txt, {right_x, right_y + 8.0f}, 15.0f, 0xFFFFFFFF);
    }
    right_y += 50.0f;

    // Bate-papo & Taunts Originais AoM
    ui_->add_label("BATE-PAPO / PROVOCACOES:", {right_x, right_y}, 16.0f, 0xFFFFD000);
    right_y += 24.0f;

    // Exibir ultimas 3 mensagens
    size_t start_msg = chat.size() > 3 ? chat.size() - 3 : 0;
    for (size_t i = start_msg; i < chat.size(); ++i) {
        std::string line = chat[i].sender + ": " + chat[i].text;
        ui_->add_label(line, {right_x, right_y}, 13.0f, chat[i].color);
        right_y += 18.0f;
    }
    right_y = std::max(right_y, vh * 0.48f);

    // Botoes de Provocacao Rapida (Taunts)
    float tw = (right_w - 12.0f) / 3.0f;
    ui_->add_button({right_x, right_y}, {tw, 32.0f}, "1 (Sim)", [&nm]() { nm.send_chat("1"); });
    ui_->add_button({right_x + tw + 6.0f, right_y}, {tw, 32.0f}, "2 (Nao)", [&nm]() { nm.send_chat("2"); });
    ui_->add_button({right_x + (tw + 6.0f) * 2.0f, right_y}, {tw, 32.0f}, "11 (Risos)", [&nm]() { nm.send_chat("11"); });
    right_y += 36.0f;

    ui_->add_button({right_x, right_y}, {tw, 32.0f}, "14 (Inicie!)", [&nm]() { nm.send_chat("14"); });
    ui_->add_button({right_x + tw + 6.0f, right_y}, {tw, 32.0f}, "25 (Ataque)", [&nm]() { nm.send_chat("25"); });
    ui_->add_button({right_x + (tw + 6.0f) * 2.0f, right_y}, {tw, 32.0f}, "33 (GG)", [&nm]() { nm.send_chat("33"); });

    // ── Barra Inferior: Acoes ──────────────────────────────────────────
    if (is_host) {
        float launch_w = 300.0f;
        ui_->add_button({(vw - launch_w) * 0.5f, vh * 0.86f}, {launch_w, 46.0f},
                       "INICIAR PARTIDA (LAUNCH)", [this, &nm]() {
            if (nm.launch_game()) {
                start_game();
            }
        }, 0xAA664411, 0xAA996622);
    }

    ui_->add_button({slot_x, vh * 0.86f}, {200.0f, 46.0f}, "SAIR DA SALA", [this, &nm]() {
        nm.leave_session();
        set_state(EngineState::MULTIPLAYER_LAN);
    });
}

void Engine::create_cinematics_menu() {
    replay_opening_cinematics();
}

void Engine::create_history_menu() {
    open_credits();
}

void Engine::create_editor_menu() {
    start_game();
}

void Engine::create_menu_screen(
    const std::string& title,
    const std::string& subtitle,
    const std::vector<std::pair<std::string, std::function<void()>>>& actions,
    const std::function<void()>& back_action) {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    if (!title.empty()) {
        ui_->add_label(title, {vw * 0.5f - 110.0f, vh * 0.12f}, 32.0f, 0xFFFFD000);
    }
    if (!subtitle.empty()) {
        ui_->add_label(subtitle, {vw * 0.5f - 140.0f, vh * 0.18f}, 18.0f, 0xFFCCCCCC);
    }

    float y = vh * 0.28f;
    const float btn_w = 280.0f;
    const float btn_h = 44.0f;
    const float gap = 12.0f;
    for (const auto& act : actions) {
        ui_->add_button({(vw - btn_w) * 0.5f, y}, {btn_w, btn_h}, act.first, act.second);
        y += btn_h + gap;
    }

    float back_w = 200.0f;
    float back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.84f}, {back_w, back_h}, "VOLTAR",
                   back_action ? back_action : [this]() { return_to_main_menu(); });
}

void Engine::replay_opening_cinematics() {
    cinematic_queue_.clear();
    cinematic_queue_idx_ = 0;
    cinematic_queue_.push_back("assets/avi/aom-logo.mp4");
    cinematic_queue_.push_back("assets/avi/introcg.mp4");
    if (!try_start_cinematic_queue()) {
        set_state(EngineState::MAIN_MENU);
    }
}

void Engine::play_titans_cinematics() {
    cinematic_queue_.clear();
    cinematic_queue_idx_ = 0;
    cinematic_queue_.push_back("assets/avi/xintrocg.mp4");
    if (!try_start_cinematic_queue()) {
        set_state(EngineState::MAIN_MENU);
    }
}

void Engine::play_outtakes_cinematic() {
    cinematic_queue_.clear();
    cinematic_queue_idx_ = 0;
    cinematic_queue_.push_back("assets/avi/outtakes.mp4");
    if (!try_start_cinematic_queue()) {
        set_state(EngineState::MAIN_MENU);
    }
}

void Engine::play_credits_cinematic() {
    open_credits();
}

void Engine::play_titans_credits_cinematic() {
    open_credits();
}

void Engine::play_named_cinematic(const char* method_name) {
    if (!method_name) return;
    cinematic_queue_.clear();
    cinematic_queue_idx_ = 0;
    cinematic_queue_.push_back(std::string("assets/avi/") + method_name + ".mp4");
    if (!try_start_cinematic_queue()) {
        set_state(EngineState::MAIN_MENU);
    }
}

TextureHandle Engine::load_ddt_texture(const std::string& material_name) {
    if (!renderer_ || !textures_bar_) return INVALID_TEXTURE;
    auto it = texture_cache_.find(material_name);
    if (it != texture_cache_.end()) return it->second;

    std::string ddt_name = material_name;
    if (ddt_name.find('.') == std::string::npos) ddt_name += ".ddt";

    const std::vector<uint8_t> data = textures_bar_->extract(ddt_name);
    if (data.empty()) return INVALID_TEXTURE;

    auto ddt = DdtLoader::load(data);
    if (!ddt) return INVALID_TEXTURE;
    auto rgba = ddt->decode_rgba(0);
    if (rgba.empty()) return INVALID_TEXTURE;

    TextureHandle handle = renderer_->create_texture(
        ddt->width, ddt->height, rgba.data());
    if (handle != INVALID_TEXTURE) {
        texture_cache_[material_name] = handle;
    }
    return handle;
}

bool Engine::load_lighting(const std::string& path) {
    (void)path;
    return true;
}

bool Engine::load_strings(const std::string& path) {
    (void)path;
    return true;
}

std::string Engine::tr(const std::string& key) const {
    auto it = strings_.find(key);
    if (it != strings_.end()) return it->second;
    return key;
}

void Engine::generate_terrain() {
    if (!renderer_) return;
    load_textures();

    const int GRID = 48;
    const float SIZE = 48.0f;
    const float STEP = SIZE / static_cast<float>(GRID);
    const float HALF = SIZE * 0.5f;

    auto calc_elevation = [](float wx, float wz) -> float {
        // Base rolling terrain
        float hills = std::sin(wx * 0.16f) * std::cos(wz * 0.16f) * 0.75f;
        // Central river valley dipping below water level (-0.4f)
        float river_dist = std::abs(wz - std::sin(wx * 0.14f) * 3.5f);
        float river = (river_dist < 5.0f) ? ((river_dist - 5.0f) * 0.18f) : 0.0f;
        // Elevated cliff/plateau on corner
        float cliff = (wx > 6.0f && wz < -4.0f) ? (std::sin((wx - 6.0f) * 0.28f) * 2.2f) : 0.0f;
        return hills + river + cliff;
    };

    std::vector<TerrainVertex> verts;
    verts.reserve((GRID + 1) * (GRID + 1));
    std::vector<uint32_t> indices;
    indices.reserve(GRID * GRID * 6);

    for (int z = 0; z <= GRID; ++z) {
        for (int x = 0; x <= GRID; ++x) {
            float wx = -HALF + x * STEP;
            float wz = -HALF + z * STEP;
            float wy = calc_elevation(wx, wz);

            // Finite difference normal computation
            float hL = calc_elevation(wx - STEP, wz);
            float hR = calc_elevation(wx + STEP, wz);
            float hD = calc_elevation(wx, wz - STEP);
            float hU = calc_elevation(wx, wz + STEP);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * STEP, hD - hU));

            // Vertex color & slope blend weight
            float slope = std::clamp(1.0f - normal.y, 0.0f, 1.0f);
            float alpha = std::clamp(1.0f - slope * 2.8f, 0.0f, 1.0f);
            float tint = 0.92f + std::sin(wx * 0.4f + wz * 0.4f) * 0.08f;

            TerrainVertex v;
            v.position = glm::vec3(wx, wy, wz);
            v.normal = normal;
            v.texcoord = glm::vec2(static_cast<float>(x) / GRID * 8.0f, static_cast<float>(z) / GRID * 8.0f);
            v.color = glm::vec4(tint, tint, tint, alpha);
            verts.push_back(v);
        }
    }

    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            uint32_t tl = z * (GRID + 1) + x;
            uint32_t tr = tl + 1;
            uint32_t bl = (z + 1) * (GRID + 1) + x;
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    terrain_mesh_ = renderer_->create_terrain_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                                   indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated authentic AoM 3D terrain mesh (%zu verts, %zu indices)", verts.size(), indices.size());
}

// ── P2.1: Terreno Real — Loader de Height-Map de Cenários .scn ───────────────
bool Engine::load_scenario_heightmap(const std::string& scenario_path) {
    if (!renderer_) return false;

    // Ler arquivo do scenarios.bar ou do filesystem
    std::vector<uint8_t> data;
    if (scenarios_bar_ && scenarios_bar_->file_count() > 0) {
        // Preferir o cenário pedido: a campanha usa 'spc\aomsp04.scn' e o
        // height-map precisa ser o do mapa realmente carregado. Sem isso,
        // o primeiro '.scn' do arquivo era usado e o terreno não batia com a
        // partida (ou nem existia, pois o archive usa separadores '\').
        const BarEntry* wanted = scenarios_bar_->find(scenario_path);
        if (wanted) {
            data = scenarios_bar_->extract(*wanted);
            if (!data.empty()) {
                LOGI("Height-map: usando o cenario pedido '%s'", wanted->filename.c_str());
            }
        }
        if (data.empty()) {
            // Fallback: primeiro '.scn' disponivel no archive
            for (const BarEntry& entry : scenarios_bar_->entries()) {
                if (ends_with(to_lower(entry.filename), ".scn")) {
                    data = scenarios_bar_->extract(entry);
                    if (!data.empty()) {
                        LOGW("Height-map: cenario '%s' nao encontrado no BAR, usando '%s'",
                             scenario_path.c_str(), entry.filename.c_str());
                        break;
                    }
                }
            }
        }
    }
    if (data.empty()) {
        // Fallback: tentar ler diretamente do filesystem
        std::ifstream file(scenario_path, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        size_t sz = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        data.resize(sz);
        file.read(reinterpret_cast<char*>(data.data()), sz);
    }

    if (data.size() < 64) {
        LOGE("Scenario file too small for height-map: %zu bytes", data.size());
        return false;
    }

    // Verificar magic do formato .scn (R3SG / SM4 / RTS3.exe)
    bool has_r3sg = false, has_sm4 = false, has_rts3 = false;
    for (size_t i = 0; i + 4 <= data.size() && i < 1024; ++i) {
        if (std::memcmp(data.data() + i, "R3SG", 4) == 0) has_r3sg = true;
        if (std::memcmp(data.data() + i, "SM4", 3) == 0) has_sm4 = true;
        if (std::memcmp(data.data() + i, "RTS3", 4) == 0) has_rts3 = true;
    }

    if (!has_r3sg && !has_sm4 && !has_rts3) {
        LOGE("Scenario file does not contain valid AoM .scn magic");
        return false;
    }

    LOGI("Scenario magic found: R3SG=%d SM4=%d RTS3=%d, size=%zu bytes",
         has_r3sg, has_sm4, has_rts3, data.size());

    // ── Extrair height-map do formato .scn ──────────────────────────────────
    // O formato .scn do Age of Mythology armazena o height-map como uma grade
    // de valores de elevação (float32 ou int16) após o cabeçalho.
    // A localização exata varia entre versões, então tentamos múltiplos offsets.

    const int HM_WIDTH = 64;
    const int HM_HEIGHT = 64;
    const size_t HM_COUNT = static_cast<size_t>(HM_WIDTH) * HM_HEIGHT;
    const size_t HM_DATA_SIZE = HM_COUNT * sizeof(float);

    // Procurar por dados de height-map após o cabeçalho.
    //
    // A busca valida a GRADE INTEIRA, não apenas o primeiro valor: um único
    // float "razoável" aparece por acaso em qualquer blob binário, e acceptá-lo
    // gerava um terreno com elevações de ~1e38 (cena invisível e vértices
    // degenerados). Só é aceito um offset em que todos os 4096 valores são
    // finitos, cabem numa faixa de elevação plausível e apresentam variação
    // real — um grid constante é padding/zeros e não um mapa.
    constexpr float kMinElevation = -500.0f;
    constexpr float kMaxElevation = 3000.0f;
    constexpr float kMinVariation = 0.05f;

    auto read_value = [&](size_t offset, size_t index, bool as_int16) -> float {
        if (as_int16) {
            int16_t raw;
            std::memcpy(&raw, data.data() + offset + index * sizeof(int16_t), sizeof(int16_t));
            return static_cast<float>(raw);
        }
        float raw;
        std::memcpy(&raw, data.data() + offset + index * sizeof(float), sizeof(float));
        return raw;
    };

    // Amostragem rápida: descarta o offset logo que um valor fora da faixa
    // aparece, sem varrer a grade inteira (o .scn tem ~1 MB e a busca é feita
    // sobre cada byte). A amostragem também exige variação entre os valores
    // — regiões de padding (zeros) são rejeitadas de imediato, o que evita
    // varreduras completas sobre áreas chapadas do arquivo.
    auto sample_is_sane = [&](size_t offset, bool as_int16) -> bool {
        static const size_t kSamples[] = {0, 1, 1023, 1024, 2047, 2048, 4094, 4095};
        float lo = 0.0f;
        float hi = 0.0f;
        for (size_t i = 0; i < sizeof(kSamples) / sizeof(kSamples[0]); ++i) {
            const float v = read_value(offset, kSamples[i], as_int16);
            if (as_int16) {
                if (v < -5000.0f || v > 30000.0f) return false;
            } else if (!std::isfinite(v) || v < kMinElevation || v > kMaxElevation) {
                return false;
            }
            if (i == 0) { lo = hi = v; }
            else { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
        return (hi - lo) > kMinVariation;
    };

    auto grid_is_sane = [&](size_t offset, bool as_int16) -> bool {
        float lo = 0.0f;
        float hi = 0.0f;
        for (size_t i = 0; i < HM_COUNT; ++i) {
            const float v = read_value(offset, i, as_int16);
            if (as_int16) {
                if (v < -5000.0f || v > 30000.0f) return false;
            } else if (!std::isfinite(v) || v < kMinElevation || v > kMaxElevation) {
                return false;
            }
            if (i == 0) { lo = hi = v; }
            else { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
        // Uma região constante (padding/zeros) não é um mapa.
        return (hi - lo) > kMinVariation;
    };

    size_t hm_offset = std::string::npos;
    bool hm_is_int16 = false;

    // A busca fica restrita aos primeiros 512 KB do .scn: no archive do AoM
    // os dados do mapa estão nessa faixa, e limitar a janela mantém o custo
    // previsível na thread da UI (o setup da partida não pode se aproximar
    // do limite de ANR do Android).
    constexpr size_t kScanWindow = 512 * 1024;
    const size_t scan_end = std::min(data.size(), kScanWindow);

    for (size_t offset = 128; offset + HM_DATA_SIZE <= scan_end; offset += 4) {
        if (!sample_is_sane(offset, false)) continue;
        if (grid_is_sane(offset, false)) {
            hm_offset = offset;
            break;
        }
    }

    if (hm_offset == std::string::npos) {
        // Tentar ler como int16 (formato alternativo)
        const size_t HM_DATA_SIZE_16 = HM_COUNT * sizeof(int16_t);
        for (size_t offset = 128; offset + HM_DATA_SIZE_16 <= scan_end; offset += 2) {
            if (!sample_is_sane(offset, true)) continue;
            if (grid_is_sane(offset, true)) {
                hm_offset = offset;
                hm_is_int16 = true;
                break;
            }
        }
        if (hm_offset == std::string::npos) {
            LOGW("Could not locate a sane height-map grid in .scn file (%zu bytes)", data.size());
            return false;
        }
    }

    // Extrair dados de elevação
    heightmap_width_ = HM_WIDTH;
    heightmap_height_ = HM_HEIGHT;
    heightmap_scale_ = hm_is_int16 ? 0.1f : 1.0f;
    heightmap_data_.resize(HM_COUNT);

    if (hm_is_int16) {
        // Formato int16
        for (size_t i = 0; i < HM_COUNT; ++i) {
            int16_t val;
            std::memcpy(&val, data.data() + hm_offset + i * sizeof(int16_t), sizeof(int16_t));
            heightmap_data_[i] = static_cast<float>(val) * heightmap_scale_;
        }
    } else {
        // Formato float32
        for (size_t i = 0; i < HM_COUNT; ++i) {
            std::memcpy(&heightmap_data_[i], data.data() + hm_offset + i * sizeof(float), sizeof(float));
            if (!std::isfinite(heightmap_data_[i])) heightmap_data_[i] = 0.0f;
        }
    }

    LOGI("Loaded height-map from .scn: %dx%d, offset=%zu, escala=%.2f, range=[%.2f, %.2f]",
         heightmap_width_, heightmap_height_, hm_offset, heightmap_scale_,
         *std::min_element(heightmap_data_.begin(), heightmap_data_.end()),
         *std::max_element(heightmap_data_.begin(), heightmap_data_.end()));

    // Gerar malha de terreno a partir do height-map
    generate_terrain_from_heightmap(heightmap_data_, heightmap_width_, heightmap_height_, 1.0f);

    return true;
}

void Engine::generate_terrain_from_heightmap(const std::vector<float>& heightmap, int width, int height, float scale) {
    if (!renderer_) return;
    if (width <= 0 || height <= 0 || heightmap.empty()) return;

    const float STEP = scale;
    const float HALF_W = (width * STEP) * 0.5f;
    const float HALF_H = (height * STEP) * 0.5f;

    // Normaliza a amplitude vertical para a mesma escala do terreno
    // procedural (~±3 unidades). As elevações do .scn vêm em unidades
    // próprias do formato e podem chegar a centenas ou milhares; sem esta
    // normalização a malha ficaria com picos verticais e a câmera ficaria
    // dentro ou fora do terreno.
    std::vector<float> heights = heightmap;
    const float h_min = *std::min_element(heights.begin(), heights.end());
    const float h_max = *std::max_element(heights.begin(), heights.end());
    constexpr float kTargetAmplitude = 3.0f;
    if (h_max > h_min) {
        const float factor = kTargetAmplitude / (h_max - h_min);
        const float mid = (h_max + h_min) * 0.5f;
        for (float& h : heights) h = (h - mid) * factor;
    } else {
        std::fill(heights.begin(), heights.end(), 0.0f);
    }

    std::vector<TerrainVertex> verts;
    verts.reserve(static_cast<size_t>(width) * height);
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(width) * height * 6);

    auto get_height = [&](int x, int z) -> float {
        if (x < 0 || x >= width || z < 0 || z >= height) return 0.0f;
        return heights[static_cast<size_t>(z) * width + x];
    };

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            float wx = -HALF_W + x * STEP;
            float wz = -HALF_H + z * STEP;
            float wy = get_height(x, z);

            // Finite difference normal computation
            float hL = get_height(x - 1, z);
            float hR = get_height(x + 1, z);
            float hD = get_height(x, z - 1);
            float hU = get_height(x, z + 1);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * STEP, hD - hU));

            // Vertex color & slope blend weight (splatting: grama em áreas planas, rocha em encostas)
            float slope = std::clamp(1.0f - normal.y, 0.0f, 1.0f);
            float alpha = std::clamp(1.0f - slope * 2.8f, 0.0f, 1.0f); // Alpha para blend de textura
            float tint = 0.92f;

            TerrainVertex v;
            v.position = glm::vec3(wx, wy, wz);
            v.normal = normal;
            v.texcoord = glm::vec2(static_cast<float>(x) / width * 8.0f,
                                   static_cast<float>(z) / height * 8.0f);
            v.color = glm::vec4(tint, tint, tint, alpha);
            verts.push_back(v);
        }
    }

    for (int z = 0; z < height - 1; ++z) {
        for (int x = 0; x < width - 1; ++x) {
            uint32_t tl = static_cast<uint32_t>(z * width + x);
            uint32_t tr = tl + 1;
            uint32_t bl = static_cast<uint32_t>((z + 1) * width + x);
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    terrain_mesh_ = renderer_->create_terrain_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                                   indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated terrain from height-map: %dx%d, %zu verts, %zu indices",
         width, height, verts.size(), indices.size());
}

void Engine::generate_skydome() {
    if (!renderer_) return;
    const int SLICES = 24;
    const int STACKS = 12;
    const float RADIUS = 65.0f;

    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    for (int i = 0; i <= STACKS; ++i) {
        float phi = (static_cast<float>(i) / STACKS) * (3.14159265f * 0.5f);
        float cos_phi = std::cos(phi);
        float sin_phi = std::sin(phi);
        float ratio = static_cast<float>(i) / STACKS;

        glm::vec4 col = glm::mix(glm::vec4(0.85f, 0.80f, 0.68f, 1.0f), glm::vec4(0.20f, 0.45f, 0.85f, 1.0f), ratio);

        for (int j = 0; j <= SLICES; ++j) {
            float theta = (static_cast<float>(j) / SLICES) * 2.0f * 3.14159265f;
            float cos_theta = std::cos(theta);
            float sin_theta = std::sin(theta);

            Vertex v;
            v.position = glm::vec3(RADIUS * cos_phi * cos_theta, RADIUS * sin_phi - 5.0f, RADIUS * cos_phi * sin_theta);
            v.normal = glm::normalize(-v.position);
            v.texcoord = glm::vec2(static_cast<float>(j) / SLICES, ratio);
            v.color = col;
            verts.push_back(v);
        }
    }

    for (int i = 0; i < STACKS; ++i) {
        for (int j = 0; j < SLICES; ++j) {
            uint32_t first = i * (SLICES + 1) + j;
            uint32_t second = first + SLICES + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
        }
    }

    skydome_mesh_ = renderer_->create_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                           indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated celestial skydome mesh (%zu verts, %zu indices)", verts.size(), indices.size());
}

void Engine::create_in_game_hud() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    // ── 1. Top Resource Banner (y = 8..36) ──────────────────────────
    if (show_fps_) {
        fps_label_ = ui_->add_label("FPS: 60", {16.0f, 9.0f}, 16.0f, 0xFF88FF88);
    } else {
        fps_label_ = nullptr;
    }

    float res_x = 110.0f;
    const float res_y = 9.0f;
    const float res_step = std::min(150.0f, (vw - 320.0f) / 5.0f);

    hud_food_label_ = ui_->add_label("Comida: 500", {res_x, res_y}, 16.0f, 0xFF55DD33);
    res_x += res_step;
    hud_wood_label_ = ui_->add_label("Madeira: 500", {res_x, res_y}, 16.0f, 0xFFD49B55);
    res_x += res_step;
    hud_gold_label_ = ui_->add_label("Ouro: 300", {res_x, res_y}, 16.0f, 0xFFFFD700);
    res_x += res_step;
    hud_favor_label_ = ui_->add_label("Favor: 100", {res_x, res_y}, 16.0f, 0xFFCC77FF);
    res_x += res_step;
    hud_pop_label_ = ui_->add_label("Pop: 0/25", {res_x, res_y}, 16.0f, 0xFFFFFFFF);

    // Menu button at top-right
    ui_->add_button({vw - 110.0f, 5.0f}, {96.0f, 26.0f}, "MENU",
                    [this]() { set_state(EngineState::MAIN_MENU); }, 0xAA221105, 0xAA44220A);

    // ── 2. Bottom RTS Selection & Action Panel (y = vh - 96) ──────────
    const float panel_y = vh - 96.0f;
    hud_sel_title_ = ui_->add_label("Toque em uma unidade para selecionar", {160.0f, panel_y + 8.0f}, 17.0f, 0xFFFFD000);
    hud_sel_hp_    = ui_->add_label("", {160.0f, panel_y + 32.0f}, 15.0f, 0xFFFFFFFF);
    hud_sel_info_  = ui_->add_label("", {160.0f, panel_y + 54.0f}, 14.0f, 0xFF88DDFF);

    // 4 Action Buttons on the bottom-right
    const float btn_w = 140.0f;
    const float btn_h = 36.0f;
    const float btn_gap = 8.0f;
    float btn_start_x = std::max(520.0f, vw - (btn_w * 2.0f + btn_gap + 16.0f));

    hud_action_btn1_ = ui_->add_button({btn_start_x, panel_y + 8.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn2_ = ui_->add_button({btn_start_x + btn_w + btn_gap, panel_y + 8.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn3_ = ui_->add_button({btn_start_x, panel_y + 8.0f + btn_h + 4.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);
    hud_action_btn4_ = ui_->add_button({btn_start_x + btn_w + btn_gap, panel_y + 8.0f + btn_h + 4.0f}, {btn_w, btn_h}, "", nullptr, 0x00000000, 0x00000000);

    // ── 3. Campaign & Mission HUD references ──────────────────────────
    hud_obj_title_        = ui_->add_label("", {vw * 0.5f - 240.0f, 38.0f}, 16.0f, 0xFFFFE066);
    hud_reinforce_label_  = ui_->add_label("", {vw - 230.0f, 38.0f}, 15.0f, 0xFF88DDFF);
    hud_subtitle_speaker_ = ui_->add_label("", {vw * 0.5f - 260.0f, vh - 138.0f}, 16.0f, 0xFFFFD700);
    hud_subtitle_text_    = ui_->add_label("", {vw * 0.5f - 260.0f, vh - 114.0f}, 14.0f, 0xFFFFFFFF);
    hud_skip_btn_         = ui_->add_button({vw * 0.5f - 90.0f, vh - 52.0f}, {180.0f, 36.0f}, "", nullptr, 0x00000000, 0x00000000);

    last_selected_entity_ = INVALID_ENTITY;
}

void Engine::update_in_game_hud(float dt) {
    (void)dt;
    if (!gameplay_world_ || state_ != EngineState::IN_GAME) return;

    // 1. Update Top Banner: Food, Wood, Gold, Favor, Pop/Cap
    const auto& res = gameplay_world_->resources();
    int cur_pop = gameplay_world_->current_population(Team::FRIENDLY);
    int max_pop = gameplay_world_->population_cap(Team::FRIENDLY);

    char buf[64];
    if (hud_food_label_) {
        std::snprintf(buf, sizeof(buf), "Comida: %d", res.food);
        hud_food_label_->text = buf;
    }
    if (hud_wood_label_) {
        std::snprintf(buf, sizeof(buf), "Madeira: %d", res.wood);
        hud_wood_label_->text = buf;
    }
    if (hud_gold_label_) {
        std::snprintf(buf, sizeof(buf), "Ouro: %d", res.gold);
        hud_gold_label_->text = buf;
    }
    if (hud_favor_label_) {
        std::snprintf(buf, sizeof(buf), "Favor: %d", res.favor);
        hud_favor_label_->text = buf;
    }
    if (hud_pop_label_) {
        std::snprintf(buf, sizeof(buf), "Pop: %d/%d", cur_pop, max_pop);
        hud_pop_label_->text = buf;
        hud_pop_label_->color = (cur_pop >= max_pop) ? 0xFFFF4444 : 0xFFFFFFFF;
    }

    // 2. Selection & Action Panel
    Entity sel = gameplay_world_->selected_entity();

    auto hide_action_buttons = [this]() {
        if (hud_action_btn1_) { hud_action_btn1_->label = ""; hud_action_btn1_->onClick = nullptr; hud_action_btn1_->normal_color = 0x00000000; }
        if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->onClick = nullptr; hud_action_btn2_->normal_color = 0x00000000; }
        if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->onClick = nullptr; hud_action_btn3_->normal_color = 0x00000000; }
        if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->onClick = nullptr; hud_action_btn4_->normal_color = 0x00000000; }
    };

    if (sel == INVALID_ENTITY) {
        if (hud_sel_title_) hud_sel_title_->text = "Nenhuma unidade selecionada";
        if (hud_sel_hp_) hud_sel_hp_->text = "";
        if (hud_sel_info_) hud_sel_info_->text = (placement_mode_ != PlacementMode::NONE) ? "Toque no terreno para construir!" : "";
        hide_action_buttons();
        last_selected_entity_ = INVALID_ENTITY;
        return;
    }

    const auto* unit = gameplay_world_->get_unit(sel);
    const auto* bld  = gameplay_world_->get_building(sel);
    const auto* hp   = gameplay_world_->get_health(sel);

    if (unit) {
        if (hud_sel_title_) {
            std::string team_str = (unit->team == Team::FRIENDLY) ? " (Aliado)" : " (Inimigo)";
            hud_sel_title_->text = unit->unit_type_name + team_str;
            hud_sel_title_->color = (unit->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
        }
        if (hud_sel_hp_ && hp) {
            std::snprintf(buf, sizeof(buf), "HP: %d / %d", hp->current_hp, hp->max_hp);
            hud_sel_hp_->text = buf;
        }
        if (hud_sel_info_) {
            if (unit->worker) {
                if (unit->returning_cargo) {
                    const char* rname = (unit->carried_resource_type == ResourceType::FOOD) ? "Comida" :
                                        (unit->carried_resource_type == ResourceType::WOOD) ? "Madeira" :
                                        (unit->carried_resource_type == ResourceType::GOLD) ? "Ouro" : "Favor";
                    std::snprintf(buf, sizeof(buf), "Depositando: %d/10 %s no Centro", unit->carried_resource_amount, rname);
                    hud_sel_info_->text = buf;
                } else if (unit->carried_resource_amount > 0) {
                    const char* rname = (unit->carried_resource_type == ResourceType::FOOD) ? "Comida" :
                                        (unit->carried_resource_type == ResourceType::WOOD) ? "Madeira" :
                                        (unit->carried_resource_type == ResourceType::GOLD) ? "Ouro" : "Favor";
                    std::snprintf(buf, sizeof(buf), "Carga: %d/10 %s", unit->carried_resource_amount, rname);
                    hud_sel_info_->text = buf;
                } else if (unit->gather_node >= 0) {
                    hud_sel_info_->text = "Coletando recurso...";
                } else if (unit->build_target != INVALID_ENTITY) {
                    hud_sel_info_->text = "Construindo estrutura...";
                } else {
                    hud_sel_info_->text = "Trabalhador ocioso";
                }
            } else {
                std::snprintf(buf, sizeof(buf), "Ataque: %.0f  Alcance: %.1f", unit->attack_damage, unit->attack_range);
                hud_sel_info_->text = buf;
            }
        }

        // Action Buttons for Worker
        if (unit->team == Team::FRIENDLY) {
            if (unit->worker) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Casa (100M)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this]() {
                        placement_mode_ = PlacementMode::HOUSE;
                        LOGI("Placement mode: HOUSE");
                    };
                }
                if (hud_action_btn2_) {
                    hud_action_btn2_->label = "Quartel (100M 50O)";
                    hud_action_btn2_->normal_color = 0xDD2A1C0E;
                    hud_action_btn2_->onClick = [this]() {
                        placement_mode_ = PlacementMode::BARRACKS;
                        LOGI("Placement mode: BARRACKS");
                    };
                }
                if (hud_action_btn3_) {
                    hud_action_btn3_->label = "Parar";
                    hud_action_btn3_->normal_color = 0xDD3A1C0E;
                    hud_action_btn3_->onClick = [this]() {
                        if (gameplay_world_) gameplay_world_->order_stop_selected();
                    };
                }
                if (hud_action_btn4_) {
                    hud_action_btn4_->label = "";
                    hud_action_btn4_->onClick = nullptr;
                    hud_action_btn4_->normal_color = 0x00000000;
                }
            } else {
                // Military unit
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Parar";
                    hud_action_btn1_->normal_color = 0xDD3A1C0E;
                    hud_action_btn1_->onClick = [this]() {
                        if (gameplay_world_) gameplay_world_->order_stop_selected();
                    };
                }
                if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->onClick = nullptr; hud_action_btn2_->normal_color = 0x00000000; }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->onClick = nullptr; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->onClick = nullptr; hud_action_btn4_->normal_color = 0x00000000; }
            }
        } else {
            hide_action_buttons();
        }
    } else if (bld) {
        if (hud_sel_title_) {
            hud_sel_title_->text = bld->name + ((bld->team == Team::FRIENDLY) ? " (Aliado)" : " (Inimigo)");
            hud_sel_title_->color = (bld->team == Team::FRIENDLY) ? 0xFFFFD000 : 0xFFFF4444;
        }
        if (hud_sel_hp_ && hp) {
            std::snprintf(buf, sizeof(buf), "HP: %d / %d", hp->current_hp, hp->max_hp);
            hud_sel_hp_->text = buf;
        }
        if (hud_sel_info_) {
            if (!bld->complete) {
                std::snprintf(buf, sizeof(buf), "Construcao: %.0f%%", bld->progress * 100.0f);
                hud_sel_info_->text = buf;
            } else if (bld->type == BuildingType::TOWN_CENTER) {
                int q_count = gameplay_world_->get_training_queue_count(sel);
                if (q_count > 0) {
                    float prog = gameplay_world_->get_training_progress(sel);
                    std::snprintf(buf, sizeof(buf), "Treinando: Aldeao (%d na fila, %.0f%%)", q_count, prog * 100.0f);
                    hud_sel_info_->text = buf;
                } else {
                    hud_sel_info_->text = "Fila livre (+15 Populacao)";
                }
            } else if (bld->type == BuildingType::BARRACKS) {
                int q_count = gameplay_world_->get_training_queue_count(sel);
                if (q_count > 0) {
                    float prog = gameplay_world_->get_training_progress(sel);
                    std::snprintf(buf, sizeof(buf), "Treinando: %s (%d fila, %.0f%%)", bld->training_unit.c_str(), q_count, prog * 100.0f);
                    hud_sel_info_->text = buf;
                } else {
                    hud_sel_info_->text = "Quartel pronto para producao";
                }
            } else if (bld->type == BuildingType::HOUSE) {
                hud_sel_info_->text = "Suporta +10 de Populacao";
            } else {
                hud_sel_info_->text = "";
            }
        }

        if (bld->team == Team::FRIENDLY && bld->complete) {
            if (bld->type == BuildingType::TOWN_CENTER) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Aldeao (50C)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            if (gameplay_world_->queue_villager(sel)) {
                                LOGI("Queued Greek Villager at Town Center %u", sel);
                            } else {
                                LOGW("Cannot queue villager: insufficient food or pop cap reached");
                            }
                        }
                    };
                }
                if (hud_action_btn2_) { hud_action_btn2_->label = ""; hud_action_btn2_->onClick = nullptr; hud_action_btn2_->normal_color = 0x00000000; }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->onClick = nullptr; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->onClick = nullptr; hud_action_btn4_->normal_color = 0x00000000; }
            } else if (bld->type == BuildingType::BARRACKS) {
                if (hud_action_btn1_) {
                    hud_action_btn1_->label = "Hoplite (60C 35O)";
                    hud_action_btn1_->normal_color = 0xDD2A1C0E;
                    hud_action_btn1_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            gameplay_world_->start_training_unit(sel, "Hoplite", 60, 0, 35, 2);
                        }
                    };
                }
                if (hud_action_btn2_) {
                    hud_action_btn2_->label = "Toxotes (55M 35O)";
                    hud_action_btn2_->normal_color = 0xDD2A1C0E;
                    hud_action_btn2_->onClick = [this, sel]() {
                        if (gameplay_world_) {
                            gameplay_world_->start_training_unit(sel, "Toxotes", 0, 55, 35, 2);
                        }
                    };
                }
                if (hud_action_btn3_) { hud_action_btn3_->label = ""; hud_action_btn3_->onClick = nullptr; hud_action_btn3_->normal_color = 0x00000000; }
                if (hud_action_btn4_) { hud_action_btn4_->label = ""; hud_action_btn4_->onClick = nullptr; hud_action_btn4_->normal_color = 0x00000000; }
            } else {
                hide_action_buttons();
            }
        } else {
            hide_action_buttons();
        }
    } else {
        if (hud_sel_title_) hud_sel_title_->text = "Nenhuma unidade selecionada";
        if (hud_sel_hp_) hud_sel_hp_->text = "";
        if (hud_sel_info_) hud_sel_info_->text = "";
        hide_action_buttons();
    }

    // ── 3. Update Campaign Mission HUD (Objectives, Subtitles, Reinforcements, Skip button)
    if (mission_mgr_) {
        // Skip Button
        if (hud_skip_btn_) {
            if (mission_mgr_->is_cinematic_active()) {
                hud_skip_btn_->label = "PULAR (SKIP)";
                hud_skip_btn_->normal_color = 0xBB2A4010;
                hud_skip_btn_->pressed_color = 0xEE406018;
                hud_skip_btn_->onClick = [this]() {
                    if (mission_mgr_) mission_mgr_->skip_cinematic(&camera_);
                };
            } else {
                hud_skip_btn_->label = "";
                hud_skip_btn_->normal_color = 0x00000000;
                hud_skip_btn_->pressed_color = 0x00000000;
                hud_skip_btn_->onClick = nullptr;
            }
        }

        // Subtitles
        const auto* sub = mission_mgr_->current_subtitle();
        if (sub) {
            if (hud_subtitle_speaker_) {
                hud_subtitle_speaker_->text = sub->speaker + ":";
                hud_subtitle_speaker_->color = sub->color;
            }
            if (hud_subtitle_text_) {
                hud_subtitle_text_->text = sub->text;
            }
        } else {
            if (hud_subtitle_speaker_) hud_subtitle_speaker_->text = "";
            if (hud_subtitle_text_) hud_subtitle_text_->text = "";
        }

        // Objectives
        if (hud_obj_title_) {
            const auto& objs = mission_mgr_->objectives();
            std::string obj_text;
            for (const auto& obj : objs) {
                if (!obj.completed && obj.primary) {
                    obj_text = "Objetivo: " + obj.description;
                    break;
                }
            }
            if (obj_text.empty() && !objs.empty()) {
                obj_text = "Objetivos Cumpridos!";
            }
            hud_obj_title_->text = obj_text;
        }

        // Reinforcement Countdown
        if (hud_reinforce_label_) {
            if (mission_mgr_->stage() == MissionStage::REINFORCEMENTS_ARRIVED) {
                hud_reinforce_label_->text = "Reforcos: CHEGARAM!";
                hud_reinforce_label_->color = 0xFF76FF03;
            } else {
                int leagues = mission_mgr_->army_leagues_away();
                hud_reinforce_label_->text = "Exercito: " + std::to_string(leagues) + " leguas";
                hud_reinforce_label_->color = 0xFF88DDFF;
            }
        }

        // Victory / Defeat Notice
        if (mission_mgr_->result() == MissionResult::VICTORY) {
            if (hud_sel_title_) {
                hud_sel_title_->text = "VITORIA! ATLANTIDA DEFENDIDA COM SUCESSO!";
                hud_sel_title_->color = 0xFF76FF03;
            }
        } else if (mission_mgr_->result() == MissionResult::DEFEAT) {
            if (hud_sel_title_) {
                hud_sel_title_->text = "DERROTA! ARKANTOS TOMBOU EM BATALHA!";
                hud_sel_title_->color = 0xFFFF3333;
            }
        }
    }
}

} // namespace aom
