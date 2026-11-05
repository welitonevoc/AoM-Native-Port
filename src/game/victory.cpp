#include "game/victory.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AoM-Victory", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#endif

namespace aom {

VictoryResult check_victory(const GameplayWorld& world, VictoryCondition condition) {
    int friendly_tc = 0;
    int enemy_tc    = 0;

    const auto& buildings = world.buildings();
    const auto& hp_map    = world.health_components();

    for (const auto& [entity, bld] : buildings) {
        if (bld.type != BuildingType::TOWN_CENTER || !bld.complete) continue;
        auto hp_it = hp_map.find(entity);
        if (hp_it != hp_map.end() && hp_it->second.current_hp <= 0) continue;
        if (bld.team == Team::FRIENDLY) ++friendly_tc;
        else                            ++enemy_tc;
    }

    switch (condition) {
        case VictoryCondition::CONQUEST:
            if (enemy_tc == 0) {
                LOGI("check_victory: VITORIA por CONQUISTA (inimigo sem TC)\n");
                return VictoryResult::WIN;
            }
            if (friendly_tc == 0) {
                LOGI("check_victory: DERROTA por CONQUISTA (jogador sem TC)\n");
                return VictoryResult::LOSE;
            }
            break;

        case VictoryCondition::SUDDEN_DEATH:
            if (friendly_tc == 0) {
                LOGI("check_victory: DERROTA por MORTE SUBITA (TC do jogador destruido)\n");
                return VictoryResult::LOSE;
            }
            if (enemy_tc == 0) {
                LOGI("check_victory: VITORIA por MORTE SUBITA (TC inimigo destruido)\n");
                return VictoryResult::WIN;
            }
            break;
    }

    return VictoryResult::NONE;
}

} // namespace aom
