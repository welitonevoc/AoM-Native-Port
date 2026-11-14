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

    LOGI("Populating MAIN_MENU background scene with authentic The Titans elements...");

    // ── 0. Conectar renderer de animações ao mundo ────────────────────────────
    gameplay_world_->set_anim_renderer(renderer_.get());

    load_animation_clips(8);
    const aom::anim::BrgAnimationClip* clip = nullptr;
    TextureHandle clip_tex = INVALID_TEXTURE;
    select_animation_clip(clip, clip_tex);
    if (clip && clip->is_valid()) {
        gameplay_world_->set_default_animation(clip, clip_tex);
    }

    // ── 1. Carregar modelos autênticos de The Titans sob demanda ──────────────
    ModelEntry* manor_me = get_model_entry("building x manor a age1.brg");
    ModelEntry* barracks_me = get_model_entry("building x barracks age1.brg");
    ModelEntry* lighthouse_me = get_model_entry("building e lighthouse.brg");
    ModelEntry* palm_me = get_model_entry("world e palm a.brg");
    ModelEntry* oak_me = get_model_entry("world g oak a.brg");
    ModelEntry* pine_me = get_model_entry("world n pine a.brg");
    ModelEntry* rock_me = get_model_entry("world a sandyriverrock a.brg");
    ModelEntry* murmillo_me = get_model_entry("infantry x 2 sword fighter_walka.brg");
    ModelEntry* arcus_me = get_model_entry("archer x arcus_walka.brg");

    // ── 2. Edifícios Atlantes na ilha e praia (flancos visíveis fora do pórtico) ────────
    // Flanco esquerdo aberto (Screen X ~ 22% a 28%): platô elevado verde
    Entity m_bld1 = gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-6.5f, 1.35f, 0.0f), 1.0f,
                                                   BuildingType::HOUSE, "Manor Atlante");
    Entity b_bld = gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(-7.2f, 1.45f, 1.5f), 1.0f,
                                                   BuildingType::BARRACKS, "Quartel Atlante");
    // Flanco direito aberto (Screen X ~ 68% a 74%): encosta costeira e mar
    Entity m_bld2 = gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(3.6f, 1.10f, -0.5f), 1.0f,
                                                   BuildingType::HOUSE, "Manor Atlante");
    Entity l_bld = gameplay_world_->place_building(Team::FRIENDLY, glm::vec3(4.2f, 0.40f, -3.5f), 1.0f,
                                                   BuildingType::TOWER, "Farol");

    auto mark_complete = [this](Entity bld, float scale) {
        if (auto* bc = gameplay_world_->building_component_mutable(bld)) {
            bc->complete = true;
            bc->progress = 1.0f;
        }
        if (auto* tr = gameplay_world_->transform_mutable(bld)) {
            tr->scale = glm::vec3(scale);
        }
    };
    mark_complete(m_bld1, 1.35f);
    mark_complete(m_bld2, 1.20f);
    mark_complete(b_bld, 1.10f);
    mark_complete(l_bld, 1.0f);

    if (manor_me && manor_me->mesh != INVALID_MESH) {
        gameplay_world_->set_building_visuals(m_bld1, manor_me->mesh, manor_me->texture);
        gameplay_world_->set_building_visuals(m_bld2, manor_me->mesh, manor_me->texture);
    }
    if (barracks_me && barracks_me->mesh != INVALID_MESH) {
        gameplay_world_->set_building_visuals(b_bld, barracks_me->mesh, barracks_me->texture);
    }
    if (lighthouse_me && lighthouse_me->mesh != INVALID_MESH) {
        gameplay_world_->set_building_visuals(l_bld, lighthouse_me->mesh, lighthouse_me->texture);
    }

    // ── 3. Vegetação e Rochas em ambos os flancos visíveis ────────────────────
    if (palm_me && palm_me->mesh != INVALID_MESH) {
        // Palmeiras na costa direita
        gameplay_world_->spawn_scenery_object("Palm Tree", glm::vec3(2.5f, 0.50f, 1.5f), 1.25f, palm_me->texture, palm_me->mesh);
        gameplay_world_->spawn_scenery_object("Palm Tree", glm::vec3(3.8f, 0.65f, 0.5f), 1.15f, palm_me->texture, palm_me->mesh);
        gameplay_world_->spawn_scenery_object("Palm Tree", glm::vec3(4.5f, 0.70f, -1.5f), 1.10f, palm_me->texture, palm_me->mesh);
        // Palmeiras no flanco esquerdo
        gameplay_world_->spawn_scenery_object("Palm Tree", glm::vec3(-5.2f, 1.25f, 1.8f), 1.20f, palm_me->texture, palm_me->mesh);
        gameplay_world_->spawn_scenery_object("Palm Tree", glm::vec3(-6.0f, 1.35f, -1.5f), 1.10f, palm_me->texture, palm_me->mesh);
    }
    if (oak_me && oak_me->mesh != INVALID_MESH) {
        gameplay_world_->spawn_scenery_object("Oak Tree", glm::vec3(-7.5f, 1.5f, -0.8f), 1.3f, oak_me->texture, oak_me->mesh);
        gameplay_world_->spawn_scenery_object("Oak Tree", glm::vec3(4.8f, 0.8f, 1.8f), 1.2f, oak_me->texture, oak_me->mesh);
    }
    if (pine_me && pine_me->mesh != INVALID_MESH) {
        gameplay_world_->spawn_scenery_object("Pine Tree", glm::vec3(-7.8f, 1.55f, 0.5f), 1.4f, pine_me->texture, pine_me->mesh);
    }
    if (rock_me && rock_me->mesh != INVALID_MESH) {
        gameplay_world_->spawn_scenery_object("Rock Sandy", glm::vec3(2.2f, 0.40f, 2.5f), 1.5f, rock_me->texture, rock_me->mesh);
        gameplay_world_->spawn_scenery_object("Rock Sandy", glm::vec3(3.2f, 0.45f, -2.5f), 1.8f, rock_me->texture, rock_me->mesh);
        gameplay_world_->spawn_scenery_object("Rock Sandy", glm::vec3(-5.0f, 1.10f, -2.2f), 1.3f, rock_me->texture, rock_me->mesh);
    }

    // ── 4. Tropas Atlantes patrulhando nos flancos visíveis ──────────────────
    UnitStats murmillo_stats;
    murmillo_stats.name = "Murmillo";
    murmillo_stats.max_hp = 140.0f;
    murmillo_stats.move_speed = 3.5f;
    murmillo_stats.attack_damage = 14.0f;
    murmillo_stats.attack_range = 2.0f;
    murmillo_stats.attack_cooldown = 1.4f;
    murmillo_stats.los = 20.0f;

    UnitStats arcus_stats;
    arcus_stats.name = "Arcus";
    arcus_stats.max_hp = 85.0f;
    arcus_stats.move_speed = 3.6f;
    arcus_stats.attack_damage = 11.0f;
    arcus_stats.attack_range = 14.0f;
    arcus_stats.attack_cooldown = 1.8f;
    arcus_stats.los = 20.0f;

    // Murmillo 1 na praia direita (Screen X ~ 67%)
    Entity m1 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(2.6f, 0.50f, 1.2f), murmillo_stats);
    // Murmillo 2 no platô esquerdo perto do Manor 1 (Screen X ~ 28%)
    Entity m2 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-5.5f, 1.30f, 0.5f), murmillo_stats);
    // Arcus 1 na praia direita (Screen X ~ 69%)
    Entity a1 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(3.2f, 0.55f, 2.0f), arcus_stats);
    // Arcus 2 no platô esquerdo (Screen X ~ 27%)
    Entity a2 = gameplay_world_->spawn_unit(Team::FRIENDLY, glm::vec3(-5.8f, 1.35f, 1.8f), arcus_stats);

    if (murmillo_me && murmillo_me->mesh != INVALID_MESH) {
        gameplay_world_->set_unit_visuals(m1, murmillo_me->mesh, murmillo_me->texture);
        gameplay_world_->set_unit_visuals(m2, murmillo_me->mesh, murmillo_me->texture);
    }
    if (arcus_me && arcus_me->mesh != INVALID_MESH) {
        gameplay_world_->set_unit_visuals(a1, arcus_me->mesh, arcus_me->texture);
        gameplay_world_->set_unit_visuals(a2, arcus_me->mesh, arcus_me->texture);
    }

    // Ordens de patrulha ao longo dos flancos visíveis
    gameplay_world_->order_move(m1, glm::vec3(3.8f, 0.60f, -1.0f));
    gameplay_world_->order_move(m2, glm::vec3(-6.8f, 1.40f, -1.2f));
    gameplay_world_->order_move(a1, glm::vec3(2.4f, 0.50f, -0.5f));
    gameplay_world_->order_move(a2, glm::vec3(-5.0f, 1.20f, -0.8f));

    LOGI("MAIN_MENU background scene populated: %zu units, %zu buildings",
         gameplay_world_->units().size(), gameplay_world_->buildings().size());
}

void Engine::start_menu_background() {
    // Menu de The Titans utiliza cena 3D nativa e imagem estática, sem vídeo de fundo
    LOGI("start_menu_background: menu video desativado (usando cena 3D/fundo limpo)");
    menu_bg_.reset();
    menu_bg_tex_ = INVALID_TEXTURE;
}

} // namespace aom
