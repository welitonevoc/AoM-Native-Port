# CURRENT_STATE — baseado na versão 1.0.11141516 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ Pipeline BRG → glTF → GLES (`brg_to_gltf.py`, bfloat16, morph targets).

## Correções aplicadas nesta iteração (Visibilidade & Texturas de Tropas)
- ✅ **Offset do Bloco MTRL em `src/assets/brg_model.cpp`**:
  - Ajustado offset de leitura do nome da textura para `pos + 64` (evita truncar `"Infantry G Hoplite Standard"` para `"ntry g hoplite..."`).
- ✅ **Candidatos de Textura em `src/engine/engine_assets.cpp`**:
  - Após remover sufixos de animação (`_idlea`, `_walka`, etc.), adiciona variantes `bronze.ddt`, `copper.ddt`, `standard.ddt` e `map.ddt`.
- ✅ **Iluminação Ambiente Mínima em `src/renderer/gles_renderer.cpp`**:
  - Shader de malhas 3D atualizado com `lighting = max(lighting, vec3(0.35));` para impedir que modelos sumam pretos contra o terreno.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141516 gerado e assinado em `/sdcard/aom-native-v1.0.11141516.apk`.
2. Instalar o APK v1.0.11141516 e verificar os logs de `get_model_entry` com `tex != 0` e o visual das tropas no mapa.