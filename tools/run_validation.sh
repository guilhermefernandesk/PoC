#!/usr/bin/env bash
#
# run_validation.sh - valida o transporte e o driver UHX15A contra o simulador.
#
# Roda:
#   1. simulador uhx15a (unit 0)
#   2. teste host do driver UHX15A (tools/host/test_uhx15a)
#   3. checagens Python no simulador (FC03/FC06/FC23 e rejeicao de FC16)
#   4. injecao de falhas + ASan/UBSan
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
        tail -n 30 "$UHX_LOG" 2>/dev/null || true
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

check_read() {
    local address="$1"
    local quantity="$2"
    local expected="$3"
    local output

    output="$(python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 3 \
        "$address" "$quantity")"
    if ! printf '%s\n' "$output" | grep -q -- "$expected"; then
        echo "ERRO: FC03 addr=$address qty=$quantity nao contem '$expected'"
        printf '%s\n' "$output"
        exit 1
    fi
}

check_exception() {
    local code="$1"
    shift
    local output
    local rc

    set +e
    output="$(python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 "$@" 2>&1)"
    rc=$?
    set -e

    if [[ $rc -ne 1 ]] || ! printf '%s\n' "$output" | grep -q "EXCECAO $code:"; then
        echo "ERRO: esperava excecao Modbus $code para: $*"
        printf '%s\n' "$output"
        exit 1
    fi
}

echo "== simulador (uhx15a:$UHX_PORT) =="
python3 tools/sew_sim.py --port "$UHX_PORT" --profile uhx15a --unit 0 \
    --fieldbus-timeout 0 >"$UHX_LOG" 2>&1 &
UHX_PID=$!

sleep 0.2
if ! kill -0 "$UHX_PID" 2>/dev/null; then
    echo "ERRO: simulador uhx15a encerrou (porta $UHX_PORT ocupada?)"
    exit 1
fi
if ! wait_port "$UHX_PORT"; then
    echo "ERRO: simulador uhx15a nao respondeu na porta $UHX_PORT"
    exit 1
fi

echo "== teste host do driver UHX15A =="
make -C tools/host clean >/dev/null
make -C tools/host UHX_PORT="$UHX_PORT" >/dev/null
./tools/host/test_uhx15a

echo "== checagens Python (oraculo independente) =="
check_read 1 16 '0x0001 (1)'

python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 6 1 1 >/dev/null

FC23_OUT="$(python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 23 3 2 3 0 45)"
if ! printf '%s\n' "$FC23_OUT" | grep -q -- '0x002D (45)'; then
    echo "ERRO: FC23 nao devolveu o setpoint 45"
    printf '%s\n' "$FC23_OUT"
    exit 1
fi

check_read 3 2 '0x002D (45)'   # setpoint do azimute
check_read 5 2 '0x002D (45)'   # actual do azimute (control=1)

python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 6 1 0 >/dev/null
check_read 5 2 '0x0000 (0)'    # actual cai com control=0

python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 6 1 1 >/dev/null
check_read 5 2 '0x002D (45)'   # actual volta com control=1

set +e
python3 tools/mbtest.py 127.0.0.1 "$UHX_PORT" 0 16 1 1 >/dev/null 2>&1
FC16_RC=$?
set -e
if [[ $FC16_RC -ne 1 ]]; then
    echo "ERRO: FC16 deveria ser rejeitada com excecao Modbus (rc=$FC16_RC)"
    exit 1
fi

check_exception 2 6 2 5
check_exception 2 23 1 1 2 5
check_read 2 1 '0x0001 (1)'   # status nao foi alterado pela tentativa de escrita

echo "== injecao de falhas + sanitizers =="
make -C tools/host test_faults_asan UHX_PORT="$UHX_PORT" >/dev/null
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_faults_asan

make -C tools/host test_uhx15a_asan UHX_PORT="$UHX_PORT" >/dev/null
ASAN_OPTIONS=detect_leaks=0 ./tools/host/test_uhx15a_asan

echo
echo "OK: transporte, driver UHX15A, falhas e perfil uhx15a validados."
