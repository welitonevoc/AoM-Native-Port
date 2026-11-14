# CURRENT_STATE — baseado na versão 1.0.11141743 (14/11/2026). Máx. ~60 linhas.
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
- ✅ Menu de Campanha Autêntico em Duas Colunas com Mapas Regionais e Seletor de Dificuldade (`campaign_select_system.cpp`).

## Correções Aplicadas nesta Versão (Texturização MTRL de BRGs & Terreno Acima do Mar)
- ✅ **Resolução Autêntica de Texturas do BRG (`brg_model.cpp` & `engine_assets.cpp`)**:
  - `parse_brg` e `get_model_entry` agora consultam primeiro as texturas embutidas nos chunks `MTRL` do `.brg`.
  - Priorização automática do corpo/fachada principal sobre atlas de anexos (`is_attachment`), atribuindo texturas canônicas (`building g age0.ddt`, `infantry g hoplite standard.ddt`, `scenario g arkantos man map.ddt`) em vez de `building attachments.ddt`.
  - Remoção de buscas inválidas por sufixos de animação (`_idlea.ddt`, `_attacka.ddt`), eliminando falhas de arquivo no log.
- ✅ **Elevação do Terreno e Splatting de Costa (`engine_terrain.cpp` & `gles_renderer.cpp`)**:
  - Relevo de cenários `.scn` normalizado a partir de $h \ge 0$, mantendo a terra firme acima do plano de água ($y = -0.4f$) e evitando que o mar cubra o mapa.
  - `shoreFactor` no fragment shader restrito à faixa de costa imediata ($y \approx water\_level$), impedindo sobreposição de textura de margem no topo de montanhas.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141743 gerado e assinado em `/sdcard/aom-native-v1.0.11141743.apk`.
2. Validar visibilidade e texturização dos modelos e terreno no gameplay da campanha.