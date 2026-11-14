# CURRENT_STATE — baseado na versão 1.0.11141122 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Pipeline de Visuais (`resolve_unit_visual` + `apply_unit_visual`): Mapeamento automático de tropas e edifícios para modelos BRG autênticos.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Unificação de Múltiplos Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`)**:
  - Modelos complexos de edifícios (como o Solar Atlante / Manor) e elementos de cenário dividem geometria em múltiplos blocos `MESI` (ex: submesh 0 = paredes, submesh 1 = telhado, submesh 2 = adereços).
  - Anteriormente, apenas `model->meshes[0]` era enviado para a GPU via `create_mesh`, deixando o telhado e adereços invisíveis.
  - Implementada unificação contínua de todos os submeshes em `load_brg_model`, `load_models` e `get_model_entry` com ajuste de deslocamento de índices (`idx + v_offset`).

## Próximos passos (ordem)
1. ✅ APK v1.0.11141122 gerado e assinado em `/sdcard/aom-native-v1.0.11141122.apk`.
2. Validar no dispositivo o telhado completo do Solar Atlante e todas as partes dos modelos 3D renderizadas.