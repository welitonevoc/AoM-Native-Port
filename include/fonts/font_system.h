#pragma once

/// @file font_system.h
/// @brief Parser and runtime manager for Age of Mythology font definitions (fonts2.xml/xfonts.xml) and Unicode coverage bitmasks (.MSK)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace aom {

/// Represents a single font configuration instance from fonts2.xml / xfonts.xml
struct FontDef {
    std::string name;
    std::string typeface;
    int ascent_height = 12;
    bool bold = false;
    bool italic = false;
    bool antialiased = true;
    bool shadow = false;
    bool outline = false;
    int background_offset = 1;
    float background_alpha = 0.5f;
    int spacing_adjustment = 0;
};

/// Represents an 8192-byte (65536-bit) Unicode Basic Multilingual Plane (BMP) coverage bitmask (.MSK)
class GlyphMask {
public:
    static constexpr size_t MASK_SIZE = 8192;
    static constexpr size_t TOTAL_CODEPOINTS = MASK_SIZE * 8; // 65536 codepoints

    GlyphMask() = default;
    explicit GlyphMask(std::vector<uint8_t> data) : data_(std::move(data)) {}

    /// Check if Unicode codepoint (0..65535) is supported by this font
    [[nodiscard]] bool supports(char32_t codepoint) const {
        if (codepoint >= TOTAL_CODEPOINTS || data_.size() < MASK_SIZE) {
            return false;
        }
        size_t byte_idx = codepoint / 8;
        size_t bit_idx = codepoint % 8;
        return (data_[byte_idx] >> bit_idx) & 1;
    }

    [[nodiscard]] bool is_valid() const { return data_.size() == MASK_SIZE; }

private:
    std::vector<uint8_t> data_;
};

/// Central Font Manager handling fonts2.xml, xfonts.xml, .MSK bitmasks and typeface fallback
class FontManager {
public:
    FontManager() = default;
    ~FontManager() = default;

    /// Load an .msk bitmask file for a typeface
    bool load_msk_file(const std::string& typeface, const std::string& msk_path);

    /// Load and parse fonts2.xml or xfonts.xml from disk
    bool load_fonts_xml(const std::string& xml_path);

    /// Parse raw UTF-16 or UTF-8 XML content defining fonts
    bool parse_fonts_xml(const std::string& content);

    /// Find font configuration by exact name (e.g. "Arial 10", "Arial Outlined 12")
    [[nodiscard]] const FontDef* find_font(const std::string& name) const;

    /// Check if a specific typeface supports a Unicode glyph
    [[nodiscard]] bool supports_glyph(const std::string& typeface, char32_t codepoint) const;

    /// Resolves best typeface for a character, falling back to CJK/other fonts if needed
    [[nodiscard]] std::string resolve_typeface(const std::string& preferred_typeface,
                                               char32_t codepoint) const;

    [[nodiscard]] size_t font_count() const { return fonts_.size(); }
    [[nodiscard]] size_t msk_count() const { return masks_.size(); }

    [[nodiscard]] const std::unordered_map<std::string, FontDef>& all_fonts() const {
        return fonts_;
    }

private:
    std::unordered_map<std::string, FontDef> fonts_;
    std::unordered_map<std::string, GlyphMask> masks_;
    std::vector<std::string> fallback_typefaces_;
};

} // namespace aom
