#!/usr/bin/env bash
# Reduz logs de HIL ao sinal que importa para o agente.
set -euo pipefail

grep -hE 'FAIL|ERROR|assert|timeout|NACK|watchdog|brownout' reports/hil/*.log 2>/dev/null \
  | tail -n 60 \
  || echo "Nenhuma falha encontrada nos logs de HIL."
