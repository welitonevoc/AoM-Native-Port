#!/usr/bin/env python3
"""
symbol_index.py - gera docs/SYMBOLS.md: um mapa compacto "arquivo -> funções (linha inicial-final)".

Objetivo: uma IA (ou você) lê ESTE arquivo (poucas centenas de linhas) e depois abre só o trecho
necessário:   sed -n '2855,3325p' src/engine/engine.cpp
em vez de ler engine.cpp inteiro (6.700 linhas).

Uso (na raiz do projeto):   python3 tools/symbol_index.py
Detecta definições `Tipo Classe::metodo(...) {` e funções livres no nível do arquivo.
"""
import os, re, sys

ROOTS = ["src", "include"]
EXTS = (".cpp", ".cc", ".h", ".hpp")
DEF = re.compile(r'^(?:[\w:<>,\*&~\s]+?\s+)?(?:(\w+)::)?(~?\w+)\s*\([^;]*$')
SKIP_NAMES = {"if", "for", "while", "switch", "return", "else", "catch", "sizeof"}

def functions(lines):
    out, depth, i = [], 0, 0
    while i < len(lines):
        l = lines[i]
        if depth == 0 and l and not l[0].isspace() and not l.startswith(("#", "//", "}", "namespace", "using", "struct", "class", "enum", "typedef")):
            m = DEF.match(l.rstrip())
            if m and m.group(2) not in SKIP_NAMES:
                # junta linhas até achar '{' (assinaturas multilinha)
                j, sig = i, l
                while "{" not in sig and ";" not in sig and j + 1 < len(lines) and j - i < 12:
                    j += 1; sig += lines[j]
                if "{" in sig and ";" not in sig.split("{")[0]:
                    # acha o fim por contagem de chaves
                    d, k, started = 0, i, False
                    while k < len(lines):
                        for ch in lines[k]:
                            if ch == "{": d += 1; started = True
                            elif ch == "}": d -= 1
                        if started and d == 0: break
                        k += 1
                    name = (m.group(1) + "::" if m.group(1) else "") + m.group(2)
                    out.append((name, i + 1, k + 1))
                    i = k + 1
                    continue
        i += 1
    return out

def main():
    rows = []
    for root in ROOTS:
        for dp, _, fs in os.walk(root):
            for f in sorted(fs):
                if not f.endswith(EXTS): continue
                p = os.path.join(dp, f).replace("\\", "/")
                try: lines = open(p, encoding="utf-8", errors="ignore").read().split("\n")
                except OSError: continue
                rows.append((p, len(lines), functions(lines) if f.endswith((".cpp", ".cc")) else []))
    rows.sort()
    os.makedirs("docs", exist_ok=True)
    with open("docs/SYMBOLS.md", "w", encoding="utf-8") as o:
        o.write("# SYMBOLS (gerado por tools/symbol_index.py - NAO edite à mão)\n\n")
        o.write("Como usar: ache a função aqui e leia só o intervalo: `sed -n 'INI,FIMp' arquivo`.\n\n")
        for p, n, fn in rows:
            big = " **GRANDE**" if n > 1500 else ""
            o.write(f"## {p} ({n} linhas){big}\n")
            for name, a, b in fn:
                o.write(f"- `{name}` {a}-{b} ({b - a + 1})\n")
            o.write("\n")
    print(f"docs/SYMBOLS.md: {len(rows)} arquivos, {sum(len(r[2]) for r in rows)} funções")

if __name__ == "__main__":
    main()
