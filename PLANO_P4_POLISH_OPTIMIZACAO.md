# P4 — Polish e Otimização (Performance e Memória)

**Objetivo:** Validar e otimizar para 60 FPS estáveis e sem OOM em 30 min de jogo no dispositivo Android alvo, além de polir o MVP jogável.

**Dependência:** P0 (Build Recovery), P1 (Gameplay Core), P2 (Cena RTS e Recursos) e P3 (Animação, Partículas e Efeitos Visuais) concluídos.

**Responsável:** Performance Analyst, Technical Artist, QA Tester  
**Agentes do Claude Code Game Studio:** performance-analyst, technical-artist, qa-tester, engine-programmer

---

## Estado Atual (Auditoria do Código)

### O que já existe:

| Componente | Arquivo | Status |
|------------|---------|--------|
| Terreno heightmap real | `src/engine/engine.cpp` | ✅ Implementado |
| Save/Load v2 binário | `src/game/save_load.cpp` | ✅ Implementado |
| Vitória Conquest/Sudden Death | `src/game/victory.cpp` | ✅ Implementado |
| Skybox / Cube map .cub | `src/assets/cube_map.cpp` | ✅ Implementado |
| Animação GRN playback | `include/ecs/gameplay.h`, `src/engine/engine.cpp` | ✅ Implementado (P3) |
| Partículas .prt | `src/engine/engine.cpp` | ✅ Implementado (P3) |
| Efeitos visuais de combate/construção | `src/engine/engine.cpp` callbacks | ✅ Implementado (P3) |
| HUD com Pop/Cap | `src/engine/engine.cpp` | ✅ Implementado |
| Seleção de grupo (box select) | `src/engine/engine.cpp` | ✅ Implementado |

### O que falta (gap P4):

| Gap | Detalhe | Prioridade |
|-----|---------|------------|
| **G1** | **Otimização de lookups de ECS no update loop**: ✅ CONCLUÍDO — `update_orders()` faz 1 busca de transform por entidade por frame (era 3-4); `gather_resources()` usa referência mutável única; `update_animations()` busca transform uma única vez | Alta |
| **G2** | **Culling de frustum / distância**: ✅ CONCLUÍDO — `render()`, `render_animated()` e `render_grn_animated()` recebem `const Camera*` opcional e descartam entidades fora do frustum (6 planos) ou além de 60 unidades de distância. Os planos do frustum são calculados 1 vez/frame (não por entidade). Chamadas no engine passam `&camera_`; com camera == nullptr não há culling (testes inalterados). | Média |
| **G3** | **Pool de partículas**: ✅ CONCLUÍDO — `particles_` pré-alocado com 2000 slots; `spawn_particle()` reutiliza slot inativo; `update_particles()` marca como inativo sem `erase`/`remove_if`; sem realocações durante o jogo | Média |
| **G4** | **Validação 60 FPS + OOM**: ⏳ Pendente validação em dispositivo Android real | Alta |
| **G5** | **Polish de transição estado jogo → menu**: ✅ CONCLUÍDO — `return_to_main_menu()` reseta `skirmish_victory_result_` e `victory_display_timer_` | Média |

---

## Plano Detalhado por Story

### Story P4.1 — Otimização de Lookups de ECS (update loop)

**Descrição:** Reduzir o número de buscas `.find()` no `unordered_map` de transforms por entidade por frame. Cada busca é O(1) média mas com constante alto (hash + comparação); fazer 3-4 buscas por entidade por frame multiplicado por 50+ entidades é desperdício.

**Agente responsável:** Engine Programmer

**Tarefas:**
1. Em `GameplayWorld::update_orders()`: fazer uma única busca de transform da entidade no início da iteração e reutilizar o ponteiro/referência
2. Em `GameplayWorld::gather_resources()`: mesma otimização
3. Em `GameplayWorld::update_animations()`: mesma otimização para transform e health
4. Em sistemas de combate (CombatSystem): verificar se há buscas duplicadas de transforms

**Critério de Aceitação:**
- Cada entidade no update loop faz no máximo 1 busca de transform por frame
- Build compila sem erros
- Todos os 20 testes continuam passando

---

### Story P4.2 — Culling por distância / frustum

**Descrição:** Não atualizar nem renderizar entidades muito distantes da câmera ou fora do frustum de visão, reduzindo trabalho por frame.

