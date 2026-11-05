#pragma once
#include "ecs/gameplay.h"
#include <cstdint>

namespace aom {

/// Condicao de vitoria do mapa.
enum class VictoryCondition : uint8_t {
    CONQUEST,     ///< Destrua todos os Town Centers inimigos
    SUDDEN_DEATH  ///< Perca ou destrua o unico Town Center (seu ou inimigo)
};

/// Resultado da verificacao de vitoria.
enum class VictoryResult : uint8_t {
    NONE,  ///< Jogo em andamento
    WIN,   ///< Jogador venceu
    LOSE   ///< Jogador perdeu
};

/**
 * Verifica a condicao de vitoria para o time FRIENDLY contra o time ENEMY.
 *
 * @param world      Mundo de jogo atual (somente leitura)
 * @param condition  Tipo de condicao de vitoria
 * @return VictoryResult::WIN se o jogador venceu, LOSE se perdeu, NONE caso contrario
 */
VictoryResult check_victory(const GameplayWorld& world, VictoryCondition condition);

} // namespace aom
