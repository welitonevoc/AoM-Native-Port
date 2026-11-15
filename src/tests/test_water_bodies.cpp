#include "data/water_bodies.h"
#include "data/unit_transform.h"
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"
#include "ecs/gameplay.h"

#include <cstdio>
#include <cstring>
#include <string>

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
    aom::BarArchive bar;
    if (!bar.open(data_bar)) {
        std::printf("SKIP: nao foi possivel abrir %s (dados do jogo nao disponiveis)\n",
                    data_bar.c_str());
        return 0;
    }

    int passed = 0, failed = 0;

    // --- waterbodies.xmb ---
    {
        const aom::BarEntry* entry = bar.find("waterbodies.xmb");
        if (entry) {
            auto data = bar.extract(*entry);
            if (!data.empty()) {
                auto root = aom::XmbParser::parse(data);
                if (root) {
                    aom::WaterBodyManager wb;
                    if (wb.load_from_xmb(*root)) {
                        std::printf("[+] waterbodies: %zu rios, %zu oceanos, %zu lagos\n",
                                    wb.river_count(), wb.ocean_count(), wb.lake_count());

                        // Verifica alguns corpos d'água conhecidos
                        if (wb.find_river("Greek River")) ++passed; else { ++failed; std::printf("[-] Greek River nao encontrado\n"); }
                        if (wb.find_ocean("Mediterranean Sea")) ++passed; else { ++failed; std::printf("[-] Mediterranean Sea nao encontrado\n"); }
                        if (wb.find_lake("Watering Hole")) ++passed; else { ++failed; std::printf("[-] Watering Hole nao encontrado\n"); }

                        // Verifica cor do Mediterrâneo (0 50 170)
                        if (const auto* med = wb.find_ocean("Mediterranean Sea")) {
                            if (med->color.x == 0 && med->color.y == 50 && med->color.z == 170) ++passed;
                            else { ++failed; std::printf("[-] Cor do Mediterraneo inesperada: %.0f %.0f %.0f\n",
                                                         med->color.x, med->color.y, med->color.z); }
                        }

                        // Verifica texturas de beira do Mediterrâneo
                        if (const auto* med = wb.find_ocean("Mediterranean Sea")) {
                            bool has_shoreline = false;
                            for (const auto& tp : med->texture_placements) {
                                if (tp.texture.find("ShorelineMediterranean") != std::string::npos) {
                                    has_shoreline = true;
                                    break;
                                }
                            }
                            if (has_shoreline) ++passed;
                            else { ++failed; std::printf("[-] Mediterraneo sem texturas de shoreline\n"); }

                            // Verifica atributos bottom, bank, outer_bank
                            if (!med->bottom.empty()) ++passed; else { ++failed; std::printf("[-] Mediterraneo sem bottom\n"); }
                            if (!med->bank.empty()) ++passed; else { ++failed; std::printf("[-] Mediterraneo sem bank\n"); }
                            if (!med->outer_bank.empty()) ++passed; else { ++failed; std::printf("[-] Mediterraneo sem outer_bank\n"); }

                            std::printf("    Mediterraneo: bottom='%s' bank='%s' outer_bank='%s' color=(%.0f,%.0f,%.0f) depth=%.1f\n",
                                        med->bottom.c_str(), med->bank.c_str(), med->outer_bank.c_str(),
                                        med->color.x, med->color.y, med->color.z, med->depth);

                            // Verifica object_clusters (rochas, árvores na beira)
                            if (!med->object_clusters.empty()) ++passed;
                            else { ++failed; std::printf("[-] Mediterraneo sem object_clusters\n"); }

                            for (const auto& oc : med->object_clusters) {
                                std::printf("    Cluster: %zu protounits", oc.protounits.size());
                                for (const auto& pu : oc.protounits) {
                                    std::printf(" '%s'", pu.c_str());
                                    if (!pu.empty()) ++passed;
                                }
                                std::printf("\n");
                            }
                        }
                    } else {
                        ++failed; std::printf("[-] Falha ao carregar waterbodies.xmb\n");
                    }
                } else {
                    ++failed; std::printf("[-] Falha ao parsear waterbodies.xmb\n");
                }
            }
        } else {
            std::printf("SKIP: waterbodies.xmb nao encontrado no data.bar\n");
        }
    }

    // --- unittransform.xmb ---
    {
        const aom::BarEntry* entry = bar.find("unittransform.xmb");
        if (entry) {
            auto data = bar.extract(*entry);
            if (!data.empty()) {
                auto root = aom::XmbParser::parse(data);
                if (root) {
                    aom::UnitTransformManager utm;
                    if (utm.load_from_xmb(*root)) {
                        std::printf("[+] unittransform: %zu transformacoes\n", utm.count());

                        // Verifica transformacoes conhecidas do XML de referencia
                        auto transforms = utm.find_from("Wall Long");
                        bool found_wall_to_gate = false;
                        for (const auto& t : transforms) {
                            if (t.to == "Gate") { found_wall_to_gate = true; break; }
                        }
                        if (found_wall_to_gate) ++passed; else { ++failed; std::printf("[-] Wall Long -> Gate nao encontrado\n"); }

                        auto vil_transforms = utm.find_from("Villager Norse");
                        bool found_vil_to_ulfsark = false;
                        for (const auto& t : vil_transforms) {
                            if (t.to == "Ulfsark") { found_vil_to_ulfsark = true; break; }
                        }
                        if (found_vil_to_ulfsark) ++passed; else { ++failed; std::printf("[-] Villager Norse -> Ulfsark nao encontrado\n"); }
                    } else {
                        ++failed; std::printf("[-] Falha ao carregar unittransform.xmb\n");
                    }
                } else {
                    ++failed; std::printf("[-] Falha ao parsear unittransform.xmb\n");
                }
            }
        } else {
            std::printf("SKIP: unittransform.xmb nao encontrado no data.bar\n");
        }
    }

    // --- Teste de transformacao de unidade no GameplayWorld ---
    {
        aom::GameplayWorld world(nullptr);  // sem proto.xmb -> defaults
        aom::UnitTransformManager utm;

        // Carrega o unittransform.xmb do data.bar se disponível
        bool utm_loaded = false;
        {
            const aom::BarEntry* entry = bar.find("unittransform.xmb");
            if (entry) {
                auto data = bar.extract(*entry);
                if (!data.empty()) {
                    auto root = aom::XmbParser::parse(data);
                    if (root && utm.load_from_xmb(*root)) {
                        utm_loaded = true;
                    }
                }
            }
        }
        world.set_unit_transform_manager(utm_loaded ? &utm : nullptr);

        // Spawna uma unidade "Wall Long" (usa defaults pois proto_root_ é null)
        glm::vec3 spawn_pos(2.0f, 0.0f, 3.0f);
        aom::UnitStats wall_stats;
        wall_stats.name = "Wall Long";
        wall_stats.max_hp = 400.0f;
        aom::Entity wall_id = world.spawn_unit(aom::Team::FRIENDLY, spawn_pos, wall_stats);

        // Verifica transformacoes disponiveis
        auto transforms = world.available_transforms(wall_id);
        bool has_wall_to_gate = false;
        for (const auto& t : transforms) {
            if (t.from == "Wall Long" && t.to == "Gate") { has_wall_to_gate = true; break; }
        }
        if (utm_loaded && has_wall_to_gate) ++passed;
        else if (!utm_loaded) std::printf("    [SKIP] unittransform.xmb nao carregado, pulando teste de transformacao\n");
        else { ++failed; std::printf("[-] Wall Long nao tem transformacao para Gate\n"); }

        // Executa a transformacao Wall Long -> Gate
        if (utm_loaded && has_wall_to_gate) {
            aom::Entity gate_id = world.transform_unit(wall_id, "Gate");
            if (gate_id != aom::INVALID_ENTITY) ++passed;
            else { ++failed; std::printf("[-] transform_unit Wall Long->Gate retornou invalido\n"); }

            // Verifica que a unidade original foi removida
            if (world.get_unit(wall_id) == nullptr) ++passed;
            else { ++failed; std::printf("[-] Unidade original nao foi removida apos transformacao\n"); }

            // Verifica que a nova unidade existe com nome "Gate"
            if (world.get_unit(gate_id) != nullptr && world.get_unit(gate_id)->unit_type_name == "Gate") ++passed;
            else { ++failed; std::printf("[-] Nova unidade nao tem nome 'Gate'\n"); }

            // Verifica que a posicao foi preservada
            auto* gt = world.get_transform(gate_id);
            if (gt && glm::length(gt->position - spawn_pos) < 0.001f) ++passed;
            else { ++failed; std::printf("[-] Posicao nao preservada apos transformacao\n"); }
        }
    }

    std::printf("\nResultado: %d passou, %d falhou\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
