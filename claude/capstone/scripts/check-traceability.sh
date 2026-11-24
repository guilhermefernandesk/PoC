#!/usr/bin/env bash
# Valida: tipo(REQ-NNN): descricao #issue
set -euo pipefail

msg_file="${1:-.git/COMMIT_EDITMSG}"
msg=$(cat "$msg_file" 2>/dev/null || true)

if [[ "$msg" =~ ^(feat|fix|test|refactor|docs|chore|ci)\(REQ-[0-9]{3}\) ]]; then
  if [[ "$msg" =~ \#([0-9]+) ]]; then
    echo "OK: requisito e issue presentes"
    exit 0
  fi
fi

echo "ERRO: use tipo(REQ-NNN): descricao #issue" >&2
echo "Recebido: $msg" >&2
exit 2
