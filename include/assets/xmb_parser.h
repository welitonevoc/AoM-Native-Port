#pragma once

/// @file xmb_parser.h
/// @brief Parser for Age of Mythology XMB (binary XML) files
///
/// XMB format is a compact binary representation of XML used throughout AoM
/// for configuration, unit definitions, tech trees, UI layouts, etc.
///
/// The format encodes an XML tree with string tables for element names,
/// attribute names, and string values to minimize storage.
///
/// XMB Header:
///   [0-1]   u16  magic (0x4D58 = "XM" in LE, or BOM marker)
///   [2-3]   u16  version
///   [4-7]   u32  num_element_names
///   [8..]   String table: element names
///   Then:   u32  num_attribute_names  
///           String table: attribute names
///   Then:   Root element node (recursive)
///
/// Element Node:
///   [0-3]   u32  name_index (into element name table)
///   [4-7]   u32  num_attributes
///   Per attribute:
///     [0-3] u32  name_index (into attribute name table)
///     [4-7] u32  value_length
///     [8..] wchar_t[value_length] value (UTF-16LE)
///   [..]    u32  text_length
///   [..]    wchar_t[text_length] text content (UTF-16LE)
///   [..]    u32  num_children
///   Then:   child element nodes (recursive)

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace aom {

/// Represents an attribute on an XML element
struct XmbAttribute {
    std::string name;
    std::string value;  ///< UTF-8 converted from UTF-16LE
};

/// Represents an XML element node
struct XmbElement {
    std::string name;
    std::string text;   ///< Text content (UTF-8)
    std::vector<XmbAttribute> attributes;
    std::vector<std::unique_ptr<XmbElement>> children;

    /// Find first child element by name
    [[nodiscard]] const XmbElement* find_child(const std::string& name) const;

    /// Find all children with a given name
    [[nodiscard]] std::vector<const XmbElement*> find_children(
        const std::string& name) const;

    /// Get attribute value by name, or empty string if not found
    [[nodiscard]] std::string get_attribute(const std::string& name) const;

    /// Check if an attribute exists
    [[nodiscard]] bool has_attribute(const std::string& name) const;

    /// Get text content as various types
    [[nodiscard]] int text_as_int(int default_val = 0) const;
    [[nodiscard]] float text_as_float(float default_val = 0.0f) const;
    [[nodiscard]] bool text_as_bool(bool default_val = false) const;

    /// Recursively visit all elements
    void visit(const std::function<void(const XmbElement&, int depth)>& fn,
               int depth = 0) const;
};

/// XMB binary XML parser
class XmbParser {
public:
    /// Parse XMB data into an element tree
    /// @param data Raw XMB data
    /// @param size Data size
    /// @return Root element, or nullptr on error
    static std::unique_ptr<XmbElement> parse(const uint8_t* data, size_t size);

    /// Parse from vector
    static std::unique_ptr<XmbElement> parse(const std::vector<uint8_t>& data);

    /// Convert XMB element tree back to XML string (for debugging)
    static std::string to_xml(const XmbElement& root, int indent = 0);

private:
    struct ParseContext {
        const uint8_t* data;
        size_t size;
        size_t pos;
        std::vector<std::string> element_names;
        std::vector<std::string> attribute_names;
    };

    static bool read_string_table(ParseContext& ctx, std::vector<std::string>& table);
    static std::unique_ptr<XmbElement> read_element(ParseContext& ctx);
    static std::string read_utf16le_string(ParseContext& ctx, uint32_t char_count);

    template <typename T>
    static bool read_value(ParseContext& ctx, T& value);
};

} // namespace aom
