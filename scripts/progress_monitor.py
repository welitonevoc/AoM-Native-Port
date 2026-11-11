#!/usr/bin/env python3
"""
progress_monitor.py
Utilitário de barra de progresso com ETA para acompanhar importações de dados.
Uso:
    from scripts.progress_monitor import ProgressMonitor

    pm = ProgressMonitor(total=1000, label="Extraindo assets")
    for i in range(1000):
        # ... trabalho ...
        pm.update(i + 1)
    pm.close()
"""

import sys
import time
import os
from typing import Optional


class ProgressMonitor:
    """Barra de progresso com porcentagem, taxa e tempo estimado de conclusão."""

    def __init__(
        self,
        total: int,
        label: str = "Processando",
        width: int = 40,
        stream=None,
        update_every: int = 1,
    ):
        self.total = max(total, 1)
        self.label = label
        self.width = width
        self.stream = stream or sys.stderr
        self.update_every = max(update_every, 1)
        self._start = time.monotonic()
        self._last = self._start
        self._count = 0
        self._is_tty = hasattr(self.stream, "isatty") and self.stream.isatty()
        self._done = False

    def _format_eta(self, seconds: float) -> str:
        if seconds < 0:
            seconds = 0
        m, s = divmod(int(seconds), 60)
        h, m = divmod(m, 60)
        if h:
            return f"{h:d}h{m:02d}m{s:02d}s"
        if m:
            return f"{m:d}m{s:02d}s"
        return f"{s:02d}s"

    def _render(self, count: int, force: bool = False) -> None:
        now = time.monotonic()
        if not force and (count - self._count) < self.update_every and not self._done:
            return
        elapsed = now - self._start
        rate = count / elapsed if elapsed > 0 else 0.0
        pct = count / self.total * 100
        pct = min(pct, 100.0)

        filled = int(self.width * count // self.total)
        bar = "#" * filled + "-" * (self.width - filled)

        if rate > 0:
            remaining = (self.total - count) / rate
            eta = self._format_eta(remaining)
        else:
            eta = "calculando..."

        line = (
            f"\r{self.label}: [{bar}] {pct:5.1f}% "
            f"({count}/{self.total}) {rate:6.1f} it/s ETA {eta}"
        )
        self.stream.write(line)
        self.stream.flush()
        self._count = count
        self._last = now

    def update(self, count: int, force: bool = False) -> None:
        """Atualiza o progresso para `count` itens processados."""
        self._render(count, force=force)

    def increment(self, n: int = 1, force: bool = False) -> None:
        """Incrementa o progresso em `n` itens."""
        self._render(self._count + n, force=force)

    def close(self, message: Optional[str] = None) -> None:
        """Finaliza a barra, imprime linha nova e mensagem opcional."""
        if not self._done:
            self._render(self.total, force=True)
            self._done = True
        self.stream.write("\n")
        self.stream.flush()
        if message:
            self.stream.write(f"{message}\n")
            self.stream.flush()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def simple_progress(total: int, label: str = "Processando") -> ProgressMonitor:
    """Cria um ProgressMonitor já iniciado."""
    return ProgressMonitor(total=total, label=label)


if __name__ == "__main__":
    import time as _t

    total = 50
    with ProgressMonitor(total=total, label="Importação de dados") as pm:
        for i in range(total):
            _t.sleep(0.05)
            pm.update(i + 1)
    print("Concluído!")
