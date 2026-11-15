#!/system/bin/sh
# Quick Performance Test (5 min)
# Uso: ./scripts/perf_quick.sh

set -eu

APP_PKG="com.aom.nativeport"
DURATION_SEC=300  # 5 min

echo "=== Quick Perf Test (5 min) ==="
echo "Package: $APP_PKG"
echo ""

am start -n $APP_PKG/.MainActivity
sleep 3

START=$(date +%s)
FRAME=0
TOTAL_FPS=0
MIN_FPS=999
MAX_FPS=0

while true; do
    NOW=$(date +%s)
    ELAPSED=$((NOW - START))
    if [ $ELAPSED -ge $DURATION_SEC ]; then break; fi

    # FPS do logcat
    FPS=$(logcat -d -s AoM-Engine 2>/dev/null | tail -1 | grep -oE 'FPS: [0-9]+' | awk '{print $2}' || echo "0")
    if [ "$FPS" -gt 0 ]; then
        FRAME=$((FRAME + 1))
        TOTAL_FPS=$((TOTAL_FPS + FPS))
        [ "$FPS" -lt "$MIN_FPS" ] && MIN_FPS=$FPS
        [ "$FPS" -gt "$MAX_FPS" ] && MAX_FPS=$FPS
        printf "\r[%ds] FPS atual=%d | Média=%.1f | Min=%d | Max=%d" "$ELAPSED" "$FPS" "$(echo "scale=1; $TOTAL_FPS/$FRAME" | bc)" "$MIN_FPS" "$MAX_FPS"
    fi
    sleep 2
done

echo ""
echo "=== RESULTADO (5 min) ==="
AVG=$(echo "scale=1; $TOTAL_FPS/$FRAME" | bc)
echo "Amostras: $FRAME"
echo "FPS médio: $AVG"
echo "FPS min: $MIN_FPS"
echo "FPS max: $MAX_FPS"
echo ""
if [ "$AVG" -ge 55 ] 2>/dev/null; then
    echo "✅ PASS: FPS médio >= 55"
else
    echo "❌ FAIL: FPS médio < 55"
fi
if [ "$MIN_FPS" -ge 30 ] 2>/dev/null; then
    echo "✅ PASS: FPS mínimo >= 30"
else
    echo "❌ FAIL: FPS mínimo < 30"
fi

# Memory quick check
echo ""
echo "Memory snapshot:"
dumpsys meminfo $APP_PKG 2>/dev/null | grep -E 'TOTAL|Native Heap|Dalvik Heap' | head -3