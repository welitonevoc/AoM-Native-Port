# CURRENT_STATE — baseado na versão 1.0.11150044 (15/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 & 5 (PALETTE 4-bit Alpha & 1-bit diffuse): Menus, botões e texturas sólidos.
- ✅ Mapeamento UV e Centralização PT-BR dos Menus (`ui_layout.cpp`, `ui.cpp`, `gles_renderer.cpp`).
- ✅ Shader de Água Estilizado (`gles_renderer.cpp`): Caustics `water0000.ddt` e espuma na costa.
- ✅ Seleção AoM (`gameplay.h`): Anel circular no chão e waypoints apenas na unidade selecionada.
- ✅ Unificação de Submeshes BRG (`brg_model.cpp` & `engine_assets.cpp`): Paredes e telhados combinados.
- ✅ Mapeamento Exaustivo dos BARs (`src/engine/generated_unit_visuals.inc`): 50+ classes mapeadas.
- ✅ `glDepthMask(GL_TRUE)` garantido no `begin_frame` antes de `glClear`.
- ✅ Terreno 128×128m (`kTerrainTileSize = 2.0f`) e Câmera RTS calibrada (`radius_ = 65.0f`, `pitch_ = 0.85f`).
- ✅ Orientação Suave / Heading Lerp (`ecs.h`) e suporte a clips Granny 2 / GRN (`grn_animation.cpp`).
- ✅ Menu de Campanha Autêntico em Duas Colunas com Mapas Regionais e Seletor de Dificuldade.
- ✅ Resolução Autêntica de Texturas MTRL em BRGs e Relevo Normalizado acima do Mar ($Y \ge 0$).

## Correções Aplicadas nesta Versão (Multiplayer LAN — ScrollList Clip + Native Keyboard)
- ✅ **ui.cpp**: ScrollList (dropdown) clipping via `glScissor` — fix "grade marrom gigante" do NumberPlayers vazando pela tela
- ✅ **renderer.h/.cpp**: Nova interface `set_ui_scissor(x,y,w,h,enable)` implementada no GLES renderer
- ✅ **MyNativeActivity.java + native_activity.cpp**: JNI bridge para teclado nativo Android — `showSoftKeyboard()`/`hideSoftKeyboard()` via `InputMethodManager`
- ✅ **engine.cpp**: EditBox focus callback agora chama JNI nativo em vez de VKB customizado
- ✅ **engine.cpp**: Removido overlay modal escuro (0xB0110802) para `MULTIPLAYER_LAN`/`MULTIPLAYER_LOBBY`
- ✅ **engine_multiplayer.cpp**: `clear_ui()` no `show_host_game_dialog()` e `create_multiplayer_lan_menu()` — fix z-order
- ✅ **ui.cpp**: `UIManager::clear()` esconde `virtual_keyboard_` + blur automático (clicar fora esconde)
- ✅ **ui.h/.cpp**: EditBox IME fields (`is_edit_box`, `onTextInput`, `max_length`)
- ✅ **native_activity.cpp**: Hardware keyboard (A-Z, 0-9, space, backspace, punctuation) via `AINPUT_EVENT_TYPE_KEY`
- ✅ **uihostgamedialog.xmb**: Game Name (editBox + teclado nativo), Max Players, Public/Private
- ✅ **uiprofileselect.xmb**: Profile list + New/Delete/OK/Exit
- ✅ APK v1.0.11150044 gerado em `/sdcard/aom-native-v1.0.11150044.apk`.

## Próximos passos (ordem)
1. Testar: Host cria sala Pública → aparece na lista LAN → Cliente Join → Lobby
2. Testar: Host cria sala Privada → NÃO aparece na lista → Cliente só entra via Invite
3. Testar fluxo completo Room Invite/Join Request + Session Connect
4. Refinar lobby multiplayer com layout XMB autêntico.