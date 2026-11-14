# CURRENT_STATE — baseado na versão 1.0.11141640 (14/11/2026). Máx. ~60 linhas.
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
  - `kTerrainTileSize` alterado de `1.0f` para `2.0f` (mapa de 128×128m).
  - Grade de navegação sincronizada com `kTerrainTileSize = 2.0f`.
- ✅ **Calibração de Câmera RTS Isométrica (`engine_campaign.cpp` & `engine_input.cpp`)**:
  - `radius_` inicial ajustado para `65.0f` (visão geral RTS equilibrada).
  - `pitch_` inicial em `0.85f` (~50° isométrico clássico AoM).
  - Clamp de pan do alvo da câmera expandido para `[-55.0f, 55.0f]` (cobre o terreno de 128m).
  - Limites de zoom estendidos para `[15.0f, 85.0f]`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141640 gerado e assinado em `/sdcard/aom-native-v1.0.11141640.apk`.
2. Validar jogabilidade em campo com a nova câmera e pan expandido.