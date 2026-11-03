#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace aom {

/// Represents a Windows Locale ID (LCID) language definition
struct LcidInfo {
    uint32_t lcid;
    std::string iso_code;       // e.g. "pt-BR", "en-US", "de-DE"
    std::string language_name;  // e.g. "Portuguese", "English"
    std::string region_name;    // e.g. "Brazil", "United States"
    bool has_bootstrapper;      // Whether it includes msiloadr.bin & webloadr.bin
};

/// Represents PE Version Info metadata extracted from Win32 resource DLLs
struct PeVersionMetadata {
    std::string file_description;
    std::string file_version;
    std::string internal_name;
    std::string legal_copyright;
    std::string original_filename;
    std::string product_name;
    std::string product_version;
    std::string company_name;
};

/// Community balance patch information found in Voobly Mods/
struct CommunityPatchInfo {
    std::string name;
    std::string version;
    std::string path;
    bool is_balance_patch;
};

/// Manages game localization, PE string table extraction, and LCID mappings
class LocalizationManager {
public:
    LocalizationManager();
    ~LocalizationManager();

    // Singleton / shared instance access
    static LocalizationManager& instance();

    /// Initialize the standard Windows LCID registry (30 languages)
    void init_lcid_registry();

    /// Retrieve LCID info by numerical identifier
    const LcidInfo* find_lcid(uint32_t lcid) const;

    /// Retrieve LCID info by ISO language code (e.g. "pt-BR", "en-US")
    const LcidInfo* find_by_iso(const std::string& iso_code) const;

    /// Return all registered LCIDs
    const std::vector<LcidInfo>& get_all_lcids() const { return lcids_; }

    /// Parse a Win32 PE file and extract all stringtable resources (RT_STRING)
    bool load_pe_stringtable(const std::string& pe_path, bool is_expansion = false);

    /// Parse PE stringtable directly from a memory buffer
    bool load_pe_stringtable_memory(const uint8_t* data, size_t size, bool is_expansion = false);

    /// Extract VS_VERSION_INFO metadata from a PE binary
    static PeVersionMetadata extract_version_metadata(const uint8_t* data, size_t size);
    static PeVersionMetadata extract_version_metadata_file(const std::string& path);

    /// Get localized string by ID (e.g. 19771 -> "Alimento")
    std::string get_string(uint32_t string_id, const std::string& fallback = "") const;

    /// Check if a string exists
    bool has_string(uint32_t string_id) const;

    /// Total count of loaded localized strings
    size_t string_count() const { return strings_.size(); }

    /// Total count of base game strings
    size_t base_string_count() const { return base_count_; }

    /// Total count of expansion strings
    size_t expansion_string_count() const { return expansion_count_; }

    /// Discover and register Voobly community mods and balance patches
    size_t discover_voobly_mods(const std::string& voobly_dir);
    const std::vector<CommunityPatchInfo>& get_community_patches() const { return patches_; }

    /// Clear all loaded strings
    void clear();

private:
    std::vector<LcidInfo> lcids_;
    std::unordered_map<uint32_t, LcidInfo> lcid_map_;
    std::unordered_map<std::string, uint32_t> iso_map_;

    std::unordered_map<uint32_t, std::string> strings_;
    size_t base_count_ = 0;
    size_t expansion_count_ = 0;

    std::vector<CommunityPatchInfo> patches_;
};

} // namespace aom
