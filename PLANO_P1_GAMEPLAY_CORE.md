# P1 — Gameplay Core Mínimo e Integração ECS

**Objetivo:** Integrar o ECS (`include/ecs/ecs.h`) ao `Engine` via `GameplayWorld` (`include/ecs/gameplay.h`), criar unidades friendly/enemy com mesh válido, validar combate isolado, seleção por toque e remoção de entidades mortas.

**Dependência:** P0 (Build Recovery) deve estar concluído para compilação e teste no dispositivo.

**Responsável:** Gameplay Programmer, Engine Programmer, Lead Programmer  
**Agentes do Claude Code Game Studio:** gameplay-programmer, engine-programmer, lead-programmer, qa-tester

---

## Estado Atual (Auditoria do Código)

### O que já existe:

| Componente | Arquivo | Status |
|------------|---------|--------|
| ECSWorld base | `include/ecs/ecs.h` | ✅ Implementado (Transform, Health, Renderable, MovementSystem, RenderSystem) |
| GameplayWorld | `include/ecs/gameplay.h` | ✅ Implementado (combate, navegação A*, construção, economia, fog of war, animação) |
| Engine::start_game() | `src/engine/engine.cpp` | ✅ Cria GameplayWorld, spawn 6 unidades + 2 Town Centers + 1 worker + 6 resource nodes |
| Engine::handle_touch() | `src/engine/engine.cpp` | ⚠️ Existe mas não conecta limpo ao GameplayWorld |
| UIManager::handle_touch() | `src/ui/ui.cpp` | ✅ Handle toques em botões |
| native_activity.cpp | `src/platform/android/` | ✅ Converte AMotionEvent → TouchEvent → engine->handle_touch() |
| gameplay_test | `src/tests/gameplay_test.cpp` | ✅ Testa combate, navegação, construção, treino, colheita |

### O que falta (gap P1):

| Gap | Detalhe |
|-----|---------|
| **G1** | ECSWorld e GameplayWorld são separados; GameplayWorld tem seus próprios maps, não usa ECSWorld |
| **G2** | Engine não registra MovementSystem, CombatSystem, SelectionSystem, BuildingSystem em um ECSWorld unificado |
| **G3** | Toque no gameplay não conecta limpo: UI consome toque primeiro, gameplay não recebe coordenadas do mundo |
| **G4** | Não há seleção de grupo (box select/arrasto) |
| **G5** | Renderização das unidades do ECS não usa RenderSystem do ECS; GameplayWorld::render() faz seu próprio desenho |
| **G6** | Nenhuma integração entre `ECSWorld::destroy_entity()` e `GameplayWorld::remove_entity()` |

---

## Plano Detalhado por Story

### Story P1.1 — Unificar ECSWorld e GameplayWorld ✅ CONCLUÍDO

**Descrição:** Fazer GameplayWorld usar ECSWorld como base, evitando duplicação de maps de componentes.

**Agente responsável:** Engine Programmer

**Tarefas realizadas:**
1. ✅ `include/ecs/ecs.h`: Adicionados métodos `transforms_mut()`, `healths_mut()`, `renderables_mut()`, `clear()` ao ECSWorld
2. ✅ `include/ecs/gameplay.h`: GameplayWorld agora contém `ECSWorld ecs_world_` como membro
3. ✅ `spawn_unit()` usa `ecs_world_.create_entity()` + `ecs_world_.transforms_mut()` + `ecs_world_.healths_mut()`
4. ✅ `place_building()` usa `ecs_world_.create_entity()` + `ecs_world_.transforms_mut()` + `ecs_world_.healths_mut()`
5. ✅ `remove_entity()` usa `ecs_world_.destroy_entity()`
6. ✅ `clear_all_entities()` usa `ecs_world_.clear()`
7. ✅ Acessores `transforms()`, `get_transform()`, `get_health()` delegam para `ecs_world_`
8. ✅ `update_orders()`, `gather_resources()`, `render()`, `render_animated()`, `get_minimap_blips()` usam `ecs_world_`
9. ✅ `order_attack_selected()`, `order_move()`, `order_attack()`, `demolish_building()`, `order_stop_selected()` usam `ecs_world_`
10. ✅ `enemy_at()`, `building_at()`, `find_spawn_position()` usam `ecs_world_`
11. ✅ Membros privados `transforms_` e `health_` removidos (duplicação eliminada)
12. ✅ `next_entity_` removido (não usado mais)
13. ✅ Construtor do GameplayWorld simplificado (inicializadores inline para MovementSystem, CombatSystem, SelectionSystem, BuildingSystem)
14. ✅ engine.cpp não precisa de mudanças (usa apenas métodos públicos)

