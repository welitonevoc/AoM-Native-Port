#pragma once

/// @file ddt_texture.h
/// @brief Parser for Age of Mythology DDT texture files
///
/// DDT (DirectDraw Texture) files are found inside textures.bar.
///
/// Header (16 bytes):
///   [0-3]   char[4] "RTS3"
///   [4]     u8      usage       (0 = texture, 1 = texture2, 3 = palette, 6 = bump, 8 = cube)
///   [5]     u8      alpha_bits  (0, 1, 4 or 8)
///   [6]     u8      format      (see DdtFormat)
///   [7]     u8      mip_count
///   [8-11]  u32     width
///   [12-15] u32     height
///
/// If format == 3 (palette), six additional u32 values follow the header:
///   num_colors, unknown, palette_offset_0, palette_offset_1,
///   palette_offset_alpha1, palette_offset_alpha4
///
/// Then a mip table of mip_count * (u32 offset, u32 size) follows.
/// Offsets are relative to the start of the DDT file.
///
/// Format 4  = DXT1 (4 bpp, 8-byte blocks)
/// Format 5  = 5 bpp AoM format (10-byte blocks: RGB555 c0/c1,
///             16-bit 1-bit alpha mask, 4 bytes of 2-bit indices)
/// Format 6  = 8 bpp AoM format (16-byte blocks: RGB444 c0/c1, then
///             per row 2 bytes of 4-bit alpha followed by 1 byte of indices)
/// Format 7  = 8-bit greyscale
/// Format 1  = uncompressed 32-bit BGRA

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace aom {

/// DDT pixel format types (byte 6 of the header)
enum class DdtFormat : uint8_t {
    UNKNOWN      = 0,
    BGRA8888     = 1,  ///< 32-bit BGRA, uncompressed
    PALETTE      = 3,  ///< 8-bit palette indices
    DXT1         = 4,  ///< BC1 compressed (no alpha or 1-bit alpha)
    AOM_5BPP     = 5,  ///< AoM custom 5 bpp format (RGB555 + 1-bit alpha)
    AOM_8BPP     = 6,  ///< AoM custom 8 bpp format (RGB444 + 4-bit alpha)
    GREYSCALE    = 7,  ///< 8-bit greyscale
};

/// Represents a single mipmap level
struct DdtMipmap {
    uint32_t width;
    uint32_t height;
    uint32_t offset;     ///< Offset of data relative to the start of the DDT
    uint32_t size;       ///< Size of compressed/raw data
    std::vector<uint8_t> data;
};

/// Parsed DDT texture
struct DdtTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    DdtFormat format = DdtFormat::UNKNOWN;
    uint32_t usage = 0;
    uint32_t alpha_bits = 0;
    uint32_t mipmap_count = 0;
    std::vector<DdtMipmap> mipmaps;
    std::vector<uint8_t> palette;  ///< RGBA palette table for PALETTE format (format 3)

    /// Convert the texture to RGBA8888 format (for use with OpenGL ES)
    /// @param mip_level Mipmap level to decode (0 = full res)
    /// @return RGBA pixel data, or empty on unsupported format
    [[nodiscard]] std::vector<uint8_t> decode_rgba(uint32_t mip_level = 0) const;

    /// Check if the texture uses a block-compressed format
    [[nodiscard]] bool is_compressed() const;
};

/// DDT texture loader
class DdtLoader {
public:
    /// Parse DDT texture data
    /// @param data Raw DDT data (starts with "RTS3")
    /// @param size Data size in bytes
    /// @return Parsed texture, or nullptr on error
    static std::unique_ptr<DdtTexture> load(const uint8_t* data, size_t size);

    /// Parse DDT from a vector
    static std::unique_ptr<DdtTexture> load(const std::vector<uint8_t>& data);
};

} // namespace aom
