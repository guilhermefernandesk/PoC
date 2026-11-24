#!/usr/bin/env bash
# PreToolUse: reduz saída de testadores longos antes de entrar no contexto.
set -euo pipefail

input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')

case "$cmd" in
  *ceedling*|*ctest*|*pytest*|*make\ test*)
    filtered="set -o pipefail; ${cmd} 2>&1 | tail -n 100"
    printf '{"hookSpecificOutput":{"hookEventName":"PreToolUse","updatedInput":{"command":%s}}}' \
      "$(printf '%s' "$filtered" | jq -Rs .)"
    ;;
  *)
    printf '%s' "$input"
    ;;
esac
