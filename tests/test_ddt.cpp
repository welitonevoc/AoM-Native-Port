#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"

#include <chrono>
#include <cstdio>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    std::string textures_bar_path = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/textures/textures.bar";
    if (argc > 1) {
        textures_bar_path = argv[1];
    }

    std::printf("========================================\n");
    std::printf("   Age of Mythology - DDT Format Test   \n");
    std::printf("========================================\n");

    aom::BarArchive bar;
    if (!bar.open(textures_bar_path)) {
        std::fprintf(stderr, "Failed to open textures.bar\n");
        return 1;
    }

    std::printf("Opened textures.bar with %zu entries\n\n", bar.file_count());

    // Test textures covering all 6 formats:
    // Format 1: BGRA
    // Format 3: PALETTE
    // Format 4: DXT1
    // Format 5: AOM_5BPP
    // Format 6: AOM_8BPP
    // Format 7: GREYSCALE
    const std::vector<std::string> sample_textures = {
        "icons/editor ocean.ddt",                          // Format 3: Palette
        "icons/building town center egypt icon 64.ddt",   // Format 3: Palette
        "animal aurochs map.ddt",                         // Format 4: DXT1
        "agamemnon map.ddt",                              // Format 4: DXT1
        "animal baboon corpse bodya.ddt",                 // Format 5: AOM_5BPP
    };

    for (const auto& name : sample_textures) {
        const aom::BarEntry* entry = bar.find(name);
        if (!entry) {
            std::printf("[-] Not found: %s\n", name.c_str());
            continue;
        }

        std::vector<uint8_t> raw = bar.extract(*entry);
        if (raw.empty()) {
            std::printf("[-] Failed to extract: %s\n", name.c_str());
            continue;
        }

        auto start = std::chrono::high_resolution_clock::now();
        auto ddt = aom::DdtLoader::load(raw);
        if (!ddt) {
            std::printf("[-] Failed to load DDT header: %s\n", name.c_str());
            continue;
        }

        std::vector<uint8_t> rgba = ddt->decode_rgba(0);
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start);

        if (rgba.empty()) {
            std::printf("[-] Failed to decode RGBA for %s (format %d)\n",
                        name.c_str(), static_cast<int>(ddt->format));
            continue;
        }

        std::printf("[+] %-45s | %4ux%-4u | Fmt: %d | Mips: %2u | RGBA: %7zu B | %5ld us\n",
                    name.c_str(), ddt->width, ddt->height,
                    static_cast<int>(ddt->format), ddt->mipmap_count,
                    rgba.size(), static_cast<long>(elapsed.count()));
    }

    // Now test a bulk sample across formats
    std::printf("\nTesting first 100 DDT textures across all formats...\n");
    size_t tested = 0, passed = 0;
    for (const auto& entry : bar.entries()) {
        if (entry.filename.size() < 4 ||
            entry.filename.compare(entry.filename.size() - 4, 4, ".ddt") != 0) {
            continue;
        }

        std::vector<uint8_t> raw = bar.extract(entry);
        if (raw.empty()) continue;

        auto ddt = aom::DdtLoader::load(raw);
        if (!ddt) continue;

        std::vector<uint8_t> rgba = ddt->decode_rgba(0);
        if (!rgba.empty()) {
            ++passed;
        }
        ++tested;
        if (tested >= 100) break;
    }

    std::printf("Bulk test results: %zu / %zu decoded successfully (%.1f%%)!\n",
                passed, tested, (passed * 100.0f) / tested);

    return 0;
}
