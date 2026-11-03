#include "locale/localization.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            std::exit(1); \
        } else { \
            std::printf("PASS: %s\n", msg); \
        } \
    } while (0)

int main() {
    std::printf("========================================\n");
    std::printf(" AoM LCID & Localization REA Test Suite \n");
    std::printf("========================================\n\n");

    const std::string base_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology";
    const std::string proj_docs = "/storage/emulated/0/Documents/AoM-Native-Port/docs";

    auto& loc = aom::LocalizationManager::instance();

    // -------------------------------------------------------------
    // 1. Testing LCID Registry (Folders 1025..1060, 2052, 2070, 3076, 3082)
    // -------------------------------------------------------------
    std::printf("--- Testing Windows LCID Registry (30 Languages) ---\n");
    const auto& all_lcids = loc.get_all_lcids();
    TEST_ASSERT(all_lcids.size() == 30, "LCID registry contains exactly 30 language definitions");

    // Check Portuguese (Brazil) - 1046
    const auto* pt_br = loc.find_lcid(1046);
    TEST_ASSERT(pt_br != nullptr, "Found LCID 1046");
    TEST_ASSERT(pt_br->iso_code == "pt-BR", "LCID 1046 ISO code is 'pt-BR'");
    TEST_ASSERT(pt_br->language_name == "Portuguese", "LCID 1046 language is 'Portuguese'");
    TEST_ASSERT(pt_br->region_name == "Brazil", "LCID 1046 region is 'Brazil'");

    // Check English (US) - 1033
    const auto* en_us = loc.find_lcid(1033);
    TEST_ASSERT(en_us != nullptr, "Found LCID 1033");
    TEST_ASSERT(en_us->iso_code == "en-US", "LCID 1033 ISO code is 'en-US'");
    TEST_ASSERT(en_us->has_bootstrapper == true, "LCID 1033 includes MSI/Web bootstrappers");

    // Check German - 1031
    const auto* de_de = loc.find_lcid(1031);
    TEST_ASSERT(de_de != nullptr, "Found LCID 1031 (German)");
    TEST_ASSERT(de_de->has_bootstrapper == true, "LCID 1031 includes MSI/Web bootstrappers");

    // Check Chinese Simplified - 2052
    const auto* zh_cn = loc.find_lcid(2052);
    TEST_ASSERT(zh_cn != nullptr, "Found LCID 2052 (Chinese Simplified)");

    // Check Spanish Modern - 3082
    const auto* es_es = loc.find_lcid(3082);
    TEST_ASSERT(es_es != nullptr, "Found LCID 3082 (Spanish Modern)");

    // Reverse lookup by ISO
    const auto* found_iso = loc.find_by_iso("pt-BR");
    TEST_ASSERT(found_iso != nullptr && found_iso->lcid == 1046, "Lookup by ISO 'pt-BR' returned LCID 1046");

    // Verify disk folders for all 30 LCIDs
    size_t folders_verified = 0;
    for (const auto& info : all_lcids) {
        std::string folder_path = base_dir + "/" + std::to_string(info.lcid);
        if (std::filesystem::exists(folder_path)) {
            // Verify dwintl.dll exists in each
            std::string dll_path = folder_path + "/dwintl.dll";
            if (std::filesystem::exists(dll_path)) {
                ++folders_verified;
            }
        }
    }
    TEST_ASSERT(folders_verified == 30, "All 30 LCID folders verified on disk with dwintl.dll");

    // -------------------------------------------------------------
    // 2. Testing In-game Localization PE Stringtable (language.dll)
    // -------------------------------------------------------------
    std::printf("\n--- Testing In-Game Stringtable (language.dll) ---\n");
    std::string lang_dll = base_dir + "/language.dll";
    TEST_ASSERT(std::filesystem::exists(lang_dll), "language.dll exists on disk");

    bool loaded_base = loc.load_pe_stringtable(lang_dll, false);
    TEST_ASSERT(loaded_base, "Parsed RT_STRING resources from language.dll");
    TEST_ASSERT(loc.base_string_count() >= 7000, "Loaded over 7,000 localized strings from language.dll");

    // Verify key in-game Brazilian Portuguese strings
    std::string food_str = loc.get_string(19771);
    TEST_ASSERT(food_str == "Alimento", "String ID 19771 maps to 'Alimento'");

    std::string favor_str = loc.get_string(19774);
    TEST_ASSERT(favor_str == "Proteção", "String ID 19774 maps to 'Proteção'");

    std::string stance_str = loc.get_string(19775);
    TEST_ASSERT(stance_str == "Posturas", "String ID 19775 maps to 'Posturas'");

    // -------------------------------------------------------------
    // 3. Testing Titans Expansion Stringtable (xpacklanguage.dll)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Expansion Stringtable (xpacklanguage.dll) ---\n");
    std::string xpack_dll = base_dir + "/xpacklanguage.dll";
    TEST_ASSERT(std::filesystem::exists(xpack_dll), "xpacklanguage.dll exists on disk");

    bool loaded_xpack = loc.load_pe_stringtable(xpack_dll, true);
    TEST_ASSERT(loaded_xpack, "Parsed RT_STRING resources from xpacklanguage.dll");
    TEST_ASSERT(loc.expansion_string_count() >= 9000, "Loaded over 9,000 localized strings from xpacklanguage.dll");
    TEST_ASSERT(loc.string_count() >= 10000, "Total unique string pool exceeds 10,000 strings");

    // Verify Titans expansion campaign dialogue
    std::string gods_bless = loc.get_string(40100);
    TEST_ASSERT(!gods_bless.empty(), "Retrieved String ID 40100 from Titans expansion");

    std::string arkantos_dlg = loc.get_string(40300);
    TEST_ASSERT(arkantos_dlg.find("Arkantos") != std::string::npos, "String ID 40300 contains Arkantos dialogue");

    std::string chiron_dlg = loc.get_string(42000);
    TEST_ASSERT(chiron_dlg.find("Chiron") != std::string::npos, "String ID 42000 contains Chiron dialogue");

    // -------------------------------------------------------------
    // 4. Testing Dr. Watson Error Reporting Metadata (dwintl.dll)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Watson Error Reporting Metadata (dwintl.dll) ---\n");
    auto meta_en = aom::LocalizationManager::extract_version_metadata_file(base_dir + "/1033/dwintl.dll");
    TEST_ASSERT(meta_en.file_description == "Microsoft Application Error Reporting", "1033 dwintl FileDescription is 'Microsoft Application Error Reporting'");
    TEST_ASSERT(meta_en.internal_name == "DWIntl", "1033 dwintl InternalName is 'DWIntl'");
    TEST_ASSERT(meta_en.company_name == "Microsoft Corporation", "1033 dwintl CompanyName is 'Microsoft Corporation'");

    auto meta_pt = aom::LocalizationManager::extract_version_metadata_file(base_dir + "/1046/dwintl.dll");
    TEST_ASSERT(meta_pt.file_description == "Relatório de erros de aplicativos Microsoft", "1046 dwintl FileDescription is 'Relatório de erros de aplicativos Microsoft'");
    TEST_ASSERT(meta_pt.internal_name == "DWIntl", "1046 dwintl InternalName is 'DWIntl'");

    // -------------------------------------------------------------
    // 5. Testing Bootstrapper Executables (msiloadr.bin & webloadr.bin)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Bootstrapper Binaries (msiloadr & webloadr) ---\n");
    std::string msiloadr_path = base_dir + "/1033/msiloadr.bin";
    TEST_ASSERT(std::filesystem::exists(msiloadr_path), "msiloadr.bin exists in 1033");
    auto meta_msi = aom::LocalizationManager::extract_version_metadata_file(msiloadr_path);
    TEST_ASSERT(meta_msi.file_description.find("Windows Installer Bootstrapper") != std::string::npos, "msiloadr is Windows Installer Bootstrapper");

    std::string webloadr_path = base_dir + "/1033/webloadr.bin";
    TEST_ASSERT(std::filesystem::exists(webloadr_path), "webloadr.bin exists in 1033");

    // -------------------------------------------------------------
    // 6. Testing Community Modifications (Voobly Mods)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Voobly Community Mods ---\n");
    std::string voobly_dir = base_dir + "/Voobly Mods";
    size_t patches = loc.discover_voobly_mods(voobly_dir);
    TEST_ASSERT(patches >= 3, "Discovered at least 3 Voobly community balance patches");

    bool found_patch6 = false;
    for (const auto& p : loc.get_community_patches()) {
        if (p.name.find("Balance Patch 6.0") != std::string::npos) {
            found_patch6 = true;
            TEST_ASSERT(p.is_balance_patch, "Voobly Balance Patch 6.0 marked as balance patch");
            TEST_ASSERT(p.version == "6.0", "Voobly Balance Patch version is 6.0");
        }
    }
    TEST_ASSERT(found_patch6, "Found Voobly Balance Patch 6.0");

    // -------------------------------------------------------------
    // 7. Testing Documentation Alignment (docs/)
    // -------------------------------------------------------------
    std::printf("\n--- Testing Documentation Alignment (docs/) ---\n");
    TEST_ASSERT(std::filesystem::exists(proj_docs + "/ANALISE_COMPLETA.md"), "Project ANALISE_COMPLETA.md present");
    TEST_ASSERT(std::filesystem::exists(proj_docs + "/ANIMATION_MAP.md"), "Project ANIMATION_MAP.md present");
    TEST_ASSERT(std::filesystem::exists(proj_docs + "/MVP.md"), "Project MVP.md present");

    TEST_ASSERT(std::filesystem::exists(base_dir + "/docs/aom ai script help file.rtf"), "Official AI help file present");
    TEST_ASSERT(std::filesystem::exists(base_dir + "/docs/aom random map help file.rtf"), "Official RM help file present");
    TEST_ASSERT(std::filesystem::exists(base_dir + "/docs/aom scenario editor faq.rtf"), "Official Scenario Editor FAQ present");
    TEST_ASSERT(std::filesystem::exists(base_dir + "/docs/aomxp_manual_bp.pdf"), "Official Brazilian Portuguese Titans manual present");

    std::printf("\n========================================\n");
    std::printf(" ALL LCID & LOCALIZATION TESTS PASSED!  \n");
    std::printf("========================================\n");

    return 0;
}
