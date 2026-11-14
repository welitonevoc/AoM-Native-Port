# CURRENT_STATE — baseado na versão 1.0.11132339 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha): decodificação autêntica de blocos 4x4 de 24 bytes (16B índices + 8B alpha 4-bit) em ordem column-major (`bx` exterior, `by` interior) com paleta RGB444 (`palette_offset_alpha4`). Fim definitivo de scanlines, listras e ruído no logo Titans, pórtico e botões jade.
- ✅ Suporte a Alpha 1-bit em Formato 3: decodificação linear A1R5G5B5 (`palette_offset_alpha1`).
- ✅ Desbloqueio de Renderização de Entidades (`gameplay.h`): removido bloqueio em `GameplayWorld::render` que abortava render de tropas, casas e árvores com malhas próprias.
- ✅ Pipeline de Render 3D do Menu (`engine.cpp` & `gles_renderer.cpp`): entidades 3D desenhadas antes da água transparente; `glDepthMask(GL_FALSE)` na água evitando mascarar objetos do cenário.
- ✅ Terreno e Câmera de The Titans: relevo costeiro diagonal com baía (terra a NW, mar a SE); câmera isométrica cinematográfica (`yaw=-2.88`, `pitch=0.38`, `radius=16`).
- ✅ Bloqueio de Zoom da Câmera no Menu: `adjust_camera_zoom` desativado fora de `IN_GAME`.
- ✅ Resolvedor de Nomes de Texturas BRG: heurística de fallback para `* map.ddt` em animações.

## Correções aplicadas (14/11/2026)
- ✅ **Sistema de Logging Unificado (`include/engine/logger.h`)**: timestamps `[HH:MM:SS.mmm]`, gravação simultânea no Logcat e `/sdcard/log.txt` com flush imediato.
- ✅ **Conexão dos Módulos Silenciosos ao `log.txt`**: `ddt_texture.cpp`, `ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp` e `bar_archive.cpp` gravam diagnósticos diretamente no arquivo.
- ✅ **Diagnósticos de DDT e UI no Log**: dumps de offsets de paleta, blocos, estatísticas de alpha e handles GL.
- ✅ **De-interleaving vertical para PALETTE alpha_bits=4**: campos verticais de 64px para texturas com altura múltipla de 64.
- ✅ **Layout fixo para Formato 5 (AOM_5BPP)** e **Scoring para Formato 6**.

## Em aberto / Resolvidos
1. ✅ **Sistema de Relatório e Logs**: Conectado e formatado em `/sdcard/log.txt`.
2. ❓ **Texturas de UI e Logo**: Analisar logs detalhados do novo APK para verificar decodificação exata.
3. ⬜ **Wire-up de Cliques XMB**: Implementação dos comandos de ação (`campaignStart`, `subModeEnter`).

## Próximos passos (ordem)
1. ✅ APK v1.0.11132339 gerado em `/sdcard/aom-native-v1.0.11132339.apk`.
2. Executar e inspecionar diagnósticos detalhados em `/sdcard/log.txt`.