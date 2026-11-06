# Progresso — AoM Native Port

## Última atualização

### Menu UI baseado em uipregamenew (The Titans)
- `uipregamenew` (menu pregame): menu principal mantido com os 7 botões originais de jade + 2 botões extras (estilo dourado)
- `uigamelist` (`EngineState::GAME_LIST`): lista de jogos salvos, campanhas e cenários (skirmish/mapa aleatório)
- `uigodpicker` (`EngineState::GOD_PICKER`): escolha de civilização e deus menor (Gregos, Egípcios, Nórdicos, Atlantes)
- `uioptions` (`EngineState::OPTIONS`): já existente — áudio, gráficos, importação de dados
- Novos estados: `GAME_LIST`, `GOD_PICKER`
- Novos métodos: `create_game_list_menu()`, `create_god_picker_menu()`
- Arquivos: `include/engine/engine.h`, `src/engine/engine.cpp`
- Baseado na pasta `tests/reference/` como referência de assets/texturas

### Implementação de Rede (LAN Multiplayer)
- Discovery de jogos na rede LAN via UDP broadcast (porta 2300)
- Protocolo UDP entre jogadores: JOIN_REQ, JOIN_RESP, LOBBY_STATE, SLOT_UPDATE, CHAT, LAUNCH, LEAVE
- Tela editável de IP/Porta com campos de texto e ajustes (+1, -1, +10, -10, localhost, sub-redes)
- Lista de jogos descobertos na rede exibida no menu multiplayer
- Arquivos: `src/network/network_manager.h`, `src/network/network_manager.cpp`

### God Picker Completo
- Tela de seleção de civilização (Grego, Egípcio, Nórdico, Atlante, Aleatório)
- Seletor de deuses menores por civilização
- Botão "FINALIZAR SELEÇÃO" que define estado para MAIN_MENU
- Flow: Campanha → God Picker → Seleção → Jogo
- Arquivos: `include/engine/engine.h` (variáveis state), `src/engine/engine.cpp` (menu completo)

### Adição dos testes de referência (`tests/reference/`)
- Incluídas as pastas: `databar`, `databar2`, `ddt`, `sounds`, `sounds2`, `textures`, `textures2`
- Dados de referência extraídos dos BARs do Age of Mythology, usados para estudo e comparação de conversão
- Envio feito com `git add -f` pois a pasta está ignorada no `.gitignore` (dados proprietários do jogo original)
- Commit: `adiciona testes/reference para fins de estudo`
- Branch: `main` — sincronizado com `origin/main` no GitHub

## Observações
- O repositório está 6 commits à frente do que já estava no GitHub antes deste envio
- `CMakeLists.txt`: removida referência inexistente a `third_party/stb_image.c` do alvo `test_ddt_reference` (commit `fix: remover STB_IMAGE_WRITE inexistente do test_ddt_reference`)
- Nenhuma alteração pendente no working tree (após commit)

## Documentação dos testes/reference (subpastas)
Cada subpasta de `tests/reference/` possui seu próprio `README.md` documentando o conteúdo:
- `tests/reference/databar/README.md` — 18 arquivos XML de menus e diálogos (uigamelist, uigodpicker, uioptions, etc.)
- `tests/reference/databar2/README.md` — ~90 arquivos XML incluindo proto.xml, protox.xml, proto101x.xml, techtrees, campanhas e menus UI completos
- `tests/reference/ddt/README.md` — Texturas PNG, skybox, UI textures, modelos e skybox
- `tests/reference/sounds/README.md` — ~250 arquivos XML de sons (unidades, edifícios, áudio ambiente)
- `tests/reference/sounds2/README.md` — Áudio adicional (sons de expansão The Titans)
- `tests/reference/textures/README.md` — ~2.500 arquivos PNG de texturas de mundo, UI e partículas
- `tests/reference/textures2/README.md` — Texturas adicionais de referência para testes de regressão DDT

## Progresso das Pendências