**Agente responsável:** Graphics Programmer

**Tarefas:**
1. Adicionar distância máxima de atualização (update) para unidades inimigas não visíveis
2. Adicionar culling simples de renderização por distância da câmera
3. Manter atualização completa para unidades amigáveis e edifícios do jogador

**Critério de Aceitação:**
- [x] Renderização culla entidades fora do frustum (frustum + distância, 1 cálculo de planos/frame)
- [x] Culling desativado quando camera == nullptr (testes e chamadas sem câmera continuam funcionando)
- [x] Unidades amigas e edifícios continuam sendo atualizados integralmente (sem culling de update que afete gameplay)

---

### Story P4.3 — Pool de partículas simples

**Descrição:** Substituir `std::vector<Particle> particles_` push_back por um pool fixo com reaproveitamento de slots inativos, evitando realocações e fragmentação.

**Agente responsável:** Technical Artist

**Tarefas:**
1. Converter `particles_` para um array/pool fixo com flag `active`
2. `spawn_particle()` reutiliza slot inativo em vez de push_back
3. `update_particles()` itera apenas slots ativos
4. `render_particles()` renderiza apenas slots ativos

**Critério de Aceitação:**
- Nenhuma realocação de `particles_` durante o jogo
- Partículas inativas são reaproveitadas
- Comportamento visual idêntico ao anterior

---

### Story P4.4 — Validação de Performance e Memória (dispositivo Android)

**Descrição:** Validar 60 FPS estáveis e sem OOM em 30 min no dispositivo alvo.

**Agente responsável:** QA Tester, Performance Analyst

**Tarefas:**
1. Medir FPS em dispositivo alvo (Snapdragon 6xx ou similar)
2. Medir consumo de memória (RAM) durante 30 min de jogo contínuo
3. Verificar ausência de vazamento de memória (file descriptors, texturas, VBOs)
4. Relatório de performance com recomendações de otimização

**Critério de Aceitação:**
- 60 FPS estáveis em dispositivo mid-range
- Sem OOM em 30 min de jogo contínuo
- Uso de memória estável (não cresce indefinidamente)

---

### Story P4.5 — Polish de transição jogo → menu

**Descrição:** Após vitória/derrota, garantir que o estado de vitória seja limpo antes de voltar ao MAIN_MENU, evitando que o botão "MENU PRINCIPAL" deixe resíduos de estado.

**Agente responsável:** Gameplay Programmer

**Tarefas:**
1. No callback do botão "MENU PRINCIPAL" da tela de vitória, garantir `skirmish_victory_result_ = VictoryResult::NONE`
2. Verificar se `return_to_main_menu()` limpa estado de jogo residual
3. Testar transição vitória → menu → novo jogo sem resíduos

**Critério de Aceitação:**
- Transição vitória → menu limpa estado de vitória
- Novo jogo após vitória começa com estado limpo
- Não há resíduos de UI da tela de vitória no menu

---

## Critério de Aceitação P4→P5 (todos confirmados no código):

- [x] Lookups de ECS otimizados (1 busca de transform por entidade por frame no update_orders)
- [x] Culling por distância/frustum implementado (render/render_animated/render_grn_animated)
- [x] Pool de partículas sem realocações durante o jogo (2000 slots pré-alocados)
- [ ] 60 FPS estáveis no dispositivo alvo (pendente validação em dispositivo Android)
- [ ] Sem OOM em 30 min de jogo (pendente validação em dispositivo Android)
- [x] Transição vitória → menu limpa estado de vitória corretamente
- [x] Todos os testes continuam passando (exit 0)
- [x] Build-m2 compila sem erros

---

## Arquivos a Modificar (P4)

| Arquivo | Modificação |
|---------|-------------|
| `include/ecs/gameplay.h` | Otimizar lookups de transform em `update_orders()`, `gather_resources()`, `update_animations()`; culling frustum/distância em `render()`, `render_animated()`, `render_grn_animated()` |
| `src/engine/engine.cpp` | Pool de partículas, culling por distância/frustum (passa `&camera_`), polish de transição vitória→menu |
| `include/engine/engine.h` | Declarar membros do pool de partículas se necessário |

---

**Nota:** O progresso só deve ser marcado como concluído após existir no código e passar por compilação/teste no dispositivo Android (P0, P1, P2 e P3 devem estar concluídos primeiro).
