# CURRENT_STATE — baseado na versão 1.0.11141529 (14/11/2026). Máx. ~60 linhas.
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

## Correções aplicadas nesta iteração (Visibilidade & Alpha de Tropas/Edifícios)
- ✅ **Tratamento de Alpha no Fragment Shader (`src/renderer/gles_renderer.cpp`)**:
  - `discard` ajustado de `< 0.25` para `< 0.01` (evita descarte de fragmentos de tropas cujas texturas DDT possuem alpha baixo para máscaras/specular).
  - Saída forçada para opaca em texturas de unidade: `outAlpha = max(baseColor.a, 0.90)`.
- ✅ **Gerenciamento de `BlendMode` (`src/renderer/gles_renderer.cpp`)**:
  - `draw_mesh` e `draw_mesh_skinned` configuram `glDisable(GL_BLEND)` e `glDepthMask(GL_TRUE)` para `BlendMode::OPAQUE`, além de suporte a `ALPHA_BLEND` e `ADDITIVE`.
- ✅ **Escala de Renderização RTS (`include/ecs/gameplay.h`)**:
  - Ajustado fator de visualização na câmera para unidades (×2.0) e edifícios (×2.5).

## Próximos passos (ordem)
1. ✅ APK v1.0.11141529 gerado e assinado em `/sdcard/aom-native-v1.0.11141529.apk`.
2. Instalar o APK v1.0.11141529 e testar na Campanha (Prologue) a visualização das malhas 3D das tropas e construções.