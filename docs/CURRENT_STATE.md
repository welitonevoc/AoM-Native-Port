# CURRENT_STATE — baseado na versão 1.0.11141620 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ Pipeline BRG → glTF → GLES (`brg_to_gltf.py`, bfloat16, morph targets).
- ✅ Offset MTRL (`pos + 64`) e candidatos de textura (`bronze.ddt`, `copper.ddt`, etc.).
- ✅ `glDepthMask(GL_TRUE)` garantido no `begin_frame` antes de `glClear`.
- ✅ Vértices e `kMeshVertexSrc` corrigidos sem skinning indevido (malhas 3D renderizando normalmente).
- ✅ Escala 1:1 dos modelos BRG mantida de fábrica.

## Correções Aplicadas nesta Versão (Escala de Terreno & Câmera RTS)
- ✅ **Expansão do Terreno para Proporção Canônica AoM (`src/engine/engine_terrain.cpp`)**:
  - `kTerrainTileSize` alterado de `1.0f` para `2.0f`.
  - O mapa heightmap de 64×64 passa de 64m para 128×128 metros de área jogável.
  - Grade de navegação sincronizada com `kTerrainTileSize = 2.0f`.
  - Log atualizado: `Generated terrain from height-map: 64x64, 4096 verts, 23814 indices (step=2.0, world=128x128)`.
- ✅ **Calibração de Câmera RTS Isométrica (`engine_campaign.cpp` & `engine_input.cpp`)**:
  - `radius_` inicial ajustado de `14.0f` (colado nas tropas) para `28.0f`.
  - `pitch_` inicial ajustado de `0.35f` (~20° plano) para `0.85f` (~50° isométrico clássico AoM).
  - Limites de zoom estendidos para `[8.0f, 60.0f]`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141620 gerado e assinado em `/sdcard/aom-native-v1.0.11141620.apk`.
2. Validar o enquadramento do mapa e proporção TC/tropas/terreno.