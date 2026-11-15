#!/system/bin/sh
# -------------------------------------------------------------------------
# monitor_import.sh
# Acompanha o progresso de qualquer importação de dados com barra de
# progresso, tempo decorrido e contador de linhas processadas.
#
# Uso:
#   ./scripts/monitor_import.sh <comando> [args...]
#
# Exemplo:
#   ./scripts/monitor_import.sh python3 scripts/extract_menu_assets.py
# -------------------------------------------------------------------------

set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/monitor_import.py" "$@"
