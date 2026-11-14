# CURRENT_STATE — baseado na versão 1.0.11141301 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Mapeamento de Campanha e Heróis (`engine_assets.cpp` & `engine_gameplay.cpp`)**:
  - `resolve_unit_visual` expandido com candidatos autênticos: Arkantos (`scenario a arkantos_idlea.brg`), Piratas (`infantry g hoplite_attacka.brg`) e Kraken (`special g minotaur_idlea.brg`).
  - `resolve_building_visual` com suporte a Torres de Defesa (`BuildingType::TOWER` -> `building e lighthouse.brg`).
  - Callbacks `on_unit_spawned` ativos em `start_campaign_mission` e `start_generic_mission` chamando `apply_unit_visual`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141301 gerado e assinado em `/sdcard/aom-native-v1.0.11141301.apk`.
2. Abrir o jogo no aparelho, clicar em **Campanha -> Prólogo** (ou Jogador Único) para spawnar as tropas no mapa e enviar o logcat.