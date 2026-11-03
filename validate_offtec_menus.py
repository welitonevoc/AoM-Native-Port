#!/usr/bin/env python3
"""Validação estática do módulo offtec_menus.py — sem dependências externas."""

import ast
import sys
from pathlib import Path

MODULE = Path(__file__).resolve().parent / "offtec_menus.py"
source = MODULE.read_text(encoding="utf-8")

print("=== 1. Análise AST (sintaxe) ===")
tree = ast.parse(source, filename=str(MODULE))
print(f"OK: AST parseado com {len(tree.body)} statement(s) no nivel do modulo")

# Coletar nomes definidos no nivel do modulo
defined = set()
for node in tree.body:
    if isinstance(node, ast.FunctionDef):
        defined.add(node.name)
    elif isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name):
                defined.add(target.id)
    elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
        defined.add(node.target.id)

expected = {
    "LOCALES", "LOCALE_NAMES", "_ROWS", "_CATALOGS", "CONFIG_PATH",
    "load_locale", "save_locale", "get_locale", "locale_choices",
    "locale_from_choice", "tr", "ask_prompt",
    "ask_confirm", "MAIN_MENU_ITEMS", "HISTORY_MENU_ITEMS",
    "OPTIONAL_MENU_ITEMS", "HELP_TEXT_KEYS", "HELP_TEXT_KEYS_HISTORY",
    "SETTINGS_PROMPTS", "show_menu", "help_texts", "settings_menu",
    "history_menu", "help_menu", "main_flow",
}

missing = expected - defined
extra = defined - expected - {"__name__", "__doc__", "__file__"}
if missing:
    print(f"FALHA: funcoes/constantes faltando: {sorted(missing)}")
else:
    print("OK: todas as funcoes/constantes esperadas estao definidas")
if extra:
    print(f"AVISO: nomes extras no nivel do modulo: {sorted(extra)}")

print("\n=== 2. Imports necessarios ===")
imports = set()
for node in ast.walk(tree):
    if isinstance(node, ast.Import):
        for alias in node.names:
            imports.add(alias.name)
    elif isinstance(node, ast.ImportFrom):
        if node.module:
            imports.add(node.module)
for imp in ("json", "os", "pathlib", "typing"):
    if imp in imports:
        print(f"OK: import {imp} presente")
    else:
        print(f"FALTA: import {imp} ausente")

print("\n=== 3. Estrutura do _ROWS ===")
for node in tree.body:
    if isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == "_ROWS":
                if isinstance(node.value, ast.Dict):
                    keys = node.value.keys
                    print(f"OK: _ROWS tem {len(keys)} entradas")
                    for k in keys:
                        if isinstance(k, ast.Constant):
                            print(f"  - {k.value}")
                break

print("\n=== 4. Estrutura dos menus ===")
for node in tree.body:
    if isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id in ("MAIN_MENU_ITEMS", "HISTORY_MENU_ITEMS", "OPTIONAL_MENU_ITEMS"):
                if isinstance(node.value, ast.List):
                    items = node.value.elts
                    print(f"OK: {target.id} tem {len(items)} item(ns)")
                    for item in items:
                        if isinstance(item, ast.Tuple) and len(item.elts) == 2:
                            t0 = item.elts[0].value if isinstance(item.elts[0], ast.Constant) else "?"
                            t1 = item.elts[1].value if isinstance(item.elts[1], ast.Constant) else "?"
                            print(f"  - ({t0!r}, {t1!r})")

print("\n=== 5. Verificacao de main_flow ===")
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name == "main_flow":
        has_while = any(isinstance(n, ast.While) for n in ast.walk(node))
        has_if = any(isinstance(n, ast.If) for n in ast.walk(node))
        print(f"OK: main_flow contem while={has_while}, if={has_if}")

print("\n=== 6. Verificacao de __main__ ===")
has_main = any(
    isinstance(node, ast.If) and
    isinstance(node.test, ast.Compare) and
    isinstance(node.test.left, ast.Name) and
    node.test.left.id == "__name__"
    for node in tree.body
)
print(f"OK: bloco if __name__ == '__main__' presente={has_main}")

print("\n=== VALIDACAO CONCLUIDA ===")