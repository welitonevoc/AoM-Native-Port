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
- ✅ DDT formato 3 (PALETTE 256 cores): corrigido layout RGB555 e offset alpha 4-bit (`ddt_texture.cpp`).
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

## Em aberto
1. ❓ **Dois sistemas de menu** coexistem: XMB (`ui_layout.cpp`), `src/ui/menu_system.cpp` (novo) e os `create_*_menu` à mão. Definir qual manda no MAIN_MENU.
3. ❓ **Unidades quebradas em pedaços** (BRG): clip/malha divergentes, só `meshes[0]`. Fase 0 de testes (A/B estático × animado) pendente.
4. ❓ **Tela cheia de água na cinemática de missão**: câmera da cinemática (`CampaignMissionManager`/`CameraTrack`). `CameraTrack::update` sobrescreve aspect/near/far.
5. ✅ **Dados**: corrigido `resolve_game_data_root` em `native_activity.cpp` para priorizar o armazenamento interno do app. Copiados todos os `.bar` (data, textures, models, scenarios, sounds, anim, godpowers) para `/storage/emulated/0/Android/data/com.aom.nativeport/files/AoM/`. Logcat confirma: `Loaded archives: data=137, textures=6782, models=11496, scenarios=99, sounds=2468, anim=1068, godpowers=71 (valid=1)`. Todos os `.bar` abertos com sucesso (`open -> ok`, `mount expansao -> ok`). Sem falhas de permissão.
   - Melhoria: `open_pair` agora loga `errno` e `strerror` quando `open`/`mount` falham, permitindo distinguir EACCES de arquivo corrompido.

## Riscos do repositório
- ⚠ `assets_bar/textures/textures.bar` (85 MB) e `textures2.bar` (34 MB) estão no GitHub público. São dados do jogo comercial:
  possível violação de direitos autorais e repo pesado. Tirar do Git (e do histórico) e usar `.gitignore`.
- Raiz poluída: `PLANO_P0..P5.md`, `PROGRESSO.md`, `fix_engine.py`, `offtec_menus.py`... Mover para `docs/archive/` e `tools/`.

## Próximos passos (ordem)
1. ✅ Menu XMB oficial com texturas PNG de titans_menu_parts e centralização widescreen concluídos.
2. Validar unidades BRG e animações esqueléticas (Fase 0 A/B).
