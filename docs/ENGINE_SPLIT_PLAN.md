# ENGINE_SPLIT_PLAN — dividir `src/engine/engine.cpp` (commit 445d1ad: 7.044 linhas, 203 métodos)

**Etapa 1 (este plano):** mover definições para vários `.cpp`. A classe `Engine` e o `engine.h` NÃO mudam.
**Etapa 2 (depois, em outro momento):** encolher a API do `engine.h` (subsistemas como `AssetManager`, menus data-driven, `set_state` em tabela).
Não misture as duas etapas.

## Regras
- **Um bloco por vez.** Compilar, rodar o fluxo e dar commit antes do próximo.
- Mover a função **inteira e inalterada** (sem "melhorar" nada junto).
- Linhas abaixo são aproximadas (commit 445d1ad); confira em `docs/SYMBOLS.md`.
- Helpers em `namespace { }` (linhas ~55, ~3832, ~5861 do engine.cpp): quem os usa leva a cópia; se mais de um arquivo precisar,
  mova para `src/engine/engine_internal.h` como `inline`.

## Procedimento de cada bloco
1. Criar `src/engine/engine_<nome>.cpp` com os mesmos `#include` do topo do `engine.cpp` (copie, depois enxugue).
2. Cortar as funções do bloco do `engine.cpp` e colar no novo arquivo, dentro de `namespace aom { ... }`.
3. Adicionar o arquivo à lista de fontes em `CMakeLists.txt` (linha ~17) **e** `android/CMakeLists.txt`.
4. Build. Erro "undefined/not declared" quase sempre é helper de `namespace {}` → ver regra acima.
5. Teste manual: abrir o app → menu → iniciar uma partida (e uma missão se o bloco tocar campanha).
6. Commit (`refactor(engine): move <bloco> para engine_<nome>.cpp`), depois `python3 tools/symbol_index.py`.
**Parar e reverter** se o build falhar por algo que não seja include/helper, ou se o comportamento mudar.

## Bloco 1 (comece por este) — `engine_assets.cpp` (~960 linhas)
Funções (linha aproximada, tamanho):
- `load_archives` (~3574, 166)
- `load_textures` (~3898, 106)
- `load_ddt_texture` (~5906, 54)
- `build_texture_key_index` (~5877, 17)
- `log_bar_names_containing` (~5894, 12)
- `load_core_ui_textures` (~288, 92)
- `load_skybox_cube` (~4004, 29)
- `load_skybox_faces` (~4033, 122)
- `load_water_body_textures` (~4172, 43)
- `load_models` (~4409, 66)
- `get_model_entry` (~7038, 7)
- `load_prt_particles` (~4380, 29)
- `load_animation_clips` (~2224, 63)
- `load_grn_clips` (~2287, 56)
- `load_game_config` (~3740, 36)
- `load_ui_strings` (~5976, 53)
- `load_strings` (~5965, 5)
- `load_lighting` (~5960, 5)

Fora deste bloco, de propósito: `recreate_renderer/init_renderer/release_renderer` (ciclo do renderer → core),
`load_scenario_heightmap` (terreno), `load_game/load_selected_save` (salvamento), `next_/prev_texture|model` (debug).
Por que este primeiro: é isolado (poucos estados compartilhados), já tem logs, e é onde estamos depurando texturas.

## Próximos blocos (ordem sugerida, um por vez)
- `engine_input.cpp` — handle_touch (~234)
- `engine_terrain.cpp` — terrain/heightmap/água
- `engine_campaign.cpp` — campanha/missões/cinemática
- `engine_hud.cpp` — update_in_game_hud (~346)
- `engine_multiplayer_menus.cpp` — menus de multiplayer (18 funções, ~508)
- `engine_menus*.cpp` — menus (~1.800 linhas → dividir em 3: principal/campanha, opções/editor, diálogos)
- `engine_states.cpp` — set_state (~524) — só mover; reescrever em tabela fica para a Etapa 2
- `engine_core.cpp` — initialize/shutdown/update/render

## Resumo medido por tema (heurística de nomes — revise)
| Arquivo | Funções | ~Linhas |
|---|---|---|
| engine_input.cpp | 1 | 234 |
| engine_multiplayer_menus.cpp | 18 | 508 |
| engine_menus.cpp | 105 | 1789 |
| engine_hud.cpp | 2 | 403 |
| engine_assets.cpp | 27 | 1231 |
| engine_terrain.cpp | 7 | 394 |
| engine_campaign.cpp | 8 | 445 |
| engine_gameplay.cpp | 17 | 527 |
| engine_states.cpp | 1 | 524 |
| engine_core.cpp | 7 | 627 |
| engine_misc.cpp | 10 | 279 |

## Maiores funções (candidatas a quebra na Etapa 2)
`set_state` 524 · `update_in_game_hud` 346 · `render` 254 · `handle_touch` 234 · `create_multiplayer_lobby_menu` 217 · `start_game` 210 · `load_scenario_heightmap` 208
