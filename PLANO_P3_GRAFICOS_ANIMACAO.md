# P3 — Animação, Partículas e Efeitos Visuais

**Objetivo:** Conectar a animação esquelética GRN (skeletal playback), completar o sistema de partículas .prt e adicionar efeitos visuais de combate/construção para o MVP jogável.

**Dependência:** P0 (Build Recovery), P1 (Gameplay Core) e P2 (Cena RTS e Recursos) devem estar concluídos.

**Responsável:** Graphics Programmer, Gameplay Programmer  
**Agentes do Claude Code Game Studio:** graphics-programmer, gameplay-programmer, technical-artist, qa-tester

---

## Estado Atual (Auditoria do Código)

### O que já existe:

| Componente | Arquivo | Status |
|------------|---------|--------|
| Parser .grn (heurístico) | `src/anim/grn_animation.cpp` | ✅ Implementado (parse defensivo) |
| GrnPlayer (interpolação de ossos) | `src/anim/grn_animation.cpp` | ✅ Implementado |
| GrnAnimationComponent | `include/ecs/gameplay.h` | ✅ Definido |
| attach_grn_animation() | `include/ecs/gameplay.h` | ✅ Implementado (cria mesh + player) |
| render_grn_animated() | `include/ecs/gameplay.h` | ✅ Definido (chama draw_mesh_skinned) |
| draw_mesh_skinned | `include/renderer/renderer.h` | ✅ Interface definida |
| BRG animation (vertex morph) | `src/anim/animation_system.cpp` | ✅ Implementado e funcionando |
| Partículas simples | `src/engine/engine.cpp` (Particle) | ✅ Implementado |
| PrtParticleSystem declaração | `include/engine/engine.h` | ✅ Declarado (P3.5) |

### O que falta (gap P3):

| Gap | Detalhe | Prioridade |
|-----|---------|------------|
| **G1** | **Animação GRN playback**: ✅ CONCLUÍDO — load_grn_clips(), update_animations() atualiza bone_matrices, render_grn_animated() no render loop | Alta |
| **G2** | **Sistema de partículas .prt**: ✅ CONCLUÍDO — update() e render() no loop de jogo, carregamento via load_prt_particles() | Alta |
| **G3** | **Efeitos visuais de combate/construção**: ✅ CONCLUÍDO — callbacks conectados, burst_emit de partículas | Média |
| **G4** | **Animação de estado das unidades**: ⚠️ Parcial — update_animations() determina desired state; GRN usa tempo da animação BRG ou 0 (pose bind) | Média |

---

## Plano Detalhado por Story

### Story P3.1 — Animação GRN Playback (Skeletal)

**Descrição:** Conectar a animação esquelética GRN completa: carregar animações .grn do anim_bar_, atualizar bone_matrices cada frame, e renderizar via draw_mesh_skinned no render loop.

**Agente responsável:** Graphics Programmer

**Tarefas:**
1. Adicionar `load_grn_clips()` no Engine para carregar animações .grn do anim_bar_ (análogo a load_animation_clips() para BRG)
2. Chamar `attach_grn_animation()` em `start_game()`/`start_campaign_mission()` para unidades que tenham animação .grn
3. Em `GameplayWorld::update_animations()`, atualizar `gac.player->evaluate(t, gac.bone_matrices)` para cada entidade com GrnAnimationComponent
4. Chamar `gameplay_world_->render_grn_animated(renderer_.get())` no render loop IN_GAME (além de render_animated)
5. Acionar estados de animação GRN (walk/idle/attack/death) com base nas ordens da unidade

**Critério de Aceitação:**
- Animações .grn são carregadas do anim_bar_ sem erro
- Entidades com GrnAnimationComponent têm bone_matrices atualizadas cada frame
- render_grn_animated é chamado no render loop IN_GAME
- Unidades exibem animação esquelética (não apenas estática)

---

### Story P3.2 — Sistema de Partículas .prt

**Descrição:** Integrar o PrtParticleSystem (.prt) no loop de jogo: carregar partículas .prt, spawnar em eventos (combate, construção, colheita, morte) e renderizar cada frame.

**Agente responsável:** Graphics Programmer

**Tarefas:**
1. Carregar arquivos .prt do anim_bar_ (ou particles.bar) via load_prt_particles()
2. Implementar spawn_prt_particle() para criar partículas .prt em posição/velocidade
3. Chamar prt_particles_->update(dt) no update loop IN_GAME
4. Chamar prt_particles_->render() no render loop IN_GAME
5. Conectar spawn de partículas a eventos: spawn_combat_hit, spawn_construction_sparks, spawn_death_smoke, spawn_harvest_chips

**Critério de Aceitação:**
- Partículas .prt são carregadas e atualizadas cada frame
- Partículas são renderizadas no loop IN_GAME
- Eventos de jogo disparam spawn de partículas .prt

---

### Story P3.3 — Efeitos Visuais de Combate e Construção

**Descrição:** Conectar efeitos visuais (partículas, flashes, linhas de ataque) aos eventos de combate e construção para feedback visual do jogador.

**Agente responsável:** Gameplay Programmer, Technical Artist

**Tarefas:**
1. Conectar spawn_combat_hit() quando uma unidade ataca outra
2. Conectar spawn_construction_sparks() quando um edifício está sendo construído
3. Conectar spawn_death_smoke() quando uma unidade morre (remove_entity)
4. Conectar spawn_harvest_chips() quando um villager coleta recursos
5. Adicionar linhas de ataque (debug lines) já existentes — manter funcional

**Critério de Aceitação:**
- Partículas de combate aparecem ao atacar
- Faíscas de construção aparecem ao construir
- Fumaça de morte aparece ao destruir unidade
- Chips de colheita aparecem ao coletar recurso

---

## Critério de Aceitação P3→P4 (todos confirmados no código):

- [x] Animação GRN playback funcionando (bone_matrices atualizadas, render_grn_animated chamado)
- [x] Sistema de partículas .prt integrado no loop de jogo (update + render)
- [x] Efeitos visuais de combate/construção conectados a eventos (on_combat_hit, on_building_sparks, on_building_completed, on_unit_death, on_resource_harvested)
- [x] Todos os testes continuam passando (exit 0)
- [x] Build-m2 compila sem erros

---

## Arquivos a Modificar (P3)

| Arquivo | Modificação |
|---------|-------------|
| `src/engine/engine.cpp` | Adicionar load_grn_clips(); chamar attach_grn_animation() em start_game; chamar render_grn_animated() no render loop; integrar prt_particles_ no update/render |
| `include/ecs/gameplay.h` | Atualizar update_animations() para atualizar bone_matrices do GrnPlayer; conectar spawn de partículas a eventos de combate/construção/morte |
| `include/engine/engine.h` | Adicionar declarações de load_grn_clips(), spawn_prt_particle() |

---

**Nota:** O progresso só deve ser marcado como concluído após existir no código e passar por compilação/teste no dispositivo Android (P0, P1 e P2 devem estar concluídos primeiro).
