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
    int passed = 0, failed = 0;

    // Teste de conversao de coordenadas size1024
    {
        auto rect = aom::UILayoutManager::convert_size1024("20 10 220 35", 1024, 768);
        if (rect.x == 20.0f && rect.y == 10.0f && rect.z == 200.0f && rect.w == 25.0f) {
            ++passed;
            std::printf("[+] convert_size1024(1024x768) = OK (w=right-left, h=bottom-top)\n");
        } else {
            ++failed;
            std::printf("[-] convert_size1024(1024x768) inesperado: %.0f,%.0f,%.0fx%.0f\n",
                        rect.x, rect.y, rect.z, rect.w);
        }

        // Teste em resolução diferente (ex: 1920x1080)
        auto rect2 = aom::UILayoutManager::convert_size1024("20 10 220 35", 1920, 1080);
        float expected_x = 20.0f * (1920.0f / 1024.0f);
        float expected_y = 10.0f * (1080.0f / 768.0f);
        float expected_w = 200.0f * (1920.0f / 1024.0f);
        float expected_h = 25.0f * (1080.0f / 768.0f);
        if (std::abs(rect2.x - expected_x) < 0.01f && std::abs(rect2.y - expected_y) < 0.01f &&
            std::abs(rect2.z - expected_w) < 0.01f && std::abs(rect2.w - expected_h) < 0.01f) {
            ++passed;
            std::printf("[+] convert_size1024(1920x1080) = OK\n");
        } else {
            ++failed;
            std::printf("[-] convert_size1024(1920x1080) inesperado\n");
        }
    }

    // Teste de parse de cor
    {
        auto c = aom::UILayoutManager::parse_color("0 50 170");
        if (c.x == 0 && c.y == 50 && c.z == 170) ++passed;
        else { ++failed; std::printf("[-] parse_color('0 50 170') inesperado\n"); }

        auto rgba = aom::UILayoutManager::parse_color_rgba("255 208 0", 255);
        if (rgba == 0xFFFFD000u) ++passed;  // 0xAARRGGBB
        else { ++failed; std::printf("[-] parse_color_rgba('255 208 0') = 0x%08X\n", rgba); }
    }

    // Teste de UV coords e comandos em UILayout + UIButton
    {
        aom::UILayoutResult lr;
        lr.success = true;
        lr.layout_name = "test_layout";

        aom::UIGadgetDef btn_def;
        btn_def.name = "TestButton";
        btn_def.type = "stateButton";
        btn_def.rect = {100, 100, 200, 50};
        btn_def.text = "$$19825$$Learn To Play";
        btn_def.command = "gadgetReal(\"MainMenuOptions\") gadgetUnReal(\"LearnSubMenu\")";
        btn_def.texture_coords = glm::vec4(0.0f, 0.5f, 0.68f, 1.0f);
        btn_def.has_texture_coords = true;
        btn_def.z = 10;
        lr.root_gadgets.push_back(btn_def);

        aom::UIManager ui;
        bool callback_called = false;
        std::unordered_map<std::string, std::function<void()>> callbacks = {
            {"return_to_main_menu", [&]() { callback_called = true; }}
        };

        size_t created = aom::apply_layout_to_ui(lr, &ui, 1024, 768, callbacks);
        if (created == 1) {
            ++passed;
            std::printf("[+] apply_layout_to_ui criou botao com sucesso\n");
        } else {
            ++failed;
            std::printf("[-] apply_layout_to_ui falhou na criacao de botao\n");
        }

        const auto& buttons = ui.buttons();
        if (!buttons.empty() && buttons[0].has_uv &&
            buttons[0].uv_min.x == 0.0f && buttons[0].uv_min.y == 0.5f &&
            buttons[0].uv_max.x == 0.68f && buttons[0].uv_max.y == 1.0f) {
            ++passed;
            std::printf("[+] UIButton recebeu UVs (0, 0.5, 0.68, 1) com sucesso\n");
        } else {
            ++failed;
            std::printf("[-] UIButton nao recebeu UVs corretas\n");
        }

        // Teste do callback via comando nativo AoM
        if (!buttons.empty() && buttons[0].onClick) {
            buttons[0].onClick();
            if (callback_called) {
                ++passed;
                std::printf("[+] Comando AoM gadgetReal(MainMenuOptions) mapeado para callback com sucesso\n");
            } else {
                ++failed;
                std::printf("[-] Comando AoM nao acionou callback return_to_main_menu\n");
            }
        }
    }

    std::string data_path = data_root();
    if (argc > 1) {
        data_path = argv[1];
    }

    std::string data_bar = data_path + "/data/data.bar";
    aom::BarArchive bar;
    if (!bar.open(data_bar)) {
        std::printf("SKIP: nao foi possivel abrir %s (dados do jogo nao disponiveis)\n",
                    data_bar.c_str());
        std::printf("\nResultado: %d passou, %d falhou\n", passed, failed);
        return failed == 0 ? 0 : 1;
    }

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

    std::printf("\nResultado: %d passou, %d falhou\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
