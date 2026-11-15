# CURRENT_STATE — baseado na versão 1.0.11142202 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ `glDepthMask(GL_TRUE)` garantido no `begin_frame` antes de `glClear`.
- ✅ Terreno 128×128m (`kTerrainTileSize = 2.0f`) e Câmera RTS calibrada (`radius_ = 65.0f`, `pitch_ = 0.85f`).
- ✅ Orientação Suave / Heading Lerp (`ecs.h`) e suporte a clips Granny 2 / GRN (`grn_animation.cpp`).
- ✅ Menu de Campanha Autêntico em Duas Colunas com Mapas Regionais e Seletor de Dificuldade.
- ✅ Resolução Autêntica de Texturas MTRL em BRGs e Relevo Normalizado acima do Mar ($Y \ge 0$).

## Correções Aplicadas nesta Versão (Widgets XMB Autênticos: ScrollList, RadioSet, CheckButton, StateButton 4 Estados)
- ✅ **UIManager estendido** (`ui.h`/`ui.cpp`):
  - `UIScrollList` (dropdown XMB): expand/colapse, seleção por toque, texturas top/bottom cap.
  - `UIRadioSet` (radio buttons XMB): grupo mutuamente exclusivo, texturas checked/unchecked.
  - `UIButton` + 4 estados: `normal_tex`, `pressed_tex`, `rollover_tex`, `glow_tex` + hover tracking.
  - `add_scroll_list()`, `add_radio_set()` + integração em `apply_layout_to_ui`.
- ✅ **ui_layout.cpp**: Carrega `scrollList`, `radioset`, `checkButton`, `slider` do XMB com texturas autênticas.
- ✅ **stateBackgrounds[0..3]** aplicados corretamente (normal, clicked, rollover, glow).
- ✅ **DDT Formato 6 (AOM_8BPP)**: Auto-decoder via `ddt6_auto.h` integrado.
- ✅ **textures2.bar / data2.bar** copiados no `MyNativeActivity.java`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11142202 gerado e assinado em `/sdcard/aom-native-v1.0.11142202.apk`.
2. Testar menu de opções XMB autêntico (dropdowns Screen Size, Unit Stance, MP Search; radios Graphics/Sound; checkboxes).
3. Testar multiplayer LAN com layout XMB autêntico.
4. Implementar callbacks XMB: `gadgetReal`, `gadgetUnReal`, `subModeEnter`, `exit`, `campaignStart`, `LoadScenario`.