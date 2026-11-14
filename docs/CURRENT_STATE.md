# CURRENT_STATE — baseado na versão 1.0.11140108 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha DecodeBt8As4444): linha a linha, 4 pixels por grupo de 6 bytes com inversão vertical (`out_y = mip.height - 1 - y`) e paleta RGB444. Logo Titans, pórtico e botões renderizam 100% opacos e na orientação correta.
- ✅ Suporte a Alpha 1-bit em Formato 3: decodificação linear A1R5G5B5 (`palette_offset_alpha1`).
- ✅ Desbloqueio de Renderização de Entidades (`gameplay.h`): removido bloqueio em `GameplayWorld::render`.
- ✅ Pipeline de Render 3D do Menu (`engine.cpp` & `gles_renderer.cpp`): entidades 3D antes da água transparente; `glDepthMask(GL_FALSE)` na água.
- ✅ Terreno e Câmera de The Titans: relevo costeiro diagonal com baía; câmera isométrica cinematográfica (`yaw=-2.88`, `pitch=0.38`, `radius=16`).

## Correções aplicadas (14/11/2026)
- ✅ **Mapeamento UV de Botões (`ui_layout.cpp`)**: Inversão do eixo V (`1.0 - w`, `1.0 - y`) em `apply_interactive` e nos ButtonSets, fazendo com que o fundo autêntico de jade e moldura dourada do `ui pregame button.ddt` seja renderizado atrás de todos os botões do menu.
- ✅ **Localização PT-BR dos Menus e Submenus (`engine_assets.cpp`)**: IDs oficiais do AoM (`19825`..`23603`) e textos literais traduzidos para Português (Aprender a Jogar, Campanha, Jogador Único, Multijogador, Opções, Mais, Sair, etc.).
- ✅ **Resolução Robusta de Strings UI (`include/ui/ui.h`)**: Suporte a múltiplos prefixos `$$ID$$`, trim de espaços em branco e fallback automático entre ID numérico e texto original.
- ✅ **Nomes de Cenários da Campanha Localizados (`campaign_select_system.cpp`)**: Chamada a `resolve_text` para traduzir e limpar IDs dos botões da lista de missões.
- ✅ **Sistema de Logging Unificado (`include/engine/logger.h`)**: timestamps `[HH:MM:SS.mmm]`, gravação simultânea no Logcat e `/sdcard/log.txt`.

## Em aberto / Resolvidos
1. ✅ **Orientação e Opacidade do Logo Titans e Pórtico**: Resolvido no decoder Formato 3.
2. ✅ **Molduras e Fundos dos Botões do Menu**: Resolvido via inversão de V nos UVs.
3. ✅ **Tradução e Nomes dos Menus**: Resolvido via mapeamento PT-BR de strings.
4. ✅ **Navegação de Submenus e Início de Cenários**: Operacional via `gadgetReal`/`gadgetUnReal` e `campaignStart`.

## Próximos passos (ordem)
1. ✅ APK v1.0.11140108 gerado em `/sdcard/aom-native-v1.0.11140108.apk`.
2. Instalar no aparelho e validar a renderização dos botões em jade e os textos em português.