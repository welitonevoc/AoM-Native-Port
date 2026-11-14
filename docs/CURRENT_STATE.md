# CURRENT_STATE — baseado na versão 1.0.11141653 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ `glDepthMask(GL_TRUE)` garantido no `begin_frame` antes de `glClear`.
- ✅ Terreno 128×128m (`kTerrainTileSize = 2.0f`) e Câmera RTS calibrada (`radius_ = 65.0f`, `pitch_ = 0.85f`).

## Correções Aplicadas nesta Versão (Animação de Movimento & Heading Lerp)
- ✅ **Orientação Suave / Heading Lerp (`include/ecs/ecs.h`)**:
  - `MovementSystem::update` agora calcula o ângulo alvo `atan2(vx, vz)` pelo menor percurso e interpola suavemente `tr.rotation.y`.
  - Elimina o deslizamento estático de lado/costas e previne rotações instantâneas ou em falso.
- ✅ **Suporte a Clips Granny 2 / GRN (`src/anim/grn_animation.cpp` & `engine_assets.cpp`)**:
  - `parse_grn` atualizado para validar a assinatura binária oficial do Granny 2 (`0x2A 0x30 0x39 0x04` / `*09\x04`).
  - `load_grn_clips` agora consome de `models_bar_` (onde residem os 584 arquivos `.grn` de heróis e tropas) com prefixos de unidades (`hop`, `ark`, `aman`, `ajax`).
- ✅ **Renderização Dinâmica de Unidades com Morphing Ativo (`include/ecs/gameplay.h`)**:
  - `render_animated` agora desenha as malhas com interpolação de vértices de unidades sem descartá-las por possuírem malha estática de pose.
  - `render` estático ignora entidades que possuem componente de animação ativo, evitando sobreposição.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141653 gerado e assinado em `/sdcard/aom-native-v1.0.11141653.apk`.
2. Validar marcha das unidades e transição de heading no mapa da campanha.