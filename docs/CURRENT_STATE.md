# CURRENT_STATE — baseado na versão 1.0.11141604 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma de rebentação na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular verde/vermelho no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados em malha única.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ Pipeline BRG → glTF → GLES (`brg_to_gltf.py`, bfloat16, morph targets).
- ✅ Offset MTRL (`pos + 64`) e candidatos de textura (`bronze.ddt`, `copper.ddt`, etc.).
- ✅ `glDepthMask(GL_TRUE)` garantido no `begin_frame` antes de `glClear`.

## Causa Raiz Descoberta e Corrigida nesta Versão (Vértices Colapsando em Zero)
- ✅ **Correção Crítica do Vertex Shader de Malhas (`kMeshVertexSrc` em `gles_renderer.cpp`)**:
  - `kMeshVertexSrc` continha erroneamente código de skinning esquelético (`skinnedPos += aBoneWeights * uBoneMatrices...`).
  - Como `create_mesh` não envia atributos de ossos para malhas estáticas e `draw_mesh` não carrega `uBoneMatrices`, `skinnedPos` resultava em `vec4(0.0)`.
  - **Resultado:** todos os vértices de todas as malhas de entidades (Hoplitas, Casas, Árvores, Rochas) colapsavam matematicamente em `gl_Position = (0, 0, 0, 0)`!
  - **Fix:** Restaurado cálculo direto de malha estática: `vec4 worldPos = uModel * vec4(aPos, 1.0); gl_Position = uViewProj * worldPos;`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11141604 gerado e assinado em `/sdcard/aom-native-v1.0.11141604.apk`.
2. Testar campanha e verificar se todas as unidades e edifícios aparecem perfeitamente renderizados na tela.