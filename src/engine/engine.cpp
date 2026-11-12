#include "engine/engine.h"
#include "engine_internal.h"
#include "assets/brg_model.h"
#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"
#include "assets/xmb_parser.h"
#include "ui/ui.h"
#include "ui/ui_layout.h"
#include "audio/audio.h"
#include "ecs/gameplay.h"
#include "ai/ai_system.h"
#include "anim/animation_system.h"
#include "anim/prt_particle.h"
#include "video/video_player.h"
#include "video/media_decoder.h"
#include "renderer/camera_track.h"
#include "network/network_manager.h"
#include "data/proto_unit.h"
#include "game/save_load.h"
#include "game/victory.h"
#include "game/scenario_trigger.h"
#include "assets/cube_map.h"
#include "aom/campaign_select_system.h"

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
    
    // Initialize Menu System
    menu_system_ = std::make_unique<aom::MenuSystem>();
    menu_system_->initialize(renderer_.get());

    // P5.6: Carrega tabela de strings da UI para localização
    load_ui_strings();

    // 4. Load Options FIRST (before loading archives, to get correct data_path)
    load_options();

    // 4b. Pré-alocar pool de partículas (sem realocações durante o jogo)
    particles_.resize(2000);

    // 5. Initialize VideoPlayer (fallback timer) + MediaDecoder (real frames)
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

    // 6. Load archives AFTER load_options, so data_path is correct
    load_archives(config.data_path);
    load_textures();
    load_models();
    load_game_config();

    // 7. Load Core UI Textures (Splash, Loading, Stone Buttons) — DEPOIS de load_archives
    // para que textures_bar_ esteja disponível e as texturas DDT do textures.bar
    // possam ser encontradas.
    load_core_ui_textures();
    if (ui_ && button_main_tex_ != INVALID_TEXTURE) {
        ui_->set_default_button_textures(button_main_tex_, button_clicked_tex_);
    }

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


// ── Aviso: load_portal_textures() foi removida.
//        Os nomes usados ("ui\portal_main.ddt", etc.) continham sequências de escape inválidas
//        (\p → descarta a barra) e eram chutados, não vindo do XMB.
//        As texturas reais do menu principal são resolvidas via texture_resolver em
//        show_ui_layout(), lendo os nomes direto do uipregamenew.xmb.

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

