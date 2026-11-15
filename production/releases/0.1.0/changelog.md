# Internal Changelog: 0.1.0
Date: Wed Nov 11 2026
Sprint(s): P5 (MVP Completo / Vertical Slice)
Commits: 10 (8ac1f7a..170a046)

## New Features
- **P5.1 — HUD Completo**: Barra de progresso de produção, tooltips dinâmicos, população, minimapa e painel de comandos no HUD em jogo.
  - Commits: 017616b
  - Owner: UI Programmer
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.1)

- **P5.2 — Town Center e Seleção de Grupo**: TOWN_CENTER no PlacementMode, botão "Centro (400M 200O)" no HUD, placement de TC no touch UP, contagem de multi-seleção no HUD.
  - Commits: def1ea2
  - Owner: Gameplay Programmer
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.2)

- **P5.3 — Save/Load Completo e Condição de Vitória**: Save header v2 com campos victory_condition/result/timer, sincronização no engine (start_game, load_game, update, callbacks).
  - Commits: 1fbd3a6
  - Owner: Systems Designer, Gameplay Programmer
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.3)

- **P5.4 — IA Econômica e Militar Básica**: ai_system_ criado e inicializado em start_game (Team::ENEMY, personalidade "standard"), chamada ai_system_->update() no loop IN_GAME.
  - Commits: 1fbd3a6
  - Owner: Systems Designer, Gameplay Programmer
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.4)

- **P5.5 — Áudio Ampliado**: SoundDefManager instanciado no engine, carregamento de soundsets.xml e *_snds.xml do sounds.bar via XMB parser, método play_sound_bank() para disparar SFX por evento/unidade via sound bank com fallback, chamadas de áudio de unidade (select, move, attack) usam sound bank com fallback.
  - Commits: b335945
  - Owner: Audio Programmer
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.5)

- **P5.6 — UI de XMB e Localização**: load_ui_strings() popula tabela de strings PT-BR, create_main_menu_from_xmb() tenta uimain.xmb → uipregamenew.xmb → fallback procedural com tr(), create_command_panel_from_xmb() tenta commandpanel.xmb como layer visual.
  - Commits: 8ac1f7a
  - Owner: UI Programmer, Localization Lead
  - Design doc: PLANO_P5_MVP_COMPLETO.md (Story P5.6)

- **P5 End-to-End Match Test**: Teste de partida completa (spawn → economia → combate → vitória) validando todo o MVP em < 10 min.
  - Commits: 8db0748
  - Owner: QA Tester

## Improvements
- **P4.G4 — Performance e Memória**: test_perf reescrito com sistemas reais do jogo (ECSWorld + MovementSystem + pool de partículas fixo). 60s/3600 frames, 2000 entidades + 2000 partículas, FPS médio 33.222, mínimo 4.447, P99 21.381, crescimento de memória 644 KB.
  - Commits: fa5a6be
  - Owner: Technical Director
  - Design doc: PLANO_P4_POLISH_OPTIMIZACAO.md

- **Método select_entity()**: Novo método público em GameplayWorld para permitir seleção programática em testes unitários.
  - Commits: 54231a7
  - Owner: Gameplay Programmer

## Bug Fixes
- Nenhum corretivo de bug específico neste período (todas as funcionalidades P5 foram implementadas como novas features).

## Balance Changes
- Nenhum ajuste de balanceamento neste período.

## Technical Debt / Refactoring
- Forward declarations para VictoryCondition/VictoryResult em gameplay.h para evitar include circular com game/victory.h — default values de enum não são permitidas com forward declaration, usou-se static_cast<...>(0).
  - Commits: 1fbd3a6

## Miscellaneous
- **Release Checklist 0.1.0**: production/releases/release-checklist-0.1.0.md criado com cert_tier=none (build interno/alpha/jam).
  - Commits: 170a046
  - Owner: Release Manager

## Known Issues
- Save/Load roundtrip de entidades não validado em ambiente Termux devido a restrições de sistema de arquivos (/tmp). Requer validação em dispositivo Android real.
- Soak test (4+ horas contínuas) não executado — requer dispositivo Android real.
- Load times não medidos.
- Nenhuma tag de versão existente no repositório.

## Metrics
- Total commits: 10 (8ac1f7a..170a046)
- Files changed: 11 (desde 8ac1f7a)
- Lines added: 1246 (desde 8ac1f7a)
- Lines removed: 0 (desde 8ac1f7a)
- Commits without task reference: 10
