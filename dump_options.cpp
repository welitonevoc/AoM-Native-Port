#include "assets/xmb_parser.h"
#include "ui/ui_layout.h"
#include <cstdio>
#include <fstream>
#include <vector>

void dump_gadget(const aom::UIGadgetDef& g, int indent) {
    for (int i = 0; i < indent; ++i) std::printf("  ");
    std::printf("- [%s] name='%s' text='%s' bg='%s' states=%zu rect=(%.0f,%.0f,%.0f,%.0f) hidden=%d\n",
                g.type.c_str(), g.name.c_str(), g.text.c_str(), g.bg_texture.c_str(),
                g.state_backgrounds.size(), g.rect.x, g.rect.y, g.rect.w, g.rect.h, g.hidden);
    for (const auto& c : g.children) {
        dump_gadget(c, indent + 1);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    aom::UILayoutManager mgr;
    auto res = mgr.load_layout_from_data("test", data);
    std::printf("Success: %d, roots: %zu\n", res.success, res.root_gadgets.size());
    for (const auto& g : res.root_gadgets) {
        dump_gadget(g, 0);
    }
    return 0;
}
