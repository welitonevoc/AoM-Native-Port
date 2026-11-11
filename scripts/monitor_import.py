#!/usr/bin/env python3
"""
monitor_import.py
Acompanha o progresso de qualquer importação de dados em tempo real.

Uso:
    python3 scripts/monitor_import.py <comando> [args...]

Exemplos:
    python3 scripts/monitor_import.py python3 scripts/extract_menu_assets.py
    python3 scripts/monitor_import.py ./scripts/some_import.sh

O monitor exibe:
  - Tempo decorrido
  - Contador de linhas processadas (stdout/stderr)
  - Spinner de atividade
  - Estimativa simples com base na taxa de linhas/segundo

Pressione Ctrl+C para interromper.
"""

import sys
import subprocess
import time
import shlex
import os
from typing import List, Optional


def spinner(frames: int) -> str:
    chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"
    return chars[frames % len(chars)]


def format_elapsed(seconds: float) -> str:
    m, s = divmod(int(seconds), 60)
    h, m = divmod(m, 60)
    if h:
        return f"{h:d}h{m:02d}m{s:02d}s"
    if m:
        return f"{m:d}m{s:02d}s"
    return f"{s:02d}s"


def run_with_monitor(cmd: List[str]) -> int:
    label = " ".join(shlex.quote(c) for c in cmd)
    start = time.monotonic()
    line_count = 0
    last_render = start
    frame = 0

    print(f"▶ Iniciando importação: {label}\n", file=sys.stderr)

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except FileNotFoundError:
        print(f"ERRO: comando não encontrado: {cmd[0]}", file=sys.stderr)
        return 127

    assert proc.stdout is not None

    try:
        for raw_line in proc.stdout:
            line = raw_line.rstrip("\n")
            line_count += 1
            now = time.monotonic()

            # Renderiza no máximo a cada 100 ms para não poluir o terminal
            if now - last_render >= 0.1:
                elapsed = now - start
                rate = line_count / elapsed if elapsed > 0 else 0.0
                sys.stderr.write(
                    f"\r{spinner(frame)}  Importando... {format_elapsed(elapsed)}  "
                    f"linhas: {line_count}  ({rate:5.1f} lin/s)"
                )
                sys.stderr.flush()
                last_render = now
                frame += 1

            # Re-imprime a linha original para o usuário ver o conteúdo real
            print(line, file=sys.stderr)

        proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("\n\n⚠ Importação interrompida pelo usuário.", file=sys.stderr)
        return 130

    elapsed = time.monotonic() - start
    sys.stderr.write(
        f"\r{' ' * 80}\r"  # limpa a linha do spinner
    )
    sys.stderr.flush()

    if proc.returncode == 0:
        print(
            f"\n✓ Importação concluída com sucesso em {format_elapsed(elapsed)} "
            f"({line_count} linhas processadas)",
            file=sys.stderr,
        )
    else:
        print(
            f"\n✗ Importação falhou (código {proc.returncode}) após {format_elapsed(elapsed)} "
            f"({line_count} linhas processadas)",
            file=sys.stderr,
        )

    return proc.returncode


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    if not argv:
        print("Uso: python3 scripts/monitor_import.py <comando> [args...]", file=sys.stderr)
        print("\nExemplo:", file=sys.stderr)
        print("  python3 scripts/monitor_import.py python3 scripts/extract_menu_assets.py", file=sys.stderr)
        return 1
    return run_with_monitor(argv)


if __name__ == "__main__":
    sys.exit(main())
