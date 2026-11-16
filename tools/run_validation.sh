#!/usr/bin/env bash
#
# run_validation.sh - valida o cliente C, o mapa e o codec contra o simulador.
#
# Roda:
#   1. simulador uhx65a (unit 0)
#   2. teste host do transporte/mapa (tools/host/test_mbtcp)
#   3. teste do codec no PC (tools/test_codec.c)
#   4. checagens Python no simulador (FC03/FC06/FC23 e rejeicao de FC16)
#
# Uso:
#   bash tools/run_validation.sh
#   UHX_PORT=1603 bash tools/run_validation.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMPDIR="${TMPDIR:-/tmp/opencode}"
UHX_PORT="${UHX_PORT:-1503}"
UHX_LOG="$TMPDIR/sew_sim_uhx.log"

mkdir -p "$TMPDIR"
cd "$ROOT"

cleanup() {
    local rc=$?
    if [[ -n "${UHX_PID:-}" ]]; then kill "$UHX_PID" 2>/dev/null || true; fi
    if [[ $rc -ne 0 ]]; then
        echo
        echo "--- $UHX_LOG (tail) ---"
        tail -n 20 "$UHX_LOG" 2>/dev/null || true
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

echo "== simulador (uhx65a:$UHX_PORT) =="
python3 tools/sew_sim.py --port "$UHX_PORT" --profile uhx65a --unit 0 \
    --fieldbus-timeout 0 >"$UHX_LOG" 2>&1 &
UHX_PID=$!

sleep 0.2
if ! kill -0 "$UHX_PID" 2>/dev/null; then
    echo "ERRO: simulador uhx65a encerrou (porta $UHX_PORT ocupada?)"
    exit 1
fi
if ! wait_port "$UHX_PORT"; then
    echo "ERRO: simulador uhx65a nao respondeu na porta $UHX_PORT"
    exit 1
fi

echo "== teste host do transporte/mapa =="
make -C tools/host clean >/dev/null
make -C tools/host >/dev/null
./tools/host/test_mbtcp "$UHX_PORT"

echo "== teste do codec =="
gcc -std=c99 -Wall -Wextra -Werror -Ilib/mbtcp \
    tools/test_codec.c lib/mbtcp/mbtcp_codec.c -lm \
    -o "$TMPDIR/test_codec"
"$TMPDIR/test_codec"

echo "== checagens Python (oraculo independente) =="
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 3 1 8 >/dev/null
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 6 1 1 >/dev/null
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 23 1 4 3 0 42 >/dev/null

set +e
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 16 1 1 >/dev/null 2>&1
FC16_RC=$?
set -e
if [[ $FC16_RC -ne 1 ]]; then
    echo "ERRO: FC16 deveria ser rejeitada com excecao Modbus (rc=$FC16_RC)"
    exit 1
fi

echo "== injecao de falhas + sanitizers =="
make -C tools/host test_faults_asan >/dev/null
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_faults_asan

make -C tools/host test_mbtcp_asan >/dev/null
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_mbtcp_asan "$UHX_PORT"

echo
echo "OK: transporte, mapa, codec, falhas e perfil uhx65a validados."
