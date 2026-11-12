// Seleção automática do layout do bloco de 10 bytes do formato DDT 5.
// Cabe em ddt_texture.cpp (namespace anônimo). Sem dependências além de <cstdint>/<vector>.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

namespace ddt5 {

struct Px { uint8_t r, g, b, a; };

struct Layout {
    uint8_t pc, pi, pa;   // offsets de: cores (4 bytes), índices (4 bytes), máscara alpha (2 bytes)
    bool idx_msb;         // índices: pixel 0 nos bits altos do byte?
    bool alpha_msb;       // máscara: pixel 0 no bit alto?
    bool c565;            // cores em RGB565 (senão RGB555)
};

inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

inline Px expand(uint16_t c, bool c565) {
    if (c565) return {uint8_t(((c >> 11) & 31) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63),
                      uint8_t((c & 31) * 255 / 31), 255};
    return {uint8_t(((c >> 10) & 31) * 255 / 31), uint8_t(((c >> 5) & 31) * 255 / 31),
            uint8_t((c & 31) * 255 / 31), 255};
}

// Decodifica um bloco em 16 pixels (ordem y*4+x)
inline void decode_block(const uint8_t* blk, const Layout& L, Px out[16]) {
    Px pal[4];
    pal[0] = expand(u16(blk + L.pc), L.c565);
    pal[1] = expand(u16(blk + L.pc + 2), L.c565);
    for (int k = 0; k < 3; ++k) {
        uint8_t* d2 = k == 0 ? &pal[2].r : k == 1 ? &pal[2].g : &pal[2].b;
        uint8_t* d3 = k == 0 ? &pal[3].r : k == 1 ? &pal[3].g : &pal[3].b;
        int a = k == 0 ? pal[0].r : k == 1 ? pal[0].g : pal[0].b;
        int b = k == 0 ? pal[1].r : k == 1 ? pal[1].g : pal[1].b;
        *d2 = uint8_t((2 * a + b) / 3);
        *d3 = uint8_t((a + 2 * b) / 3);
    }
    pal[2].a = pal[3].a = 255;
    const uint16_t am = u16(blk + L.pa);
    for (int y = 0; y < 4; ++y) {
        const uint8_t row = blk[L.pi + y];
        for (int x = 0; x < 4; ++x) {
            const int sh = L.idx_msb ? 2 * (3 - x) : 2 * x;
            Px p = pal[(row >> sh) & 3];
            const int bit = L.alpha_msb ? 15 - (4 * y + x) : 4 * y + x;
            p.a = ((am >> bit) & 1) ? 255 : 0;
            out[4 * y + x] = p;
        }
    }
}

// Pontuação: menor = imagem mais coerente (suave nos pixels opacos + alpha uniforme por bloco)
inline double score_window(const uint8_t* data, size_t data_size, uint32_t blocks_x, uint32_t blocks_y,
                           uint32_t i0, uint32_t j0, uint32_t n, uint32_t m, const Layout& L) {
    std::vector<Px> img(size_t(n) * 4 * m * 4);
    const uint32_t W = n * 4;
    int non_uniform = 0, nblocks = 0;
    for (uint32_t j = 0; j < m; ++j)
        for (uint32_t i = 0; i < n; ++i) {
            const size_t off = (size_t(j0 + j) * blocks_x + (i0 + i)) * 10;
            if (off + 10 > data_size) return 1e9;
            Px b[16];
            decode_block(data + off, L, b);
            int opaque = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    img[size_t(j * 4 + y) * W + i * 4 + x] = b[4 * y + x];
                    opaque += b[4 * y + x].a > 127;
                }
            non_uniform += (opaque != 0 && opaque != 16);
            ++nblocks;
        }
    const uint32_t H = m * 4;
    double sum = 0; long cnt = 0;
    auto lum = [](const Px& p) { return int(p.r) + p.g + p.b; };
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const Px& p = img[size_t(y) * W + x];
            if (p.a < 128) continue;
            if (x + 1 < W && img[size_t(y) * W + x + 1].a >= 128) { sum += std::abs(lum(p) - lum(img[size_t(y) * W + x + 1])); ++cnt; }
            if (y + 1 < H && img[size_t(y + 1) * W + x].a >= 128) { sum += std::abs(lum(p) - lum(img[size_t(y + 1) * W + x])); ++cnt; }
        }
    const double smooth = cnt ? sum / cnt : 999.0;
    return smooth + 200.0 * (nblocks ? double(non_uniform) / nblocks : 1.0);
}

// Escolhe o melhor layout olhando uma janela central de até 16x16 blocos.
// 'prefer' vence em caso de empate (textura lisa/vazia = sem informação).
inline Layout choose_layout(const uint8_t* data, size_t size, uint32_t w, uint32_t h, const Layout& prefer,
                            double* best_score_out = nullptr) {
    const uint32_t bx = (w + 3) / 4, by = (h + 3) / 4;
    const uint32_t n = std::min<uint32_t>(16, bx), m = std::min<uint32_t>(16, by);
    const uint32_t i0 = (bx - n) / 2, j0 = (by - m) / 2;
    struct P { uint8_t pc, pi, pa; };
    static const P orders[6] = {{0, 4, 8},   // C I A
                                {0, 6, 4},   // C A I   (layout antigo do engine)
                                {4, 0, 8},   // I C A
                                {2, 6, 0},   // A C I
                                {6, 0, 4},   // I A C
                                {6, 2, 0}};  // A I C
    Layout best = prefer;
    double best_s = score_window(data, size, bx, by, i0, j0, n, m, prefer);
    for (const P& o : orders)
        for (int im = 0; im < 2; ++im)
            for (int am = 0; am < 2; ++am)
                for (int c = 0; c < 2; ++c) {
                    Layout L{o.pc, o.pi, o.pa, im != 0, am != 0, c != 0};
                    const double s = score_window(data, size, bx, by, i0, j0, n, m, L);
                    if (s < best_s - 0.5) { best_s = s; best = L; }   // margem p/ preferir o padrão
                }
    if (best_score_out) *best_score_out = best_s;
    return best;
}

} // namespace ddt5