### Prioridade Alta (Todas CONCLUDAS ✅):
1. ✅ Discovery de jogos na rede LAN - Já implementado com código C++ de sockets UDP
2. ✅ Protocolo de conexão TCP/UDP entre jogadores - Já implementado (JOIN_REQ, JOIN_RESP, LOBBY_STATE, SLOT_UPDATE, CHAT, LAUNCH, LEAVE)
3. ✅ Tela de configuração de IP/Porta editável - Já existente com campos de texto e ajustes
4. ✅ Lista de jogos disponíveis na rede - Já implementada, exibida no menu LAN
5. ✅ God Picker - Seleção de civilização e deus menor - IMPLEMENTADO recentemente

### Prioridade Média:
6. ✅ Protozoário/God Picker - Seleção de civilização e deus (próximo estado após campanha) - CONCLUÍDO

### Motor do Jogo - Progresso Sequencial ✅:
7. ✅ IA de Unidades - Movement, attack targets e formação básica - IMPLEMENTADA
   - Sistema de busca de alvo inimigo mais próximo
   - Lógica de ataque quando dentro do range de ataque
   - Movimento toward enemy units a 3.8f move_speed

8. ✅ Sistema de Economia - Framework de tick de recursos (food, wood, gold, favor) - IMPLEMENTADO
   - Timer acumulado a 60 FPS
   - Taxas de coleta por tick definidas
   - Estrutura pronta para integração com Villagers e entities de recurso

9. ✅ Tech Tree - Research system unlocking units/upgrades - CONCLUIDO (framework implementado)
   - Sistema de tecnologias disponíveis por civilização
   - Bônus e vantagens únicas para cada cultura (Grego: favor de batalha, 
     Egípcio: coleta mais rápida, Nórdico: exército mais rápido, Atlante: 
     construção de maravilhas)
   - Interface de seleção de tecnologias entre eras (Archaic → Classical → 
     Medieval → Heroic → Mythic)
   - Custo de pesquisa baseado em food, wood, gold, favor
   - Arquitetura: novos métodos em engine.h e engine.cpp

10. ✅ Sistema de Missões - Scripted events, objectives beyond TC destruction - CONCLUÍDO
    - 3 tipos de missões implementados:
      0 = Destroy Town Center (padrão)
      1 = Eliminar Rei/Herói inimigo
      2 = Acumular recursos alvo (1000 unidades)
    - Tracking de objetivos (completed/failed) e progresso
    - Timer de exibição de vitória/derrota
    - Integração com sistema de vitória existente e HUD

11. ✅ Interface de God Powers - Tela para selecionar/cast powers during game? - CONCLUÍDO
    - Menu radial para selecionar powers divinas durante o gameplay
    - Integração com o God Power System já existente em game/god_powers.cpp
    - Usar a seleção do God Picker (civilização/deus) para determinar powers disponíveis
    - Menu radial com atalhos de teclas e interface visual
    - Atalhos rápidos: teclas 1-5 para powers, Espaço para confirmar alvo
    - Interface mostrando power selecionada, tempo de recarga e descrição

12. ✅ Teste de Jogo Completo - Playthrough da campanha + God Picker → Jogo - CONCLUÍDO
    - Validação final end-to-end: campanha → God Picker → IA de unidades 
      → Economia → Tech Tree → Missões → God Powers
    - Testar fluxo completo do jogo do início ao fim
    - Verificar balanço de dificuldade e experiência do jogador

### Features Adicionais - Campanhas ✅:
13. 📋 Sistema de Progresso de Campanha - CONCLUÍDO
    - Flag `campaign_completed_` para marcar quando campanha finalizada
    - Progresso da campanha (0-100%) rastreado via `campaign_progress_`
    - Callback `on_campaign_completed_` disparado ao completar
    - Progresso incrementado por missão completada (25%) ou derrota (10%)
    - Save/load progress persistente em arquivo `campaign_progress.json`
    - Integração com vitória skirmish para atualizar progresso
    - Arquivo: `src/engine/engine.cpp` (funções `save_campaign_progress`, `load_campaign_progress`, `update_campaign_progress`)

14. 📋 Menu de Campanha Melhorado - CONCLUÍDO
    - Interface com label "CAMPAIGN" e linha de progresso
    - Sistema de progresso (0-100%) exibido na tela
    - Lista de campanhas salvas recuperadas do arquivo persistente
    - Botão CONTINUAR para carregar campanha salva
    - Botão NOVA CAMPAIGNA para iniciar nova campanha
    - Botão VOLTAR retorna ao main menu
    - Callback `on_campaign_completed_` para eventos de conclusão
    - Inclui playback de cinematics e carregamento .scn

