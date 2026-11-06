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

### Adição dos testes de referência (`tests/reference/`)
- Incluídas as pastas: `databar`, `databar2`, `ddt`, `sounds`, `sounds2`, `textures`, `textures2`
- Dados de referência extraídos dos BARs do Age of Mythology, usados para estudo e comparação de conversão
- Envio feito com `git add -f` pois a pasta está ignorada no `.gitignore` (dados proprietários do jogo original)
- Commit: `adiciona testes/reference para fins de estudo`
- Branch: `main` — sincronizado com `origin/main` no GitHub

## Observações
- O repositório está 6 commits à frente do que já estava no GitHub antes deste envio
- `CMakeLists.txt`: removida referência inexistente a `third_party/stb_image.c` do alvo `test_ddt_reference` (commit `fix: remover STB_IMAGE_WRITE inexistente do test_ddt_reference`)
- Nenhuma alteração pendente no working tree

### Documentação dos testes/reference (subpastas)
Cada subpasta de `tests/reference/` possui seu próprio `README.md` documentando o conteúdo:
- `tests/reference/databar/README.md` — 18 arquivos XML de menus e diálogos (uigamelist, uigodpicker, uioptions, etc.)
- `tests/reference/databar2/README.md` — ~90 arquivos XML incluindo proto.xml, protox.xml, proto101x.xml, techtrees, campanhas e menus UI completos
- `tests/reference/ddt/README.md` — Texturas PNG, skybox, UI textures, modelos e skybox
- `tests/reference/sounds/README.md` — ~250 arquivos XML de sons (unidades, edifícios, áudio ambiente)
- `tests/reference/sounds2/README.md` — Áudio adicional (sons de expansão The Titans)
- `tests/reference/textures/README.md` — ~2.500 arquivos PNG de texturas de mundo, UI e partículas
- `tests/reference/textures2/README.md` — Texturas adicionais de referência para testes de regressão DDT

