#!/usr/bin/env bash
# PreToolUse (Edit|Write): protege código gerado pelo CubeMX.
set -euo pipefail

input=$(cat)
path=$(printf '%s' "$input" | jq -r '.tool_input.file_path // .tool_input.path // ""')

case "$path" in
  *Drivers/*|*Middlewares/*)
    echo "BLOQUEADO: $path é código gerado. Altere o .ioc e regenere." >&2
    exit 2
    ;;
esac

printf '%s' "$input"
