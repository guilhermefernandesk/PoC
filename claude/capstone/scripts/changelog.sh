#!/usr/bin/env bash
# Gera notas de release a partir do histórico Git, não da memória da conversa.
set -euo pipefail

previous=$(git describe --tags --abbrev=0 HEAD^ 2>/dev/null || echo "")

echo "## Mudanças"
if [[ -n "$previous" ]]; then
  git log --pretty=format:'- %s' "${previous}..HEAD"
else
  git log --pretty=format:'- %s' -n 20
fi | grep -E '^- (feat|fix)' || true

echo
echo "## Evidência"
echo "- Pipeline: ${CI_PIPELINE_URL:-local}"
echo "- HIL: artifact reports/hil/results.xml"
echo "- Cobertura: artifact build/artifacts/gcov/"
