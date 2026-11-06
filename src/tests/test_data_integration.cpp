#include "data/game_data.h"
#include "assets/bar_archive.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* data_root() {
    if (const char* env = std::getenv("AOM_DATA_ROOT")) return env;
    return "/storage/emulated/0/Download/Games/PC/Age of Mythology";
}

} // anonymous namespace

int main(int argc, char** argv) {
    std::string data_path = data_root();
    if (argc > 1) {
        data_path = argv[1];
    }

    std::string data_bar = data_path + "/data/data.bar";
    std::string data2_bar = data_path + "/data/data2.bar";

    aom::BarArchive base_bar;
    if (!base_bar.open(data_bar)) {
        std::printf("SKIP: nao foi possivel abrir %s\n", data_bar.c_str());
        return 0;
    }

    // Tentar montar expansão (data2.bar) se existir
    bool has_expansion = false;
    if (aom::BarArchive expansion_bar; expansion_bar.open(data2_bar)) {
        base_bar.mount(data2_bar);
        has_expansion = true;
        std::printf("[INFO] Expansao The Titans montada: %s\n", data2_bar.c_str());
    }

    int passed = 0, failed = 0;

    // --- Carregar todos os dados via GameDataManager ---
    aom::GameDataManager gdm;
    if (!gdm.load_from_bar(base_bar)) {
        std::printf("[-] Falha ao carregar dados do data.bar\n");
        return 1;
    }

    std::printf("\n=== REGISTRO DE DADOS CARREGADO ===\n");
    std::printf("Unidades (proto):     %zu\n", gdm.unit_count());
    std::printf("Deuses maiores:       %zu\n", gdm.god_count());
    std::printf("Deuses menores:       %zu\n", gdm.minor_god_count());
    std::printf("Tecnologias:          %zu\n", gdm.tech_count());
    std::printf("Rios:                 %zu\n", gdm.water_bodies().river_count());
    std::printf("Oceanos:              %zu\n", gdm.water_bodies().ocean_count());
    std::printf("Lagos:                %zu\n", gdm.water_bodies().lake_count());
    std::printf("Transformacoes:       %zu\n", gdm.unit_transforms().count());
    std::printf("Soundsets:            %zu\n", gdm.soundset_count());

    // --- 1. Validar ProtoUnits ---
    std::printf("\n--- Validacao de ProtoUnits ---\n");

    const char* expected_units[] = {"Hoplite", "Toxotes", "Villager Greek", "Town Center", "Trireme"};
    for (const auto* uname : expected_units) {
        if (const auto* u = gdm.find_unit(uname); u) {
            std::printf("  [+] '%s': hp=%.0f, vel=%.1f, cost(F:%.0f W:%.0f G:%.0f), worker=%d, ship=%d, building=%d, military=%d\n",
                        u->name.c_str(), u->max_hitpoints, u->max_velocity,
                        u->cost.food, u->cost.wood, u->cost.gold,
                        u->worker, u->ship, u->building, u->military);
            ++passed;
        } else {
            std::printf("  [-] '%s' nao encontrado\n", uname);
            ++failed;
        }
    }

    // Validar que protox.xmb sobrescreve proto.xmb (unidades da expansão)
    if (const auto* tremor = gdm.find_unit("Tremor"); tremor) {
        std::printf("  [+] Unidade da expansao 'Tremor' encontrada\n");
        ++passed;
    } else {
        std::printf("  [INFO] Unidade 'Tremor' nao encontrada (pode nao estar nos dados)\n");
    }

    // Contar workers e ships
    auto workers = gdm.proto_units().find_workers();
    auto ships = gdm.proto_units().find_by_type(true);
    auto military = gdm.proto_units().find_military();
    std::printf("  Workers: %zu, Ships: %zu, Military: %zu\n",
                workers.size(), ships.size(), military.size());

    // --- 2. Validar Deuses ---
    std::printf("\n--- Validacao de Deuses ---\n");

    const char* expected_majors[] = {"Age 1 Zeus", "Age 1 Poseidon", "Age 1 Hades",
                                      "Age 1 Ra", "Age 1 Isis", "Age 1 Set",
                                      "Age 1 Odin", "Age 1 Thor", "Age 1 Loki"};
    for (const auto* gname : expected_majors) {
        if (const auto* g = gdm.find_god(gname); g) {
            std::printf("  [+] Major God '%s': %zu minor gods, civ=%s\n",
                        g->tech_name.c_str(), g->minor_gods.size(), g->civilization.c_str());
            ++passed;
        } else {
            std::printf("  [-] Major God '%s' nao encontrado\n", gname);
            ++failed;
        }
    }

    const char* expected_minors[] = {"Age 2 Athena", "Age 2 Ares", "Age 2 Anubis", "Age 2 Bast"};
    for (const auto* mname : expected_minors) {
        if (const auto* m = gdm.find_minor_god(mname); m) {
            std::printf("  [+] Minor God '%s': age=%d, civ=%s\n",
                        m->tech_name.c_str(), m->age, m->civilization.c_str());
            ++passed;
        } else {
            std::printf("  [-] Minor God '%s' nao encontrado\n", mname);
            ++failed;
        }
    }

    // --- 3. Validar Water Bodies ---
    std::printf("\n--- Validacao de Water Bodies ---\n");

    if (const auto* med = gdm.water_bodies().find_ocean("Mediterranean Sea"); med) {
        std::printf("  [+] Mediterranean Sea: color=(%.0f,%.0f,%.0f), depth=%.1f, bank='%s', outer_bank='%s'\n",
                    med->color.x, med->color.y, med->color.z, med->depth,
                    med->bank.c_str(), med->outer_bank.c_str());
        if (med->color.x == 0 && med->color.y == 50 && med->color.z == 170) ++passed;
        else { ++failed; std::printf("  [-] Cor inesperada\n"); }
        if (!med->bank.empty() && !med->outer_bank.empty()) ++passed;
        else { ++failed; std::printf("  [-] Sem texturas de beira\n"); }
    } else {
        std::printf("  [-] Mediterranean Sea nao encontrado\n");
        ++failed;
    }

    if (const auto* river = gdm.water_bodies().find_river("Greek River"); river) {
        std::printf("  [+] Greek River: color=(%.0f,%.0f,%.0f), clusters=%zu\n",
                    river->color.x, river->color.y, river->color.z,
                    river->object_clusters.size());
        ++passed;
    } else {
        std::printf("  [-] Greek River nao encontrado\n");
        ++failed;
    }

    // --- 4. Validar UnitTransforms ---
    std::printf("\n--- Validacao de UnitTransforms ---\n");

    const struct { const char* from; const char* to; } expected_transforms[] = {
        {"Wall Long", "Gate"},
        {"Villager Norse", "Ulfsark"},
        {"Dwarf", "Ulfsark"},
    };
    for (const auto& t : expected_transforms) {
        auto transforms = gdm.unit_transforms().find_from(t.from);
        bool found = false;
        for (const auto& tr : transforms) {
            if (tr.to == t.to) { found = true; break; }
        }
        if (found) {
            std::printf("  [+] %s -> %s\n", t.from, t.to);
            ++passed;
        } else {
            std::printf("  [-] %s -> %s nao encontrado\n", t.from, t.to);
            ++failed;
        }
    }

    // --- 5. Validar Soundsets ---
    std::printf("\n--- Validacao de Soundsets ---\n");

    const char* expected_soundsets[] = {"GatherChop", "GatherMine", "Arrow", "Build", "MaleDeath"};
    for (const auto* ssname : expected_soundsets) {
        if (const auto* ss = gdm.find_soundset(ssname); ss) {
            std::printf("  [+] Soundset '%s': %zu sons, volume=%.2f, maxnum=%d\n",
                        ss->name.c_str(), ss->sounds.size(), ss->volume, ss->max_num);
            ++passed;
        } else {
            std::printf("  [-] Soundset '%s' nao encontrado\n", ssname);
            ++failed;
        }
    }

    // Validar protounit sounds (Hoplite)
    if (const auto* psd = gdm.find_protounit_sound("Hoplite"); psd) {
        std::printf("  [+] Hoplite sound def: %zu sound types\n", psd->sound_types.size());
        ++passed;
    } else {
        std::printf("  [-] Hoplite sound def nao encontrado\n");
        ++failed;
    }

    // --- 6. Validar Tecnologias ---
    std::printf("\n--- Validacao de Tecnologias ---\n");

    const char* expected_techs[] = {"Age 1 Hoplite", "Age 1 Toxotes"};
    for (const auto* tname : expected_techs) {
        if (const auto* t = gdm.find_tech(tname); t) {
            std::printf("  [+] Tech '%s': dbid=%d, research_points=%.0f\n",
                        t->name.c_str(), t->dbid, t->research_points);
            ++passed;
        } else {
            std::printf("  [-] Tech '%s' nao encontrado\n", tname);
            ++failed;
        }
    }

    // --- Resumo ---
    std::printf("\n=== RESUMO ===\n");
    std::printf("Passou: %d, Falhou: %d\n", passed, failed);
    std::printf("Expansao montada: %s\n", has_expansion ? "sim" : "nao");

    return failed == 0 ? 0 : 1;
}
