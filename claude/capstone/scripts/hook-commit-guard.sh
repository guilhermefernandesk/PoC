#!/usr/bin/env bash
# PreToolUse: barra git commit sem rastreabilidade.
set -euo pipefail

input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')

case "$cmd" in
  *"git commit"*)
    msg=$(printf '%s' "$cmd" | sed -n "s/.*-m [\"']\([^\"']*\)[\"'].*/\1/p" || true)
    if [[ -z "$msg" && -f .git/COMMIT_EDITMSG ]]; then
      msg=$(cat .git/COMMIT_EDITMSG)
    fi
    if ! ./scripts/check-traceability.sh <(printf '%s' "$msg"); then
      echo "BLOQUEADO: commit sem REQ-NNN e #issue" >&2
      exit 2
    fi
    ;;
esac

printf '%s' "$input"
