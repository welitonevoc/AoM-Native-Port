# P5 — MVP Completo / Vertical Slice (Partida Jogável Início→Fim)

**Objetivo:** Entregar uma partida local mínima que começa, permite jogar (economia, construção, combate) e termina corretamente com condição de vitória, atendendo ao escopo do MVP definido em `docs/MVP.md` e à "Ação 3 — vertical slice" de `docs/Processo de evolução.md`.

**Dependência:** P0 (Build Recovery), P1 (Gameplay Core), P2 (Cena RTS e Recursos), P3 (Animação/Partículas/Efeitos Visuais) e P4 (Polish e Otimização) concluídos.

**Responsáveis:** Gameplay Designer, Gameplay Programmer, UI Programmer, Systems Designer, QA Tester  
**Agentes do Claude Code Game Studio:** gameplay-programmer, ui-programmer, systems-designer, qa-tester, game-designer

---

## Estado Atual (Auditoria da Pasta de Testes e Documentação)

### Testes existentes (`src/tests/` — 23 arquivos)

| Arquivo | O que cobre | Status |
|---------|-------------|--------|
| `gameplay_test.cpp` | Combate, movimento, navegação A*, construção/demolição, pegada de recursos | ✅ |
| `test_ai_system.cpp` | Rule engine/XS scheduler, personalidades, idades, construção de edifícios, coleta, combate | ✅ |
| `test_perf.cpp` (P4.G4) | FPS + memória com sistemas reais (ECS + partículas) | ✅ |
| `test_grn_animation.cpp` | Animação esquelética GRN | ✅ |
| `test_particles_anim.cpp` | Partículas .prt + animação | ✅ |
| `test_prt_particle.cpp` | Sistema de partículas .prt | ✅ |
| `test_campaign_mission.cpp` | Missões de campanha | ✅ |
| `test_godpowers_scenarios.cpp` | Poderes divinos e cenários | ✅ |
| `test_ui_layout.cpp` | Layout de UI | ✅ |
| `ui_test.cpp` | UI (botões, labels, hit-test) | ✅ |
| `test_anim_video.cpp` | Animação de vídeo | ✅ |
| `test_audio_3d.cpp` | Áudio 3D/OpenSL | ✅ |
| `test_bar_overlay.cpp` | Archives BAR | ✅ |
| `test_terrain_water.cpp` | Terreno e água | ✅ |
| `test_water_bodies.cpp` | Corpos d'água | ✅ |
| `test_ddt_reference.cpp` | Texturas DDT | ✅ |
| `test_fonts_gfx.cpp` | Fontes e gráficos | ✅ |
| `test_localization_lcids.cpp` | Localização/LCIDs | ✅ |
| `test_network.cpp` | Rede (infraestrutura básica) | ✅ |
| `test_data_docs.cpp` | Dados/documentação | ✅ |
| `test_data_integration.cpp` | Integração de dados | ✅ |
| `test_camera_track.cpp` | Câmera/trilha | ✅ |
| `test_fonts_gfx.cpp` | Fontes | ✅ |

### Lacunas do MVP (fonte: `docs/ANALISE_COMPLETA.md` + `docs/MVP.md`)

| Área | O que falta para o MVP completo |
|------|---------------------------------|
| **HUD** | População, minimapa, painel de comandos, fila de produção (tooltip/drag-and-drop ausentes) |
| **Construção** | Town Center ainda não implementado (só House, Barracks, Tower) |
| **Seleção** | Seleção de grupo (box select) ainda a concluir; TouchInputManager não integrado como entrada geral |
| **Save/Load** | Experimental: persiste unidades/posições, não restaura todos recursos/estados |
| **Vitória** | Condição de vitória/derrota pendente |
| **Economia** | Só 4 recursos básicos; sem farms, minas, Favor desativado |
| **Áudio** | MP3/Ogg não consumidos; sound banks (320 XMB + 319 XML) não consumidos; áudio 3D ausente; múltiplos SFX simultâneos cortam |
| **UI** | `uimain.xmb` / `commandpanel.xmb` não carregados de XMB; menus hard-coded; strings hardcoded em português |

---

## Plano Detalhado por Story

### Story P5.1 — HUD Completo (População, Minimapa, Painel de Comandos, Fila de Produção)

**Descrição:** Completar o HUD do jogo com contador de população, minimapa funcional, painel de comandos contextual e fila de produção de unidades.

**Agente responsável:** UI Programmer

