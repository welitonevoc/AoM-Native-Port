#pragma once

/// @file gfx_config.h
/// @brief Parser and runtime manager for Age of Mythology graphics configurations (.GFX), vendor device tables, and texture stage states (.TSS)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace aom {

/// Represents a GPU device entry parsed from vendor files (e.g. 0x10de_nvidia.gfx)
struct GfxDeviceInfo {
    std::string vendor_id;
    std::string device_id;
    std::string device_name;
    std::string profile_file;
};

/// Represents graphics rendering profile and feature capabilities (.GFX)
struct GfxProfile {
    std::string name;
    std::string quality_level = "high";
    int anisotropy = 1;
    float font_uv_offset = 0.0f;
    bool win32_cursor = true;
    std::vector<std::string> resolutions;
    std::unordered_map<std::string, std::string> features;
    std::unordered_map<std::string, std::string> render_states;
};

/// Represents one stage/pass in a texture blending pipeline (.TSS)
struct TextureStagePass {
    std::unordered_map<std::string, std::string> operations;
};

/// Represents a multi-texture rendering pipeline definition (.TSS)
struct TextureStagePipeline {
    std::string name;
    int passes = 1;
    std::vector<TextureStagePass> pass_list;
};

/// Central Graphics Configuration Manager for device capability matching and GLES3 configuration
class GfxConfigManager {
public:
    GfxConfigManager() = default;
    ~GfxConfigManager() = default;

    /// Parse a vendor device mapping file (e.g. 0x10de_nvidia.gfx, 0x1002_ati.gfx)
    bool load_vendor_file(const std::string& path);

    /// Parse a graphics card profile or hardware settings file (e.g. geforce3.gfx, hardware.gfx)
    bool load_profile_file(const std::string& path);

    /// Parse a texture stage state pipeline file (e.g. mtlinearblend.tss, mtlightmap.tss)
    bool load_tss_file(const std::string& path);

    /// Find device entry by Vendor ID and Device ID (e.g. "0x10DE", "0x0200")
    [[nodiscard]] const GfxDeviceInfo* find_device(const std::string& vendor_id,
                                                   const std::string& device_id) const;

    /// Find profile by name (e.g. "geforce3.gfx", "hardware.gfx")
    [[nodiscard]] const GfxProfile* find_profile(const std::string& name) const;

    /// Find TSS pipeline by name (e.g. "mtlinearblend.tss")
    [[nodiscard]] const TextureStagePipeline* find_pipeline(const std::string& name) const;

    /// Automatically configure graphics profile based on Android OpenGL ES 3.0 renderer string
    void configure_mobile_gles3(const std::string& gl_renderer);

    /// Currently active graphics profile
    [[nodiscard]] const GfxProfile* active_profile() const;

    [[nodiscard]] size_t vendor_device_count() const { return devices_.size(); }
    [[nodiscard]] size_t profile_count() const { return profiles_.size(); }
    [[nodiscard]] size_t pipeline_count() const { return pipelines_.size(); }

private:
    std::unordered_map<std::string, GfxDeviceInfo> devices_; // key: "vendor:device"
    std::unordered_map<std::string, GfxProfile> profiles_;   // key: filename
    std::unordered_map<std::string, TextureStagePipeline> pipelines_; // key: filename
    std::string active_profile_name_;
};

} // namespace aom
