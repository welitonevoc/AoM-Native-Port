#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::string data_bar_path = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data/data.bar";
    if (argc > 1) {
        data_bar_path = argv[1];
    }

    std::printf("========================================\n");
    std::printf("   Age of Mythology - XMB Parser Test   \n");
    std::printf("========================================\n");
    std::printf("Opening BAR: %s\n", data_bar_path.c_str());

    aom::BarArchive bar;
    if (!bar.open(data_bar_path)) {
        std::fprintf(stderr, "Failed to open BAR archive: %s\n", data_bar_path.c_str());
        return 1;
    }

    std::printf("BAR opened successfully (%zu entries)\n\n", bar.file_count());

    // Test a list of XMB files
    const std::vector<std::string> test_files = {
        "corpsedecay.xmb",
        "resources.xmb",
        "playercolors.xmb",
        "clifftypes.xmb",
        "minorgods.xmb",
        "proto.xmb"
    };

    for (const auto& filename : test_files) {
        const aom::BarEntry* entry = bar.find(filename);
        if (!entry) {
            std::printf("[-] File not found in archive: %s\n", filename.c_str());
            continue;
        }

        std::vector<uint8_t> raw = bar.extract(*entry);
        if (raw.empty()) {
            std::printf("[-] Failed to extract: %s\n", filename.c_str());
            continue;
        }

        auto start = std::chrono::high_resolution_clock::now();
        auto root = aom::XmbParser::parse(raw);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        if (!root) {
            std::printf("[-] FAILED to parse XMB: %s\n", filename.c_str());
            continue;
        }

        size_t total_nodes = 0;
        root->visit([&total_nodes](const aom::XmbElement&, int) {
            ++total_nodes;
        });

        std::printf("[+] %-20s | Size: %7zu B | Root: <%s> | Children: %4zu | Nodes: %5zu | Parsed in: %6ld us\n",
                    filename.c_str(), raw.size(), root->name.c_str(),
                    root->children.size(), total_nodes, static_cast<long>(elapsed.count()));

        // Print details for a couple of nodes
        if (filename == "proto.xmb" && !root->children.empty()) {
            const auto* first_unit = root->children[0].get();
            std::printf("    -> First unit: name=\"%s\", id=\"%s\", sub-elements: %zu\n",
                        first_unit->get_attribute("name").c_str(),
                        first_unit->get_attribute("id").c_str(),
                        first_unit->children.size());

            const auto* hp = first_unit->find_child("initialhitpoints");
            if (hp) {
                std::printf("    -> Hit points: %f\n", hp->text_as_float());
            }
        }
    }

    std::printf("\nAll XMB parser tests completed successfully!\n");
    return 0;
}
