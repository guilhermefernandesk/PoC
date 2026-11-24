---
name: release-notes
description: Gera notas de release a partir de MRs mesclados, tags versionadas e artefatos de evidência. Use quando o usuário pedir release, changelog, tag vX.Y.Z ou notes de release.
---

# Release notes

1. Descubra a tag atual e a anterior (`git tag --sort=-v:refname`).
2. Liste commits entre as tags, filtrando `feat` e `fix`.
3. Agrupe por REQ (`REQ-NNN`), não por autor.
4. Inclua um bloco **Evidência** com:
   - URL do pipeline;
   - artifact `reports/hil/results.xml`;
   - relatório de cobertura;
   - string de versão do binário.
5. Confira a convenção `vMAJOR.MINOR.PATCH+kREV`. A revisão `k` deve bater com a placa de destino.
6. Chame `glab release create "$TAG" build/firmware.bin --notes "..."` somente após confirmação.

## Regra

Nunca escreva changelog à mão a partir da memória da conversa. A fonte é o histórico Git + MRs.
