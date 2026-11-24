#!/usr/bin/env bash
# Gate determinístico de scripts shell.
set -euo pipefail

if command -v shellcheck >/dev/null 2>&1; then
  shellcheck scripts/*.sh
else
  echo "shellcheck ausente: usando bash -n como fallback" >&2
  for f in scripts/*.sh; do
    bash -n "$f"
  done
fi
