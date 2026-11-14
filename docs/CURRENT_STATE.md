# CURRENT_STATE — baseado na versão 1.0.11141710 (14/11/2026). Máx. ~60 linhas.
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
- ✅ Orientação Suave / Heading Lerp (`ecs.h`) e suporte a clips Granny 2 / GRN (`grn_animation.cpp`).
- ✅ Renderização Dinâmica de Unidades com Morphing Ativo (`gameplay.h`).

## Correções Aplicadas nesta Versão (Menu de Campanha Autêntico em Duas Colunas)
- ✅ **Menu de Seleção em Duas Colunas (`campaign_select_system.cpp` & `.h`)**:
  - Coluna esquerda: Lista de missões paginada (7 por tela) com navegação `[ < Anterior ]`, `[ Próximo > ]` e destaque dourado no cenário selecionado.
  - Coluna direita: Cartão detalhado da missão contendo número e título, mapa regional 2D autêntico (`campmap_atlantis7.ddt`, `campmap_greece7.ddt`, `campmap_egypt7.ddt`, `campmap_norselands3.ddt`), sinopse/briefing do cenário, seletor de 4 níveis de dificuldade (Fácil, Moderado, Difícil, Titã) e botão proeminente `[ ▶ INICIAR MISSÃO ]`.
- ✅ **Segurança de Execução da UI (`src/ui/ui.cpp`)**:
  - Callback de botões armazena ponteiro de função local antes de restaurar cor do botão, prevenindo use-after-free quando o clique reconstrói ou limpa a UI.
- ✅ **API de Textura (`include/engine/engine.h`)**:
  - `load_ddt_texture` tornado público no `Engine` para permitir carregamento dinâmico de mapas da campanha e ícones de interface.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141710 gerado e assinado em `/sdcard/aom-native-v1.0.11141710.apk`.
2. Validar transições de mapa e início de missão a partir da nova interface de seleção.