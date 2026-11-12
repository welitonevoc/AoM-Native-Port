#include "engine/engine.h"
#include "engine_internal.h"
#include "game/save_load.h"
#include "ui/ui.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace aom {

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
        // Restaurar estado de vitoria (v2)
        if (gameplay_world_) {
            gameplay_world_->set_victory_condition(static_cast<VictoryCondition>(data.victory_condition));
            gameplay_world_->set_victory_result(static_cast<VictoryResult>(data.victory_result));
            gameplay_world_->set_victory_timer(data.victory_timer);
        }
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

// ── Saved Game Browser (multiple slots) ───────────────────────────────────────
void Engine::open_saved_game_browser() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("CARREGAR JOGO", {vw * 0.5f - 110.0f, vh * 0.08f}, 32.0f, 0xFFFFD000);

    // Listar saves
    const auto saves = list_saved_games();
    if (saves.empty()) {
        ui_->add_label("Nenhum jogo salvo encontrado.", {vw * 0.5f - 150.0f, vh * 0.35f}, 20.0f, 0xFFCCCCCC);
    } else {
        // Cabeçalho da lista
        ui_->add_label("Slots salvos:", {vw * 0.1f, vh * 0.18f}, 18.0f, 0xFFDDDDDD);

        const float item_h = 40.0f;
        const float start_y = vh * 0.23f;
        for (size_t i = 0; i < saves.size(); ++i) {
            const float y = start_y + i * item_h;
            const uint32_t color = (static_cast<int>(i) == selected_save_index_) ? 0xFF00FF00 : 0xFFFFFFFF;
            ui_->add_label(saves[i], {vw * 0.15f, y}, 16.0f, color);
        }
    }

    // Botões de ação
    float btn_w = 200.0f, btn_h = 48.0f;
    float spacing = 10.0f;
    float total_w = btn_w * 2 + spacing;
    float start_x = (vw - total_w) * 0.5f;
    float btn_y = vh * 0.75f;

    ui_->add_button({start_x, btn_y}, {btn_w, btn_h}, "CARREGAR",
                   [this]() { load_selected_save(); });
    ui_->add_button({start_x + btn_w + spacing, btn_y}, {btn_w, btn_h}, "EXCLUIR",
                   [this]() { delete_selected_save(); });

    // Voltar
    float back_w = 200.0f, back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.88f}, {back_w, back_h}, "VOLTAR",
                   [this]() { return_to_main_menu(); });
}

std::vector<std::string> Engine::list_saved_games() const {
    std::vector<std::string> saves;
    std::error_code ec;

    // Diretório de saves do app
    std::filesystem::path save_dir = config_.data_path;
    if (!std::filesystem::exists(save_dir, ec)) return saves;

    for (const auto& entry : std::filesystem::directory_iterator(save_dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.rfind("save_", 0) == 0 && name.size() > 5 && name.substr(name.size() - 4) == ".dat") {
            saves.push_back(name);
        }
    }

    // Ordenar por data de modificação (mais recente primeiro)
    std::sort(saves.begin(), saves.end(), [&](const std::string& a, const std::string& b) {
        auto ta = std::filesystem::last_write_time(save_dir / a, ec);
        auto tb = std::filesystem::last_write_time(save_dir / b, ec);
        return ta > tb;
    });

    return saves;
}

void Engine::refresh_saved_game_list() {
    // Re-abre o browser para atualizar a lista
    open_saved_game_browser();
}

void Engine::load_selected_save() {
    const auto saves = list_saved_games();
    if (selected_save_index_ >= 0 && static_cast<size_t>(selected_save_index_) < saves.size()) {
        std::string path = config_.data_path + "/" + saves[selected_save_index_];
        load_game(path);
    }
}

void Engine::delete_selected_save() {
    const auto saves = list_saved_games();
    if (selected_save_index_ >= 0 && static_cast<size_t>(selected_save_index_) < saves.size()) {
        std::string path = config_.data_path + "/" + saves[selected_save_index_];
        std::error_code ec;
        std::filesystem::remove(path, ec);
        selected_save_index_ = -1;
        refresh_saved_game_list();
    }
}

void Engine::open_save_game_menu() {
    clear_ui();
    const float vw = static_cast<float>(config_.window_width);
    const float vh = static_cast<float>(config_.window_height);

    ui_->add_label("SALVAR JOGO", {vw * 0.5f - 110.0f, vh * 0.15f}, 32.0f, 0xFFFFD000);

    // Mostrar slots existentes
    const auto saves = list_saved_games();
    if (saves.empty()) {
        ui_->add_label("Nenhum slot salvo. Novo save será criado.", {vw * 0.5f - 180.0f, vh * 0.35f}, 18.0f, 0xFFCCCCCC);
    } else {
        ui_->add_label("Slots existentes (sobrescrever ou novo):", {vw * 0.1f, vh * 0.2f}, 18.0f, 0xFFDDDDDD);
        const float item_h = 36.0f;
        const float start_y = vh * 0.26f;
        for (size_t i = 0; i < saves.size() && i < 5; ++i) {
            const float y = start_y + i * item_h;
            const uint32_t color = (static_cast<int>(i) == selected_save_index_) ? 0xFF00FF00 : 0xFFFFFFFF;
            ui_->add_label(saves[i], {vw * 0.15f, y}, 14.0f, color);
        }
    }

    // Campo de nome do slot (simplificado: usa timestamp)
    ui_->add_label("Nome do save:", {vw * 0.15f, vh * 0.55f}, 16.0f, 0xFFFFFFFF);
    // No momento, gera nome automático com timestamp
    char auto_name[64];
    time_t now = time(nullptr);
    strftime(auto_name, sizeof(auto_name), "save_%Y%m%d_%H%M%S.dat", localtime(&now));

    float btn_w = 200.0f, btn_h = 48.0f;
    float spacing = 10.0f;

    ui_->add_button({(vw - btn_w) * 0.5f, vh * 0.65f}, {btn_w, btn_h}, "SALVAR COMO " + std::string(auto_name),
                   [this, auto_name]() { save_game_slot(auto_name); });

    float back_w = 200.0f, back_h = 44.0f;
    ui_->add_button({(vw - back_w) * 0.5f, vh * 0.82f}, {back_w, back_h}, "VOLTAR",
                   [this]() { return_to_main_menu(); });
}

void Engine::save_game_slot(const std::string& slot_name) {
    std::string path = config_.data_path + "/" + slot_name;
    std::error_code ec;
    std::filesystem::create_directories(config_.data_path, ec);
    if (save_game_state(path, *gameplay_world_, accumulated_time_)) {
        LOGI("Jogo salvo em '%s'", path.c_str());
        return_to_main_menu();
    } else {
        LOGE("Falha ao salvar jogo em '%s'", path.c_str());
    }
}
} // namespace aom
