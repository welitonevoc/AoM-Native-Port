# P2 — Cena RTS e Recursos (MVP)

**Objetivo:** Implementar o terreno real (heightmap), HUD completo com Pop/Cap, Town Center funcional, seleção de grupo, save/load básico e condição de vitória para o MVP jogável.

**Dependência:** P0 (Build Recovery) e P1 (Gameplay Core) devem estar concluídos para compilação e teste.

**Responsável:** Graphics Programmer, Gameplay Programmer, UI Programmer  
**Agentes do Claude Code Game Studio:** graphics-programmer, gameplay-programmer, ui-ux-designer, qa-tester

---

## Estado Atual (Auditoria do Código)

### O que já existe:

| Componente | Arquivo | Status |
|------------|---------|--------|
| Terreno procedural 41×41 | `src/engine/engine.cpp` (generate_terrain) | ✅ Implementado (procedural, não heightmap real) |
| HUD com Pop/Cap | `src/engine/engine.cpp` (create_in_game_hud, update_in_game_hud) | ✅ Implementado |
| Town Center | `include/ecs/gameplay.h` (place_town_center) | ✅ Implementado |
| Seleção de grupo (box select) | `src/engine/engine.cpp` (handle_touch) | ✅ Implementado (P1.6) |
| Save/Load experimental | `src/engine/engine.cpp` (save_game, load_game) | ⚠️ Experimental (slot simples) |
| Condição de vitória | Não implementada | ❌ Pendente |
| Fog of War | `include/ecs/gameplay.h` (FogOfWar class) | ✅ Implementado |

### O que falta (gap P2):

| Gap | Detalhe | Prioridade |
|-----|---------|------------|
| **G1** | Terreno real: loader de height-map de cenários originais (.scn) | Alta |
| **G2** | Save/Load completo: restaurar todos os recursos e estados | Alta |
| **G3** | Condição de vitória: Conquest (eliminar todos inimigos) ou Sudden Death (destruir citadel) | Alta |
| **G4** | Skybox / Cube map (.cub) | Média |
| **G5** | Animação GRN playback | Média (pode ir para P3) |

---

## Plano Detalhado por Story

### Story P2.1 — Terreno Real (Heightmap)

**Descrição:** Substituir o terreno procedural 41×41 por um terreno carregado de height-map de cenários originais do AoM.

**Agente responsável:** Graphics Programmer

**Tarefas:**
1. Implementar parser de height-map a partir de arquivos .scn (cenários) ou .tga (heightmaps)
2. Carregar texturas de terreno autênticas (terrain/grassa.ddt, terrain/cliffgreeka.ddt) do textures.bar
3. Aplicar splatting de terreno baseado na inclinação da normal (grama em áreas planas, rocha em encostas)
4. Implementar shader de terreno com multi-textura e blending
5. Integrar com o NavigationGrid para bloquear células de terreno intransitável

**Critério de Aceitação:**
- Terreno carregado de arquivo de height-map (não apenas procedural)
- Texturas de grama e rocha aplicadas com splatting
- NavigationGrid reflete obstáculos do terreno

---

### Story P2.2 — Save/Load Completo

**Descrição:** Implementar persistência completa do estado do jogo: unidades, edifícios, recursos, população, posições, training queues.

**Agente responsável:** Gameplay Programmer

**Tarefas:**
1. Serializar GameplayWorld para arquivo binário (.sav ou .svx)
2. Serializar: unidades (Transform, Health, UnitComponent, SelectionComponent), edifícios (BuildingComponent), nós de recursos, ResourcePool, training queues
3. Implementar load_game() para restaurar estado completo
4. Adicionar slot de save no menu (Save Game / Load Game)
5. Validar roundtrip: save → modify → load → estado restaurado

**Critério de Aceitação:**
- Save grava estado completo do jogo
- Load restaura estado com todas as unidades, edifícios e recursos
- Treinamento em andamento é preservado (timer, fila)

---

### Story P2.3 — Condição de Vitória

**Descrição:** Implementar condições de vitória/derrota: Conquest (eliminar todas forças inimigas) e Sudden Death (destruir citadel/Town Center inimigo).

**Agente responsável:** Gameplay Programmer