**Tarefas:**
1. Adicionar contador de população (pop atual / pop cap) ao HUD topo
2. Implementar minimapa com blips de unidades e edifícios (já tem `get_minimap_blips()` — conectar ao render)
3. Painel de comandos: botões contextuais conforme seleção (mover, atacar, construir, coletar)
4. Fila de produção visual em edifícios (Barracks/Town Center) com progress bar
5. Tooltips nos botões de comando

**Critério de Aceitação:**
- HUD mostra população (atual/cap) e recursos (Food/Wood/Gold/Favor)
- Minimapa renderiza blips de unidades amigas e inimigas
- Painel de comandos aparece ao selecionar unidade/edifício
- Fila de produção mostra progresso no edifício selecionado

---

### Story P5.2 — Town Center e Seleção de Grupo ✅ CONCLUÍDA

**Descrição:** Implementar o Town Center (edifício central da facção) e concluir a seleção de grupo (box select/drag).

**Agente responsável:** Gameplay Programmer

**Tarefas:**
1. ✅ `place_town_center()` já existia (TCs iniciais friendly/enemy em start_game) — fornece +15 pop cada e ponto de retorno de recursos
2. ✅ TC como fonte de população (`population_cap`: 10 base + tc_count*15 + house_count*10) e ponto de retorno de recursos (`get_nearest_town_center`) — já existiam
3. ✅ `TouchInputManager`/box select concluído (DOWN/MOVE/UP handling, `box_selecting_`, `box_start_`, `box_current_`)
4. ✅ Seleção múltipla com arrasto (`select_box()`) com anéis de seleção visuais no chão — já existiam
5. ✅ Deselect ao clicar no vazio — já existia
6. ✅ Adicionado `TOWN_CENTER` ao `PlacementMode` enum
7. ✅ Botão "Centro (400M 200O)" no HUD para trabalhadores (btn4)
8. ✅ Handling de placement de TC no touch UP (deduz 400M 200O, constrói via `place_building` ou `order_worker_build`)
9. ✅ Contagem de selecionados no HUD ("N selecionadas" quando multi-seleção)

**Critério de Aceitação:**
- ✅ Town Center é construível pelo jogador via HUD e funciona como depósito de recursos
- ✅ Box select seleciona múltiplas unidades ao arrastar
- ✅ Anéis de seleção aparecem em unidades selecionadas
- ✅ Clique no vazio limpa seleção
- ✅ Contagem de multi-seleção exibida no HUD

---

### Story P5.3 — Save/Load Completo e Condição de Vitória

**Descrição:** Completar save/load (restaurar todos recursos e estados) e implementar condição de vitória (Conquest / Sudden Death).

**Agente responsável:** Systems Designer, Gameplay Programmer

**Tarefas:**
1. Save/load v2: restaurar recursos, populações, estados de unidades/edifícios, waypoints
2. Condição de vitória Conquest: eliminar todas as unidades/inimigos
3. Condição de vitória Sudden Death: destruir o Town Center inimigo
4. Tela de vitória/derrota com botão "MENU PRINCIPAL" (já feito em P4.G5 — integrar)
5. Reset de estado ao voltar ao menu (já feito em P4.G5)

**Critério de Aceitação:**
- Carregar save restaura partida com recursos, posições e estados corretos
- Vitória por Conquest (sem inimigos) aciona tela de vitória
- Vitória por Sudden Death (TC inimigo destruído) aciona tela de vitória
- Transição vitória → menu limpa estado (sem resíduos)

---

### Story P5.4 — IA Econômica e Militar Básica

**Descrição:** IA simples para o jogador inimigo: treinar unidades, coletar recursos, construir e atacar.

**Agente responsável:** Systems Designer, Gameplay Programmer

**Tarefas:**
1. IA econômica: vilões inimigos coletam recursos e devolvem ao TC
2. IA militar: treinar Hoplites/Toxotes e atacar unidades do jogador
3. IA de construção: construir House/Barracks quando necessário
4. Integração com `AiSystem` (rule engine/XS scheduler já existente)

**Critério de Aceitação:**
- Inimigo treina unidades e coleta recursos sozinho
- Inimigo ataca unidades/jogador quando detecta alvo
- Partida contra IA é jogável do início ao fim

---

### Story P5.5 — Áudio Ampliado (MP3, Sound Banks, 3D, Múltiplos SFX)

**Descrição:** Corrigir áudio de menu (MP3), consumir sound banks dos XMB, áudio posicional 3D e pool de SFX simultâneos.