**Critério de Aceitação:**
- ✅ ECSWorld contém maps para Transform, Health, Renderable
- ✅ GameplayWorld delega Transform/Health storage para ECSWorld
- ✅ Nenhum código duplica component storage entre ECSWorld e GameplayWorld
- ✅ engine.cpp compila sem mudanças (usa apenas API pública)

---

### Story P1.2 — Criar unidades friendly/enemy com mesh válido

**Descrição:** Spawnar unidades usando o ECSWorld unificado, com mesh e textura reais (hoplite_mesh_ já carregado em start_game) ou fallback procedural.

**Agente responsável:** Gameplay Programmer

**Tarefas:**
1. Em `Engine::start_game()`, após carregar BRG e texturas:
   - Usar `gameplay_world_->spawn_unit()` para criar unidades friendly e enemy
   - Associar `hoplite_mesh_` e `hoplite_texture_` às unidades
   - Criar pelo menos 3 friendly + 3 enemy em posições opostas

2. Garantir que cada unidade spawned tenha:
   - TransformComponent (posição)
   - HealthComponent (HP baseado em proto.xmb)
   - UnitComponent (time, ataque, movimento)
   - SelectionComponent (selecionável)
   - AnimationComponent (se clip disponível)

**Critério de Aceitação:**
- Pelo menos 6 unidades spawnadas (3 friendly, 3 enemy)
- Unidades têm mesh e textura válidas (ou fallback procedural)
- Unidades são renderizadas no frame IN_GAME

---

### Story P1.3 — Teste de combate isolado sem renderer

**Descrição:** Validar que o sistema de combate funciona corretamente entre duas unidades em equipes opostas, sem depender do renderer.

**Agente responsável:** QA Tester

**Tarefas:**
1. O `gameplay_test.cpp` já testa:
   - Duas unidades em equipes opostas dentro de alcance
   - Um passo do CombatSystem aplica dano
   - Inimigo morre (HP <= 0)
   - combat.dead_entities() reporta o alvo morto

2. Executar o teste e confirmar:
   - `friendly unit damages nearby enemy` → PASS
   - `combat reports dead enemy` → PASS

3. Adicionar teste de combate via ECSWorld (através de GameplayWorld):
   - Spawn duas unidades via GameplayWorld
   - Executar mundo.update(dt)
   - Verificar dano e morte

**Critério de Aceitação:**
- `gameplay_test` retorna 0 (todos PASS)
- Combate via ECSWorld/GameplayWorld aplica dano corretamente
- Entidade morta é reportada em dead_entities()

---

### Story P1.4 — Loop de combate, seleção, remoção de mortos

**Descrição:** Garantir que o loop de update do GameplayWorld:
1. Processa combate (timers, dano, aquisição de alvo)
2. Remove entidades mortas de TODOS os maps (transforms, health, units, selections, buildings, animations)
3. Apenas depois atualiza animações (para não iterar sobre map limpo)
4. Processa construção (progresso, treinamento)
5. Atualiza Fog of War
6. Atualiza animações (vertex morphing)

**Agente responsável:** Engine Programmer

**Tarefas:**
1. Verificar `GameplayWorld::update()` (linha 981 de gameplay.h):
   - Ordem correta: combat_.update() → remover mortos → buildings_.update() → drain_spawn_orders() → fog of war → update_animations()
   - `remove_entity()` limpa transforms_, health_, units_, selections_, building_components_, animations_
   - `remove_entity()` também limpa referências de attack_target em outras unidades

2. Verificar `CombatSystem::update()`:
   - dead_.clear() no início
   - Popula dead_entities() quando HP <= 0
   - Reseta attack_target do atacante quando alvo morre

3. Verificar `SelectionSystem::select_at()`:
   - Encontra entidade mais próxima do ponto
   - Marca apenas ela como selecionada

**Critério de Aceitação:**
- Entidades mortas são removidas de todos os maps
- Nenhum ponteiro pendente após remoção
- Seleção funciona: clique em unidade a seleciona
- Combate aplica dano e remove mortos

---

### Story P1.5 — Integração toque → seleção/construção ✅ CONCLUÍDO

**Descrição:** Conectar o fluxo de toque Android ao GameplayWorld.

**Estado atual:** `Engine::handle_touch()` (engine.cpp:1178) já implementa:
- ✅ Forward para UI primeiro (ui_->handle_touch), se retornar true não processa gameplay
- ✅ Raycast toque → plano do chão 3D
- ✅ Toque DOWN: inicia arrasto (dragging_)
- ✅ Toque MOVE: pan da câmera se arrasto > 14px E não está em modo box_select
- ✅ Toque UP sem arrasto: comando RTS completo
  - ✅ Ataque a inimigo (order_attack_selected)
  - ✅ Coleta por worker (order_gather)
  - ✅ Construção por worker (order_build / order_worker_build)
  - ✅ Seleção de unidade (select_at)
  - ✅ Movimento em área vazia (order_move_selected)