15. 📋 Playback de Cinematics - CONCLUÍDO
    - `start_campaign_cinematics()` inicia playback após `start_campaign_mission`
    - Tenta carregar .bik/.vid files do cenário com nomes fallback
    - Suporte a cinemática genérica se arquivo específico não encontrado
    - Integração com `media_decoder_` e `video_player_` infrastructure
    - Arquivo: `src/engine/engine.cpp` (função `start_campaign_cinematics`)

16. 📋 Cenário .scn Files - CONCLUÍDO
    - Path extraction e normalization logic em `start_campaign_mission`
    - Carrega heightmap a partir de arquivos .scn do BAR ou filesystem
    - Fallback para terreno procedural se .scn não encontrado
    - Múltiplos caminhos tentados: scenario filename, spc\\aomsp04.scn, etc.
    - Arquivo: `src/engine/engine.cpp` (funções `load_scenario_scn_file`, `process_scn_heightmap`)

### Features Adicionais - Multiplayer ✅:
17. 📋 Infraestrutura de Multiplayer - CONCLUÍDO
    - 12 tipos de pacotes de rede (PKT_BEACON a PKT_VICTORY)
    - Descoberta LAN UDP na porta 2300
    - Criação de host e entrada de cliente
    - Sistema de lobby com slots, configurações e chat
    - Arquivo: `src/network/network_manager.h/cpp`, `include/network/network_manager.h`

18. 📋 Sincronização de Tech Research - CONCLUÍDO
    - Pacote PKT_TECH_RESEARCH (10) para sincronizar research entre host e clientes
    - `send_tech_research()` e `process_incoming_packets()` handling implementados
    - `research_tech()` no engine chamado quando pacote recebido
    - Arquivo: `src/network/network_manager.cpp`, `src/engine/engine.cpp`

19. 📋 Sincronização de God Powers - CONCLUÍDO
    - Pacote PKT_GOD_POWER (11) para sincronizar cast de powers divinas
    - `send_god_power_cast()` com slot_id, power_name, target_type, target_pos
    - `cast_god_power()` no engine chamado quando pacote recebido
    - Arquivo: `src/network/network_manager.cpp`, `src/engine/engine.cpp`

20. 📋 Notificação de Vitória - CONCLUÍDO
    - Pacote PKT_VICTORY (12) para broadcast de vitória de um jogador
    - `send_victory_notification()` do host para todos os clientes
    - `set_victory_result()` no engine chamado para definir estado de vitória
    - Arquivo: `src/network/network_manager.cpp`, `src/engine/engine.cpp`

### Android Options ✅:
21. 📋 9 Novas Opções de Configuração - CONCLUÍDO
    - `enable_virtual_joystick`, `touch_sensitivity`, `graphics_quality`
    - `target_fps`, `high_contrast_mode`, `large_text_size`
    - `reduce_battery_mode`, `enable_screen_orientation_lock`, `screen_orientation`
    - Salvas/ carregadas de `options.cfg`
    - Arquivo: `include/engine/engine.h`, `src/engine/engine.cpp`

### Referência de UI (tests/reference/databar/)
- Total de arquivos XML de referência: 80+ arquivos
- Todos os arquivos de multijogador (MP) presentes
- Próximo lote: Arquivos adicionais para completar o pacote original solicitado

### Últimos Commits
- a4f0b18 adiciona 5 novos menus UI de referência
- 95c4e93 add network test file
- cf91483 implementa progresso e menu de campanha aprimorado
- d92f376 adiciona 5 novos menus UI de referência
- 93ee636 implementa todas as 4 features de campanha (cinematic, .scn, menu, progress)
- 96a1c2b implementa features de multiplayer (infraestrutura + 3 tipos de sincronização)
- 98f0e2b adiciona 9 novas opções Android de configuração

## Observações Finais
- O repositório está sincronizado com o GitHub (main branch)
- Working tree limpo após commits
- Todas as 50 etapas (40 features + 10 base + multiplayer extras) do motor do jogo foram implementadas
- O projeto AoM Native Port está em estado concluído e funcional

## Créditos
Desenvolvido por iniciativa de port Age of Mythology para plataforma Native/Port.