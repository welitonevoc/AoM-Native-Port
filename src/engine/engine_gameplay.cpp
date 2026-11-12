#include "engine/engine.h"
#include "engine_internal.h"
#include "ecs/gameplay.h"
#include "ai/ai_system.h"
#include "anim/animation_system.h"
#include "anim/prt_particle.h"
#include "audio/audio.h"
#include "game/victory.h"
#include "video/media_decoder.h"
#include "assets/xmb_parser.h"
#include "data/proto_unit.h"
#include <android/native_activity.h>
#include <chrono>
#include <algorithm>
#include <vector>
#include <string>

namespace aom {

void Engine::start_game() {
    const auto setup_start = std::chrono::steady_clock::now();
    LOGI("Starting Game...");
    skirmish_victory_result_ = VictoryResult::NONE;
    victory_display_timer_   = 0.0f;
    skirmish_victory_condition_ = VictoryCondition::CONQUEST;
    set_state(EngineState::IN_GAME);

    // Sincronizar estado inicial de vitoria com o GameplayWorld (para save/load)
    if (gameplay_world_) {
        gameplay_world_->set_victory_condition(skirmish_victory_condition_);
        gameplay_world_->set_victory_result(skirmish_victory_result_);
        gameplay_world_->set_victory_timer(victory_display_timer_);
    }

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

    // --- Inicializar IA do inimigo (P5.4) ---
    ai_system_ = std::make_unique<ai::AiSystem>();
    ai_system_->initialize(Team::ENEMY, "standard");

    // --- Load BRG animation clips from models.bar (carga limitada + cache) ---
    load_animation_clips();

    // --- Load GRN skeletal animation clips from anim.bar (carga limitada + cache) ---
    load_grn_clips();

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
                    LOGI("Spawned %s (%s) brg=%s clip=%s mesh=%u tex=%u",
                         s.unit_name, s.team == Team::FRIENDLY ? "friendly" : "enemy",
                         s.brg_name, clip ? clip->name.c_str() : "none",
                         me->mesh, me->texture);
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
         spawns.size(), anim_system_->clip_count(), static_cast<long long>(setup_ms));
}

