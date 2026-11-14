#!/usr/bin/env python3
"""
tools/generate_unit_visuals_table.py — Extrai nomes autênticos de BRGs do models.bar/models2.bar
e gera a tabela de mapeamento C++ para Engine::resolve_unit_visual.

Baseado na biblioteca ptasev/Age-of-Mythology e no formato de assets clássico.
"""

import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
from bar_tool import BarArchive

BAR_PATHS = [
    "/sdcard/AoM/models/models.bar",
    "/sdcard/AoM/models/models2.bar",
    "/storage/emulated/0/AoM/models/models.bar",
    "/storage/emulated/0/AoM/models/models2.bar",
    "assets_bar/models/models.bar",
    "assets_bar/models/models2.bar"
]

def find_bars():
    found = []
    seen = set()
    for p in BAR_PATHS:
        if os.path.exists(p):
            canon = os.path.realpath(p)
            if canon not in seen:
                seen.add(canon)
                found.append(p)
    return found

def classify_brg(filename):
    """Retorna (categoria, nome_base, acao, is_low)"""
    is_low = "low\\" in filename.lower() or "low/" in filename.lower() or "cin " in filename.lower()
    clean = filename.replace("\\", "/")
    if is_low:
        return None  # Preferimos malhas de alta definição

    base = clean.split("/")[-1]
    if not base.endswith(".brg"):
        return None

    # Detectar ação/pose
    action = "other"
    for a in ["_idlea", "_idle", "_walka", "_walk", "_attacka", "_attack", "_flaila", "_deatha"]:
        if a in base.lower():
            action = a.strip("_")
            break

    return base, action

def main():
    bars = find_bars()
    if not bars:
        print("[ERRO] Nenhum models.bar encontrado nos caminhos padrão.", file=sys.stderr)
        sys.exit(1)

    print(f"[*] Escaneando arquivos BAR: {bars}")

    entries_by_keyword = defaultdict(list)

    keywords = [
        # Gregos
        "hoplite", "toxotes", "hippikon", "prodromos", "hypaspist", "petrobolos", "helepole",
        "villager g", "minotaur", "cyclops", "manticore", "hydra", "colossus", "medusa",
        "pegasus", "centaur", "nemean", "chimera", "scylla",
        # Egípcios
        "spearman", "axeman", "slinger", "chariot", "camel", "elephant", "catapult e",
        "villager e", "anubite", "sphinx", "wadjet", "mummy", "scarab", "scorpion", "roc",
        # Nórdicos
        "ulfsark", "throwing axeman", "huskarl", "jarl", "raider", "ballista n", "portable ram",
        "villager n", "valkyrie", "frost giant", "fire giant", "mountain giant", "einherjar",
        # Atlantes (The Titans)
        "murmillo", "arcus", "katapeltes", "turma", "contarius", "cheiroballista", "fanatic",
        "destroyer", "citizen", "automaton", "promethean", "stymphalian", "satyr", "behemoth",
        # Heróis e Campanha
        "arkantos", "ajax", "chiron", "amanra", "setna", "reginleif", "kastor", "agprompt", "pirate", "kraken"
    ]

    for bar_path in bars:
        bar = BarArchive(bar_path)
        print(f"[*] {bar_path}: {len(bar.entries)} entradas catalogadas")
        for e in bar.entries:
            info = classify_brg(e.filename)
            if not info:
                continue
            base, action = info
            base_lower = base.lower()
            for kw in keywords:
                if kw in base_lower:
                    entries_by_keyword[kw].append((base, action))

    print("\n" + "=" * 60)
    print("TABELA DE MODELOS BRG IDENTIFICADOS (RESOLVE_UNIT_VISUAL)")
    print("=" * 60)

    resolved_table = {}

    for kw in keywords:
        candidates = entries_by_keyword[kw]
        if not candidates:
            continue
        # Ordenar por prioridade: idlea > idle > walka > walk > attacka > other
        priority_order = {"idlea": 1, "idle": 2, "walka": 3, "walk": 4, "attacka": 5, "attack": 6, "other": 7, "flaila": 8, "deatha": 9}
        unique_cands = sorted(list(set(candidates)), key=lambda x: priority_order.get(x[1], 99))
        best = unique_cands[0][0]
        resolved_table[kw] = [c[0] for c in unique_cands]
        print(f"[{kw.upper()}] -> Preferido: '{best}' (Total variantes: {len(unique_cands)})")

    # Gerar código C++ pronto para incluir
    out_cpp_path = "src/engine/generated_unit_visuals.inc"
    with open(out_cpp_path, "w", encoding="utf-8") as f:
        f.write("// generated_unit_visuals.inc — Gerado automaticamente por tools/generate_unit_visuals_table.py\n")
        f.write("// Contém os arquivos BRG de mais alta prioridade catalogados em models.bar/models2.bar\n\n")
        for kw, files in sorted(resolved_table.items()):
            file_list = ", ".join(f'"{fn}"' for fn in files[:4])
            f.write(f'    if (low.find("{kw}") != std::string::npos) {{\n')
            f.write(f'        candidates = {{{file_list}}};\n')
            f.write('    }\n')

    print(f"\n[OK] Tabela C++ gerada com sucesso em: {out_cpp_path}")

if __name__ == "__main__":
    main()
