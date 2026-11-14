# CURRENT_STATE — baseado na versão 1.0.11141501 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.

## Correções aplicadas nesta iteração (Opção B: Pipeline BRG → glTF → GLES)
- ✅ **Decodificação Fiel BRG (Baseada em `ptasev/Age-of-Mythology` / `AoMEngineLibrary`)**:
  - `half_to_float` corrigido para truncated-float32 de AoM (bfloat16: upper 16 bits de float32).
  - Cabeçalho `MESI` alinhado estritamente em 80 bytes com leitura de `objectCenter`, `hotspot`, `radius`, e flags de propriedades.
  - Distinção de malhas primárias vs malhas secundárias (`0x0400 = SECONDARYMESH`): keyframes de animação por morph targets preservados em `BrgModel::morph_frames` sem corromper a geometria estática.
  - Parsing de blocos `MTRL` e texturas DDT associadas diretamente às malhas.
  - Registro dinâmico de `BrgAnimationClip` ao carregar modelos com morph targets no `Engine::get_model_entry`.
- ✅ **Conversor Standalone `tools/brg_to_gltf.py`**:
  - Gera arquivos glTF 2.0 / GLB binários com base mesh, canais de morph targets, animação de pesos (`weights`), samplers de tempo e materiais.
  - Validado com extração direta dos BARs para Hoplitas, Aldeões Gregos e Minotauros.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141501 gerado e assinado em `/sdcard/aom-native-v1.0.11141501.apk`.
2. Instalar o APK v1.0.11141501 e validar a renderização das unidades 3D no gameplay.