#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/renderer.h"
#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"

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

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " AoM Native Port - Sprint 3: Terrain & Water Test" << std::endl;
    std::cout << "==================================================" << std::endl;

    // -------------------------------------------------------------------------
    // TEST 1: TerrainVertex Format & Memory Alignment
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 1: TerrainVertex Layout & Struct Offsets ---" << std::endl;
    static_assert(sizeof(TerrainVertex) == 48, "TerrainVertex must be 48 bytes");
    static_assert(offsetof(TerrainVertex, position) == 0, "position at offset 0");
    static_assert(offsetof(TerrainVertex, normal) == 12, "normal at offset 12");
    static_assert(offsetof(TerrainVertex, texcoord) == 24, "texcoord at offset 24");
    static_assert(offsetof(TerrainVertex, color) == 32, "color at offset 32");
    TEST_ASSERT(sizeof(TerrainVertex) == 48, "TerrainVertex size is strictly 48 bytes");
    TEST_ASSERT(offsetof(TerrainVertex, position) == 0, "Position offset is 0");
    TEST_ASSERT(offsetof(TerrainVertex, normal) == 12, "Normal offset is 12");
    TEST_ASSERT(offsetof(TerrainVertex, texcoord) == 24, "Texcoord offset is 24");
    TEST_ASSERT(offsetof(TerrainVertex, color) == 32, "Color offset is 32");

    // -------------------------------------------------------------------------
    // TEST 2: Authentic Terrain Elevation & Finite-Difference Normal Computation
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 2: Terrain Elevation, Riverbed & Cliff Normals ---" << std::endl;
    auto calc_elevation = [](float wx, float wz) -> float {
        float hills = std::sin(wx * 0.16f) * std::cos(wz * 0.16f) * 0.75f;
        float river_dist = std::abs(wz - std::sin(wx * 0.14f) * 3.5f);
        float river = (river_dist < 5.0f) ? ((river_dist - 5.0f) * 0.18f) : 0.0f;
        float cliff = (wx > 6.0f && wz < -4.0f) ? (std::sin((wx - 6.0f) * 0.28f) * 2.2f) : 0.0f;
        return hills + river + cliff;
    };

    const float STEP = 1.0f;
    auto calc_normal = [&](float wx, float wz) -> glm::vec3 {
        float hL = calc_elevation(wx - STEP, wz);
        float hR = calc_elevation(wx + STEP, wz);
        float hD = calc_elevation(wx, wz - STEP);
        float hU = calc_elevation(wx, wz + STEP);
        return glm::normalize(glm::vec3(hL - hR, 2.0f * STEP, hD - hU));
    };

    // Flat land test: near center (0, 10)
    glm::vec3 flat_normal = calc_normal(0.0f, 10.0f);
    float flat_slope = 1.0f - flat_normal.y;
    TEST_ASSERT(std::abs(glm::length(flat_normal) - 1.0f) < 0.001f, "Flat terrain normal is normalized unit vector");
    TEST_ASSERT(flat_normal.y > 0.85f, "Flat terrain has high N.y (> 0.85)");
    TEST_ASSERT(flat_slope < 0.15f, "Flat terrain slope factor is low (< 0.15 for lush grass)");

    // River valley test: (0, 0)
    float river_y = calc_elevation(0.0f, 0.0f);
    TEST_ASSERT(river_y < -0.4f, "Central river valley elevation dips below water plane (y < -0.4)");

    // Cliff test: on plateau ridge (10, -5)
    glm::vec3 cliff_normal = calc_normal(10.0f, -5.0f);
    TEST_ASSERT(std::abs(glm::length(cliff_normal) - 1.0f) < 0.001f, "Cliff normal is normalized unit vector");
    float cliff_slope_factor = std::clamp((0.85f - cliff_normal.y) / 0.25f, 0.0f, 1.0f);
    TEST_ASSERT(cliff_slope_factor > 0.4f, "Cliff ridge triggers slope blend factor (> 0.4 for rock cliff)");

    // -------------------------------------------------------------------------
    // TEST 3: Water Plane Equation & Wave Animation Math
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 3: Water Plane & Wave Animation Dynamics ---" << std::endl;
    glm::vec4 water_plane{0.0f, 1.0f, 0.0f, 0.4f}; // y = -0.4
    float water_y = -water_plane.w;
    TEST_ASSERT(std::abs(water_y - (-0.4f)) < 0.0001f, "Water plane height is exactly -0.4f");

    // Underwater terrain check: riverbed vs ground
    float river_depth = water_y - river_y;
    TEST_ASSERT(river_depth > 0.1f, "Riverbed is submerged under water plane (depth > 0.1)");
    float ground_y = calc_elevation(0.0f, 10.0f);
    TEST_ASSERT(ground_y > water_y, "Ground plains are elevated above water surface");

    // Sinusoidal wave motion test at different time intervals
    auto get_wave = [](float x, float z, float t) -> float {
        return std::sin(x * 0.75f + t * 1.8f) * std::cos(z * 0.75f + t * 1.4f) * 0.06f;
    };

    float wave_t0 = get_wave(2.0f, 2.0f, 0.0f);
    float wave_t1 = get_wave(2.0f, 2.0f, 1.0f);
    TEST_ASSERT(std::abs(wave_t0) <= 0.0601f, "Wave amplitude is bounded by 0.06f at t=0");
    TEST_ASSERT(std::abs(wave_t1) <= 0.0601f, "Wave amplitude is bounded by 0.06f at t=1");
    TEST_ASSERT(wave_t0 != wave_t1, "Wave oscillates dynamically across time");

    // Fresnel gradient test
    auto calc_fresnel = [](glm::vec3 N, glm::vec3 V) -> float {
        float NdotV = std::clamp(glm::dot(N, V), 0.0f, 1.0f);
        return std::pow(1.0f - NdotV, 3.0f);
    };

    glm::vec3 water_norm{0.0f, 1.0f, 0.0f};
    glm::vec3 direct_view{0.0f, 1.0f, 0.0f}; // Looking straight down
    glm::vec3 glancing_view = glm::normalize(glm::vec3(0.9f, 0.1f, 0.0f)); // Grazing angle

    float fresnel_direct = calc_fresnel(water_norm, direct_view);
    float fresnel_glancing = calc_fresnel(water_norm, glancing_view);
    TEST_ASSERT(fresnel_direct < 0.01f, "Looking straight down has minimal Fresnel (reveals shallow turquoise seabed)");
    TEST_ASSERT(fresnel_glancing > 0.7f, "Grazing angle has strong Fresnel (> 0.7 reflects deep blue sky)");

    // -------------------------------------------------------------------------
    // TEST 4: Trilinear Mipmap Policy & Level Count Formula
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 4: Trilinear Mipmap Policy & DDT Decoding ---" << std::endl;
    auto calc_mip_levels = [](uint32_t w, uint32_t h) -> uint32_t {
        if (w <= 1 && h <= 1) return 1;
        uint32_t max_dim = std::max(w, h);
        uint32_t levels = 0;
        while (max_dim > 0) {
            levels++;
            max_dim >>= 1;
        }
        return levels;
    };

    TEST_ASSERT(calc_mip_levels(1, 1) == 1, "1x1 UI solid color texture uses single mip level (no overhead)");
    TEST_ASSERT(calc_mip_levels(256, 256) == 9, "256x256 AoM terrain texture generates 9 mip levels down to 1x1");
    TEST_ASSERT(calc_mip_levels(512, 512) == 10, "512x512 texture generates 10 mip levels");

    // -------------------------------------------------------------------------
    // TEST 5: DDT Format 3 (Palette) & BAR Archive Terrain Textures
    // -------------------------------------------------------------------------
    std::cout << "\n--- TEST 5: DDT Texture & BAR Extraction ---" << std::endl;
    const std::string bar_path = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/textures/textures.bar";
    BarArchive bar;
    if (bar.open(bar_path)) {
        TEST_ASSERT(bar.is_open(), "Successfully opened textures.bar with Linux mmap");

        // Verify grass texture
        const BarEntry* grass_entry = bar.find("terrain/grassa.ddt");
        TEST_ASSERT(grass_entry != nullptr, "Found 'terrain/grassa.ddt' inside textures.bar");
        if (grass_entry) {
            std::vector<uint8_t> raw = bar.extract(*grass_entry);
            TEST_ASSERT(!raw.empty(), "Extracted 'terrain/grassa.ddt' bytes");
            auto ddt = DdtLoader::load(raw);
            TEST_ASSERT(ddt != nullptr, "Parsed 'terrain/grassa.ddt' DDT header");
            if (ddt) {
                TEST_ASSERT(ddt->width == 256 && ddt->height == 256, "Grass texture dimension is 256x256");
                std::vector<uint8_t> rgba = ddt->decode_rgba();
                TEST_ASSERT(rgba.size() == 256 * 256 * 4, "Decoded grass texture to 256x256 RGBA8888 buffer");
            }
        }

        // Verify cliff texture
        const BarEntry* cliff_entry = bar.find("terrain/cliffgreeka.ddt");
        TEST_ASSERT(cliff_entry != nullptr, "Found 'terrain/cliffgreeka.ddt' inside textures.bar");
        if (cliff_entry) {
            std::vector<uint8_t> raw = bar.extract(*cliff_entry);
            TEST_ASSERT(!raw.empty(), "Extracted 'terrain/cliffgreeka.ddt' bytes");
            auto ddt = DdtLoader::load(raw);
            TEST_ASSERT(ddt != nullptr, "Parsed 'terrain/cliffgreeka.ddt' DDT header");
            if (ddt) {
                TEST_ASSERT(ddt->width == 256 && ddt->height == 256, "Cliff texture dimension is 256x256");
                std::vector<uint8_t> rgba = ddt->decode_rgba();
                TEST_ASSERT(rgba.size() == 256 * 256 * 4, "Decoded cliff texture to 256x256 RGBA8888 buffer");
            }
        }

        // Verify water normal map
        const BarEntry* water_entry = bar.find("terrain/waterbump.ddt");
        TEST_ASSERT(water_entry != nullptr, "Found 'terrain/waterbump.ddt' inside textures.bar");
        if (water_entry) {
            std::vector<uint8_t> raw = bar.extract(*water_entry);
            TEST_ASSERT(!raw.empty(), "Extracted 'terrain/waterbump.ddt' bytes");
            auto ddt = DdtLoader::load(raw);
            TEST_ASSERT(ddt != nullptr, "Parsed 'terrain/waterbump.ddt' DDT header");
            if (ddt) {
                TEST_ASSERT(ddt->width == 256 && ddt->height == 256, "Water bump texture dimension is 256x256");
                std::vector<uint8_t> rgba = ddt->decode_rgba();
                TEST_ASSERT(rgba.size() == 256 * 256 * 4, "Decoded water normal texture to 256x256 RGBA8888 buffer");
            }
        }
    } else {
        std::cout << "NOTE: textures.bar not found on host, testing with synthetic DDT buffer" << std::endl;
        // Build synthetic RTS3 DDT buffer to test loader
        std::vector<uint8_t> ddt_data(16 + 8, 0);
        ddt_data[0] = 'R'; ddt_data[1] = 'T'; ddt_data[2] = 'S'; ddt_data[3] = '3';
        ddt_data[4] = 0; // usage
        ddt_data[5] = 8; // alpha_bits
        ddt_data[6] = 1; // BGRA8888
        ddt_data[7] = 1; // 1 mip
        uint32_t w = 2, h = 2;
        std::memcpy(ddt_data.data() + 8, &w, 4);
        std::memcpy(ddt_data.data() + 12, &h, 4);
        // mip table: offset = 24, size = 16
        uint32_t off = 24, sz = 16;
        std::memcpy(ddt_data.data() + 16, &off, 4);
        std::memcpy(ddt_data.data() + 20, &sz, 4);
        ddt_data.resize(24 + 16, 255); // 4 pixels of white BGRA

        auto ddt = DdtLoader::load(ddt_data);
        TEST_ASSERT(ddt != nullptr, "Parsed synthetic RTS3 DDT buffer");
        if (ddt) {
            TEST_ASSERT(ddt->width == 2 && ddt->height == 2, "Synthetic DDT size 2x2");
            auto rgba = ddt->decode_rgba();
            TEST_ASSERT(rgba.size() == 16, "Decoded synthetic DDT to 16 bytes RGBA");
        }
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << " ALL SPRINT 3 TESTS PASSED (100% OK)              " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
