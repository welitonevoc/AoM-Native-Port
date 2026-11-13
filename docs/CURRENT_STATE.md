# CURRENT_STATE — baseado no commit c75aced (13/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Logger em arquivo no shared storage (`/sdcard/AoM/log.txt`) integrado em `engine_internal.h` e `native_activity.cpp`. Desativado vídeo com glitch no fundo de `MAIN_MENU` (`engine_gameplay.cpp`).
- ✅ BRG parser & assets: chunk `MTRL` atribui textura a TODOS os meshes (`brg_model.cpp`) e fallback `textflare -> cursors/cursor_flare.ddt` em `load_models()` (`engine_assets.cpp`).
- ✅ `engine.h` sem as duplicatas antigas (enum e métodos de menu).
- ✅ Resolvedor tolerante de nomes de textura (`texture_key`, logs `[TEX]`) em `load_ddt_texture`.
- ✅ DDT formatos 5 e 6: decodificação com escolha automática de layout (`ddt5_auto.h` e `ddt6_auto.h`).
- ✅ Toque: `interactive` + percurso reverso dos botões; `color == 0` não vira mais branco opaco.
- ✅ `recreate_renderer()` chama `invalidate_gpu_handles()`: zera todos os handles de GPU (UI, malhas, terreno, água) evitando colisão de texturas.
- ✅ `ui_layout.cpp`: lê `stateentry`, `hidden`, `statebutton`, `z`, `texturecoords` (UV atlas) e comandos nativos AoM (`gadgetReal`, `modeEnter`, `exit`); remove o prefixo `$$id$$` do texto.
- ✅ `MyNativeActivity.java`: copia todos os archives base e expansão (`textures2.bar`, `data2.bar`, `models2.bar`, `scenarios2.bar`, `sounds2.bar`, `anim2.bar`, `godpowers2.bar`).
- ✅ DDT formato 3 (PALETTE): corrigido layout RGB444 (12-bit) e validação de alpha a4 (`ddt_texture.cpp`). Texturas `ui_pregame` priorizam PNGs limpos de `titans_menu_parts/` fatiados com transparência alfa autêntica (`engine_menus.cpp`).
- ✅ LightingManager e arquivos `.lgt`: 11 presets de iluminação solar/ambiente integrados ao motor e empacotados no APK.
- ✅ Cenários `.scn`/`.scx`: suporte unificado e resolução flexível de caminhos no `scenarios.bar`/`scenarios2.bar` e disco.
- ✅ `ButtonSetManager` e `buttonsets.xmb`: decodificação e aplicação de texturas e UVs para botões (`buttonLong`, `checkBtn`, etc.) e empacotamento no APK.
- ✅ Modularização concluída (Blocos 1 a 10):
  - `src/engine/engine_assets.cpp` (1.096 linhas, 24 métodos de assets/texturas).
  - `src/engine/engine_multiplayer.cpp` (367 linhas de MP integradas e desduplicadas).
  - `src/engine/engine_options_menu.cpp` (opções/configuração de menus integradas).
  - `src/engine/engine_input.cpp` (238 linhas, `handle_touch` e `adjust_camera_zoom`).
  - `src/engine/engine_particles.cpp` (117 linhas, 7 métodos de spawn/update/render particles).
  - `src/engine/engine_save_load.cpp` (296 linhas, 12 métodos de save/load/slots).
  - `src/engine/engine_terrain.cpp` (606 linhas, 8 métodos de terreno, heightmap, água, skydome/skybox).
  - `src/engine/engine_campaign.cpp` (388 linhas, 10 métodos de campanha, missões e playback de cinemáticas).
  - `src/engine/engine_hud.cpp` (422 linhas, 4 métodos de HUD in-game e transformação de unidades).
  - `src/engine/engine_menus.cpp` (1.564 linhas, 104 métodos de menus procedurais e telas de UI).
  - `src/engine/engine_states.cpp` (533 linhas, máquina de estados do motor `set_state`).
  - `src/engine/engine_gameplay.cpp` (436 linhas, partida RTS `start_game`, entidades iniciais e background 3D).
  - `src/engine/engine.cpp` reduzido de 7.307 para 840 linhas (apenas Core: init/update/render/lifecycle). 100% dos testes e libaom_native passando.

## Em aberto / Resolvidos
1. ✅ **Decodificação DDT Formato 3**: Índices contínuos + buffer alpha 4-bit (sem row-interleave falso) corrigem as scanlines e restauram silhueta do pórtico e botões (`ddt_texture.cpp`).
2. ✅ **Botões Autênticos XMB**: `gd.bg_texture` e `state_backgrounds` sobrepõem textura padrão, aplicando textura autêntica `ui\ui pregame button` com coordenadas UV (`ui_layout.cpp`).
3. ✅ **Cenário 3D com Tropas Atlantes e Floresta**: `gameplay_world_` instanciado no `MAIN_MENU` executa `populate_menu_background_scene()`, renderizando tropas, árvores e casas na praia.
4. ✅ **Ondas e Espuma Dinâmica**: Shader de água atualizado com ondulação costeira e arrebentação de espuma na praia da ilha (`gles_renderer.cpp`).
5. ❓ **Unidades quebradas em pedaços** (BRG): clip/malha divergentes, só `meshes[0]`. Fase 0 de testes (A/B estático × animado) pendente.

## Próximos passos (ordem)
1. ✅ Menu autêntico com pórtico de mármore e botões autênticos no APK v1.0.11131921.
2. Validar tropas atlantes patrulhando a beira-mar e ausência de scanlines no pórtico.
3. Validar unidades BRG e animações esqueléticas (Fase 0 A/B).

