# CURRENT_STATE — baseado na versão 1.0.11141309 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Mapeamento Exaustivo dos BARs via `generate_unit_visuals_table.py` (`engine_assets.cpp`)**:
  - Criado `tools/generate_unit_visuals_table.py` que escaneou 11.806 entradas dos arquivos `models.bar` e `models2.bar`.
  - Gerado `src/engine/generated_unit_visuals.inc` com mapeamento exato de mais de 50 classes de tropas, heróis e monstros míticos (Hoplita, Toxotes, Hippikon, Aldeões Gregos/Nórdicos/Egípcios, Arkantos, Ajax, Chiron, Amanra, Murmillo, Arcus, Promethean, Minotauro, etc.) priorizando poses `idlea` > `walka`.
  - Integrado no `Engine::resolve_unit_visual`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141309 gerado e assinado em `/sdcard/aom-native-v1.0.11141309.apk`.
2. Testar no dispositivo (**Campanha -> Prólogo** ou **Jogador Único**) e verificar as malhas das unidades no jogo.