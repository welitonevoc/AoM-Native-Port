#include "assets/ddt_texture.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace aom {

namespace {

uint16_t read_u16(const uint8_t* ptr) {
    return static_cast<uint16_t>(ptr[0]) |
           (static_cast<uint16_t>(ptr[1]) << 8);
}

uint32_t read_u32(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0]) |
           (static_cast<uint32_t>(ptr[1]) << 8) |
           (static_cast<uint32_t>(ptr[2]) << 16) |
           (static_cast<uint32_t>(ptr[3]) << 24);
}

void expand_565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
    g = static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
    b = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
}

void expand_555(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((c >> 10) & 0x1F) * 255 / 31);
    g = static_cast<uint8_t>(((c >> 5) & 0x1F) * 255 / 31);
    b = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
}

void expand_444(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((c >> 8) & 0x0F) * 17);
    g = static_cast<uint8_t>(((c >> 4) & 0x0F) * 17);
    b = static_cast<uint8_t>((c & 0x0F) * 17);
}

struct Rgba {
    uint8_t r, g, b, a;
};

void store_pixel(std::vector<uint8_t>& out, uint32_t width, uint32_t height,
                 uint32_t x, uint32_t y, const Rgba& c) {
    if (x >= width || y >= height) return;
    const size_t p = (static_cast<size_t>(y) * width + x) * 4;
    out[p + 0] = c.r;
    out[p + 1] = c.g;
    out[p + 2] = c.b;
    out[p + 3] = c.a;
}

Rgba lerp_color(const Rgba& a, const Rgba& b, int num, int den) {
    return Rgba{
        static_cast<uint8_t>((a.r * (den - num) + b.r * num) / den),
        static_cast<uint8_t>((a.g * (den - num) + b.g * num) / den),
        static_cast<uint8_t>((a.b * (den - num) + b.b * num) / den),
        255,
    };
}

void decode_dxt1_block(const uint8_t* block, std::vector<uint8_t>& out,
                       uint32_t width, uint32_t height,
                       uint32_t bx, uint32_t by) {
    const uint16_t c0 = read_u16(block + 0);
    const uint16_t c1 = read_u16(block + 2);
    const uint32_t bits = read_u32(block + 4);

    Rgba pal[4];
    expand_565(c0, pal[0].r, pal[0].g, pal[0].b);
    expand_565(c1, pal[1].r, pal[1].g, pal[1].b);
    pal[0].a = pal[1].a = 255;

    if (c0 > c1) {
        pal[2] = lerp_color(pal[0], pal[1], 1, 3);
        pal[3] = lerp_color(pal[0], pal[1], 2, 3);
    } else {
        pal[2] = lerp_color(pal[0], pal[1], 1, 2);
        pal[2].a = 255;
        pal[3] = Rgba{0, 0, 0, 0};
    }

    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            const uint32_t code = (bits >> (2 * (4 * y + x))) & 0x03;
            store_pixel(out, width, height, bx * 4 + x, by * 4 + y, pal[code]);
        }
    }
}

/// AoM 5 bpp: RGB555 color0/color1, 16-bit 1-bit alpha mask, 4 index bytes.
void decode_aom_5bpp_block(const uint8_t* block, std::vector<uint8_t>& out,
                           uint32_t width, uint32_t height,
                           uint32_t bx, uint32_t by) {
    const uint16_t c0 = read_u16(block + 0);
    const uint16_t c1 = read_u16(block + 2);
    const uint16_t alpha_mask = read_u16(block + 4);

    Rgba pal[4];
    expand_555(c0, pal[0].r, pal[0].g, pal[0].b);
    expand_555(c1, pal[1].r, pal[1].g, pal[1].b);
    pal[0].a = pal[1].a = 255;
    pal[2] = lerp_color(pal[0], pal[1], 1, 3);
    pal[3] = lerp_color(pal[0], pal[1], 2, 3);

    for (uint32_t y = 0; y < 4; ++y) {
        const uint8_t row_indices = block[6 + y];
        for (uint32_t x = 0; x < 4; ++x) {
            const uint32_t code = (row_indices >> (2 * x)) & 0x03;
            Rgba c = pal[code];
            c.a = ((alpha_mask >> (4 * y + x)) & 1) ? 255 : 0;
            store_pixel(out, width, height, bx * 4 + x, by * 4 + y, c);
        }
    }
}

