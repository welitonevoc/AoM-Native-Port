#include "game/save_load.h"
#include <fstream>
#include <cstring>
#include <utility>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AoM-SaveLoad", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AoM-SaveLoad", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

/// Converte um buffer de tamanho fixo (com possivel falta de '\0') em string.
std::string fixed_string(const char* data, size_t max_len) {
    size_t len = 0;
    while (len < max_len && data[len] != '\0') ++len;
    return std::string(data, len);
}

} // anonymous namespace

#pragma pack(push, 1)
struct SaveUnit {
    uint32_t entity;
    float    pos_x, pos_y, pos_z;
    int32_t  current_hp, max_hp;
    uint8_t  team;    // 0=FRIENDLY, 1=ENEMY
    uint8_t  worker;
    uint8_t  _pad[2];
    int32_t  pop_cost;
    char     unit_type[64];
};

struct SaveBuilding {
    uint32_t entity;
    float    pos_x, pos_y, pos_z;
    int32_t  current_hp, max_hp;
    uint8_t  team;
    uint8_t  btype; // BuildingType cast to uint8_t
    uint8_t  complete;
    uint8_t  _pad;
    float    progress;
    char     name[64];
};

struct SaveHeader {
    uint32_t magic;
    uint32_t version;
    float    game_time;
    int32_t  map_seed;
    int32_t  food[2];
    int32_t  wood[2];
    int32_t  gold[2];
    int32_t  favor[2];
    uint32_t unit_count;
    uint32_t building_count;
};
#pragma pack(pop)

bool save_game_state(const std::string& path,
                     const GameplayWorld& world,
                     float game_time_seconds,
                     int map_seed)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        LOGE("save_game_state: falha ao abrir '%s' para gravacao", path.c_str());
        return false;
    }

    const auto& transforms = world.transforms();
    const auto& units_map  = world.units();
    const auto& bldgs_map  = world.buildings();
    const auto& hp_map     = world.health_components();

    std::vector<SaveUnit>     save_units;
    std::vector<SaveBuilding> save_bldgs;
    save_units.reserve(units_map.size());
    save_bldgs.reserve(bldgs_map.size());

    for (const auto& [entity, unit] : units_map) {
        SaveUnit su{};
        su.entity = static_cast<uint32_t>(entity);
        auto it = transforms.find(entity);
        if (it != transforms.end()) {
            su.pos_x = it->second.position.x;
            su.pos_y = it->second.position.y;
            su.pos_z = it->second.position.z;
        }
        auto hp_it = hp_map.find(entity);
        if (hp_it != hp_map.end()) {
            su.current_hp = hp_it->second.current_hp;
            su.max_hp     = hp_it->second.max_hp;
        }
        su.team     = (unit.team == Team::FRIENDLY) ? 0 : 1;
        su.worker   = unit.worker ? 1 : 0;
        su.pop_cost = unit.pop_cost;
        std::strncpy(su.unit_type, unit.unit_type_name.c_str(), sizeof(su.unit_type) - 1);
        save_units.push_back(su);
    }

    for (const auto& [entity, bld] : bldgs_map) {
        SaveBuilding sb{};
        sb.entity = static_cast<uint32_t>(entity);
        auto it = transforms.find(entity);
        if (it != transforms.end()) {
            sb.pos_x = it->second.position.x;
            sb.pos_y = it->second.position.y;
            sb.pos_z = it->second.position.z;
        }
        auto hp_it = hp_map.find(entity);
        if (hp_it != hp_map.end()) {
            sb.current_hp = hp_it->second.current_hp;
            sb.max_hp     = hp_it->second.max_hp;
        }
        sb.team     = (bld.team == Team::FRIENDLY) ? 0 : 1;
        sb.btype    = static_cast<uint8_t>(bld.type);
        sb.complete = bld.complete ? 1 : 0;
        sb.progress = bld.progress;
        std::strncpy(sb.name, bld.name.c_str(), sizeof(sb.name) - 1);
        save_bldgs.push_back(sb);
    }

    const auto& pool_f = world.resource_pool(Team::FRIENDLY);
    const auto& pool_e = world.resource_pool(Team::ENEMY);

    SaveHeader hdr{};
    hdr.magic          = kSaveMagic;
    hdr.version        = kSaveFormatVersion;
    hdr.game_time      = game_time_seconds;
    hdr.map_seed       = map_seed;
    hdr.food[0]        = pool_f.food;   hdr.food[1]  = pool_e.food;
    hdr.wood[0]        = pool_f.wood;   hdr.wood[1]  = pool_e.wood;
    hdr.gold[0]        = pool_f.gold;   hdr.gold[1]  = pool_e.gold;
    hdr.favor[0]       = pool_f.favor;  hdr.favor[1] = pool_e.favor;
    hdr.unit_count     = static_cast<uint32_t>(save_units.size());
    hdr.building_count = static_cast<uint32_t>(save_bldgs.size());

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!save_units.empty()) {
        out.write(reinterpret_cast<const char*>(save_units.data()), save_units.size() * sizeof(SaveUnit));
    }
    if (!save_bldgs.empty()) {
        out.write(reinterpret_cast<const char*>(save_bldgs.data()), save_bldgs.size() * sizeof(SaveBuilding));
    }

    if (!out) {
        LOGE("save_game_state: erro de gravacao em '%s'", path.c_str());
        return false;
    }

    LOGI("save_game_state: salvo com sucesso em '%s' (%u unidades, %u edificios)",
         path.c_str(), hdr.unit_count, hdr.building_count);
    return true;
}

