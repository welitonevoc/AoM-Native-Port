#include "engine/engine.h"
#include "engine_internal.h"
#include "campaign/campaign.h"
#include "campaign/campaign_mission.h"
#include "assets/bar_archive.h"
#include "video/media_decoder.h"
#include "ui/ui.h"
#include "ecs/gameplay.h"
#include "anim/animation_system.h"
#include "anim/prt_particle.h"
#include "game/scenario_trigger.h"
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <chrono>
#include <filesystem>
#include <vector>

namespace aom {

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

    // --- Inicializar IA do inimigo (P5.4) ---
    ai_system_ = std::make_unique<ai::AiSystem>();
    ai_system_->initialize(Team::ENEMY, "standard");

    // --- Load BRG animation clips from models.bar (carga limitada + cache) ---
    load_animation_clips();

    // --- Load GRN skeletal animation clips from anim.bar (carga limitada + cache) ---
    load_grn_clips();

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
    mission_mgr_->on_mission_completed = [this, scenario](MissionResult res) {
        if (res == MissionResult::VICTORY) {
            LOGI("Campaign Mission '%s' COMPLETED with VICTORY!", scenario->display_name.c_str());
        } else if (res == MissionResult::DEFEAT) {
            LOGI("Campaign Mission '%s' FAILED with DEFEAT!", scenario->display_name.c_str());
        }
        // Fase 2: aqui vai a transição para o próximo cenário
    };

    // Montar caminho do .scn/.scx a partir do filename do cenário
    // filename do .cpn/.cpx vem como "spc\\aomsp04" ou "xspc\\aomx01" (sem extensão)
    std::string scn_base = scenario->filename;
    bool is_scx = ends_with(to_lower(scn_base), ".scx") || (scn_base.find("xspc") != std::string::npos);
    // Remove extensão se houver
    if (ends_with(to_lower(scn_base), ".scn")) scn_base = scn_base.substr(0, scn_base.size() - 4);
    if (ends_with(to_lower(scn_base), ".scx")) scn_base = scn_base.substr(0, scn_base.size() - 4);

    // Normaliza separadores para / e adiciona extensão correta
    std::string scn_path = scn_base;
    for (char& c : scn_path) {
        if (c == '\\') c = '/';
    }
    if (!ends_with(to_lower(scn_path), ".scn") && !ends_with(to_lower(scn_path), ".scx")) {
        scn_path += (is_scx ? ".scx" : ".scn");
    }

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
        std::string backslash_path = scn_path;
        for (char& c : backslash_path) { if (c == '/') c = '\\'; }

        const std::string kMissionScn[] = {
            scn_path,
            backslash_path,
            "spc\\aomsp04.scn", "spc/aomsp04.scn", "aomsp04.scn"
        };
        for (const auto& scn : kMissionScn) {
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

void Engine::start_generic_mission(const std::string& scenario_filename, bool menu_background_mode) {
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

    // No modo menu_background, NÃO spawnar entidades — apenas carregar terrain/heightmap
    if (!menu_background_mode) {
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

        gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(4.0f, 0.0f, 3.5f), 1200);
        gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(7.5f, 0.0f, 3.5f), 1500);
        gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(5.5f, 0.0f, -3.5f), 2000);

        LOGI("Generic mission: entidades básicas spawnadas");
    } else {
        LOGI("Generic mission: modo menu background — apenas terrain/heightmap carregado (sem entidades)");
    }
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

} // namespace aom
