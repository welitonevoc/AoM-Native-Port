#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace {

const char* data_root() {
    if (const char* env = std::getenv("AOM_DATA_ROOT")) return env;
    return "/storage/emulated/0/Download/Games/PC/Age of Mythology";
}

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

bool ends_with_ic(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return to_lower(s.substr(s.size() - suffix.size())) == to_lower(suffix);
}

/// Compara um buffer RGBA decodificado com um PNG de referência.
/// Retorna o número de pixels diferentes e a diferença absoluta média.
struct CompareResult {
    bool success = false;
    size_t total_pixels = 0;
    size_t diff_pixels = 0;
    double mean_abs_diff = 0.0;
    std::string error;
};

CompareResult compare_rgba_to_png(const std::vector<uint8_t>& rgba,
                                   uint32_t width, uint32_t height,
                                   const std::string& png_path) {
    CompareResult result;
    result.total_pixels = static_cast<size_t>(width) * height;

    int png_w = 0, png_h = 0, png_ch = 0;
    unsigned char* png_data = stbi_load(png_path.c_str(), &png_w, &png_h, &png_ch, 4);
    if (!png_data) {
        result.error = "falha ao carregar PNG de referência: " + png_path;
        return result;
    }

    if (static_cast<uint32_t>(png_w) != width || static_cast<uint32_t>(png_h) != height) {
        result.error = "tamanho diverge: decoder=" + std::to_string(width) + "x" +
                       std::to_string(height) + " PNG=" + std::to_string(png_w) + "x" +
                       std::to_string(png_h);
        stbi_image_free(png_data);
        return result;
    }

    size_t diff = 0;
    long long abs_sum = 0;
    for (size_t i = 0; i < result.total_pixels * 4; i++) {
        int d = std::abs(static_cast<int>(rgba[i]) - static_cast<int>(png_data[i]));
        abs_sum += d;
        if (d > 0) diff++;
    }

    stbi_image_free(png_data);

    result.success = true;
    result.diff_pixels = diff;
    result.mean_abs_diff = result.total_pixels > 0
        ? static_cast<double>(abs_sum) / (result.total_pixels * 4.0)
        : 0.0;
    return result;
}

} // anonymous namespace

int main(int argc, char** argv) {
    std::string data_path = data_root();
    std::string textures_dir = std::string(argv[1]); // pasta tests/reference/textures
    std::string textures_bar = data_path + "/textures/textures.bar";

    if (argc > 2) {
        textures_bar = argv[2];
    }

    aom::BarArchive bar;
    if (!bar.open(textures_bar)) {
        std::printf("SKIP: nao foi possivel abrir %s\n", textures_bar.c_str());
        return 0;
    }

    std::printf("Opened textures.bar with %zu entries\n\n", bar.file_count());

    int tested = 0, passed = 0, failed = 0, skipped = 0;

    // Itera sobre todos os PNGs na pasta de referencia
    for (const auto& entry : fs::directory_iterator(textures_dir)) {
        if (!entry.is_regular_file()) continue;
        if (!ends_with_ic(entry.path().string(), ".png")) continue;

        std::string png_name = entry.path().filename().string();
        std::string png_path = entry.path().string();

        // O nome do PNG do ModHQ costuma ser o nome da textura no BAR
        // (ex: "agamemnon map.png" -> "agamemnon map.ddt")
        std::string ddt_name = png_name;
        if (ends_with_ic(ddt_name, ".png")) {
            ddt_name = ddt_name.substr(0, ddt_name.size() - 4);
        }
        ddt_name += ".ddt";

        const aom::BarEntry* bar_entry = bar.find(ddt_name);
        if (!bar_entry) {
            // Tenta sem espacos ou com underline
            std::string alt = ddt_name;
            std::replace(alt.begin(), alt.end(), ' ', '_');
            bar_entry = bar.find(alt);
            if (!bar_entry) {
                skipped++;
                continue;
            }
        }

        auto data = bar.extract(*bar_entry);
        if (data.empty()) {
            std::printf("[-] Falha ao extrair %s\n", ddt_name.c_str());
            failed++;
            continue;
        }

        auto ddt = aom::DdtLoader::load(data);
        if (!ddt) {
            std::printf("[-] Falha ao carregar DDT: %s (format=%d)\n",
                        ddt_name.c_str(), ddt ? static_cast<int>(ddt->format) : -1);
            failed++;
            continue;
        }

        auto rgba = ddt->decode_rgba(0);
        if (rgba.empty()) {
            std::printf("[-] Falha ao decodificar RGBA: %s\n", ddt_name.c_str());
            failed++;
            continue;
        }

        auto cmp = compare_rgba_to_png(rgba, ddt->width, ddt->height, png_path);
        tested++;

        if (!cmp.success) {
            std::printf("[-] %s: %s\n", ddt_name.c_str(), cmp.error.c_str());
            failed++;
        } else if (cmp.diff_pixels == 0) {
            std::printf("[+] %s: %ux%u fmt=%d -- IDENTICO ao gabarito\n",
                        ddt_name.c_str(), ddt->width, ddt->height, static_cast<int>(ddt->format));
            passed++;
        } else {
            double pct = 100.0 * cmp.diff_pixels / cmp.total_pixels;
            std::printf("[~] %s: %ux%u fmt=%d -- %zu/%zu pixels diferentes (%.2f%%), media abs diff=%.2f\n",
                        ddt_name.c_str(), ddt->width, ddt->height,
                        static_cast<int>(ddt->format),
                        cmp.diff_pixels, cmp.total_pixels, pct, cmp.mean_abs_diff);
            // Diferenças pequenas (alpha premultiplicado, etc.) podem ser aceitaveis
            if (pct < 5.0 && cmp.mean_abs_diff < 5.0) {
                passed++;
            } else {
                failed++;
            }
        }
    }

    std::printf("\n=== RESULTADO ===\n");
    std::printf("Testados: %d | Passou: %d | Falhou: %d | Pulados (sem DDT): %d\n",
                tested, passed, failed, skipped);
    std::printf("Taxa de sucesso: %.1f%%\n",
                tested > 0 ? 100.0 * passed / tested : 0.0);

    return failed == 0 ? 0 : 1;
}
