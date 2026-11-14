# CURRENT_STATE — baseado na versão 1.0.11140043 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha DecodeBt8As4444): linha a linha, 4 pixels por grupo de 6 bytes com inversão vertical (`out_y = mip.height - 1 - y`) e paleta RGB444. Logo Titans, pórtico e botões renderizam 100% opacos e na orientação correta (right-side up).
- ✅ Suporte a Alpha 1-bit em Formato 3: decodificação linear A1R5G5B5 (`palette_offset_alpha1`).
- ✅ Desbloqueio de Renderização de Entidades (`gameplay.h`): removido bloqueio em `GameplayWorld::render` que abortava render de tropas, casas e árvores com malhas próprias.
- ✅ Pipeline de Render 3D do Menu (`engine.cpp` & `gles_renderer.cpp`): entidades 3D desenhadas antes da água transparente; `glDepthMask(GL_FALSE)` na água evitando mascarar objetos do cenário.
- ✅ Terreno e Câmera de The Titans: relevo costeiro diagonal com baía (terra a NW, mar a SE); câmera isométrica cinematográfica (`yaw=-2.88`, `pitch=0.38`, `radius=16`).
- ✅ Bloqueio de Zoom da Câmera no Menu: `adjust_camera_zoom` desativado fora de `IN_GAME`.
- ✅ Resolvedor de Nomes de Texturas BRG: heurística de fallback para `* map.ddt` em animações.

## Correções aplicadas (14/11/2026)
- ✅ **Sistema de Logging Unificado (`include/engine/logger.h`)**: timestamps `[HH:MM:SS.mmm]`, gravação simultânea no Logcat e `/sdcard/log.txt` com flush imediato.
- ✅ **Conexão dos Módulos Silenciosos ao `log.txt`**: `ddt_texture.cpp`, `ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp` e `bar_archive.cpp` gravam diagnósticos diretamente no arquivo.
- ✅ **DecodeBt8As4444 em `ddt_texture.cpp`**: layout sequencial por linha com decodificação de nibbles 4444 e correção de orientação vertical (`out_y = h - 1 - y`).
- ✅ **Layout fixo para Formato 5 (AOM_5BPP)** e **Scoring para Formato 6**.

## Em aberto / Resolvidos
1. ✅ **Sistema de Relatório e Logs**: Conectado e formatado em `/sdcard/log.txt`.
2. ✅ **Decoder DDT Formato 3**: Orientação e opacidade corrigidas com sucesso.
3. ⬜ **Wire-up de Cliques XMB**: Implementação dos comandos de ação (`campaignStart`, `subModeEnter`).

## Próximos passos (ordem)
1. ✅ APK v1.0.11140043 gerado em `/sdcard/aom-native-v1.0.11140043.apk`.
2. Instalar no aparelho e validar o alinhamento visual final do menu.