/// AoM 8 bpp: RGB444 color0/color1; per row: 2 bytes 4-bit alpha, 1 byte indices.
void decode_aom_8bpp_block(const uint8_t* block, std::vector<uint8_t>& out,
                           uint32_t width, uint32_t height,
                           uint32_t bx, uint32_t by) {
    const uint16_t c0 = read_u16(block + 0);
    const uint16_t c1 = read_u16(block + 2);

    Rgba pal[4];
    expand_444(c0, pal[0].r, pal[0].g, pal[0].b);
    expand_444(c1, pal[1].r, pal[1].g, pal[1].b);
    pal[0].a = pal[1].a = 255;
    pal[2] = lerp_color(pal[0], pal[1], 1, 3);
    pal[3] = lerp_color(pal[0], pal[1], 2, 3);

    for (uint32_t y = 0; y < 4; ++y) {
        const uint8_t* alpha = block + 4 + y * 3;
        const uint16_t alpha_bits = read_u16(alpha);
        const uint8_t row_indices = block[4 + y * 3 + 2];
        for (uint32_t x = 0; x < 4; ++x) {
            const uint32_t code = (row_indices >> (2 * x)) & 0x03;
            Rgba c = pal[code];
            c.a = static_cast<uint8_t>(((alpha_bits >> (4 * x)) & 0x0F) * 17);
            store_pixel(out, width, height, bx * 4 + x, by * 4 + y, c);
        }
    }
}

uint32_t block_size_for(DdtFormat format) {
    switch (format) {
        case DdtFormat::DXT1:     return 8;
        case DdtFormat::AOM_5BPP: return 10;
        case DdtFormat::AOM_8BPP: return 16;
        default:                  return 0;
    }
}

} // anonymous namespace

std::unique_ptr<DdtTexture> DdtLoader::load(const uint8_t* data, size_t size) {
    if (!data || size < 16 || std::memcmp(data, "RTS3", 4) != 0) {
        return nullptr;
    }

    auto tex = std::make_unique<DdtTexture>();
    tex->usage = data[4];
    tex->alpha_bits = data[5];
    tex->format = static_cast<DdtFormat>(data[6]);
    tex->mipmap_count = data[7];
    tex->width = read_u32(data + 8);
    tex->height = read_u32(data + 12);

    if (tex->width == 0 || tex->height == 0 ||
        tex->mipmap_count == 0 || tex->mipmap_count > 32) {
        return nullptr;
    }

    size_t mip_table_offset = 16;
    if (tex->format == DdtFormat::PALETTE) {
        if (size < 40) return nullptr;
        const uint32_t num_colors = read_u32(data + 16);
        uint32_t pal_offset = read_u32(data + 24);
        if (pal_offset == 0) {
            pal_offset = read_u32(data + 32);
        }
        if (pal_offset == 0) {
            pal_offset = 48;
        }
        if (pal_offset + num_colors * 2 <= size) {
            tex->palette.assign(data + pal_offset, data + pal_offset + num_colors * 2);
        }
        mip_table_offset += 24; // palette metadata
    }

    const size_t mip_table_size =
        static_cast<size_t>(tex->mipmap_count) * 8;
    if (mip_table_offset + mip_table_size > size) {
        return nullptr;
    }

    const uint32_t block_size = block_size_for(tex->format);

    tex->mipmaps.reserve(tex->mipmap_count);
    for (uint32_t i = 0; i < tex->mipmap_count; ++i) {
        const uint8_t* entry = data + mip_table_offset + i * 8;
        DdtMipmap mip;
        mip.offset = read_u32(entry + 0);
        mip.size = read_u32(entry + 4);
        mip.width = std::max<uint32_t>(1, tex->width >> i);
        mip.height = std::max<uint32_t>(1, tex->height >> i);

        if (mip.offset > size || mip.size > size - mip.offset) {
            return nullptr;
        }

        if (block_size != 0) {
            const uint64_t blocks =
                static_cast<uint64_t>((mip.width + 3) / 4) *
                static_cast<uint64_t>((mip.height + 3) / 4);
            const uint64_t required = blocks * block_size;
            if (mip.size < required) {
                return nullptr;
            }
        }

        mip.data.assign(data + mip.offset, data + mip.offset + mip.size);
        tex->mipmaps.push_back(std::move(mip));
    }

    return tex;
}

