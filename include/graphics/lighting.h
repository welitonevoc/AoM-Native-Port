#pragma once

/// @file lighting.h
/// @brief Parser and runtime manager for Age of Mythology .LGT lighting set files

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

namespace aom {

/// Represents an RGB color configuration from an LGT file
struct LightColor {
    float r = 255.0f;
    float g = 255.0f;
    float b = 255.0f;

    /// Returns normalized color vector in range [0.0, 1.0]
    [[nodiscard]] glm::vec3 normalized() const {
        return glm::vec3(
            glm::clamp(r / 255.0f, 0.0f, 1.0f),
            glm::clamp(g / 255.0f, 0.0f, 1.0f),
            glm::clamp(b / 255.0f, 0.0f, 1.0f)
        );
    }
};

/// Represents a parsed Age of Mythology LightingSet
struct LightingSet {
    std::string name;
    LightColor sun_color;
    LightColor sun_specular;
    LightColor ambient_color;
    LightColor terrain_ambient_color;
    float sun_inclination = 45.0f; ///< Elevation angle in degrees
    float sun_rotation = 45.0f;    ///< Azimuth angle in degrees

    /// Calculates directional sun vector pointing towards scene surfaces
    [[nodiscard]] glm::vec3 calculate_sun_direction() const;

    [[nodiscard]] glm::vec3 get_sun_color_normalized() const { return sun_color.normalized(); }
    [[nodiscard]] glm::vec3 get_sun_specular_normalized() const { return sun_specular.normalized(); }
    [[nodiscard]] glm::vec3 get_ambient_normalized() const { return ambient_color.normalized(); }
    [[nodiscard]] glm::vec3 get_terrain_ambient_normalized() const { return terrain_ambient_color.normalized(); }
};

/// Manager for lighting sets in Age of Mythology
class LightingManager {
public:
    LightingManager() = default;
    ~LightingManager() = default;

    /// Load an .lgt file from disk
    bool load_file(const std::string& path);

    /// Parse raw .lgt text content
    bool parse_lgt_text(const std::string& text, const std::string& fallback_name = "");

    /// Get a lighting set by name (case-insensitive)
    [[nodiscard]] const LightingSet* get_set(const std::string& name) const;

    /// Get the currently active lighting set (defaults to "Default")
    [[nodiscard]] const LightingSet* active_set() const;

    /// Set the active lighting set by name
    bool set_active(const std::string& name);

    /// Total number of loaded lighting sets
    [[nodiscard]] size_t count() const { return sets_.size(); }

    /// Access all loaded sets
    [[nodiscard]] const std::unordered_map<std::string, LightingSet>& all_sets() const {
        return sets_;
    }

private:
    std::unordered_map<std::string, LightingSet> sets_;
    std::string active_set_name_;
};

} // namespace aom