bool load_game_state(const std::string& path, SaveGameData& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LOGE("load_game_state: arquivo '%s' nao encontrado", path.c_str());
        return false;
    }

    SaveHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in || hdr.magic != kSaveMagic) {
        LOGE("load_game_state: magic invalido em '%s'", path.c_str());
        return false;
    }
    if (hdr.version != kSaveFormatVersion) {
        LOGE("load_game_state: versao incompativel %u (esperado %u)", hdr.version, kSaveFormatVersion);
        return false;
    }

    out.magic             = hdr.magic;
    out.version           = hdr.version;
    out.game_time_seconds = hdr.game_time;
    out.map_seed          = hdr.map_seed;
    out.food[0]  = hdr.food[0];   out.food[1]  = hdr.food[1];
    out.wood[0]  = hdr.wood[0];   out.wood[1]  = hdr.wood[1];
    out.gold[0]  = hdr.gold[0];   out.gold[1]  = hdr.gold[1];
    out.favor[0] = hdr.favor[0];  out.favor[1] = hdr.favor[1];

    // ── Seções de entidades ────────────────────────────────────────────────
    // As seções ja eram gravadas desde a versao 1 do formato; aqui elas sao
    // lidas de volta para permitir reconstruir o mundo ao carregar a partida.
    // Um limite de sanidade evita alocacoes enormes em arquivos corrompidos.
    constexpr uint32_t kMaxEntities = 100000;
    if (hdr.unit_count > kMaxEntities || hdr.building_count > kMaxEntities) {
        LOGE("load_game_state: contagem de entidades invalida (u=%u b=%u)", hdr.unit_count, hdr.building_count);
        return false;
    }

    out.units.clear();
    out.buildings.clear();

    if (hdr.unit_count > 0) {
        std::vector<SaveUnit> raw(hdr.unit_count);
        in.read(reinterpret_cast<char*>(raw.data()), raw.size() * sizeof(SaveUnit));
        if (!in) {
            LOGE("load_game_state: falha ao ler a secao de unidades");
            return false;
        }
        out.units.reserve(raw.size());
        for (const SaveUnit& su : raw) {
            SavedUnit u;
            u.entity     = su.entity;
            u.pos_x      = su.pos_x;
            u.pos_y      = su.pos_y;
            u.pos_z      = su.pos_z;
            u.current_hp = su.current_hp;
            u.max_hp     = su.max_hp;
            u.team       = su.team;
            u.worker     = su.worker != 0;
            u.pop_cost   = su.pop_cost;
            u.unit_type = fixed_string(su.unit_type, sizeof(su.unit_type));
            out.units.push_back(std::move(u));
        }
    }

    if (hdr.building_count > 0) {
        std::vector<SaveBuilding> raw(hdr.building_count);
        in.read(reinterpret_cast<char*>(raw.data()), raw.size() * sizeof(SaveBuilding));
        if (!in) {
            LOGE("load_game_state: falha ao ler a secao de edificios");
            return false;
        }
        out.buildings.reserve(raw.size());
        for (const SaveBuilding& sb : raw) {
            SavedBuilding b;
            b.entity       = sb.entity;
            b.pos_x        = sb.pos_x;
            b.pos_y        = sb.pos_y;
            b.pos_z        = sb.pos_z;
            b.current_hp   = sb.current_hp;
            b.max_hp       = sb.max_hp;
            b.team         = sb.team;
            b.building_type = sb.btype;
            b.complete     = sb.complete != 0;
            b.progress     = sb.progress;
            b.name = fixed_string(sb.name, sizeof(sb.name));
            out.buildings.push_back(std::move(b));
        }
    }

    LOGI("load_game_state: '%s' carregado — tempo=%.1fs unidades=%zu edificios=%zu",
         path.c_str(), hdr.game_time, out.units.size(), out.buildings.size());
    return true;
}

bool has_valid_save(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return in && magic == kSaveMagic;
}

} // namespace aom