void Engine::populate_menu_background_scene() {
    if (!gameplay_world_ || !renderer_) return;

    LOGI("Populating MAIN_MENU background scene with The Titans elements...");

    // Carregar clips de animação para as unidades
    load_animation_clips(8);
    const aom::anim::BrgAnimationClip* clip = nullptr;
    TextureHandle clip_tex = INVALID_TEXTURE;
    select_animation_clip(clip, clip_tex);
    if (clip && clip->is_valid()) {
        gameplay_world_->set_default_animation(clip, clip_tex);
    }

    // ── 1. Edifícios na costa ────────────────────────────────────────────────
    gameplay_world_->place_town_center(Team::FRIENDLY, glm::vec3(-8.0f, 0.0f, -5.0f), true);
    gameplay_world_->place_town_center(Team::ENEMY, glm::vec3(10.0f, 0.0f, 6.0f), true);
    gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-6.0f, 0.0f, -2.0f), 8.0f,
                                    BuildingType::BARRACKS, "Quartel Militar");
    gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-9.0f, 0.0f, -3.0f), 5.0f,
                                    BuildingType::HOUSE, "Casa");
    gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-7.0f, 0.0f, -6.0f), 5.0f,
                                    BuildingType::HOUSE, "Casa");
    gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-5.0f, 0.0f, 2.0f), 10.0f,
                                    BuildingType::TOWER, "Torre de Guarda");

    // ── 2. Árvores, pinheiros e rochas na encosta e beira-mar ────────────────
    const std::vector<std::pair<std::string, glm::vec3>> scenery_objects = {
        {"Pine Tree Large", glm::vec3(-10.0f, 0.0f, -4.0f)},
        {"Pine Tree Large", glm::vec3(-11.0f, 0.0f, -2.0f)},
        {"Pine Tree Large", glm::vec3(-12.0f, 0.0f, -1.0f)},
        {"Pine Tree Medium", glm::vec3(-9.0f, 0.0f, -6.0f)},
        {"Pine Tree Medium", glm::vec3(-8.0f, 0.0f, -7.0f)},
        {"Oak Tree", glm::vec3(-6.0f, 0.0f, -5.0f)},
        {"Oak Tree", glm::vec3(-4.0f, 0.0f, -4.0f)},
        {"Rock River Sandy", glm::vec3(-3.0f, 0.0f, 1.0f)},
        {"Rock River Sandy", glm::vec3(-2.0f, 0.0f, 2.0f)},
        {"Rock Large", glm::vec3(-1.0f, 0.0f, -1.0f)},
        {"Rock Large", glm::vec3(0.0f, 0.0f, 0.0f)},
        {"Rock River Sandy", glm::vec3(2.0f, 0.0f, -1.0f)},
        {"Rock River Sandy", glm::vec3(3.0f, 0.0f, 0.0f)},
        {"Rock Large", glm::vec3(4.0f, 0.0f, 1.0f)},
    };

    for (const auto& [name, pos] : scenery_objects) {
        TextureHandle tex = INVALID_TEXTURE;
        if (textures_bar_) {
            tex = load_ddt_texture(name + ".ddt");
        }
        gameplay_world_->spawn_scenery_object(name, pos, 1.0f, tex);
    }

    // ── 3. Tropas terrestres na praia ────────────────────────────────────────
    UnitStats hoplite_stats;
    hoplite_stats.name = "Hoplite";
    hoplite_stats.max_hp = 120.0f;
    hoplite_stats.move_speed = 3.8f;
    hoplite_stats.attack_damage = 12.0f;
    hoplite_stats.attack_range = 2.2f;
    hoplite_stats.attack_cooldown = 1.5f;
    hoplite_stats.los = 14.0f;
    hoplite_stats.worker = false;
    hoplite_stats.pop_cost = 2;

    UnitStats toxotes_stats;
    toxotes_stats.name = "Toxotes";
    toxotes_stats.max_hp = 80.0f;
    toxotes_stats.move_speed = 4.2f;
    toxotes_stats.attack_damage = 9.0f;
    toxotes_stats.attack_range = 12.0f;
    toxotes_stats.attack_cooldown = 2.0f;
    toxotes_stats.los = 16.0f;
    toxotes_stats.worker = false;
    toxotes_stats.pop_cost = 2;

    // Tropas amigas patrulhando
    Entity h1 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-4.0f, 0.0f, 1.5f), hoplite_stats);
    Entity h2 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-3.0f, 0.0f, 2.5f), hoplite_stats);
    Entity h3 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-2.0f, 0.0f, 1.0f), hoplite_stats);
    Entity t1 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-5.0f, 0.0f, 2.0f), toxotes_stats);
    Entity t2 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-4.0f, 0.0f, 3.0f), toxotes_stats);

    gameplay_world_->order_move(h1, glm::vec3(-2.0f, 0.0f, 2.0f));
    gameplay_world_->order_move(h2, glm::vec3(-1.0f, 0.0f, 3.0f));
    gameplay_world_->order_move(h3, glm::vec3(0.0f, 0.0f, 1.5f));
    gameplay_world_->order_move(t1, glm::vec3(-1.0f, 0.0f, 2.5f));
    gameplay_world_->order_move(t2, glm::vec3(0.0f, 0.0f, 2.0f));

    // Tropas inimigas
    Entity eh1 = gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(6.0f, 0.0f, 4.0f), hoplite_stats);
    Entity eh2 = gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(7.0f, 0.0f, 5.0f), hoplite_stats);
    Entity et1 = gameplay_world_->spawn_unit(Team::ENEMY, glm::vec3(8.0f, 0.0f, 3.0f), toxotes_stats);

    gameplay_world_->order_move(eh1, glm::vec3(5.0f, 0.0f, 5.0f));
    gameplay_world_->order_move(eh2, glm::vec3(6.0f, 0.0f, 6.0f));
    gameplay_world_->order_move(et1, glm::vec3(7.0f, 0.0f, 4.0f));

    // ── 4. Unidade Mítica Voadora (Pegasus) cruzando a tela ──────────────────
    UnitStats pegasus_stats;
    pegasus_stats.name = "Pegasus";
    pegasus_stats.max_hp = 200.0f;
    pegasus_stats.move_speed = 18.0f;
    pegasus_stats.attack_damage = 25.0f;
    pegasus_stats.attack_range = 3.0f;
    pegasus_stats.attack_cooldown = 3.0f;
    pegasus_stats.los = 24.0f;
    pegasus_stats.worker = false;
    pegasus_stats.pop_cost = 4;

    Entity pegasus = gameplay_world_->spawn_unit(Team::FRIENDLY,
        glm::vec3(-20.0f, 8.0f, -10.0f), pegasus_stats);
    gameplay_world_->order_move(pegasus, glm::vec3(20.0f, 8.0f, 10.0f));

    // ── 5. Villagers coletando recursos ──────────────────────────────────────
    UnitStats villager_stats;
    villager_stats.name = "Villager Greek";
    villager_stats.max_hp = 50.0f;
    villager_stats.move_speed = 3.5f;
    villager_stats.attack_damage = 3.0f;
    villager_stats.attack_range = 1.0f;
    villager_stats.attack_cooldown = 1.5f;
    villager_stats.los = 12.0f;
    villager_stats.worker = true;
    villager_stats.pop_cost = 1;

    Entity v1 = gameplay_world_->spawn_worker(glm::vec3(-7.0f, 0.0f, 3.0f), villager_stats);
    Entity v2 = gameplay_world_->spawn_worker(glm::vec3(-8.0f, 0.0f, 4.0f), villager_stats);
    Entity v3 = gameplay_world_->spawn_worker(glm::vec3(-6.0f, 0.0f, 4.5f), villager_stats);

    gameplay_world_->add_resource_node(ResourceType::FOOD, glm::vec3(-7.0f, 0.0f, 5.0f), 1200);
    gameplay_world_->add_resource_node(ResourceType::WOOD, glm::vec3(-9.0f, 0.0f, 2.0f), 1500);
    gameplay_world_->add_resource_node(ResourceType::GOLD, glm::vec3(-5.0f, 0.0f, 5.0f), 800);

    gameplay_world_->order_gather(v1, glm::vec3(-7.0f, 0.0f, 5.0f));
    gameplay_world_->order_gather(v2, glm::vec3(-9.0f, 0.0f, 2.0f));
    gameplay_world_->order_gather(v3, glm::vec3(-5.0f, 0.0f, 5.0f));

    LOGI("MAIN_MENU background scene populated: %zu units, %zu buildings",
         gameplay_world_->units().size(), gameplay_world_->buildings().size());
}

