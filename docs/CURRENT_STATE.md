# CURRENT_STATE — baseado na versão 1.0.11141609 (14/11/2026). Máx. ~60 linhas.
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

## Correções Aplicadas nesta Versão (Escala Canônica 1:1)
- ✅ **Remoção de Multiplicadores Paliativos (`include/ecs/gameplay.h`)**:
  - Removidos `scale *= 2.0f` (unidades) e `scale *= 2.5f` (edifícios).
  - Como o `[BRGBOUNDS]` confirmou que as malhas BRG originais possuem dimensões exatas de fábrica em metros (ex.: Hoplita com 1,75 m, Centro da Cidade com 10,5 m), a cena agora renderiza com escala 1:1 autêntica do jogo.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141609 gerado e assinado em `/sdcard/aom-native-v1.0.11141609.apk`.
2. Validar proporções das unidades, construções e recursos no mapa.