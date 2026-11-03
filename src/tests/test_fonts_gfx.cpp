#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "fonts/font_system.h"
#include "graphics/gfx_config.h"

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
    std::cout << "========================================" << std::endl;
    std::cout << " AoM Fonts & GfxConfig REA Test Suite  " << std::endl;
    std::cout << "========================================" << std::endl;

    const std::string fonts_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/fonts";
    const std::string gfx_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/gfxconfig";
    const std::string gfx2_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/gfxconfig2";

    // ---------------------------------------------------------
    // TEST 1: Fonts System (fonts2.xml & xfonts.xml)
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Font Definitions (fonts2.xml & xfonts.xml) ---" << std::endl;
    FontManager font_mgr;

    bool f2_ok = font_mgr.load_fonts_xml(fonts_dir + "/fonts2.xml");
    TEST_ASSERT(f2_ok, "Loaded and parsed 'fonts2.xml' (UTF-16)");
    TEST_ASSERT(font_mgr.font_count() == 104, "fonts2.xml defines 104 unique fonts (105 tags with duplicate SmallIngame 12)");

    const FontDef* arial10 = font_mgr.find_font("Arial 10");
    TEST_ASSERT(arial10 != nullptr, "Found 'Arial 10'");
    if (arial10) {
        TEST_ASSERT(arial10->typeface == "Arial", "'Arial 10' typeface is 'Arial'");
        TEST_ASSERT(arial10->ascent_height == 10, "'Arial 10' ascent height is 10");
        TEST_ASSERT(arial10->bold == true, "'Arial 10' bold is true");
        TEST_ASSERT(arial10->antialiased == false, "'Arial 10' antialiased is false");
        TEST_ASSERT(arial10->shadow == true, "'Arial 10' shadow is true");
    }

    const FontDef* arial_out12 = font_mgr.find_font("Arial Outlined 12");
    TEST_ASSERT(arial_out12 != nullptr, "Found 'Arial Outlined 12'");
    if (arial_out12) {
        TEST_ASSERT(arial_out12->outline == true, "'Arial Outlined 12' outline is true");
        TEST_ASSERT(arial_out12->ascent_height == 12, "'Arial Outlined 12' ascent height is 12");
    }

    // Load expansion fonts (xfonts.xml)
    FontManager xfont_mgr;
    bool xf_ok = xfont_mgr.load_fonts_xml(fonts_dir + "/xfonts.xml");
    TEST_ASSERT(xf_ok, "Loaded and parsed 'xfonts.xml' (UTF-16)");
    TEST_ASSERT(xfont_mgr.font_count() == 109, "xfonts.xml defines 109 unique fonts (110 tags)");

    // ---------------------------------------------------------
    // TEST 2: MSK Bitmasks & Unicode Glyphs Fallback
    // ---------------------------------------------------------
    std::cout << "\n--- Testing MSK Bitmasks & Unicode Fallback ---" << std::endl;
    bool msk_arial_ok = font_mgr.load_msk_file("Arial", fonts_dir + "/arial.msk");
    TEST_ASSERT(msk_arial_ok, "Loaded 'arial.msk' (8192 bytes = 65536 bits)");

    bool msk_simsun_ok = font_mgr.load_msk_file("SimSun", fonts_dir + "/simsun.msk");
    TEST_ASSERT(msk_simsun_ok, "Loaded 'simsun.msk' (8192 bytes = 65536 bits)");

    // Test glyph support
    TEST_ASSERT(font_mgr.supports_glyph("Arial", 'A'), "Arial supports ASCII 'A'");
    TEST_ASSERT(font_mgr.supports_glyph("Arial", 'Z'), "Arial supports ASCII 'Z'");
    TEST_ASSERT(font_mgr.supports_glyph("Arial", '5'), "Arial supports numeric '5'");
    TEST_ASSERT(font_mgr.supports_glyph("Arial", 0x03A9), "Arial supports Greek Capital Omega (U+03A9)");
    TEST_ASSERT(!font_mgr.supports_glyph("Arial", 0x4E00), "Arial does NOT support CJK Han (U+4E00)");
    TEST_ASSERT(font_mgr.supports_glyph("SimSun", 0x4E00), "SimSun supports CJK Han (U+4E00)");

    // Test font fallback resolution
    std::string resolved_ascii = font_mgr.resolve_typeface("Arial", 'A');
    TEST_ASSERT(resolved_ascii == "Arial", "Resolved typeface for 'A' is 'Arial'");

    std::string resolved_cjk = font_mgr.resolve_typeface("Arial", 0x4E00);
    TEST_ASSERT(resolved_cjk == "SimSun", "Resolved typeface for CJK (U+4E00) automatically fell back to 'SimSun'");

    // ---------------------------------------------------------
    // TEST 3: GFX Vendor and Device Mappings
    // ---------------------------------------------------------
    std::cout << "\n--- Testing GFX Vendor & Device Mappings ---" << std::endl;
    GfxConfigManager gfx_mgr;

    bool nvidia_ok = gfx_mgr.load_vendor_file(gfx2_dir + "/0x10de_nvidia.gfx");
    TEST_ASSERT(nvidia_ok, "Loaded '0x10de_nvidia.gfx'");

    const GfxDeviceInfo* dev_gf3 = gfx_mgr.find_device("0x10de", "0x0200");
    TEST_ASSERT(dev_gf3 != nullptr, "Found device 0x0200 (GeForce3)");
    if (dev_gf3) {
        TEST_ASSERT(dev_gf3->device_name == "GeForce3", "Device name matches 'GeForce3'");
        TEST_ASSERT(dev_gf3->profile_file == "geforce3.gfx", "Device profile is 'geforce3.gfx'");
    }

    bool ati_ok = gfx_mgr.load_vendor_file(gfx2_dir + "/0x1002_ati.gfx");
    TEST_ASSERT(ati_ok, "Loaded '0x1002_ati.gfx'");

    bool intel_ok = gfx_mgr.load_vendor_file(gfx2_dir + "/0x8086_intel.gfx");
    TEST_ASSERT(intel_ok, "Loaded '0x8086_intel.gfx'");

    TEST_ASSERT(gfx_mgr.vendor_device_count() >= 100, "Loaded over 100 GPU device mappings");

    // ---------------------------------------------------------
    // TEST 4: GFX Profiles & Hardware Render States
    // ---------------------------------------------------------
    std::cout << "\n--- Testing GFX Profiles & Render States ---" << std::endl;
    bool prof_gf3_ok = gfx_mgr.load_profile_file(gfx_dir + "/geforce3.gfx");
    TEST_ASSERT(prof_gf3_ok, "Loaded 'geforce3.gfx' profile");

    const GfxProfile* gf3_profile = gfx_mgr.find_profile("geforce3.gfx");
    TEST_ASSERT(gf3_profile != nullptr, "Found 'geforce3.gfx' profile");
    if (gf3_profile) {
        TEST_ASSERT(gf3_profile->anisotropy == 1, "GeForce3 anisotropy is 1");
        TEST_ASSERT(gf3_profile->quality_level == "high", "GeForce3 quality level is 'high'");
        TEST_ASSERT(!gf3_profile->resolutions.empty(), "GeForce3 has supported resolutions");
    }

    bool hw_ok = gfx_mgr.load_profile_file(gfx_dir + "/hardware.gfx");
    TEST_ASSERT(hw_ok, "Loaded 'hardware.gfx'");
    const GfxProfile* hw_profile = gfx_mgr.find_profile("hardware.gfx");
    TEST_ASSERT(hw_profile != nullptr, "Found 'hardware.gfx' profile");
    if (hw_profile) {
        auto it_cull = hw_profile->render_states.find("D3DRS_CULLMODE");
        TEST_ASSERT(it_cull != hw_profile->render_states.end() && it_cull->second == "D3DCULL_CCW",
                    "D3DRS_CULLMODE is D3DCULL_CCW (counter-clockwise)");
        auto it_z = hw_profile->render_states.find("D3DRS_ZENABLE");
        TEST_ASSERT(it_z != hw_profile->render_states.end() && it_z->second == "TRUE",
                    "D3DRS_ZENABLE is TRUE");
    }

    // ---------------------------------------------------------
    // TEST 5: TSS Multi-Texture Stage State Pipelines
    // ---------------------------------------------------------
    std::cout << "\n--- Testing TSS Texture Stage State Pipelines ---" << std::endl;
    bool tss_blend_ok = gfx_mgr.load_tss_file(gfx2_dir + "/mtlinearblend.tss");
    TEST_ASSERT(tss_blend_ok, "Loaded 'mtlinearblend.tss'");
    const TextureStagePipeline* blend_pipe = gfx_mgr.find_pipeline("mtlinearblend.tss");
    TEST_ASSERT(blend_pipe != nullptr, "Found 'mtlinearblend.tss' pipeline");
    if (blend_pipe) {
        TEST_ASSERT(blend_pipe->passes == 1, "mtlinearblend.tss has 1 pass");
        TEST_ASSERT(!blend_pipe->pass_list.empty(), "mtlinearblend pass list not empty");
        auto it_cop = blend_pipe->pass_list[0].operations.find("D3DTSS_COLOROP[1]");
        TEST_ASSERT(it_cop != blend_pipe->pass_list[0].operations.end() &&
                    it_cop->second == "D3DTOP_BLENDFACTORALPHA",
                    "mtlinearblend stage 1 color op is D3DTOP_BLENDFACTORALPHA (splatting)");
    }

    bool tss_light_ok = gfx_mgr.load_tss_file(gfx2_dir + "/mtlightmap.tss");
    TEST_ASSERT(tss_light_ok, "Loaded 'mtlightmap.tss'");

    bool tss_fog_ok = gfx_mgr.load_tss_file(gfx2_dir + "/mtfogmask.tss");
    TEST_ASSERT(tss_fog_ok, "Loaded 'mtfogmask.tss'");

    // ---------------------------------------------------------
    // TEST 6: Mobile GLES3 Profile Configuration
    // ---------------------------------------------------------
    std::cout << "\n--- Testing Mobile GLES3 Profile Configuration ---" << std::endl;
    gfx_mgr.configure_mobile_gles3("Qualcomm Adreno (TM) 640");
    const GfxProfile* mob_profile = gfx_mgr.active_profile();
    TEST_ASSERT(mob_profile != nullptr, "Active mobile profile configured");
    if (mob_profile) {
        TEST_ASSERT(mob_profile->quality_level == "high", "Adreno 640 configured to 'high' quality");
        TEST_ASSERT(mob_profile->anisotropy == 4, "Mobile anisotropy configured to 4x");
        auto it_tc = mob_profile->features.find("textureCompression");
        TEST_ASSERT(it_tc != mob_profile->features.end() && it_tc->second == "ETC2/EAC",
                    "Standard mobile texture compression is ETC2/EAC");
    }

    std::cout << "========================================" << std::endl;
    std::cout << " ALL FONTS & GFX TESTS PASSED! (100%)  " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