**Agente responsável:** Audio Programmer (sound-designer)

**Tarefas:**
1. Suporte a MP3/Ogg para música de menu (atual provavelmente falha lendo como WAV)
2. Consumir sound banks (320 XMB + 319 XML em sounds.bar) para mapear WAVs por evento/unidade
3. Áudio posicional 3D (OpenAL-soft ou OpenSL ES 3D)
4. Pool de players SFX para múltiplos sons simultâneos sem cortes
5. Controle de volume por categoria (master/music/sfx)

**Critério de Aceitação:**
- Música de menu (MP3) toca corretamente
- SFX disparados por eventos de unidade/construção/combate via sound bank
- Áudio posicional 3D funciona (distância/atualização do listener)
- Múltiplos SFX simultâneos não se cortam

---

### Story P5.6 — UI de XMB (uimain.xmb, commandpanel.xmb) e Localização

**Descrição:** Carregar menus e painéis de comando a partir de XMB (`uimain.xmb`, `commandpanel.xmb`) em vez de hard-code, e preparar strings para localização futura.

**Agente responsável:** UI Programmer, Localization Lead

**Tarefas:**
1. Carregar `uimain.xmb` para montar o menu principal dinamicamente
2. Carregar `commandpanel.xmb` para o painel de comandos do jogo
3. Extrair strings hardcoded para tabela de strings (base para localização)
4. Manter fallback para menus hard-coded se XMB não estiver disponível

**Critério de Aceitação:**
- Menu principal pode ser montado a partir de `uimain.xmb` (com fallback)
- Painel de comandos usa `commandpanel.xmb` quando disponível
- Strings da UI extraídas para tabela (não hardcoded em lógica de jogo)

---

## Gate Check P5→Release (MVP Completo)

Antes de avançar para release beta, todos os itens abaixo devem ser confirmados:

- [ ] HUD completo: população, minimapa, painel de comandos, fila de produção
- [ ] Town Center implementado e funcional
- [ ] Seleção de grupo (box select) funcionando
- [ ] Save/load restaura partida completa (recursos + estados)
- [ ] Vitória Conquest e Sudden Death implementadas
- [ ] IA inimiga joga sozinha (economia + militares)
- [ ] Áudio MP3 + sound banks + 3D + pool de SFX funcionando
- [ ] UI de XMB (uimain/commandpanel) com fallback
- [ ] Todos os testes continuam passando (exit 0)
- [ ] Build-m2 compila sem erros
- [ ] Partida completa (spawn → economia → combate → vitória) em < 10 min
- [ ] 60 FPS estáveis e sem OOM em 30 min (revalidar com novo conteúdo)

---

## Arquivos a Modificar (P5)

| Área | Arquivos prováveis |
|------|-------------------|
| HUD | `src/engine/engine.cpp` (render HUD), `include/engine/engine.h` |
| Town Center | `include/ecs/gameplay.h` (`place_town_center`), `src/engine/engine.cpp` (`start_game`) |
| Seleção de grupo | `src/platform/touch_input.cpp`, `include/ecs/gameplay.h` (`selections_`) |
| Save/Load | `src/game/save_load.cpp`, `include/game/save_load.h` |
| Vitória | `src/game/victory.cpp`, `include/game/victory.h` |
| IA | `src/ai/ai_system.cpp`, `include/ai/ai_system.h` |
| Áudio | `src/audio/opensl_engine.cpp`, `include/audio/audio.h` |
| UI XMB | `src/ui/ui_from_xmb.cpp`, `src/ui/ui.cpp` |

---

## Testes a Adicionar (P5)

| Novo teste | O que valida |
|------------|--------------|
| `test_hud_complete.cpp` | População, minimapa, painel de comandos, fila de produção |
| `test_town_center.cpp` | Town Center construível, depósito de recursos, população |
| `test_selection_group.cpp` | Box select, múltipla seleção, anéis, deselect |
| `test_save_load_complete.cpp` | Save/restaura recursos e estados completos |
| `test_victory_conditions.cpp` | Conquest e Sudden Death disparam telas corretas |
| `test_ai_basic.cpp` | IA inimiga treina, coleta, constrói e ataca |
| `test_audio_mp3.cpp` | Música MP3 toca; sound banks consomem XMB/XML |

---

**Nota:** O progresso só deve ser marcado como concluído após existir no código, compilar no build-m2, passar nos testes e ser validado em dispositivo Android (quando disponível).
