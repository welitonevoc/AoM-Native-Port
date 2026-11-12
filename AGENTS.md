# AGENTS.md — leia antes de abrir qualquer arquivo

Porte nativo (C++20, Android, GLES3) de Age of Mythology. Estado atual: `docs/CURRENT_STATE.md`.
Mapa de funções (arquivo → função → linhas): `docs/SYMBOLS.md` (regenerar: `python3 tools/symbol_index.py`).

## Regras

1. **Nunca leia `src/engine/engine.cpp` inteiro** (7.000+ linhas, 200+ métodos) nem `PROGRESSO.md`/`PLANO_P*.md` (histórico).
   Ache a função em `docs/SYMBOLS.md` e leia só o intervalo: `sed -n 'INI,FIMp' src/engine/engine.cpp`.
2. Antes de editar, diga em 3 linhas: qual função, quais linhas, qual mudança.
3. Entregue **diff mínimo**. Não reescreva funções/arquivos inteiros. Não "limpe" o que não foi pedido.
4. **Uma tarefa por conversa.** Outros problemas vão para `docs/CURRENT_STATE.md`, não para o código.
5. **Não invente** nomes de arquivo/textura, formato binário ou API. Na dúvida: adicione um log e peça o logcat.
6. Procure em `SYMBOLS.md` antes de criar função (evita duplicata).
7. **Não rode scripts que reescrevem código** (`fix_engine.py`, `move_skydome.py`...) sem o usuário pedir.
8. Se o GitHub e o que o usuário compila divergirem, pergunte qual vale antes de editar.
9. Terminou? Atualize `docs/CURRENT_STATE.md` (≤ 60 linhas) e rode o gerador de `SYMBOLS.md`.

## Fatos verificados

- Dados do jogo: `<files>/AoM/{data,textures,models,...}/*.bar`; com The Titans também `*2.bar` (montados por cima).
- `TextureHandle` = nome cru do GL; ao recriar o contexto os handles antigos morrem.
- Menu real: `tests/reference/databar2/uipregamenew.xml` (`stateButton`, `stateentry`, `texturecoords`, `hidden`, `z`,
  texto `$$id$$...`, comandos `gadgetReal/gadgetUnReal/subModeEnter/exit`).
- Referências corretas do jogo em `tests/reference/` (XMLs e PNGs). Valide contra elas, não chute.

## Depuração

`adb logcat -d | grep -E "AoM|\[TEX\]|\[GADGET\]|\[DIAG\]|\[FONT\]|\[DDT\]" > log.txt` e cole só ~40 linhas.

## Modelo de pedido

```
Tarefa: <1 frase>
Arquivo/função: <ex.: src/engine/engine.cpp, Engine::load_ddt_texture>
Atual (log/erro): <...>
Esperado: <1 frase>
Restrições: diff mínimo; não ler arquivos inteiros.
```
