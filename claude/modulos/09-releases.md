# Módulo 09 — Releases e governança leve

**Tempo:** 25 min · **Entrega:** tag de release com changelog e evidência anexada.

---

## 1. Release é um contrato, não um zip

Para firmware, release precisa responder:

| Pergunta | Onde está |
|---|---|
| O que mudou? | changelog gerado dos MRs mesclados |
| Qual versão do firmware está gravada? | string de versão no binário + tag `vX.Y.Z+k<revisão>` |
| Quais requisitos foram validados? | relatório de cobertura + junit do HIL |
| Quem aprovou? | MR com aprovação e compliance check |
| Como reproduzir? | commit SHA + imagem da toolchain + `docs/ops/verification.md` |

## 2. Versionamento com chave de hardware

O padrão que funciona bem quando o firmware acompanha revisão de placa:

```text
v1.4.2+k7
│ │ │ └─ revisão de hardware/placa (k7)
│ │ └─── patch
│ └───── minor
└─────── major
```

A chave `k7` evita um clássico: gravar `v1.4.2` de placa revisão 6 em placa revisão 7. No `CLAUDE.md`:

```markdown
- A tag de release segue vMAJOR.MINOR.PATCH+kREV.
- Nunca grave firmware com `k` diferente da revisão do hardware de destino.
```

E no C:

```c
#define FW_VERSION "v1.4.2+k7"
```

O job de build extrai a tag e falha se a constante divergir:

```bash
tag="${CI_COMMIT_TAG:-dev}"
grep -q "$tag" app/version.h || { echo "versão divergente" >&2; exit 2; }
```

## 3. Changelog a partir de MRs

```yaml
release:tag:
  stage: release
  rules:
    - if: '$CI_COMMIT_TAG =~ /^v\d+\.\d+\.\d+/'
  script:
    - glab release create "$CI_COMMIT_TAG" build/firmware.bin \
        --notes "$(./scripts/changelog.sh)"
```

```bash
#!/usr/bin/env bash
# scripts/changelog.sh
echo "## Mudanças"
git log --pretty=format:'- %s' "$(git describe --tags --abbrev=0 HEAD^)"..HEAD \
  | grep -E '^- (feat|fix)'
echo
echo "## Evidência"
echo "- Pipeline: $CI_PIPELINE_URL"
echo "- HIL: artifact reports/hil/results.xml"
echo "- Cobertura: artifact build/artifacts/gcov/"
```

## 4. Runbook de verificação

`docs/ops/verification.md` deve ser curto e executável:

```markdown
# Verificação de release v1.4.2+k7

1. Baixe o artefato `fw-<sha>` do pipeline <link>.
2. Confira a string de versão na UART: `FW v1.4.2+k7`.
3. Rode `./scripts/verify-image.sh firmware.bin` (checksum + header).
4. Reexecutar HIL: ver `reports/hil/results.xml`.
5. Se falhar: `/rewind` no commit anterior e abra Issue com label `regression`.
```

Nota importante sobre o caso real que estudamos antes: num repositório sem histórico Git local, commits/issues são conhecidos apenas por documentação e referências `#NN`. **Não faça isso.** Release sem histórico verificável é só um binário órfão.

## 5. Revisão final antes de taguear

Quatro verificações, em ordem de custo:

1. **Prompt/agente:** `/verify` — Claude builda, roda e olha o resultado.
2. **Revisor independente:** `/code-review` numa MR, subagent que não viu a conversa.
3. **Gates determinísticos:** Stop hook local + CI (unit, static, HIL, traceabilidade).
4. **Checklist humana:** 5 itens no máximo. Checklist longa ninguém lê.

```markdown
- [ ] Versão no binário bate com a tag
- [ ] HIL verde no commit exato da tag
- [ ] ADRs de REQs afetados atualizados
- [ ] Changelog gerado de MRs
- [ ] Artefato e junit anexados ao release
```

## 6. Governança corporativa (referência rápida)

Não é alvo deste workshop, mas saiba que existe:

- **Compliance frameworks** no GitLab: exigem aprovação/checks específicos em MRs.
- **Managed settings**: única fonte de regra que o dev não consegue sobrescrever; vem de console/MDM.
- **ZDR / HIPAA / CMEK**: restrições de recursos gerenciados — alguns recursos de review na nuvem ficam indisponíveis.

Para dev individual, o equivalente saudável é: `.claude/settings.json` versionado + CI obrigatória + proteção de branch. Não precisa de MDM para ter disciplina.

## 7. Exercício 09

1. Crie `app/version.h` com `FW_VERSION`.
2. Rode o job de build com `CI_COMMIT_TAG=v1.4.2+k7` e veja falhar (versão divergente).
3. Ajuste a constante, re rode, e crie o release com `glab release create`.
4. Baixe o artefato e rode o checksum do runbook.

## 8. Checklist

- [ ] Tag `vMAJOR.MINOR.PATCH+kREV`.
- [ ] Versão no binário validada no CI.
- [ ] Changelog gerado de MRs, não escrito à mão.
- [ ] Evidência (junit, cobertura, pipeline) anexada ao release.
- [ ] Runbook com 5 passos executáveis.
- [ ] `/verify` + `/code-review` rodados antes da tag.

**Próximo:** [Módulo 10 — Capstone](10-capstone.md).
