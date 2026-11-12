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

### Documentação dos testes/reference (subpastas)
Cada subpasta de `tests/reference/` possui seu próprio `README.md` documentando o conteúdo:
- `tests/reference/databar/README.md` — 18 arquivos XML de menus e diálogos (uigamelist, uigodpicker, uioptions, etc.)
- `tests/reference/databar2/README.md` — ~90 arquivos XML incluindo proto.xml, protox.xml, proto101x.xml, techtrees, campanhas e menus UI completos
- `tests/reference/ddt/README.md` — Texturas PNG, skybox, UI textures, modelos e skybox
- `tests/reference/sounds/README.md` — ~250 arquivos XML de sons (unidades, edifícios, áudio ambiente)
- `tests/reference/sounds2/README.md` — Áudio adicional (sons de expansão The Titans)
- `tests/reference/textures/README.md` — ~2.500 arquivos PNG de texturas de mundo, UI e partículas
- `tests/reference/textures2/README.md` — Texturas adicionais de referência para testes de regressão DDT

## Progresso das Pendências

### Prioridade Alta (Todas CONCLUIDAS ✅):
1. ✅ Discovery de jogos na rede LAN - Já implementado com código C++ de sockets UDP
2. ✅ Protocolo de conexão TCP/UDP entre jogadores - Já implementado (JOIN_REQ, JOIN_RESP, LOBBY_STATE, SLOT_UPDATE, CHAT, LAUNCH, LEAVE)
3. ✅ Tela de configuração de IP/Porta editável - Já existente com campos de texto e ajustes
4. ✅ Lista de jogos disponíveis na rede - Já implementada, exibida no menu LAN
5. ✅ God Picker - Seleção de civilização e deus menor - IMPLEMENTADO recentemente

### Prioridade Média:
6. ✅ Protozoário/God Picker - Seleção de civilização e deus (próximo estado após campanha) - CONCLUÍDO

### Próximos Passos (Motor do Jogo):
- Cenários e missões
- Dinâmica de unidades guerreiras
- Poderes divinos (god powers)
- Sistema de progressão de campanha

