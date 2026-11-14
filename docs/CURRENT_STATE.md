# CURRENT_STATE — baseado na versão 1.0.11141035 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Parser BRG (`brg_model.cpp`): Vértices float32 de 12 bytes corrigidos para construções, palmeiras e rochas.
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Mapeamento e Aplicação de Visuais (`resolve_unit_visual` + `apply_unit_visual`)**:
  - `resolve_unit_visual(unit_name)` em `engine_assets.cpp`: Mapeia tipo → candidatos preferidos (`idlea` > `walka` > outros), fallback fuzzy no `models.bar` e fallback genérico para Hoplite.
  - `apply_unit_visual(entity, name)` em `engine_gameplay.cpp`: Chama `set_unit_visuals(mesh, tex)` e anexa clip de animação padrão à entidade.
  - `start_generic_mission` em `engine_campaign.cpp`: Chama `apply_unit_visual` explicitamente após cada `spawn_unit` e `spawn_worker`.
  - `on_unit_spawned` em `gameplay.h`: Encaminha todo spawn dinâmico para `apply_unit_visual`.
  - `render_animated` em `gameplay.h`: Não sobrepõe malha placeholder de 32 vértices se a unidade já tiver modelo próprio atribuído.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141035 gerado e assinado em `/sdcard/aom-native-v1.0.11141035.apk`.
2. Validar no logcat: `resolve_unit_visual(...)` e `apply_unit_visual entity=...`.