#pragma once
// Seleção automática do layout dos blocos de 16 bytes (DDT formato 6 / AOM_8BPP).
// Candidatos: DXT3 / DXT5 padrão (alpha primeiro) e "DXT1 + alpha 4 bits" (cor primeiro),
// em RGB565/555.

#include "assets/ddt5_auto.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ddt6 {

enum Mode : uint8_t { DXT3 = 0, DXT5 = 1, A4_THEN_C = 2, C_THEN_A4 = 3 };
struct Layout { Mode mode; bool c565; };

inline void decode_block(const uint8_t* blk, const Layout& L, ddt5::Px out[16]) {
    const bool alpha_first = (L.mode != C_THEN_A4);
    const uint8_t* cb = alpha_first ? blk + 8 : blk;
    const uint8_t* ab = alpha_first ? blk : blk + 8;
    ddt5::Px pal[4];
    const uint16_t c0 = ddt5::u16(cb);
    const uint16_t c1 = ddt5::u16(cb + 2);
    pal[0] = ddt5::expand(c0, L.c565);
    pal[1] = ddt5::expand(c1, L.c565);
    const bool four = c0 > c1;
    for (int k = 0; k < 3; ++k) {
        const int a = k == 0 ? pal[0].r : k == 1 ? pal[0].g : pal[0].b;
        const int b = k == 0 ? pal[1].r : k == 1 ? pal[1].g : pal[1].b;
        uint8_t* d2 = k == 0 ? &pal[2].r : k == 1 ? &pal[2].g : &pal[2].b;
        uint8_t* d3 = k == 0 ? &pal[3].r : k == 1 ? &pal[3].g : &pal[3].b;
        *d2 = four ? static_cast<uint8_t>((2 * a + b) / 3)
                   : static_cast<uint8_t>((a + b) / 2);
        *d3 = four ? static_cast<uint8_t>((a + 2 * b) / 3) : 0;
    }
    pal[2].a = 255;
    pal[3].a = 255;
    uint8_t al[8];
    if (L.mode == DXT5) {
        const int a0 = ab[0];
        const int a1 = ab[1];
        al[0] = static_cast<uint8_t>(a0);
        al[1] = static_cast<uint8_t>(a1);
        if (a0 > a1) {
            for (int i = 2; i < 8; ++i)
                al[i] = static_cast<uint8_t>(((8 - i) * a0 + (i - 1) * a1) / 7);
        } else {
            for (int i = 2; i < 6; ++i)
                al[i] = static_cast<uint8_t>(((6 - i) * a0 + (i - 1) * a1) / 5);
            al[6] = 0;
            al[7] = 255;
        }
    }
    uint64_t abits = 0;
    if (L.mode == DXT5) {
        for (int i = 0; i < 6; ++i)
            abits |= static_cast<uint64_t>(ab[2 + i]) << (8 * i);
    }
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int n = 4 * y + x;
            ddt5::Px p = pal[(cb[4 + y] >> (2 * x)) & 3];
            p.a = (L.mode == DXT5)
                    ? al[(abits >> (3 * n)) & 7]
                    : static_cast<uint8_t>(((ab[n / 2] >> (4 * (n % 2))) & 15) * 17);
            out[n] = p;
        }
    }
}

inline Layout choose_layout(const uint8_t* data, size_t size, uint32_t w, uint32_t h,
                            const Layout& prefer, double* best_score_out = nullptr) {
    const uint32_t bx = (w + 3) / 4;
    const uint32_t by = (h + 3) / 4;
    const uint32_t n = std::min<uint32_t>(16, bx);
    const uint32_t m = std::min<uint32_t>(16, by);
    const uint32_t i0 = (bx - n) / 2;
    const uint32_t j0 = (by - m) / 2;
    auto score = [&](const Layout& L) -> double {
        std::vector<ddt5::Px> img(static_cast<size_t>(n) * 4 * m * 4);
        const uint32_t W = n * 4;
        const uint32_t H = m * 4;
        int non_uniform = 0, nb = 0;
        for (uint32_t j = 0; j < m; ++j) {
            for (uint32_t i = 0; i < n; ++i) {
                const size_t off = (static_cast<size_t>(j0 + j) * bx + (i0 + i)) * 16;
                if (off + 16 > size) return 1e9;
                ddt5::Px b[16];
                decode_block(data + off, L, b);
                int mid = 0;
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        img[static_cast<size_t>(j * 4 + y) * W + i * 4 + x] = b[4 * y + x];
                        mid += (b[4 * y + x].a > 8 && b[4 * y + x].a < 247);
                    }
                }
                non_uniform += (mid > 0);
                ++nb;
            }
        }
        double sum = 0, asum = 0;
        long cnt = 0, acnt = 0;
        auto lum = [](const ddt5::Px& p) { return static_cast<int>(p.r) + p.g + p.b; };
        for (uint32_t y = 0; y < H; ++y) {
            for (uint32_t x = 0; x < W; ++x) {
                const ddt5::Px& p = img[static_cast<size_t>(y) * W + x];
                if (x + 1 < W) {
                    asum += std::abs(static_cast<int>(p.a) -
                                     static_cast<int>(img[static_cast<size_t>(y) * W + x + 1].a));
                    ++acnt;
                }
                if (y + 1 < H) {
                    asum += std::abs(static_cast<int>(p.a) -
                                     static_cast<int>(img[static_cast<size_t>(y + 1) * W + x].a));
                    ++acnt;
                }
                if (p.a < 128) continue;
                if (x + 1 < W && img[static_cast<size_t>(y) * W + x + 1].a >= 128) {
                    sum += std::abs(lum(p) - lum(img[static_cast<size_t>(y) * W + x + 1]));
                    ++cnt;
                }
                if (y + 1 < H && img[static_cast<size_t>(y + 1) * W + x].a >= 128) {
                    sum += std::abs(lum(p) - lum(img[static_cast<size_t>(y + 1) * W + x]));
                    ++cnt;
                }
            }
        }
        return (cnt ? sum / static_cast<double>(cnt) : 999.0) +
               100.0 * (nb ? static_cast<double>(non_uniform) / nb : 1.0) +
               0.5 * (acnt ? asum / static_cast<double>(acnt) : 0.0);
    };
    Layout best = prefer;
    double best_s = score(prefer);
    for (int mo = 0; mo < 4; ++mo) {
        for (int c = 0; c < 2; ++c) {
            Layout L{Mode(mo), c != 0};
            const double s = score(L);
            if (s < best_s - 0.5) {
                best_s = s;
                best = L;
            }
        }
    }
    if (best_score_out) *best_score_out = best_s;
    return best;
}

} // namespace ddt6
