#include "locale/localization.h"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AoM-Locale"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

// Helper: Convert UTF-16LE buffer to UTF-8 string
std::string utf16le_to_utf8(const uint16_t* u16, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        uint32_t cp = u16[i];
        // Handle surrogate pairs
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
            uint32_t next = u16[i + 1];
            if (next >= 0xDC00 && next <= 0xDFFF) {
                cp = 0x10000 + (((cp - 0xD800) << 10) | (next - 0xDC00));
                ++i;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // anonymous namespace

LocalizationManager::LocalizationManager() {
    init_lcid_registry();
}

LocalizationManager::~LocalizationManager() = default;

LocalizationManager& LocalizationManager::instance() {
    static LocalizationManager inst;
    return inst;
}

void LocalizationManager::init_lcid_registry() {
    lcids_ = {
        {1025, "ar-SA", "Arabic", "Saudi Arabia", false},
        {1028, "zh-TW", "Chinese", "Taiwan (Traditional)", true},
        {1029, "cs-CZ", "Czech", "Czech Republic", false},
        {1030, "da-DK", "Danish", "Denmark", false},
        {1031, "de-DE", "German", "Germany", true},
        {1032, "el-GR", "Greek", "Greece", false},
        {1033, "en-US", "English", "United States", true},
        {1035, "fi-FI", "Finnish", "Finland", false},
        {1036, "fr-FR", "French", "France", true},
        {1037, "he-IL", "Hebrew", "Israel", false},
        {1038, "hu-HU", "Hungarian", "Hungary", false},
        {1040, "it-IT", "Italian", "Italy", true},
        {1041, "ja-JP", "Japanese", "Japan", true},
        {1042, "ko-KR", "Korean", "Korea", true},
        {1043, "nl-NL", "Dutch", "Netherlands", false},
        {1044, "nb-NO", "Norwegian", "Bokmål", false},
        {1045, "pl-PL", "Polish", "Poland", false},
        {1046, "pt-BR", "Portuguese", "Brazil", false},
        {1048, "ro-RO", "Romanian", "Romania", false},
        {1049, "ru-RU", "Russian", "Russia", false},
        {1050, "hr-HR", "Croatian", "Croatia", false},
        {1051, "sk-SK", "Slovak", "Slovakia", false},
        {1053, "sv-SE", "Swedish", "Sweden", false},
        {1054, "th-TH", "Thai", "Thailand", false},
        {1055, "tr-TR", "Turkish", "Turkey", false},
        {1060, "sl-SI", "Slovenian", "Slovenia", false},
        {2052, "zh-CN", "Chinese", "PRC (Simplified)", true},
        {2070, "pt-PT", "Portuguese", "Portugal", false},
        {3076, "zh-HK", "Chinese", "Hong Kong SAR", false},
        {3082, "es-ES", "Spanish", "Spain (Modern)", true}
    };

    lcid_map_.clear();
    iso_map_.clear();
    for (const auto& info : lcids_) {
        lcid_map_[info.lcid] = info;
        iso_map_[info.iso_code] = info.lcid;
    }
}

const LcidInfo* LocalizationManager::find_lcid(uint32_t lcid) const {
    auto it = lcid_map_.find(lcid);
    return (it != lcid_map_.end()) ? &it->second : nullptr;
}

const LcidInfo* LocalizationManager::find_by_iso(const std::string& iso_code) const {
    auto it = iso_map_.find(iso_code);
    if (it != iso_map_.end()) {
        return find_lcid(it->second);
    }
    return nullptr;
}

bool LocalizationManager::load_pe_stringtable(const std::string& pe_path, bool is_expansion) {
    std::ifstream file(pe_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open PE file: %s", pe_path.c_str());
        return false;
    }

    size_t size = static_cast<size_t>(file.tellg());
    if (size < 0x200) return false;

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return load_pe_stringtable_memory(buffer.data(), buffer.size(), is_expansion);
}

bool LocalizationManager::load_pe_stringtable_memory(const uint8_t* data, size_t size, bool is_expansion) {
    if (!data || size < 0x200) return false;

    // DOS Header Check
    if (data[0] != 'M' || data[1] != 'Z') return false;

    uint32_t pe_offset = read_u32_le(data + 0x3C);
    if (pe_offset + 0x100 > size) return false;

    // PE Signature
    if (std::memcmp(data + pe_offset, "PE\0\0", 4) != 0) return false;

    uint16_t num_sections = read_u16_le(data + pe_offset + 6);
    uint16_t opt_hdr_size = read_u16_le(data + pe_offset + 20);
    size_t sec_table_offset = pe_offset + 24 + opt_hdr_size;

    if (sec_table_offset + num_sections * 40 > size) return false;

    // Find .rsrc section
    uint32_t rsrc_vaddr = 0;
    uint32_t rsrc_raw_ptr = 0;
    uint32_t rsrc_raw_size = 0;

    for (uint16_t i = 0; i < num_sections; ++i) {
        const uint8_t* sec = data + sec_table_offset + i * 40;
        char name[9] = {};
        std::memcpy(name, sec, 8);
        if (std::strcmp(name, ".rsrc") == 0) {
            rsrc_vaddr = read_u32_le(sec + 12);
            rsrc_raw_size = read_u32_le(sec + 16);
            rsrc_raw_ptr = read_u32_le(sec + 20);
            break;
        }
    }

    if (rsrc_raw_ptr == 0 || rsrc_raw_ptr + rsrc_raw_size > size) {
        LOGE("PE has no valid .rsrc section");
        return false;
    }

    auto rva_to_offset = [&](uint32_t rva) -> size_t {
        if (rva >= rsrc_vaddr && rva < rsrc_vaddr + rsrc_raw_size) {
            return rsrc_raw_ptr + (rva - rsrc_vaddr);
        }
        return 0;
    };

    // Parse Resource Directory Tree (Level 1: Types)
    if (rsrc_raw_ptr + 16 > size) return false;
    uint16_t num_named = read_u16_le(data + rsrc_raw_ptr + 12);
    uint16_t num_id = read_u16_le(data + rsrc_raw_ptr + 14);

    size_t new_strings_loaded = 0;

    for (uint16_t i = num_named; i < num_named + num_id; ++i) {
        size_t entry_offset = rsrc_raw_ptr + 16 + i * 8;
        if (entry_offset + 8 > size) break;

        uint32_t res_type = read_u32_le(data + entry_offset);
        uint32_t off_to_sub = read_u32_le(data + entry_offset + 4);

        if (res_type == 6) { // RT_STRING
            size_t sub_offset = rsrc_raw_ptr + (off_to_sub & 0x7FFFFFFF);
            if (sub_offset + 16 > size) break;

            uint16_t sub_named = read_u16_le(data + sub_offset + 12);
            uint16_t sub_id = read_u16_le(data + sub_offset + 14);

            // Level 2: String Block IDs (1..N)
            for (uint16_t j = 0; j < sub_named + sub_id; ++j) {
                size_t block_entry_off = sub_offset + 16 + j * 8;
                if (block_entry_off + 8 > size) break;

                uint32_t block_id = read_u32_le(data + block_entry_off);
                uint32_t off_to_lang = read_u32_le(data + block_entry_off + 4);

                size_t lang_offset = rsrc_raw_ptr + (off_to_lang & 0x7FFFFFFF);
                if (lang_offset + 24 > size) continue;

                // Level 3: Language leaf -> Data Entry
                uint32_t off_to_data = read_u32_le(data + lang_offset + 16 + 4);
                size_t data_entry_off = rsrc_raw_ptr + off_to_data;
                if (data_entry_off + 16 > size) continue;

                uint32_t data_rva = read_u32_le(data + data_entry_off);
                uint32_t data_size = read_u32_le(data + data_entry_off + 4);

                size_t raw_data_off = rva_to_offset(data_rva);
                if (raw_data_off == 0 || raw_data_off + data_size > size) continue;

                const uint8_t* block_bytes = data + raw_data_off;
                size_t cur = 0;
                uint32_t base_string_id = (block_id - 1) * 16;

                // Each block contains exactly 16 strings
                for (uint32_t s_idx = 0; s_idx < 16; ++s_idx) {
                    if (cur + 2 > data_size) break;
                    uint16_t s_len = read_u16_le(block_bytes + cur);
                    cur += 2;
                    if (s_len > 0) {
                        if (cur + s_len * 2 > data_size) break;
                        const uint16_t* u16_chars = reinterpret_cast<const uint16_t*>(block_bytes + cur);
                        std::string utf8_text = utf16le_to_utf8(u16_chars, s_len);
                        strings_[base_string_id + s_idx] = utf8_text;
                        ++new_strings_loaded;
                        cur += s_len * 2;
                    }
                }
            }
        }
    }

    if (is_expansion) {
        expansion_count_ += new_strings_loaded;
    } else {
        base_count_ += new_strings_loaded;
    }

    LOGI("Loaded %zu strings from PE stringtable (total: %zu)", new_strings_loaded, strings_.size());
    return new_strings_loaded > 0;
}

PeVersionMetadata LocalizationManager::extract_version_metadata(const uint8_t* data, size_t size) {
    PeVersionMetadata meta;
    if (!data || size < 0x200) return meta;

    // Signature: "VS_VERSION_INFO" in UTF-16LE
    static const uint8_t vs_magic[] = {
        'V',0,'S',0,'_',0,'V',0,'E',0,'R',0,'S',0,'I',0,'O',0,'N',0,'_',0,'I',0,'N',0,'F',0,'O',0
    };

    auto it = std::search(data, data + size, std::begin(vs_magic), std::end(vs_magic));
    if (it == data + size) return meta;

    size_t start_idx = it - data;

    auto read_field = [&](const std::string& key) -> std::string {
        std::vector<uint8_t> k_u16;
        k_u16.reserve(key.size() * 2);
        for (char c : key) {
            k_u16.push_back(static_cast<uint8_t>(c));
            k_u16.push_back(0);
        }

        auto k_it = std::search(data + start_idx, data + size, k_u16.begin(), k_u16.end());
        if (k_it == data + size) return "";

        size_t val_pos = (k_it - data) + k_u16.size();
        while (val_pos + 1 < size && data[val_pos] == 0 && data[val_pos + 1] == 0) {
            val_pos += 2;
        }

        std::vector<uint16_t> val_u16;
        while (val_pos + 1 < size) {
            uint16_t ch = read_u16_le(data + val_pos);
            if (ch == 0) break;
            val_u16.push_back(ch);
            val_pos += 2;
        }

        return utf16le_to_utf8(val_u16.data(), val_u16.size());
    };

    meta.file_description = read_field("FileDescription");
    meta.file_version = read_field("FileVersion");
    meta.internal_name = read_field("InternalName");
    meta.legal_copyright = read_field("LegalCopyright");
    meta.original_filename = read_field("OriginalFilename");
    meta.product_name = read_field("ProductName");
    meta.product_version = read_field("ProductVersion");
    meta.company_name = read_field("CompanyName");

    return meta;
}

PeVersionMetadata LocalizationManager::extract_version_metadata_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return extract_version_metadata(buf.data(), buf.size());
}

std::string LocalizationManager::get_string(uint32_t string_id, const std::string& fallback) const {
    auto it = strings_.find(string_id);
    if (it != strings_.end()) {
        return it->second;
    }
    return fallback;
}

bool LocalizationManager::has_string(uint32_t string_id) const {
    return strings_.find(string_id) != strings_.end();
}

size_t LocalizationManager::discover_voobly_mods(const std::string& voobly_dir) {
    patches_.clear();
    std::error_code ec;
    std::string data_mods = voobly_dir + "/AOMT/Data Mods";
    if (!std::filesystem::exists(data_mods, ec)) {
        return 0;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_mods, ec)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            CommunityPatchInfo patch;
            patch.name = name;
            patch.path = entry.path().string();
            if (name.find("Balance Patch") != std::string::npos) {
                patch.is_balance_patch = true;
                if (name.find("6.0") != std::string::npos) patch.version = "6.0";
                else if (name.find("4.0") != std::string::npos) patch.version = "4.0";
            } else {
                patch.is_balance_patch = false;
                patch.version = "1.0";
            }
            patches_.push_back(patch);
        }
    }

    return patches_.size();
}

void LocalizationManager::clear() {
    strings_.clear();
    base_count_ = 0;
    expansion_count_ = 0;
    patches_.clear();
}

} // namespace aom
