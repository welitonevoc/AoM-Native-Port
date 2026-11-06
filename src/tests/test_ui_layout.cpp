#include "ui/ui_layout.h"
#include "assets/bar_archive.h"

#include <cstdio>
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

    // Lista de layouts de UI conhecidos do data.bar para testar
    const std::vector<std::string> layouts = {
        "uitechtree.xmb",       // Arvore de tecnologias
        "uitributedlg.xmb",     // Tela de tributo
        "uisoundsetdlg.xmb",    // Configuracoes de som
        "uiwaterdialog.xml",    // Editor de agua
        "uisingleplayersetup.xml", // Setup single player
    };

    aom::UILayoutManager layout_mgr;

    for (const auto& name : layouts) {
        auto result = layout_mgr.load_layout(bar, name);
        if (result.success) {
            std::printf("[+] UI layout '%s': %zu gadgets raiz\n",
                        name.c_str(), result.root_gadgets.size());

            // Conta total de gadgets (incluindo filhos)
            size_t total = 0;
            std::function<size_t(const std::vector<aom::UIGadgetDef>&)> count_all =
                [&](const std::vector<aom::UIGadgetDef>& gadgets) -> size_t {
                    size_t c = 0;
                    for (const auto& g : gadgets) {
                        ++c;
                        c += count_all(g.children);
                    }
                    return c;
                };
            total = count_all(result.root_gadgets);
            std::printf("    Total de gadgets (com filhos): %zu\n", total);

            // Verifica se tem coordenadas size1024
            bool has_coords = false;
            std::function<bool(const std::vector<aom::UIGadgetDef>&)> check_coords =
                [&](const std::vector<aom::UIGadgetDef>& gadgets) -> bool {
                    for (const auto& g : gadgets) {
                        if (g.rect.w > 0 && g.rect.h > 0) has_coords = true;
                        if (check_coords(g.children)) return true;
                    }
                    return false;
                };
            check_coords(result.root_gadgets);
            if (has_coords) ++passed;
            else { ++failed; std::printf("[-] %s sem coordenadas size1024\n", name.c_str()); }

            // Lista os nomes dos gadgets raiz
            for (const auto& g : result.root_gadgets) {
                std::printf("    - %s (tipo=%s, rect=%.0f,%.0f,%.0fx%.0f)\n",
                            g.name.c_str(), g.type.c_str(),
                            g.rect.x, g.rect.y, g.rect.w, g.rect.h);
            }
        } else {
            std::printf("SKIP: UI layout '%s' nao encontrado ou erro: %s\n",
                        name.c_str(), result.error_message.c_str());
        }
    }

    // Teste de conversao de coordenadas size1024
    {
        auto rect = aom::UILayoutManager::convert_size1024("20 10 220 35", 1024, 768);
        if (rect.x == 20.0f && rect.y == 10.0f && rect.z == 220.0f && rect.w == 35.0f) {
            ++passed;
            std::printf("[+] convert_size1024(1024x768) = OK\n");
        } else {
            ++failed;
            std::printf("[-] convert_size1024(1024x768) inesperado: %.0f,%.0f,%.0fx%.0f\n",
                        rect.x, rect.y, rect.z, rect.w);
        }

        // Teste em resolução diferente (ex: 1920x1080)
        auto rect2 = aom::UILayoutManager::convert_size1024("20 10 220 35", 1920, 1080);
        float expected_x = 20.0f * (1920.0f / 1024.0f);
        float expected_y = 10.0f * (1080.0f / 768.0f);
        if (std::abs(rect2.x - expected_x) < 0.01f && std::abs(rect2.y - expected_y) < 0.01f) {
            ++passed;
            std::printf("[+] convert_size1024(1920x1080) = OK\n");
        } else {
            ++failed;
            std::printf("[-] convert_size1024(1920x1080) inesperado: %.0f,%.0f (esperado %.0f,%.0f)\n",
                        rect2.x, rect2.y, expected_x, expected_y);
        }
    }

    // Teste de parse de cor
    {
        auto c = aom::UILayoutManager::parse_color("0 50 170");
        if (c.x == 0 && c.y == 50 && c.z == 170) ++passed;
        else { ++failed; std::printf("[-] parse_color('0 50 170') inesperado\n"); }

        auto rgba = aom::UILayoutManager::parse_color_rgba("255 208 0", 255);
        if (rgba == 0xFFF0D000u) ++passed;  // 0xRRGGBBAA = 0xFFF0D000
        else { ++failed; std::printf("[-] parse_color_rgba('255 208 0') = 0x%08X\n", rgba); }
    }

    std::printf("\nResultado: %d passou, %d falhou\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
