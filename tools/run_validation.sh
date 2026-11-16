#!/usr/bin/env bash
#
# run_validation.sh - valida o cliente C e o mapa contra o simulador.
#
# Roda:
#   1. simulador uhx65a (unit 0) e generic (unit 1)
#   2. teste host do transporte/mapa (tools/host/test_mbtcp)
#   3. teste do codec no PC (tools/test_codec.c)
#   4. checagens Python: FC3/6/23 e rejeicao de FC16 no perfil uhx65a
#
# Uso:
#   tools/run_validation.sh
#   UHX_PORT=1603 GEN_PORT=1604 tools/run_validation.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMPDIR="${TMPDIR:-/tmp/opencode}"
UHX_PORT="${UHX_PORT:-1503}"
GEN_PORT="${GEN_PORT:-1504}"
UHX_LOG="$TMPDIR/sew_sim_uhx.log"
GEN_LOG="$TMPDIR/sew_sim_gen.log"

mkdir -p "$TMPDIR"
cd "$ROOT"

cleanup() {
    local rc=$?
    if [[ -n "${UHX_PID:-}" ]]; then kill "$UHX_PID" 2>/dev/null || true; fi
    if [[ -n "${GEN_PID:-}" ]]; then kill "$GEN_PID" 2>/dev/null || true; fi
    if [[ $rc -ne 0 ]]; then
        echo
        echo "--- $UHX_LOG (tail) ---"
        tail -n 20 "$UHX_LOG" 2>/dev/null || true
        echo "--- $GEN_LOG (tail) ---"
        tail -n 20 "$GEN_LOG" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

wait_port() {
    local port="$1"
    local i

    for i in $(seq 1 50); do
        if python3 - "$port" <<'PY'
import socket
import sys

s = socket.socket()
s.settimeout(0.1)
try:
    s.connect(("127.0.0.1", int(sys.argv[1])))
    sys.exit(0)
except OSError:
    sys.exit(1)
finally:
    s.close()
PY
        then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

echo "== simuladores (uhx65a:$UHX_PORT generic:$GEN_PORT) =="
python3 tools/sew_sim.py --port "$UHX_PORT" --profile uhx65a --unit 0 \
    --fieldbus-timeout 0 >"$UHX_LOG" 2>&1 &
UHX_PID=$!

python3 tools/sew_sim.py --port "$GEN_PORT" --profile generic --unit 1 \
    --fieldbus-timeout 0 >"$GEN_LOG" 2>&1 &
GEN_PID=$!

if ! wait_port "$UHX_PORT"; then
    echo "ERRO: simulador uhx65a nao subiu (porta $UHX_PORT ocupada?)"
    exit 1
fi
if ! wait_port "$GEN_PORT"; then
    echo "ERRO: simulador generic nao subiu (porta $GEN_PORT ocupada?)"
    exit 1
fi

echo "== teste host do transporte/mapa =="
make -C tools/host >/dev/null
./tools/host/test_mbtcp "$UHX_PORT" "$GEN_PORT"

echo "== teste do codec =="
gcc -std=c99 -Wall -Wextra -Ilib/mbtcp \
    tools/test_codec.c lib/mbtcp/mbtcp_codec.c -lm \
    -o "$TMPDIR/test_codec"
"$TMPDIR/test_codec"

echo "== checagens Python =="
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 3 1 8 >/dev/null
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 6 1 1 >/dev/null
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 23 1 8 1 1 0 0 0 0 0 0 0 >/dev/null

if python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 16 1 1 >/dev/null 2>&1; then
    echo "ERRO: FC16 deveria ser rejeitada no perfil uhx65a"
    exit 1
fi

echo
echo "OK: transporte, mapa, codec e perfis validados."
