# CURRENT_STATE — baseado na versão 1.0.11132108 (13/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Logger em arquivo no shared storage (`/sdcard/AoM/log.txt` e `/sdcard/log.txt`) integrado. Desativado vídeo com glitch no fundo de `MAIN_MENU`.
- ✅ BRG parser & assets: chunk `MTRL` atribui textura a TODOS os meshes (`brg_model.cpp`) e fallback `textflare -> cursors/cursor_flare.ddt` em `load_models()`.
- ✅ `engine.h` sem as duplicatas antigas (enum e métodos de menu).
- ✅ Resolvedor tolerante de nomes de textura (`texture_key`, logs `[TEX]`) e heurística `* map.ddt` para clips de animação.
- ✅ DDT formato 3 (PALETTE): corrigido layout 16-bit RGB565/RGB555 (`ddt_texture.cpp`) eliminando artefatos esverdeados e faixas.
- ✅ Submenus XMB Dinâmicos: botões herdam `group_tag = parent_tag`; `set_group_visible` alterna grupos do pórtico (`LearnSubMenu`, `CampaignSubMenu`...).
- ✅ Menu de Opções Restaurado: removido interceptador `uisoundsetdlg.xmb` que quebrava as opções após carregar o `data.bar`; suportadas abas AUDIO, VIDEO, GAMEPLAY, CONTROLS sobre o fundo 3D sem overlay de PNG estático.
- ✅ Cenário 3D & Praia do Menu: relevo com praia costeira e areia dourada na beira-mar; câmera cinematográfica de The Titans (`radius=18`, `pitch=0.35`, `yaw=0.45`); zoom bloqueado fora do jogo.
- ✅ Modelos BRG On-Demand: `get_model_entry` carrega qualquer modelo sob demanda de `models.bar`/`models2.bar` com resolução automática de texturas (`.ddt`, `map.ddt`).
- ✅ Entidades e Cenário Autênticos no Menu: Manors e Quartel Atlantes com malhas reais, palmeiras, carvalhos, rochas e tropas Murmillo e Arcus patrulhando na praia.
- ✅ LightingManager e arquivos `.lgt`: 11 presets de iluminação solar/ambiente integrados ao motor e empacotados no APK.
- ✅ Cenários `.scn`/`.scx`: suporte unificado e resolução flexível de caminhos no `scenarios.bar`/`scenarios2.bar` e disco.
- ✅ `ButtonSetManager` e `buttonsets.xmb`: decodificação e aplicação de texturas e UVs para botões e empacotamento no APK.
- ✅ Modularização concluída (Blocos 1 a 10): `src/engine/engine.cpp` reduzido de 7.307 para 840 linhas (apenas Core). 100% dos testes e libaom_native passando.

## Em aberto / Resolvidos
1. ✅ **Zoom do Menu**: Bloqueado em `adjust_camera_zoom` fora do modo `IN_GAME`.
2. ✅ **Câmera do Menu**: Enquadramento cinematográfico raso voltado para a praia de Atlântida.
3. ✅ **Praia e Linha Costeira**: Formação de praia de areia descendo até o mar e leito marinho com ondas.
4. ✅ **Tropas, Casas e Árvores do Menu**: Modelos reais Atlantes (Manors, Quartel, Palmeiras, Murmillos, Arcus) renderizados com malhas e texturas corretas.
5. ❓ **Unidades quebradas em pedaços** (BRG): clip/malha divergentes, só `meshes[0]`. Fase 0 de testes pendente.

## Próximos passos (ordem)
1. ✅ APK v1.0.11132108 gerado em `/sdcard/aom-native-v1.0.11132108.apk`.
2. Validar visual da praia, tropas e casas no menu em dispositivo real.
3. Validar unidades BRG e animações esqueléticas (Fase 0 A/B).