std::unique_ptr<DdtTexture> DdtLoader::load(const std::vector<uint8_t>& data) {
    return load(data.data(), data.size());
}

std::vector<uint8_t> DdtTexture::decode_rgba(uint32_t mip_level) const {
    if (mip_level >= mipmaps.size()) {
        return {};
    }

    const DdtMipmap& mip = mipmaps[mip_level];
    const size_t pixel_count = static_cast<size_t>(mip.width) * mip.height;
    std::vector<uint8_t> output(pixel_count * 4);

    switch (format) {
        case DdtFormat::BGRA8888: {
            if (mip.data.size() < output.size()) return {};
            for (size_t i = 0; i < pixel_count; ++i) {
                const uint8_t* src = mip.data.data() + i * 4;
                uint8_t* dst = output.data() + i * 4;
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
            }
            return output;
        }

        case DdtFormat::GREYSCALE: {
            if (mip.data.size() < pixel_count) return {};
            for (size_t i = 0; i < pixel_count; ++i) {
                const uint8_t g = mip.data[i];
                output[i * 4 + 0] = g;
                output[i * 4 + 1] = g;
                output[i * 4 + 2] = g;
                output[i * 4 + 3] = 255;
            }
            return output;
        }

        case DdtFormat::PALETTE: {
            if (palette.size() < 2 || mip.data.size() < pixel_count) {
                return {};
            }
            const size_t num_colors = palette.size() / 2;
            for (size_t i = 0; i < pixel_count; ++i) {
                size_t idx = mip.data[i];
                if (idx >= num_colors) idx = 0;
                const uint16_t val = read_u16(palette.data() + idx * 2);
                uint8_t* dst = output.data() + i * 4;
                expand_555(val, dst[0], dst[1], dst[2]);
                dst[3] = (alpha_bits > 0 && idx == 0 && val == 0) ? 0 : 255;
            }
            return output;
        }

        case DdtFormat::DXT1:
        case DdtFormat::AOM_5BPP:
        case DdtFormat::AOM_8BPP: {
            const uint32_t bs = block_size_for(format);
            const uint32_t blocks_x = (mip.width + 3) / 4;
            const uint32_t blocks_y = (mip.height + 3) / 4;
            const uint8_t* in = mip.data.data();

            for (uint32_t by = 0; by < blocks_y; ++by) {
                for (uint32_t bx = 0; bx < blocks_x; ++bx) {
                    switch (format) {
                        case DdtFormat::DXT1:
                            decode_dxt1_block(in, output, mip.width, mip.height, bx, by);
                            break;
                        case DdtFormat::AOM_5BPP:
                            decode_aom_5bpp_block(in, output, mip.width, mip.height, bx, by);
                            break;
                        case DdtFormat::AOM_8BPP:
                            decode_aom_8bpp_block(in, output, mip.width, mip.height, bx, by);
                            break;
                        default:
                            break;
                    }
                    in += bs;
                }
            }
            return output;
        }

        default:
            return {};
    }
}

bool DdtTexture::is_compressed() const {
    return format == DdtFormat::DXT1 ||
           format == DdtFormat::AOM_5BPP ||
           format == DdtFormat::AOM_8BPP;
}

} // namespace aom
