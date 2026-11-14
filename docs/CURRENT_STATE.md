# CURRENT_STATE — baseado na versão 1.0.11141547 (14/11/2026). Máx. ~60 linhas.
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
- ✅ Tratamento de Alpha Shader (`discard < 0.01`, `outAlpha >= 0.90`) e estados de `BlendMode`.

## Diagnósticos Ativos nesta Versão (`render_diag.patch`)
- ✅ **Log `[RENDER]` a cada ~3s (`include/ecs/gameplay.h`)**:
  - Contagem precisa de `entidades`, `culled` (frustum/distância), `sem_malha` e `desenhadas`.
  - Reporta `pos`, `scale`, `mesh`, `tex` da 1ª entidade, e coordenadas da `cam` e `alvo`.
- ✅ **Log `[BRGBOUNDS]` no carregamento (`src/engine/engine_assets.cpp`)**:
  - Reporta contagem de vértices e caixa envolvente (`min`, `max`, `tamanho`) das malhas BRG sob demanda.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141547 gerado e assinado em `/sdcard/aom-native-v1.0.11141547.apk`.
2. Rodar campanha e coletar saída logcat com `[RENDER]` e `[BRGBOUNDS]`.
3. Prosseguir com decodificação do DDT formato 3 (paleta com 4-bit alpha) via referências PNG em `tests/reference/`.