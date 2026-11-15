# CURRENT_STATE — baseado na versão 1.0.11150831 (15/11/2026). Máx. ~60 linhas.
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

## Correções Aplicadas nesta Versão (Campanha XMB Autêntica + Multiplayer LAN)
- ✅ **engine_menus.cpp**: `create_campaign_menu()` usa `uicampaign.xmb` autêntico — CampaignSelection (scrollList), ScenarioList (list), DifficultySetting (radioset), mapa preview, titulo/descricao
- ✅ **engine_menus.cpp**: `create_campaign_selection_menu()` usa `uicampaignsetup.xmb` autêntico — ScenarioList (list), ScenarioImage (map preview), ScenarioDetails, DifficultySetting, Play/MainMenu buttons
- ✅ **engine_menus.cpp**: Fix mapa de cabeça para baixo — UV flip (`uv_min={0,1}`, `uv_max={1,0}`) no ScenarioImage e Campaign-Map-Section
- ✅ **engine_menus.cpp**: Texturas autenticas de borda (UI\border_a straight/corner) carregadas do XMB
- ✅ **engine_menus.cpp**: Populacao dinamica de campanhas (Fall of the Trident / New Atlantis) e cenarios desbloqueados
- ✅ **engine_multiplayer.cpp**: `create_multiplayer_lobby_menu()` reescrito com estilo XMB autentico
- ✅ **ui.cpp**: ScrollList clipping via `glScissor` — fix "grade marrom gigante"
- ✅ **renderer.h/.cpp**: Interface `set_ui_scissor(x,y,w,h,enable)` no GLES renderer
- ✅ **MyNativeActivity.java + native_activity.cpp**: JNI bridge teclado nativo Android
- ✅ APK v1.0.11150831 gerado em `/sdcard/aom-native-v1.0.11150831.apk`.

## Próximos passos (ordem)
1. Testar: Main Menu → Single Player → Campaign → Fall of the Trident → lista de cenarios com mapa preview correto
2. Testar: Selecionar cenario → mapa regional aparece direito (nao invertido), titulo/descricao atualizam
3. Testar: Multiplayer LAN → Host cria sala → Lobby estilo autentico (pedra/dourado)
4. Validar teclado nativo Android no Host Game dialog (Game Name editBox)