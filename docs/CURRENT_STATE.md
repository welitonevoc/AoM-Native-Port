# CURRENT_STATE — baseado na versão 1.0.11141030 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Parser BRG (`brg_model.cpp`): Vértices float32 de 12 bytes corrigidos para construções, palmeiras e rochas.
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Resolução e Vinculação de Visuais de Unidades/Edifícios (`gameplay.h`, `engine.h`, `engine_gameplay.cpp`, `engine_campaign.cpp`)**:
  - Implementado `on_unit_spawned` e `on_building_placed` em `GameplayWorld`, disparados em todo spawn/construção.
  - Criado `resolve_unit_visual` e `resolve_building_visual` no `Engine`, mapeando tipos (Hoplite, Toxotes, Villager Greek/Norse/Egyptian, Murmillo, Arcus, Minotaur, Town Center, Barracks, Manor, House) diretamente para seus arquivos `.brg` e `.ddt` autênticos dos BARs.
  - Corrigido `start_game()` e `start_generic_mission()` para associar malhas e texturas BRG às unidades e edifícios em vez de cair em meshes nulas ou placeholders de 32 vértices com linhas de depuração.
  - Em `render_animated`, evitada sobreposição de mesh placeholder se a unidade já possuir modelo específico atribuído.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141030 gerado e assinado em `/sdcard/aom-native-v1.0.11141030.apk`.
2. Validar no dispositivo as malhas 3D de soldados, aldeões e centros de cidade renderizando no gameplay.