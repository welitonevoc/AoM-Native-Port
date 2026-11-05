#pragma once
#include "ecs/gameplay.h"
#include <string>
#include <cstdint>

namespace aom {

/// Versao do formato de save (bump ao mudar o layout binario)
static constexpr uint32_t kSaveFormatVersion = 1;
/// Magic bytes para validacao do arquivo ('AOMS')
static constexpr uint32_t kSaveMagic = 0x414F4D53;

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
