# CURRENT_STATE — baseado no commit 445d1ad (11/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ `engine.h` sem as duplicatas antigas (enum e métodos de menu).
- ✅ Resolvedor tolerante de nomes de textura (`texture_key`, logs `[TEX]`) em `load_ddt_texture`.
- ✅ DDT formato 5 com escolha automática de layout (`ddt5_auto.h`).
- ✅ Toque: `interactive` + percurso reverso dos botões; `color == 0` não vira mais branco opaco.
- ✅ `recreate_renderer()` limpa `texture_cache_`. ❓ conferir se os demais handles (água, terreno, UI) também são zerados.
- ✅ `ui_layout.cpp`: lê `stateentry`, `hidden`, `statebutton` e remove o prefixo `$$id$$` do texto.
- ✅ Fonte/texto na UI funciona (captura de 08/11).

## Em aberto
1. ⬜ **DDT formato 6**: o `ddt6_auto.h` NÃO está no repo; formato 6 ainda usa o decodificador antigo "RGB444".
   ❓ Provável causa do retângulo azul no portal (gadget `BordersBackground`, só textura, sem `backgroundcolor`).
2. ⬜ **`textures2.bar` / `data2.bar` não são copiados** por `MyNativeActivity.java` (só `textures.bar` e `data.bar`, linhas ~311-313).
   ❓ Texturas do portal do Titans (ex.: "AOM Titan Logo") provavelmente estão em `textures2.bar`. Verificar: `adb shell ls` na pasta `textures/` do app.
3. ⬜ **Layout XMB**: faltam `texturecoords` e os comandos `gadgetReal/gadgetUnReal/subModeEnter/exit` (nenhuma ocorrência em `src/`).
4. ❓ **Dois sistemas de menu** coexistem: XMB (`ui_layout.cpp`), `src/ui/menu_system.cpp` (novo) e os `create_*_menu` à mão. Definir qual manda no MAIN_MENU.
5. ❓ **Unidades quebradas em pedaços** (BRG): clip/malha divergentes, só `meshes[0]`. Fase 0 de testes (A/B estático × animado) pendente.
6. ❓ **Tela cheia de água na cinemática de missão**: câmera da cinemática (`CampaignMissionManager`/`CameraTrack`). `CameraTrack::update` sobrescreve aspect/near/far.
7. ❓ **Dados**: se o app mostrar "DADOS DO JOGO NAO ENCONTRADOS", checar `adb shell ls -R .../files/AoM` e o log `Discovered AoM data`.

## Riscos do repositório
- ⚠ `assets_bar/textures/textures.bar` (85 MB) e `textures2.bar` (34 MB) estão no GitHub público. São dados do jogo comercial:
  possível violação de direitos autorais e repo pesado. Tirar do Git (e do histórico) e usar `.gitignore`.
- Raiz poluída: `PLANO_P0..P5.md`, `PROGRESSO.md`, `fix_engine.py`, `offtec_menus.py`... Mover para `docs/archive/` e `tools/`.

## Próximos passos (ordem)
1. Copiar `textures2.bar`/`data2.bar` no `MyNativeActivity` e conferir o log `[TEX]`.
2. Aplicar `ddt6_auto.h` (formato 6) e ver se o retângulo azul some; coletar `[GADGET]` do `uipregamenew`.
3. Definir qual sistema de menu é o oficial; implementar `texturecoords` + comandos básicos.
4. Só depois: dividir `engine.cpp` (ver `ENGINE_SPLIT_PLAN.md`), um bloco por vez.
