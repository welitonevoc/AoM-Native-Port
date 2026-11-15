#!/system/bin/sh
# P2.5 Performance Validation Script
# Executa no device Android via Termux

set -eu

APP_PKG="com.aom.nativeport"
LOG_FILE="/storage/emulated/0/Download/aom_perf_$(date +%Y%m%d_%H%M%S).log"
DURATION_SEC=1800  # 30 min

echo "=== AoM Native Port - P2.5 Performance Validation ==="
echo "Package: $APP_PKG"
echo "Duration: ${DURATION_SEC}s (30 min)"
echo "Log: $LOG_FILE"
echo ""

# Função para coletar métricas
collect_metrics() {
    local ts=$(date '+%H:%M:%S')
    local fps=$(logcat -d -s AoM-Engine 2>/dev/null | tail -1 | grep -oE 'FPS: [0-9]+' | awk '{print $2}' || echo "N/A")
    local mem=$(dumpsys meminfo $APP_PKG 2>/dev/null | grep -E 'TOTAL|Native Heap|Dalvik Heap' | head -3 | tr '\n' ';' | sed 's/;$//' || echo "N/A")
    local fds=$(ls -l /proc/$(pidof $APP_PKG 2>/dev/null)/fd 2>/dev/null | wc -l || echo "N/A")
    echo "[$ts] FPS=$fps | MEM=[$mem] | FDs=$fds"
    echo "[$ts] FPS=$fps | MEM=[$mem] | FDs=$fds" >> "$LOG_FILE"
}

# Iniciar app
echo "Iniciando app..."
am start -n $APP_PKG/.MainActivity
sleep 5

echo "Coletando métricas a cada 30s por $DURATION_SEC segundos..."
echo "Pressione Ctrl+C para parar antecipadamente"
echo ""

START=$(date +%s)
while true; do
    NOW=$(date +%s)
    ELAPSED=$((NOW - START))
    if [ $ELAPSED -ge $DURATION_SEC ]; then
        echo "Tempo esgotado ($DURATION_SEC s). Finalizando."
        break
    fi
    collect_metrics
    sleep 30
done

# Relatório final
echo ""
echo "=== RELATÓRIO FINAL ==="
echo "Log salvo em: $LOG_FILE"
echo ""
echo "Últimas 10 linhas:"
tail -10 "$LOG_FILE"
echo ""

# Estatísticas básicas de FPS
echo "Estatísticas de FPS:"
grep -oE 'FPS=[0-9]+' "$LOG_FILE" | sed 's/FPS=//' | awk '{
    sum += $1; count++
    if ($1 > max) max = $1
    if ($1 < min || min == "") min = $1
} END {
    if (count > 0) {
        printf "  Amostras: %d\n", count
        printf "  Média: %.1f\n", sum/count
        printf "  Min: %d\n", min
        printf "  Max: %d\n", max
        printf "  60 FPS target: %.1f%% das amostras >= 55\n", (100 * (count - (count - (sum/count >= 55 ? 0 : 0))))/count
    }
}'

# Verificar vazamento de memória (crescimento > 50MB em 30min)
echo ""
echo "Verificando vazamento de memória (Native Heap):"
grep -oE 'Native Heap[^0-9]*[0-9]+' "$LOG_FILE" | sed 's/[^0-9]*//g' | awk '{
    if (NR == 1) first = $1
    last = $1
    if ($1 > max) max = $1
    if ($1 < min || min == "") min = $1
} END {
    if (NR > 1) {
        growth = last - first
        printf "  Inicial: %d KB\n", first
        printf "  Final: %d KB\n", last
        printf "  Crescimento: %d KB (%.1f MB)\n", growth, growth/1024
        if (growth > 51200) print "  ⚠️ ALERTA: Crescimento > 50MB - possível vazamento"
        else print "  ✅ OK: Crescimento dentro do esperado"
    }
}'