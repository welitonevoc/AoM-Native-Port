# CURRENT_STATE — baseado na versão 1.0.11141811 (14/11/2026). Máx. ~60 linhas.
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

## Correções Aplicadas nesta Versão (Menu de Opções Confinado & Controles Cíclicos)
- ✅ **Layout Confinado à Tela (`engine_options_menu.cpp`)**:
  - Eliminação da escala quadrática `vh * 0.18f * sy` nas abas e painel de conteúdo.
  - Painel de conteúdo estritamente contido entre as abas e o botão `VOLTAR` (`content_h = back_y - content_y - 12*sy`).
  - Botão `VOLTAR` reposicionado no rodapé (`vh * 0.95f - back_h - 10*sy`) com contorno dourado e hitbox ampliada.
- ✅ **Controles Cíclicos e Tácteis com `add_button` (`engine_options_menu.cpp`)**:
  - Volume Master, Música e Efeitos com botões cíclicos (+10% por toque: 0%..100%).
  - Toggles funcionais com feedback de cor (Verde=Ativado / Vermelho=Desativado) para Música, SFX e Joystick Virtual.
  - Seletores de Qualidade Gráfica, Exibir FPS e Sensibilidade Touch com persistência em `options.cfg`.
  - Log consistente do estado das abas: `Entered state: OPTIONS / Tab %d (%s)`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141811 gerado e assinado em `/sdcard/aom-native-v1.0.11141811.apk`.
2. Validar resposta de toque no menu de opções e navegação entre abas via logcat.