- ✅ Prioridade: ataque → coleta → construção → seleção → movimento

**Critério de Aceitação:**
- ✅ Toque em unidade friendly a seleciona
- ✅ Toque em área livre com unidade selecionada emite order_move()
- ✅ Toque em inimigo com unidade friendly selecionada emite order_attack_selected()
- ✅ Toque em nó de recurso com worker selecionado emite order_gather()
- ✅ UI consome toque de botão e não repassa para gameplay

---

### Story P1.6 — Seleção de grupo (box select) ✅ CONCLUÍDO

**Descrição:** Implementar seleção múltipla de unidades friendly via arrasto (caixa de seleção) no touch.

**Estado atual:**
- `GameplayWorld::select_box()` já existe (gameplay.h:1185) - funciona corretamente
- `Engine::handle_touch()` foi modificado para suportar box select:
  - DOWN: salva box_start_ e box_current_
  - MOVE: se arrasto > 14px, ativa box_selecting_ e desativa pan da câmera
  - UP: se box_selecting_, faz raycast dos dois cantos para world coords e chama select_box()
- Renderização da caixa de seleção adicionada ao render() (engine.cpp: seção "Box Selection Overlay"):
  - Preenchimento translúcido azul (0x440000FF)
  - Borda dourada em todos os 4 lados (0xFFFFD000)

**Critério de Aceitação:**
- ✅ Arrasto de dedo cria caixa de seleção visual com borda dourada
- ✅ Soltar dedo com arrasto significativo seleciona unidades friendly na área
- ✅ Toque curto continua funcionando como seleção única
- ✅ Camera pan desativado durante box select

---

## Gate Check P1→P2

Antes de avançar para P2 (Cena RTS e Recursos), todos os itens abaixo devem ser confirmados:

- [ ] ECSWorld e GameplayWorld unificados (sem duplicação de component storage)
- [ ] Engine::start_game() spawn unidades via ECSWorld/GameplayWorld
- [ ] gameplay_test passa 100% (7/7 PASS)
- [ ] Toque em unidade seleciona; toque em área livre move unidade selecionada
- [ ] Toque em inimigo emite ordem de ataque
- <del>Seleção de grupo (arrasto) funciona</del> ← opcional para P1, pode ir para P2

---

## Integração com as 4 Skills

| Skill | Papel no P1 |
|-------|-------------|
| **@claude-code-game-studio** | Estrutura stories P1.1–P1.6, define gate check P1→P2, atribui agentes (gameplay-programmer, engine-programmer, qa-tester) |
| **@reverse-engineer-anything** | Inspeciona `libaom_native.so` após build para verificar símbolos exportados (ANativeActivity_onCreate), valida que não há dependências ausentes |
| **@r8-analyzer** | Planejamento pós-P1: quando R8 estiver habilitado (AGP >= 9.3), analisar regras de keep para o código C++ nativo |
| **@offtec-portai** | Validar portabilidade do código de toque/input para Android GLES3, verificar compatibilidade de gestos multitouch |

---

## Próxima Ação Imediata (P1.1)

```bash
# Após P0 concluído (build funcionando no Termux/Antigravity):

# 1. Modificar include/ecs/ecs.h — adicionar maps de UnitComponent, SelectionComponent, BuildingComponent
# 2. Modificar include/ecs/gameplay.h — GameplayWorld delega para ECSWorld
# 3. Modificar src/engine/engine.cpp — start_game() usa ECSWorld unificado
# 4. Rebuild: bash scripts/build_native.sh
# 5. Testar: ./build-m2/gameplay_test
```

---

## Arquivos a Modificar (P1)

| Arquivo | Modificação |
|---------|-------------|
| `include/ecs/ecs.h` | Adicionar maps units_, selections_, buildings_ e acessores; destruir entidade limpa todos os maps |
| `include/ecs/gameplay.h` | GameplayWorld usa ECSWorld como base; delega component storage |
| `src/engine/engine.cpp` | start_game() registra sistemas no ECSWorld; handle_touch() conecta ao GameplayWorld |
| `src/engine/engine.h` | Adicionar membro ECSWorld se necessário; expor acesso ao gameplay world |
| `src/tests/gameplay_test.cpp` | Adicionar teste de combate via ECSWorld/GameplayWorld integrado |

---

**Nota:** O progresso só deve ser marcado como concluído após existir no código e passar por compilação/teste no dispositivo Android (P0 deve estar concluído primeiro).