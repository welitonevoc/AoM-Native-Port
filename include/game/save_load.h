#pragma once
#include "ecs/gameplay.h"
#include <string>
#include <cstdint>
#include <vector>

namespace aom {

/// Versao do formato de save (bump ao mudar o layout binario)
static constexpr uint32_t kSaveFormatVersion = 2;
/// Magic bytes para validacao do arquivo ('AOMS')
static constexpr uint32_t kSaveMagic = 0x414F4D53;

/// Unidade serializada (versao em memoria, sem o packing do arquivo).
struct SavedUnit {
    uint32_t entity   = 0;
    float    pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    int32_t  current_hp = 0;
    int32_t  max_hp     = 0;
    uint8_t  team       = 0;   ///< 0 = FRIENDLY, 1 = ENEMY
    bool     worker     = false;
    int32_t  pop_cost   = 1;
    std::string unit_type;

    /// Estado de ordem de movimento / ataque / construcao
    bool     has_move_order   = false;
    float    move_target_x = 0.0f, move_target_z = 0.0f;
    uint32_t attack_target    = 0;   ///< entity id ou 0 = INVALID_ENTITY
    uint32_t build_target     = 0;   ///< entity id ou 0 = INVALID_ENTITY
    int32_t  gather_node      = -1;  ///< indice no resources_nodes_ ou -1
    bool     returning_cargo  = false;
    int32_t  carried_resource_amount = 0;
    uint8_t  carried_resource_type   = 0;  ///< ResourceType (0=FOOD,1=WOOD,2=GOLD,3=FAVOR)
};

/// Edificio serializado (versao em memoria, sem o packing do arquivo).
struct SavedBuilding {
    uint32_t entity   = 0;
    float    pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    int32_t  current_hp = 0;
    int32_t  max_hp     = 0;
    uint8_t  team       = 0;   ///< 0 = FRIENDLY, 1 = ENEMY
    uint8_t  building_type = 0;///< BuildingType serializado
    bool     complete   = false;
    float    progress   = 0.0f;
    std::string name;

    /// Estado de treinamento de unidades
    std::string training_unit;   ///< vazio se nao esta treinando
    float     train_timer = 0.0f;
    std::vector<std::string> train_queue;  ///< fila de unidades a treinar
};

struct SaveGameData {
    uint32_t magic             = kSaveMagic;
    uint32_t version           = kSaveFormatVersion;
    float    game_time_seconds = 0.0f;
    int      map_seed          = 42;

    // Recursos por time (friendly=0, enemy=1)
    int food[2]  = {};
    int wood[2]  = {};
    int gold[2]  = {};
    int favor[2] = {};

    // Entidades do mundo (gravadas em disco desde a v1 e agora restauradas)
    std::vector<SavedUnit>     units;
    std::vector<SavedBuilding> buildings;
};

/// Serializa o estado do jogo para um arquivo binario.
/// Retorna true em caso de sucesso.
bool save_game_state(const std::string& path,
                     const GameplayWorld& world,
                     float game_time_seconds,
                     int map_seed = 42);

/// Desserializa o estado do jogo a partir de um arquivo binario.
/// Retorna true e preenche 'out' em caso de sucesso.
bool load_game_state(const std::string& path, SaveGameData& out);

/// Verifica se existe um save valido no caminho especificado.
bool has_valid_save(const std::string& path);

} // namespace aom
