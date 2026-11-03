#include "assets/xmb_parser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <zlib.h>

namespace aom {

namespace {

// Safe UTF-16LE to UTF-8 converter
std::string utf16le_to_utf8(const uint8_t* data, size_t char_count) {
    std::string out;
    out.reserve(char_count);

    size_t i = 0;
    while (i < char_count) {
        uint32_t cp = data[i * 2] | (static_cast<uint32_t>(data[i * 2 + 1]) << 8);
        ++i;

        // Handle surrogate pairs
        if (cp >= 0xD800 && cp <= 0xDBFF && i < char_count) {
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

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// XmbElement Implementation
// ---------------------------------------------------------------------------

const XmbElement* XmbElement::find_child(const std::string& target_name) const {
    for (const auto& child : children) {
        if (child && child->name == target_name) {
            return child.get();
        }
    }
    return nullptr;
}

std::vector<const XmbElement*> XmbElement::find_children(const std::string& target_name) const {
    std::vector<const XmbElement*> result;
    for (const auto& child : children) {
        if (child && child->name == target_name) {
            result.push_back(child.get());
        }
    }
    return result;
}

std::string XmbElement::get_attribute(const std::string& target_name) const {
    for (const auto& attr : attributes) {
        if (attr.name == target_name) {
            return attr.value;
        }
    }
    return "";
}

bool XmbElement::has_attribute(const std::string& target_name) const {
    for (const auto& attr : attributes) {
        if (attr.name == target_name) {
            return true;
        }
    }
    return false;
}

int XmbElement::text_as_int(int default_val) const {
    std::string s = trim(text);
    if (s.empty()) return default_val;
    try {
        return std::stoi(s);
    } catch (...) {
        return default_val;
    }
}

float XmbElement::text_as_float(float default_val) const {
    std::string s = trim(text);
    if (s.empty()) return default_val;
    try {
        return std::stof(s);
    } catch (...) {
        return default_val;
    }
}

bool XmbElement::text_as_bool(bool default_val) const {
    std::string s = trim(text);
    if (s.empty()) return default_val;
    std::string lower;
    lower.reserve(s.size());
    for (char c : s) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower == "true" || lower == "1" || lower == "yes") return true;
    if (lower == "false" || lower == "0" || lower == "no") return false;
    return default_val;
}

void XmbElement::visit(const std::function<void(const XmbElement&, int depth)>& fn,
                       int depth) const {
    fn(*this, depth);
    for (const auto& child : children) {
        if (child) {
            child->visit(fn, depth + 1);
        }
    }
}

// ---------------------------------------------------------------------------
// XmbParser Implementation
// ---------------------------------------------------------------------------

template <typename T>
bool XmbParser::read_value(ParseContext& ctx, T& value) {
    if (ctx.pos + sizeof(T) > ctx.size) {
        return false;
    }
    std::memcpy(&value, ctx.data + ctx.pos, sizeof(T));
    ctx.pos += sizeof(T);
    return true;
}

std::string XmbParser::read_utf16le_string(ParseContext& ctx, uint32_t char_count) {
    const size_t byte_count = static_cast<size_t>(char_count) * 2;
    if (ctx.pos + byte_count > ctx.size) {
        ctx.pos = ctx.size; // mark EOF
        return "";
    }
    std::string s = utf16le_to_utf8(ctx.data + ctx.pos, char_count);
    ctx.pos += byte_count;
    return s;
}

bool XmbParser::read_string_table(ParseContext& ctx, std::vector<std::string>& table) {
    uint32_t count = 0;
    if (!read_value(ctx, count)) {
        return false;
    }
    table.clear();
    table.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t char_count = 0;
        if (!read_value(ctx, char_count)) {
            return false;
        }
        table.push_back(read_utf16le_string(ctx, char_count));
    }
    return true;
}

std::unique_ptr<XmbElement> XmbParser::read_element(ParseContext& ctx) {
    // Each node begins with 'XN' (0x4E58)
    uint16_t magic = 0;
    if (!read_value(ctx, magic) || magic != 0x4E58) {
        return nullptr;
    }

    uint32_t node_len = 0;
    if (!read_value(ctx, node_len)) {
        return nullptr;
    }

    uint32_t text_len = 0;
    if (!read_value(ctx, text_len)) {
        return nullptr;
    }

    auto element = std::make_unique<XmbElement>();

    if (text_len > 0) {
        element->text = read_utf16le_string(ctx, text_len);
    }

    uint32_t name_id = 0;
    uint32_t num_attributes = 0;
    if (!read_value(ctx, name_id) || !read_value(ctx, num_attributes)) {
        return nullptr;
    }

    if (name_id < ctx.element_names.size()) {
        element->name = ctx.element_names[name_id];
    } else {
        element->name = "elem_" + std::to_string(name_id);
    }

    element->attributes.reserve(num_attributes);
    for (uint32_t i = 0; i < num_attributes; ++i) {
        uint32_t attr_name_id = 0;
        uint32_t attr_val_len = 0;
        if (!read_value(ctx, attr_name_id) || !read_value(ctx, attr_val_len)) {
            return nullptr;
        }

        XmbAttribute attr;
        if (attr_name_id < ctx.attribute_names.size()) {
            attr.name = ctx.attribute_names[attr_name_id];
        } else {
            attr.name = "attr_" + std::to_string(attr_name_id);
        }

        if (attr_val_len > 0) {
            attr.value = read_utf16le_string(ctx, attr_val_len);
        }
        element->attributes.push_back(std::move(attr));
    }

    uint32_t num_children = 0;
    if (!read_value(ctx, num_children)) {
        return nullptr;
    }

    element->children.reserve(num_children);
    for (uint32_t i = 0; i < num_children; ++i) {
        auto child = read_element(ctx);
        if (!child) {
            return nullptr;
        }
        element->children.push_back(std::move(child));
    }

    return element;
}

std::unique_ptr<XmbElement> XmbParser::parse(const uint8_t* data, size_t size) {
    if (!data || size < 20) {
        return nullptr;
    }

    // Check if data is l33t compressed (4 bytes magic + 4 bytes decompressed size + zlib)
    if (size >= 8 && std::memcmp(data, "l33t", 4) == 0) {
        uint32_t decomp_size = 0;
        std::memcpy(&decomp_size, data + 4, 4);
        if (decomp_size == 0 || decomp_size > 64 * 1024 * 1024) {
            return nullptr;
        }

        std::vector<uint8_t> decomp(decomp_size);
        z_stream strm{};
        strm.next_in = const_cast<Bytef*>(data + 8);
        strm.avail_in = static_cast<uInt>(size - 8);
        strm.next_out = decomp.data();
        strm.avail_out = decomp_size;

        if (inflateInit(&strm) != Z_OK) {
            return nullptr;
        }

        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (ret != Z_STREAM_END && ret != Z_OK) {
            return nullptr;
        }

        return parse(decomp.data(), decomp_size);
    }

    ParseContext ctx{data, size, 0, {}, {}};

    // File header: 'X1' (0x3158) + uint32 total_length
    uint16_t header_magic = 0;
    uint32_t file_length = 0;
    if (!read_value(ctx, header_magic) || header_magic != 0x3158) {
        return nullptr;
    }
    if (!read_value(ctx, file_length)) {
        return nullptr;
    }

    // String tables header: 'XR' (0x5258) + uint32 v1 + uint32 v2
    uint16_t xr_magic = 0;
    uint32_t v1 = 0, v2 = 0;
    if (!read_value(ctx, xr_magic) || xr_magic != 0x5258) {
        return nullptr;
    }
    if (!read_value(ctx, v1) || !read_value(ctx, v2)) {
        return nullptr;
    }

    // Read element name table
    if (!read_string_table(ctx, ctx.element_names)) {
        return nullptr;
    }

    // Read attribute name table
    if (!read_string_table(ctx, ctx.attribute_names)) {
        return nullptr;
    }

    // Parse root node
    return read_element(ctx);
}

std::unique_ptr<XmbElement> XmbParser::parse(const std::vector<uint8_t>& data) {
    return parse(data.data(), data.size());
}

std::string XmbParser::to_xml(const XmbElement& root, int indent) {
    std::string ind(indent * 2, ' ');
    std::ostringstream ss;

    ss << ind << "<" << root.name;
    for (const auto& attr : root.attributes) {
        ss << " " << attr.name << "=\"" << attr.value << "\"";
    }

    if (root.children.empty() && root.text.empty()) {
        ss << " />\n";
    } else if (root.children.empty()) {
        ss << ">" << root.text << "</" << root.name << ">\n";
    } else {
        ss << ">\n";
        if (!root.text.empty()) {
            ss << ind << "  " << root.text << "\n";
        }
        for (const auto& child : root.children) {
            if (child) {
                ss << to_xml(*child, indent + 1);
            }
        }
        ss << ind << "</" << root.name << ">\n";
    }

    return ss.str();
}

} // namespace aom
