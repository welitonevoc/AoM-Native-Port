# CURRENT_STATE — baseado na versão 1.0.11132207 (13/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha): decodificação autêntica de blocos 4x4 de 24 bytes (16B índices + 8B alpha 4-bit) em ordem column-major (`bx` exterior, `by` interior) com paleta RGB444 (`palette_offset_alpha4`). Fim definitivo de scanlines, listras e ruído no logo Titans, pórtico e botões jade.
- ✅ Suporte a Alpha 1-bit em Formato 3: decodificação linear A1R5G5B5 (`palette_offset_alpha1`).
- ✅ Desbloqueio de Renderização de Entidades (`gameplay.h`): removido bloqueio em `GameplayWorld::render` que abortava render de tropas, casas e árvores com malhas próprias.
- ✅ Pipeline de Render 3D do Menu (`engine.cpp` & `gles_renderer.cpp`): entidades 3D desenhadas antes da água transparente; `glDepthMask(GL_FALSE)` na água evitando mascarar objetos do cenário.
- ✅ Terreno e Câmera de The Titans: relevo costeiro diagonal com baía (terra a NW, mar a SE); câmera isométrica cinematográfica (`yaw=-2.88`, `pitch=0.38`, `radius=16`).
- ✅ Bloqueio de Zoom da Câmera no Menu: `adjust_camera_zoom` desativado fora de `IN_GAME`.
- ✅ Resolvedor de Nomes de Texturas BRG: heurística de fallback para `* map.ddt` em animações.

## Em aberto / Resolvidos
1. ✅ **Logo e Botões de UI**: Decodificação 4x4 column-major 24B/bloco perfeitamente alinhada e nítida.
2. ✅ **Tropas, Casas e Árvores do Menu**: Removido guard clause de `GameplayWorld::render`.
3. ⬜ **Wire-up de Cliques XMB**: Implementação dos comandos de ação (`campaignStart`, `subModeEnter`).
4. ❓ **Unidades quebradas em pedaços** (BRG): clip/malha divergentes, só `meshes[0]`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11132207 gerado em `/sdcard/aom-native-v1.0.11132207.apk`.
2. Validar interatividade dos botões do menu pregame.