**Tarefas:**
1. Implementar VictoryConditionEvaluator com suporte a Conquest e Sudden Death
2. Detectar vitória: todos inimigos eliminados (Conquest) ou Town Center inimigo destruído (Sudden Death)
3. Detectar derrota: Town Center do jogador destruído ou todas unidades perdidas
4. Exibir tela de vitória/derrota com opção de voltar ao menu
5. Integrar com a máquina de estados do Engine

**Critério de Aceitação:**
- Vitória por Conquest: todos inimigos eliminados → tela de vitória
- Vitória por Sudden Death: Town Center inimigo destruído → tela de vitória
- Derrota: Town Center do jogador destruído → tela de derrota
- Transição limpa para MAIN_MENU após vitória/derrota

---

### Story P2.4 — Skybox / Cube Map

**Descrição:** Implementar skybox usando arquivos .cub do textures.bar para ambiente 3D completo.

**Agente responsável:** Graphics Programmer

**Tarefas:**
1. Implementar parser de .cub (cube map) do textures.bar
2. Criar skydome/skybox com 6 faces do cube map
3. Integrar no render loop IN_GAME
4. Suporte a diferentes skyboxes por cenário

**Critério de Aceitação:**
- Skybox renderizado ao fundo da cena 3D
- 6 faces do cube map mapeadas corretamente
- Skybox não interfere no rendering de entidades

---

### Story P2.5 — Validação de Performance e Memória

**Descrição:** Validar que o jogo roda a 60 FPS estável e sem OOM em dispositivo alvo.

**Agente responsável:** QA Tester, Performance Analyst

**Tarefas:**
1. Medir FPS em dispositivo alvo (Snapdragon 6xx ou similar)
2. Medir consumo de memória (RAM) durante 30 min de jogo
3. Verificar ausência de vazamento de memória (file descriptors, texturas, VBOs)
4. Validar streaming de BAR (não carregar arquivo inteiro em RAM)
5. Relatório de performance com recomendações de otimização

**Critério de Aceitação:**
- 60 FPS estáveis em dispositivo mid-range
- Sem OOM em 30 min de jogo contínuo
- Uso de memória estável (não cresce indefinidamente)
- Relatório de performance documentado

---

## Gate Check P2→P3

Antes de avançar para P3 (Animação, Partículas e Efeitos Visuais), todos os itens abaixo devem ser confirmados:

- [ ] Terreno real carregado de height-map
- [ ] Save/Load funciona (roundtrip completo)
- [ ] Condição de vitória (Conquest e Sudden Death) implementada
- [ ] HUD completo com Pop/Cap funcionando
- [ ] Seleção de grupo (box select) funcionando
- [ ] 60 FPS estáveis no dispositivo alvo
- [ ] Sem OOM em 30 min de jogo

---

## Integração com as 4 Skills

| Skill | Papel no P2 |
|-------|-------------|
| **@claude-code-game-studio** | Estrutura stories P2.1–P2.5, define gate check P2→P3, atribui agentes |
| **@reverse-engineer-anything** | Inspeciona arquivos .scn (formato binário de cenários) para extrair height-map e dados de terreno |
| **@r8-analyzer** | Planejamento pós-P2: otimização de build com R8 para reduzir tamanho do APK |
| **@offtec-portai** | Valida portabilidade do terreno/heightmap para Android GLES3, automação de empacotamento de assets |

---

## Próxima Ação Imediata (P2.1)

```bash
# Após P0 e P1 concluídos:

# 1. Implementar parser de height-map de .scn
# 2. Carregar texturas de terreno do textures.bar
# 3. Aplicar splatting baseado na normal
# 4. Integrar com NavigationGrid
# 5. Rebuild: bash scripts/build_native.sh
# 6. Testar: ./build-m2/gameplay_test
```

---

## Arquivos a Modificar (P2)

| Arquivo | Modificação |
|---------|-------------|
| `src/engine/engine.cpp` | generate_terrain() usar height-map real; adicionar load_heightmap() |
| `include/ecs/gameplay.h` | Adicionar VictoryConditionEvaluator; integrar com NavigationGrid |
| `src/engine/engine.cpp` | save_game() e load_game() completos |
| `include/engine/engine.h` | Adicionar membros para skybox, victory state |
| `src/renderer/gles_renderer.cpp` | Suporte a cube map (.cub) para skybox |

---

**Nota:** O progresso só deve ser marcado como concluído após existir no código e passar por compilação/teste no dispositivo Android (P0 e P1 devem estar concluídos primeiro).