void Engine::return_to_main_menu() {
    save_options();
    // Limpar estado residual de vitória/derrota antes de voltar ao menu
    skirmish_victory_result_ = VictoryResult::NONE;
    victory_display_timer_   = 0.0f;
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
    } else if (state_ == EngineState::MAIN_MENU) {
        // ── MAIN_MENU: fundo 3D animado (ondas, tropas, câmera panorâmica) ──
        // Câmera orbital panorâmica lenta
        yaw_ += delta_time * 0.02f;  // rotação bem lenta
        const float cx = camera_.target.x + std::sin(yaw_) * std::cos(pitch_) * radius_;
        const float cy = camera_.target.y + std::sin(pitch_) * radius_ + 2.0f;
        const float cz = camera_.target.z + std::cos(yaw_) * std::cos(pitch_) * radius_;
        camera_.position = glm::vec3(cx, cy, cz);

        // Update gameplay world (tropas andando, coletando, ondas)
        if (gameplay_world_) gameplay_world_->update(delta_time);

        // Partículas 3D
        update_particles(delta_time);
        if (prt_particles_) {
            prt_particles_->update(delta_time);
        }

        // Atualiza vídeo de fundo do menu (se houver)
        if (menu_bg_) {
            menu_bg_->update(delta_time);
            if (menu_bg_restart_) {
                menu_bg_restart_ = false;
                start_menu_background();
            }
        }

        // Áudio posicional do listener
        if (audio_) {
            glm::vec3 diff = camera_.target - camera_.position;
            glm::vec3 forward = (glm::length(diff) > 0.001f) ? glm::normalize(diff) : glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            audio_->set_listener(camera_.target, forward, right);
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

        // P5.4: IA do inimigo (economia, construcao, treinamento militar, ataque)
        if (ai_system_ && gameplay_world_) {
            ai_system_->update(delta_time, gameplay_world_.get());
        }

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
                // Sincronizar com o GameplayWorld (para save/load)
                gameplay_world_->set_victory_condition(skirmish_victory_condition_);
                gameplay_world_->set_victory_result(skirmish_victory_result_);
                gameplay_world_->set_victory_timer(victory_display_timer_);
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
            gameplay_world_->set_victory_timer(victory_display_timer_);
            if (victory_display_timer_ >= 2.5f && victory_display_timer_ < 2.5f + delta_time) {
                const float vw = static_cast<float>(config_.window_width);
                const float vh = static_cast<float>(config_.window_height);
                float btn_w = 220.0f, btn_h = 48.0f;
                float cy    = vh * 0.58f;
                ui_->add_button({(vw - btn_w) * 0.5f - 120.0f, cy}, {btn_w, btn_h}, "MENU PRINCIPAL",
                               [this]() {
                                   skirmish_victory_result_ = VictoryResult::NONE;
                                   victory_display_timer_   = 0.0f;
                                   if (gameplay_world_) {
                                       gameplay_world_->set_victory_result(VictoryResult::NONE);
                                       gameplay_world_->set_victory_timer(0.0f);
                                   }
                                   return_to_main_menu();
                               });
                ui_->add_button({(vw - btn_w) * 0.5f + 120.0f, cy}, {btn_w, btn_h}, "JOGAR NOVAMENTE",
                               [this]() {
                                   skirmish_victory_result_ = VictoryResult::NONE;
                                   victory_display_timer_   = 0.0f;
                                   if (gameplay_world_) {
                                       gameplay_world_->set_victory_result(VictoryResult::NONE);
                                       gameplay_world_->set_victory_timer(0.0f);
                                   }
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
        // ── MAIN_MENU com fundo 3D animado (cenário costeiro The Titans) ──────
        // 1. Vídeo de fundo do menu (se disponível) — desenha primeiro para ficar atrás de tudo
        if (menu_bg_tex_ != INVALID_TEXTURE) {
            const float aspect_vid = static_cast<float>(menu_bg_w_) / menu_bg_h_;
            const float aspect_dst = vw / vh;
            float fw = vw, fh = vh;
            if (aspect_dst > aspect_vid) { fw = vh * aspect_vid; }
            else                         { fh = vw / aspect_vid; }
            renderer_->draw_ui_quad({(vw - fw) * 0.5f, (vh - fh) * 0.5f},
                                    {fw, fh}, menu_bg_tex_, 0xFFFFFFFF);
        } else {
            // Imagem de fundo autêntica do menu principal (main_menu.png)
            if (main_menu_tex_ != INVALID_TEXTURE) {
                renderer_->draw_ui_quad({0.0f, 0.0f}, {vw, vh}, main_menu_tex_, 0xFFFFFFFF);
            }
            // 1b. Skybox procedural atmosférico (cubemap ou faces)
            if (skybox_cubemap_ != INVALID_TEXTURE) {
                renderer_->draw_skybox_cubemap(skybox_cubemap_, 65.0f);
            } else {
                renderer_->draw_skybox(skybox_faces_, 65.0f);
            }
        }

        // 2. Terreno 3D & Water Plane
        if (terrain_mesh_ != INVALID_MESH) {
            renderer_->draw_terrain(terrain_mesh_, terrain_texture_, terrain_blend_tex_,
                                 terrain_bank_tex_, terrain_outer_bank_tex_);
        }
        renderer_->draw_water(water_plane_, water_normal_tex_, accumulated_time_, water_color_,
                         -water_plane_.w,  // water_level (Y world)
                         terrain_height_,  // terrain_height
                         water_tex_);      // water texture from textures.bar

        // 3. Entidades 3D (tropas, prédios, animações)
        if (gameplay_world_) {
            gameplay_world_->render(renderer_.get(), model_mesh_, model_texture_, hoplite_mesh_, hoplite_texture_, &camera_);
            gameplay_world_->render_animated(renderer_.get(), &camera_);
            gameplay_world_->render_grn_animated(renderer_.get(), &camera_);
        }

        // 4. Partículas 3D
        render_particles();
        if (prt_particles_) {
            prt_particles_->render(renderer_.get(), prt_particle_tex_);
        }

        // 5. UI por cima (pórtico central + 7 botões)
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

        // 1. Skybox (cubemap se disponível, senão procedural via faces)
        if (skybox_cubemap_ != INVALID_TEXTURE) {
            renderer_->draw_skybox_cubemap(skybox_cubemap_, 65.0f);
        } else {
            renderer_->draw_skybox(skybox_faces_, 65.0f);
        }

        // 2. Authentic AoM 3D Terrain & Water Plane
        if (terrain_mesh_ != INVALID_MESH) {
            renderer_->draw_terrain(terrain_mesh_, terrain_texture_, terrain_blend_tex_,
                                 terrain_bank_tex_, terrain_outer_bank_tex_);
        }
        renderer_->draw_water(water_plane_, water_normal_tex_, accumulated_time_, water_color_,
                         -water_plane_.w,  // water_level (Y world)
                         terrain_height_,  // terrain_height
                         water_tex_);      // water texture from textures.bar

        if (model_mesh_ != INVALID_MESH) {
            glm::mat4 model_transform = glm::mat4(1.0f);
            float angle = static_cast<float>(frame_count_) * 0.015f;
            model_transform = glm::rotate(model_transform, angle, glm::vec3(0, 1, 0));
            renderer_->draw_mesh(model_mesh_, model_transform, model_texture_, BlendMode::OPAQUE);
        }

        // 3. RTS Entities, Buildings, Health Bars, Selection Rings, Tactical Vectors
        if (gameplay_world_) {
            gameplay_world_->render(renderer_.get(), model_mesh_, model_texture_, hoplite_mesh_, hoplite_texture_, &camera_);
            gameplay_world_->render_animated(renderer_.get(), &camera_);
            gameplay_world_->render_grn_animated(renderer_.get(), &camera_);
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

        // 6b. Barra de progresso de producao (fila de treinamento) no painel inferior.
        if (gameplay_world_) {
            Entity sel = gameplay_world_->selected_entity();
            const auto* bld = gameplay_world_->get_building(sel);
            if (bld && bld->team == Team::FRIENDLY && bld->complete &&
                gameplay_world_->get_training_queue_count(sel) > 0) {
                float prog = gameplay_world_->get_training_progress(sel);
                const float bar_x = 160.0f;
                const float bar_y = panel_y + 68.0f;
                const float bar_w = 300.0f;
                const float bar_h = 5.0f;
                // Fundo escuro
                renderer_->draw_ui_quad({bar_x, bar_y}, {bar_w, bar_h}, INVALID_TEXTURE, 0x88221105);
                // Preenchimento dourado proporcional ao progresso
                float fill_w = bar_w * std::clamp(prog, 0.0f, 1.0f);
                if (fill_w > 0.0f) {
                    renderer_->draw_ui_quad({bar_x, bar_y}, {fill_w, bar_h}, INVALID_TEXTURE, 0xFFFFD000);
                }
                // Borda dourada fina
                renderer_->draw_ui_quad({bar_x, bar_y}, {bar_w, 1.0f}, INVALID_TEXTURE, 0xFFFFFFFF);
                renderer_->draw_ui_quad({bar_x, bar_y + bar_h - 1.0f}, {bar_w, 1.0f}, INVALID_TEXTURE, 0xFFFFFFFF);
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

void Engine::invalidate_gpu_handles() {
    // UI
    splash_tex_ = loading_tex_ = main_menu_tex_ = INVALID_TEXTURE;
    button_main_tex_ = button_clicked_tex_ = minimap_tex_ = INVALID_TEXTURE;
    menu_bg_tex_ = INVALID_TEXTURE; menu_bg_w_ = menu_bg_h_ = 0;
    cinematic_frame_tex_ = INVALID_TEXTURE;
    // Modelos / partículas
    model_mesh_ = hoplite_mesh_ = INVALID_MESH;
    model_texture_ = hoplite_texture_ = INVALID_TEXTURE;
    model_cache_.clear();
    prt_particle_tex_ = INVALID_TEXTURE;
    prt_particles_.reset();
    // Céu
    skydome_mesh_ = skybox_mesh_ = INVALID_MESH;
    skydome_texture_ = skybox_cubemap_ = INVALID_TEXTURE;
    for (auto& f : skybox_faces_) f = INVALID_TEXTURE;
    // Terreno / água / névoa
    terrain_mesh_ = INVALID_MESH;
    terrain_texture_ = terrain_blend_tex_ = INVALID_TEXTURE;
    terrain_bank_tex_ = terrain_outer_bank_tex_ = INVALID_TEXTURE;
    water_normal_tex_ = water_tex_ = INVALID_TEXTURE;
    fog_tex_ = INVALID_TEXTURE;
    // Caches por nome
    texture_cache_.clear();
    texture_dimensions_.clear();
    texture_miss_logged_.clear();
}

bool Engine::recreate_renderer(void* native_window) {
    if (!renderer_) return init_renderer(config_, native_window);
    renderer_->shutdown();
    if (!renderer_->initialize(native_window, config_.window_width, config_.window_height)) {
        LOGE("Failed to recreate renderer");
        return false;
    }
    // O contexto antigo morreu: nenhum handle guardado no Engine vale mais (ver invalidate_gpu_handles).
    invalidate_gpu_handles();
    load_core_ui_textures();
    // Após recriar o renderer, todos os handles de OpenGL são invalidados.
    // Limpa o cache para que as texturas sejam recarregadas sob demanda
    // (evita usar handles antigos inválidos em texturas de portal, água, etc.).
    texture_cache_.clear();
    texture_dimensions_.clear();
    resources_loaded_ = false; // forçar revalidação de recursos no próximo MAIN_MENU
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

bool Engine::play_sound_bank(const std::string& unit_type_name, const std::string& sound_type,
                             const glm::vec3& world_pos, bool worker,
                             const std::string& damage_type) {
    if (!audio_ || !audio_->is_sfx_enabled()) return false;

    // 1. Tenta encontrar definicao de som da protounit
    const ProtoUnitSoundDef* pud = sound_def_mgr_.find_protounit_sound(unit_type_name);
    if (pud) {
        for (const auto& st : pud->sound_types) {
            if (st.type_name != sound_type) continue;

            std::string set_name;
            if (!damage_type.empty() && !st.damage_type_to_set.empty()) {
                auto it = st.damage_type_to_set.find(damage_type);
                if (it != st.damage_type_to_set.end()) set_name = it->second;
            }
            if (set_name.empty() && !st.target_to_set.empty()) {
                set_name = st.target_to_set.count("default") ? st.target_to_set.at("default")
                                                             : st.sound_sets.empty() ? std::string() : st.sound_sets[0];
            }
            if (set_name.empty() && !st.sound_sets.empty()) set_name = st.sound_sets[0];

            if (!set_name.empty()) {
                if (const SoundSetDef* ss = sound_def_mgr_.find_soundset(set_name)) {
                    static uint32_t seed = 0;
                    seed = (seed * 1103515245u + 12345u) % 0x7fffffffu;
                    if (const std::string* fname = sound_def_mgr_.pick_sound(*ss, seed)) {
                        // Toca via 3D para posicional, ou 2D se posicao invalida
                        glm::vec3 pos = world_pos;
                        if (pos == glm::vec3(0.0f)) pos = audio_->listener_position();
                        audio_->play_sfx_3d(*fname, pos, 1.0f, 5.0f, 60.0f);
                        return true;
                    }
                }
            }
        }
    }

    // 2. Fallback: tenta soundset direto por nome (ex: "GatherChop", "MilitaryGreekSelect")
    if (!unit_type_name.empty()) {
        std::string set_name = unit_type_name + sound_type;
        if (const SoundSetDef* ss = sound_def_mgr_.find_soundset(set_name)) {
            static uint32_t seed = 0;
            seed = (seed * 1103515245u + 12345u) % 0x7fffffffu;
            if (const std::string* fname = sound_def_mgr_.pick_sound(*ss, seed)) {
                glm::vec3 pos = world_pos;
                if (pos == glm::vec3(0.0f)) pos = audio_->listener_position();
                audio_->play_sfx_3d(*fname, pos, 1.0f, 5.0f, 60.0f);
                return true;
            }
        }
    }

    return false;
}

void Engine::next_texture() {}
void Engine::prev_texture() {}
void Engine::next_model() {}
void Engine::prev_model() {}



} // namespace aom
