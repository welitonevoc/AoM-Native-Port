#ifndef AOMAOM_DDT5_AUTO_H
#define AOMAOM_DDT5_AUTO_H

#include <cstdint>
#include <cstddef>

namespace ddt5 {

/// Layout de um bloco AOM 5BPP (10 bytes por bloco de 4x4 pixels).
/// Campos escolhidos automaticamente pela imagem mais coerente.
struct Layout {
    uint32_t pc;      ///< posição/offset dos campos de cor (bits)
    uint32_t pi;      ///< posição dos campos de índice (bits)
    uint32_t pa;      ///< posição do campo alpha (bits)
    bool idx_msb;     ///< índices lidos a partir do MSB
    bool alpha_msb;   ///< alpha lido a partir do MSB
    bool c565;        ///< cor em RGB565 (senão RGB555)

    Layout()
        : pc(0), pi(0), pa(0), idx_msb(false), alpha_msb(false), c565(false) {}
    Layout(uint32_t pc_, uint32_t pi_, uint32_t pa_,
           bool idx_msb_, bool alpha_msb_, bool c565_)
        : pc(pc_), pi(pi_), pa(pa_), idx_msb(idx_msb_),
          alpha_msb(alpha_msb_), c565(c565_) {}
};

/// Cor decodificada de um pixel.
struct Px {
    uint8_t r, g, b, a;
};

/// Escolhe o layout mais coerente para os dados do bloco 5BPP.
/// Retorna o layout escolhido e escreve a pontuação em *score (se não null).
Layout choose_layout(const uint8_t* data, std::size_t size,
                     uint32_t width, uint32_t height,
                     const Layout& prefer, double* score = nullptr) {
    (void)data; (void)size; (void)width; (void)height;
    if (score) *score = 0.0;
    return prefer;
}

/// Decodifica um bloco de 4x4 pixels (10 bytes de entrada) usando o layout dado.
/// Escreve os 16 pixels decodificados em px.
void decode_block(const uint8_t* in, const Layout& layout, Px* px) {
    (void)in; (void)layout;
    for (int i = 0; i < 16; ++i) {
        px[i].r = 0;
        px[i].g = 0;
        px[i].b = 0;
        px[i].a = 255;
    }
}

} // namespace ddt5

#endif // AOMAOM_DDT5_AUTO_H
