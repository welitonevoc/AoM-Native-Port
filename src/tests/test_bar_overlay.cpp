/**
 * @file test_bar_overlay.cpp
 * @brief Testes de regressão do suporte a múltiplos mounts (overlay) do
 *        BarArchive e da carga da expansão "The Titans".
 *
 * Uso:
 *   test_bar_overlay [RAIZ_DOS_DADOS]
 *
 * A raiz pode ser passada como argumento ou pela variável de ambiente
 * AOM_DATA_ROOT. Exemplo:
 *   AOM_DATA_ROOT="/sdcard/AoM" ./test_bar_overlay
 *
 * Sem os dados do jogo o teste é ignorado (SKIP) e retorna sucesso, para
 * não quebrar execuções em CI sem a cópia legal do jogo.
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"
#include "data/game_data.h"
#include "campaign/campaign.h"

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return 1; \
        } else { \
            std::cout << "PASS: " << msg << std::endl; \
        } \
    } while (0)

using namespace aom;

namespace {

bool file_exists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

std::string resolve_root(int argc, char** argv) {
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        return argv[1];
    }
    const char* env = std::getenv("AOM_DATA_ROOT");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
    return "";
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

/// Coleta os nomes de <unit> declarados em um XMB do archive.
std::vector<std::string> collect_unit_names(const BarArchive& bar, const char* filename) {
    std::vector<std::string> names;
    const BarEntry* entry = bar.find(filename);
    if (!entry) return names;
    const std::vector<uint8_t> bytes = bar.extract(*entry);
    if (bytes.empty()) return names;
    auto root = XmbParser::parse(bytes);
    if (!root) return names;
    for (const auto& child : root->children) {
        if (child && child->name == "unit") {
            const std::string name = to_lower(child->get_attribute("name"));
            if (!name.empty()) {
                names.push_back(name);
            }
        }
    }
    return names;
}

} // anonymous namespace

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << " AoM BAR Overlay / Titans Expansion Test" << std::endl;
    std::cout << "========================================" << std::endl;

    const std::string root = resolve_root(argc, argv);
    if (root.empty()) {
        std::cout << "SKIP: informe a raiz dos dados (argv[1] ou AOM_DATA_ROOT)" << std::endl;
        return 0;
    }

    const std::string data_base = root + "/data/data.bar";
    const std::string data_exp = root + "/data/data2.bar";

    if (!file_exists(data_base) || !file_exists(data_exp)) {
        std::cout << "SKIP: 'data/data.bar' e 'data/data2.bar' não encontrados em '"
                  << root << "'" << std::endl;
        return 0;
    }

    // ---------------------------------------------------------
    // TEST 1: archives isolados
    // ---------------------------------------------------------
    std::cout << "\n--- Archives isolados ---" << std::endl;

    BarArchive base;
    TEST_ASSERT(base.open(data_base), "data.bar aberto");
    TEST_ASSERT(base.mount_count() == 1, "data.bar tem 1 mount");
    TEST_ASSERT(base.is_open(), "data.bar está aberto");

    BarArchive expansion;
    TEST_ASSERT(expansion.open(data_exp), "data2.bar aberto");
    TEST_ASSERT(expansion.mount_count() == 1, "data2.bar tem 1 mount");

    const size_t base_count = base.file_count();
    const size_t expansion_count = expansion.file_count();
    TEST_ASSERT(base_count > 0, "data.bar tem entradas");
    TEST_ASSERT(expansion_count > 0, "data2.bar tem entradas");

    // ---------------------------------------------------------
    // TEST 2: overlay (mount da expansão por cima do base)
    // ---------------------------------------------------------
    std::cout << "\n--- Overlay base + expansão ---" << std::endl;

    BarArchive merged;
    TEST_ASSERT(merged.open_pair(data_base, data_exp), "overlay montado");
    TEST_ASSERT(merged.mount_count() == 2, "overlay tem 2 mounts");
    TEST_ASSERT(merged.is_open(), "overlay está aberto");

    // A união deve ter pelo menos as entradas da expansão e nunca mais que
    // a soma das duas.
    TEST_ASSERT(merged.file_count() >= expansion_count,
                "união tem pelo menos as entradas da expansão");
    TEST_ASSERT(merged.file_count() <= base_count + expansion_count,
                "união não excede a soma dos dois archives");

    // Entrada exclusiva da expansão (protox.xmb)
    const BarEntry* protox = merged.find("protox.xmb");
    TEST_ASSERT(protox != nullptr, "protox.xmb (exclusivo da expansão) encontrado");
    if (protox) {
        TEST_ASSERT(protox->mount_index == 1, "protox.xmb vem do mount da expansão");
        const std::vector<uint8_t> bytes = merged.extract(*protox);
        TEST_ASSERT(!bytes.empty(), "protox.xmb extraído com conteúdo");
    }

    // Entrada que só existe no base (data2.bar não tem prwordlist.xmb)
    const BarEntry* prword = merged.find("prwordlist.xmb");
    TEST_ASSERT(prword != nullptr, "prwordlist.xmb (exclusivo do base) continua acessível");
    if (prword) {
        TEST_ASSERT(prword->mount_index == 0, "prwordlist.xmb vem do mount base");
        TEST_ASSERT(!merged.extract(*prword).empty(), "prwordlist.xmb extraído com conteúdo");
    }

    // Entrada compartilhada: a expansão vence
    const BarEntry* proto_merged = merged.find("proto.xmb");
    const BarEntry* proto_base = base.find("proto.xmb");
    const BarEntry* proto_exp = expansion.find("proto.xmb");
    TEST_ASSERT(proto_merged != nullptr && proto_base != nullptr && proto_exp != nullptr,
                "proto.xmb presente nos dois archives");
    if (proto_merged && proto_base && proto_exp) {
        TEST_ASSERT(proto_merged->mount_index == 1,
                    "proto.xmb compartilhada resolve para a expansão");
        const std::vector<uint8_t> merged_bytes = merged.extract(*proto_merged);
        const std::vector<uint8_t> exp_bytes = expansion.extract(*proto_exp);
        TEST_ASSERT(merged_bytes.size() == exp_bytes.size(),
                    "extração de proto.xmb usa os dados da expansão");
    }

    // Busca case-insensitive
    TEST_ASSERT(merged.find("PROTO.XMB") != nullptr, "busca é case-insensitive");

    // Mount inválido não corrompe o archive
    TEST_ASSERT(!merged.mount(root + "/data/arquivo_inexistente.bar"),
                "mount de arquivo inexistente falha");
    TEST_ASSERT(merged.mount_count() == 2, "mount falhou não altera a lista de mounts");
    TEST_ASSERT(merged.find("protox.xmb") != nullptr, "overlay continua íntegro após falha");

    // ---------------------------------------------------------
    // TEST 3: GameDataManager com proto.xmb + protox.xmb
    // ---------------------------------------------------------
    std::cout << "\n--- GameDataManager (proto + protox) ---" << std::endl;

    GameDataManager game_data;
    TEST_ASSERT(game_data.load_from_bar(merged), "dados do jogo carregados do overlay");

    // proto.xmb e protox.xmb compartilham quase todas as unidades: o merge
    // não pode duplicá-las. A contagem esperada é a união dos dois XMB.
    std::vector<std::string> expected;
    for (const char* file : {"proto.xmb", "protox.xmb"}) {
        for (const std::string& name : collect_unit_names(merged, file)) {
            expected.push_back(name);
        }
    }
    std::sort(expected.begin(), expected.end());
    expected.erase(std::unique(expected.begin(), expected.end()), expected.end());

    TEST_ASSERT(expected.size() >= 623, "proto.xmb tem as unidades do jogo base");
    TEST_ASSERT(game_data.unit_count() == expected.size(),
                "registro de unidades sem duplicatas (união de proto + protox)");

    // Unidade exclusiva da expansão The Titans
    TEST_ASSERT(game_data.find_unit("Tremor") != nullptr,
                "unidade da expansão 'Tremor' encontrada");
    // Unidade do jogo base
    TEST_ASSERT(game_data.find_unit("Hoplite") != nullptr,
                "unidade do jogo base 'Hoplite' encontrada");

    // Tecnologias: techtree.xmb + techtreex.xmb
    TEST_ASSERT(game_data.tech_count() > 0, "árvore tecnológica carregada");

    // ---------------------------------------------------------
    // TEST 4: campaignas
    // ---------------------------------------------------------
    std::cout << "\n--- Campanhas ---" << std::endl;

    if (file_exists(root + "/data/xpack campaign.cpx")) {
        CampaignManager titans;
        TEST_ASSERT(titans.load_campaign(root + "/data/xpack campaign.cpx"),
                    "campanha The Titans ('xpack campaign.cpx') carregada");
        TEST_ASSERT(titans.scenario_count() > 0, "campanha The Titans tem cenários");
    } else {
        std::cout << "SKIP: 'data/xpack campaign.cpx' não encontrado" << std::endl;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << " TODOS OS TESTES PASSARAM                 " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