void Engine::start_menu_background() {
    LOGI("start_menu_background: iniciando vídeo de fundo do menu");

    menu_bg_ = std::make_unique<aom::video::MediaDecoder>();
    menu_bg_->on_frame = [this](const aom::video::VideoFrame& frame) {
        if (!renderer_ || frame.width == 0 || frame.height == 0) return;
        if (menu_bg_tex_ == INVALID_TEXTURE ||
            frame.width != menu_bg_w_ || frame.height != menu_bg_h_) {
            if (menu_bg_tex_ != INVALID_TEXTURE) {
                renderer_->destroy_texture(menu_bg_tex_);
            }
            menu_bg_tex_ = renderer_->create_texture(
                frame.width, frame.height, frame.rgba.data());
            menu_bg_w_ = frame.width;
            menu_bg_h_ = frame.height;
            LOGI("menu_bg: textura criada %ux%u", menu_bg_w_, menu_bg_h_);
        } else {
            renderer_->update_texture(menu_bg_tex_, frame.width, frame.height, frame.rgba.data());
        }
    };
    menu_bg_->on_eos = [this]() {
        menu_bg_restart_ = true;
    };

    // Tenta abrir o vídeo de fundo do menu (mesmo mecanismo de assets do cinematic queue)
    std::string cache_dir;
    if (native_activity_) {
        auto* act = static_cast<ANativeActivity*>(native_activity_);
        if (act && act->internalDataPath) cache_dir = act->internalDataPath;
    }
    if (cache_dir.empty() && !config_.data_path.empty()) {
        cache_dir = config_.data_path + "/cinematic";
    }
    if (cache_dir.empty()) cache_dir = "/data/local/tmp";

    // Tenta abrir assets comuns de vídeo de fundo do menu do AoM: The Titans
    const char* candidates[] = {
        "assets/avi/menu_bg.mp4",
        "assets/avi/introcg.mp4",
        "assets/avi/xintrocg.mp4",
        "assets/avi/aom-logo.mp4",
    };
    bool opened = false;
    for (const char* candidate : candidates) {
        if (asset_manager_ && media_decoder_->open_asset(asset_manager_, candidate, cache_dir)) {
            // Reutiliza o media_decoder_ já existente? Não: queremos um decodidor dedicado
            // para o menu. O open_asset acima foi só para testar se o arquivo existe.
            // Na verdade precisamos abrir no menu_bg_:
            if (menu_bg_->open_asset(asset_manager_, candidate, cache_dir)) {
                LOGI("menu_bg: vídeo aberto: %s", candidate);
                opened = true;
                break;
            }
        }
    }
    if (!opened) {
        LOGW("menu_bg: nenhum vídeo de fundo encontrado; usará fundo estático");
        menu_bg_.reset();
    }
}

} // namespace aom
