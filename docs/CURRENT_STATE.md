# CURRENT_STATE — baseado na versão 1.0.11141556 (14/11/2026). Máx. ~60 linhas.
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
- ✅ Diagnósticos `[RENDER]` (~3s) e `[BRGBOUNDS]` em execução.

## Correções Aplicadas nesta Versão (GLES Pipeline & Depth Clear)
- ✅ **Garantia de Limpeza do Depth Buffer (`src/renderer/gles_renderer.cpp`)**:
  - `glDepthMask(GL_TRUE)` forçado imediatamente antes de `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` no `begin_frame` (evita falha silenciosa de `glClear(GL_DEPTH_BUFFER_BIT)` quando passadas 2D/UI anteriores deixam `glDepthMask(GL_FALSE)`).
- ✅ **Shader de Malha 3D sem Discard e com Fallback Sólido (`src/renderer/gles_renderer.cpp`)**:
  - Removido `discard` prematuro (evita sumiço de malhas cujas texturas possuem canal alpha zerado ou não-preenchido).
  - Malhas sem textura (`uHasTex == 0`) usam cor magenta sólida `(1.0, 0.0, 1.0, 1.0)` como diagnóstico visual imediato.
  - Saída do fragmento forçada com alpha `1.0` opaco.
- ✅ **Log de Draw Call (`src/renderer/gles_renderer.cpp`)**:
  - `[DRAW_MESH]` periódico reportando `mesh`, contagem de índices, `tex` e `blend`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141556 gerado e assinado em `/sdcard/aom-native-v1.0.11141556.apk`.
2. Rodar e verificar se os modelos aparecem texturizados (ou magenta se sem textura) e checar os logs `[RENDER]`, `[BRGBOUNDS]` e `[DRAW_MESH]`.