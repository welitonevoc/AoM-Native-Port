# CURRENT_STATE — baseado na versão 1.0.11140156 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha DecodeBt8As4444): linha a linha, 4 pixels por grupo de 6 bytes com inversão vertical e paleta RGB444. Logo Titans, pórtico e botões renderizam 100% opacos e na orientação correta.
- ✅ Mapeamento UV de Botões (`ui_layout.cpp`): inversão de V nos botões e ButtonSets; jade e molduras renderizam perfeitamente.
- ✅ Localização PT-BR dos Menus (`engine_assets.cpp`): IDs oficiais (`19825`..`23603`) traduzidos para Português (Aprender a Jogar, Campanha, Jogador Único, Multijogador, Opções, Mais, Sair, etc.).
- ✅ Centralização Exata de Textos de Botões (`ui.cpp` & `gles_renderer.cpp`): `measure_ui_text` com suporte a caracteres acentuados PT-BR.
- ✅ Orientação da Costa e Mar 3D (`engine_terrain.cpp` & `engine_states.cpp`): Ilha/platô elevado no lado esquerdo da tela (`wx < shore_x`), mar aberto à direita. Câmera isométrica frontal ajustada.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Decodificação de Alpha DDT Formato 5 (`ddt5_auto.h` & `ddt_texture.cpp`)**: Corrigida a máscara de 1-bit alpha (`am`) de 5bpp. O bit 1 representa cutout/transparência; revertido para `((am >> bit) & 1) ? 0 : 255` e forçado `alpha = 255` em texturas diffuse padrão (`usage == 1`). Telhados de edifícios (Solar/Manor), corpos de tropas e copas de palmeiras agora são 100% sólidos.
- ✅ **Shaders de Água Autênticos (`gles_renderer.cpp`)**: Removido padrão artificial de círculos/bolinhas brancas (`waveCrest`). Declarado e amostrado `uNormalMap` (`waterbump.ddt` / `waterbump0000.ddt`) com coordenadas UV duplas em movimento. Adicionados reflexão especular do sol, gradiente costeiro turquesa -> marinho e atenuação Fresnel.
- ✅ **Transparência de Folhagem e Two-Sided Meshes (`gles_renderer.cpp`)**: Adicionado `if (baseColor.a < 0.25) discard;` em `kMeshFragmentSrc`. Desativado backface culling (`glDisable(GL_CULL_FACE)`) para garantir visibilidade bidirecional de folhas de árvores e acessórios 3D.
- ✅ **Carregamento Direto de Texturas do BAR (`engine_assets.cpp`)**: Texturas de terreno e água agora usam caminhos reais de `textures.bar` via `load_ddt_texture`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11140156 gerado e assinado em `/sdcard/aom-native-v1.0.11140156.apk`.
2. Validar no dispositivo o mar fluido, o telhado do Solar/Manor e a folhagem das árvores.