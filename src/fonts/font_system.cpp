#include "fonts/font_system.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string utf16le_to_utf8(const uint8_t* data, size_t size) {
    std::string out;
    size_t count = size / 2;
    out.reserve(count);

    size_t i = 0;
    while (i < count) {
        uint32_t cp = data[i * 2] | (static_cast<uint32_t>(data[i * 2 + 1]) << 8);
        ++i;

        if (cp >= 0xD800 && cp <= 0xDBFF && i < count) {
            uint32_t low = data[i * 2] | (static_cast<uint32_t>(data[i * 2 + 1]) << 8);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + (((cp & 0x3FF) << 10) | (low & 0x3FF));
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
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::string extract_attr(const std::string& tag, const std::string& attr_name) {
    std::vector<std::string> prefixes = {
        attr_name + "=\"",
        attr_name + " = \"",
        attr_name + "='",
        attr_name + " = '"
    };

    for (const auto& prefix : prefixes) {
        size_t pos = tag.find(prefix);
        if (pos != std::string::npos) {
            pos += prefix.size();
            char quote = tag[pos - 1];
            size_t end_pos = tag.find(quote, pos);
            if (end_pos != std::string::npos) {
                return tag.substr(pos, end_pos - pos);
            }
        }
    }
    return "";
}

bool parse_bool(const std::string& str, bool default_val = false) {
    std::string s = to_lower(str);
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
    return default_val;
}

} // anonymous namespace

bool FontManager::load_msk_file(const std::string& typeface, const std::string& msk_path) {
    std::ifstream in(msk_path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (data.size() != GlyphMask::MASK_SIZE) {
        return false;
    }

    std::string key = to_lower(typeface);
    masks_[key] = GlyphMask(std::move(data));

    if (std::find(fallback_typefaces_.begin(), fallback_typefaces_.end(), typeface) == fallback_typefaces_.end()) {
        fallback_typefaces_.push_back(typeface);
    }
    return true;
}

bool FontManager::load_fonts_xml(const std::string& xml_path) {
    std::ifstream in(xml_path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    if (raw.empty()) {
        return false;
    }

    std::string content;
    // Check for UTF-16LE BOM
    if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
        content = utf16le_to_utf8(raw.data() + 2, raw.size() - 2);
    } else {
        content = std::string(reinterpret_cast<const char*>(raw.data()), raw.size());
    }

    return parse_fonts_xml(content);
}

bool FontManager::parse_fonts_xml(const std::string& xml) {
    size_t pos = 0;

    // Parse <Font Name="..."> elements (space after <Font to avoid matching <Fonts>)
    while ((pos = xml.find("<Font ", pos)) != std::string::npos) {
        size_t tag_end = xml.find('>', pos);
        if (tag_end == std::string::npos) break;

        std::string font_tag = xml.substr(pos, tag_end - pos + 1);
        FontDef font;
        font.name = extract_attr(font_tag, "Name");

        size_t font_close = xml.find("</Font>", tag_end);
        if (font_close == std::string::npos) {
            font_close = xml.size();
        }

        size_t res_override = xml.find("<ResolutionOverride", tag_end);
        size_t base_limit = (res_override != std::string::npos && res_override < font_close) ? res_override : font_close;

        // Parse child <param .../> tags for base font definition
        size_t param_pos = tag_end;
        while ((param_pos = xml.find("<param", param_pos)) != std::string::npos && param_pos < base_limit) {
            size_t param_end = xml.find('>', param_pos);
            if (param_end == std::string::npos || param_end > font_close) break;

            std::string ptag = xml.substr(param_pos, param_end - param_pos + 1);

            std::string tf = extract_attr(ptag, "TypeFace");
            if (!tf.empty()) font.typeface = tf;

            std::string h = extract_attr(ptag, "AscentHeight");
            if (!h.empty()) font.ascent_height = std::stoi(h);

            std::string aa = extract_attr(ptag, "Antialiased");
            if (!aa.empty()) font.antialiased = parse_bool(aa, true);

            std::string b = extract_attr(ptag, "Bold");
            if (!b.empty()) font.bold = parse_bool(b, false);

            std::string it = extract_attr(ptag, "Italic");
            if (!it.empty()) font.italic = parse_bool(it, false);

            std::string bo = extract_attr(ptag, "BackgroundOutline");
            if (!bo.empty()) font.outline = parse_bool(bo, false);

            std::string bs = extract_attr(ptag, "BackgroundShadow");
            if (!bs.empty()) font.shadow = parse_bool(bs, false);

            std::string boff = extract_attr(ptag, "BackgroundOffset");
            if (!boff.empty()) font.background_offset = std::stoi(boff);

            std::string ba = extract_attr(ptag, "BackgroundAlpha");
            if (!ba.empty()) font.background_alpha = std::stof(ba);

            std::string sa = extract_attr(ptag, "SpacingAdjustment");
            if (!sa.empty()) font.spacing_adjustment = std::stoi(sa);

            param_pos = param_end + 1;
        }

        if (!font.name.empty()) {
            fonts_[font.name] = font;
        }

        pos = font_close + 7;
    }

    return !fonts_.empty();
}

const FontDef* FontManager::find_font(const std::string& name) const {
    auto it = fonts_.find(name);
    if (it != fonts_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool FontManager::supports_glyph(const std::string& typeface, char32_t codepoint) const {
    auto it = masks_.find(to_lower(typeface));
    if (it != masks_.end()) {
        return it->second.supports(codepoint);
    }
    // If no mask is loaded for this font, assume supported for basic ASCII
    return (codepoint < 128);
}

std::string FontManager::resolve_typeface(const std::string& preferred, char32_t codepoint) const {
    if (supports_glyph(preferred, codepoint)) {
        return preferred;
    }

    // Fall back to any loaded font that supports this codepoint
    for (const auto& fb : fallback_typefaces_) {
        if (supports_glyph(fb, codepoint)) {
            return fb;
        }
    }

    return preferred;
}

} // namespace aom
