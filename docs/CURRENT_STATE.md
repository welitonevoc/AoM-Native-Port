# CURRENT_STATE — baseado na versão 1.0.11151522 (15/11/2026). Máx. ~60 linhas.
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

## Correções Aplicadas nesta Versão (Campanha XMB Corrigida + Multiplayer LAN)
- ✅ **engine_menus.cpp**: `create_campaign_menu()` usa `uicampaign.xmb` — CampaignSelection (scrollList p/ campanhas), ScenarioList (list p/ cenários), DifficultySetting (radioset), Campaign-Map-Section (mapa preview, hidden→visible), Scenario-Title/Details
- ✅ **engine_menus.cpp**: `create_campaign_selection_menu()` usa `uicampaignsetup.xmb` — ScenarioList (list), ScenarioImage (background gadget p/ mapa, UV flip), ScenarioDetails/HelpDisplay (labels), Play/MainMenu buttons
- ✅ **engine_menus.cpp**: Fix mapa invertido — UV flip (`uv_min={0,1}`, `uv_max={1,0}`) em ScenarioImage e Campaign-Map-Section
- ✅ **engine_menus.cpp**: Campaign-Map-Section torna-se visível ao setar textura (era hidden="" no XMB)
- ✅ **engine_menus.cpp**: Tipos de widget corretos por XMB (scrollList/radioset em uicampaign, textField dropdown em uicampaignsetup)
- ✅ **engine_multiplayer.cpp**: texture_resolver corrigido (png_name computado localmente, sem std::filesystem)
- ✅ APK v1.0.11151142 gerado em `/sdcard/aom-native-v1.0.11151142.apk`.

## Correções de Multiplayer LAN (sessão 15/11/2026 — fix "caixa marrom congelada")
- ✅ **ui.cpp**: `UIManager::clear()` agora limpa `scroll_lists_`, `radio_sets_` e `lists_` (lista de jogos LAN "MPLanIP-LanGamesList" ficava viva e congelada ao trocar de tela; `hit_test_list` bloqueava toques na área)
- ✅ **ui.cpp**: `set_group_visible()` também atualiza `scroll_lists_`, `radio_sets_` e `lists_` por `tag`/`group_tag`
- ✅ **ui.cpp**: toque fora do teclado virtual chama `hide_virtual_keyboard()` (teclado não ficava mais preso na tela)
- ✅ **ui.h**: `resolve_text()` traduz `$$ID$$` também dentro de itens de scroll_lists/radio_sets/lists
- ✅ **engine_gameplay.cpp + gameplay.h**: unidades spawnadas em jogo recebem mesh/textura via `spawn_unit(..., mesh, texture)`; `attach_animation` stub morto removido (animações morph/GRN voltaram)
- ✅ **engine_gameplay.cpp**: mapa in-game com palmeiras/carvalhos/pinheiros/rochas (`world e/g/n ...`) além do menu
- ✅ **engine_gameplay.cpp**: fauna decorativa no mapa in-game (gazela, aurochs, doe, leão, javali) via `spawn_scenery_object` — cenário estático, sem HP/AI/interação (placeholder até sistema de caça real)
- ✅ APK v1.0.11151522 gerado em `/sdcard/aom-native-v1.0.11151522.apk`.

## Correções de Menus de Campanha (sessão 15/11/2026)
- ✅ **campaign_select_system.cpp**: nomes de dificuldade corrigidos → Easy/Moderate/Hard/Titan (era Facil/Moderado/Dificil/Tita)
- ✅ **engine_menus.cpp**: callback XMB `open_campaign` → `create_campaign_menu()` (uicampaign.xmb) e `open_single_player` → `create_single_player_setup_menu()` (era invertido)
- ✅ **engine_menus.cpp**: menu procedural fallback — CAMPAIGN → `create_campaign_menu()`, SINGLE PLAYER → `create_single_player_setup_menu()`
- ✅ **engine_menus.cpp**: `update_campaign_preview` / `update_campaign_setup_selection` — LOGE de textura falha/gadget não encontrado (item 3: validar layout do preview no aparelho)
- ✅ **campaign_select_system.cpp**: quad do mapa só é desenhado se a textura carregou (painel escuro caso falhe; antes virava quad branco)
- ✅ **engine_assets.cpp**: traduções $$ID$$ — 18297-18300 (Fácil/Moderado/Difícil/Titã), 18445, 18588, 18589, 18593, 18691, 20064, 20189 + fallbacks ingleses Easy/Moderate/Hard/Titan → PT-BR
- ✅ **engine_menus.cpp**: helper `resolve_loc_ids()` resolve `$$ID$$` dos nomes de campanha/cenário vindos do `main campaign.cpn`/`xpack campaign.cpx` (lista ScenarioList, combo CampaignSelection, título Scenario-Title)
- ✅ **engine_assets.cpp**: strings faltantes 20597-20617, 23702/23703-23714/23867, 20623 + fallbacks "Fall of the Trident"/"The New Atlantis" → PT-BR
- ⚠️ Achado (não corrigido): `get_campaign_briefing` hardcoded mapeja errado os `aomsp##` (aomsp04=Omens recebe texto de Troia); briefing real está no `<Scenario>` do cpn
- ⬜ Rebuild do APK e teste: Main Menu → Campanha → mapa preview, dificuldade, textos PT-BR, item "18300" no radioset

## Próximos passos (ordem)
1. Testar: Main Menu → Single Player → Campaign → Fall of the Trident → lista cenários, mapa preview certo
2. Testar: Selecionar cenário → mapa regional aparece direito, título/descrição atualizam
3. Testar: Multiplayer LAN → Host cria sala → Lobby estilo autêntico
4. Validar teclado nativo Android no Host Game dialog (Game Name editBox)