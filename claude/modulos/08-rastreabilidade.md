# Módulo 08 — Rastreabilidade: do requisito ao artefato

**Tempo:** 30 min · **Entrega:** cadeia `REQ → Issue → commit → MR → pipeline → release` validada por script.

---

## 1. Rastreabilidade é cadeia, não documento

Um documento de requisitos bonito que ninguém atualiza é pior que nada. O que importa é conseguir responder, em 30 segundos:

> "Por que essa linha de `blink.c` existe, quem aprovou e qual evidência prova que ela funciona?"

A cadeia:

```text
SPEC.md (REQ-001)
   ↓
Issue #42  (label REQ-001)
   ↓
commit feat(REQ-001): piscar LED de status #42
   ↓
MR !17  (Closes #42, checklist de evidência)
   ↓
pipeline: unit ✓  HIL ✓  junit anexado
   ↓
release v1.4.2  (changelog gerado dos MRs)
```

## 2. GitLab Issues como âncora

O GitLab Requirements Management está em transição para *work items* (epic #5171). Para um dev individual, o desenho pragmático é:

- spec versionada no repo (`SPEC.md` ou `docs/specs/REQ-001.md`);
- Issue com o mesmo ID e label;
- MR que **fecha** a Issue (`Closes #42`);
- evidência de CI como artefato do pipeline.

```bash
glab issue create \
  --title "REQ-001: LED de status" \
  --label REQ-001,firmware \
  --description "Critérios em SPEC.md. Evidência: job hil:gpio."

glab mr create --fill \
  --title "REQ-001: piscar LED de status" \
  --description "Closes #42\n\n- [ ] unit\n- [ ] HIL\n- [ ] ADR atualizado"
```

## 3. Convenção de commit

```text
<tipo>(<REQ>): <resumo imperativo> #<issue>

feat(REQ-001): piscar LED de status #42
fix(REQ-003): retry no I2C após NACK #47
test(REQ-001): cobre rollover do timer #42
docs(REQ-001): registra decisão de OSAL #42
```

Tipos: `feat`, `fix`, `test`, `refactor`, `docs`, `chore`, `ci`.

O ID aparece **duas vezes** (no escopo e no `#issue`) de propósito: uma para o script validar, outra para o GitLab linkar.

## 4. Validador determinístico

Não confie no agente para lembrar. Coloque no `PreToolUse` ou no CI:

```bash
#!/usr/bin/env bash
# scripts/check-traceability.sh
set -euo pipefail

msg_file="${1:-.git/COMMIT_EDITMSG}"
msg=$(cat "$msg_file")

if [[ "$msg" =~ ^(feat|fix|test|refactor|docs|chore|ci)\(REQ-[0-9]{3}\) ]]; then
  if [[ "$msg" =~ \#([0-9]+) ]]; then
    echo "OK: requisito e issue presentes"
    exit 0
  fi
fi

echo "ERRO: use tipo(REQ-NNN): descricao #issue" >&2
exit 2
```

No GitLab CI, valide o *range* do MR:

```yaml
trace:commits:
  stage: static
  script:
    - |
      for c in $(git log --format=%s "$CI_MERGE_REQUEST_DIFF_BASE_SHA"..HEAD); do
        ./scripts/check-traceability.sh <(printf '%s' "$c") || exit 1
      done
  rules:
    - if: '$CI_PIPELINE_SOURCE == "merge_request_event"'
```

E no hook local:

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash",
        "hooks": [
          { "command": "./scripts/hook-commit-guard.sh", "onFailure": "block" }
        ]
      }
    ]
  }
}
```

O script só intercepta `git commit`, chama o validador e deixa todo o resto passar.

## 5. ADR com seção `Compliance`

```markdown
# ADR-007 — Isolar HAL atrás de GpioPort

- Status: aceito
- REQ: REQ-001, REQ-004

## Contexto
A HAL gerada pelo CubeMX muda entre revisões. Testes no host não podem incluí-la.

## Decisão
Toda lógica de app/ recebe uma interface GpioPort injetada.

## Alternativas
1. Chamar HAL diretamente — rejeitado: impossível testar no host.
2. Copiar a HAL para test/ — rejeitado: divergência silenciosa.

## Consequências
+ Testes determinísticos no host.
+ Um adaptador por família de MCU.

## Compliance
- Verificação: `test/test_blink.c` usa mock_gpio_port.h; CI falha se app/ incluir stm32f1xx_hal.h.
- Onde roda: job `unit:ceedling` + grep no stage static.
- Evidência: artifact `build/artifacts/test/*.xml`.
```

A seção `Compliance` é o que impede a ADR de virar literatura. Ela responde: **como uma máquina prova que essa decisão foi respeitada?**

## 6. Skill `/user-stories` como gerador de rastreabilidade

A skill do Módulo 02 deve sempre emitir:

```markdown
REQ-001 — Piscar LED de status
História: Como operador, quero ver o LED piscar em IDLE para saber que o firmware está vivo.

Critérios (Given-When-Then):
- Dado o estado IDLE, quando 500 ms se passam, então o LED inverte.
- Dado o estado ERROR, quando a task roda, então o LED fica aceso.

Rastreabilidade:
- Issue: #42
- Teste unitário: test/test_blink.c
- Teste HIL: tests/hil/test_gpio.py
- Evidência: job hil:gpio
```

Se a skill não consegue preencher os quatro campos, o requisito não está pronto — e é melhor parar aqui.

## 7. Exercício 08

1. Crie a Issue `REQ-001` com `glab`.
2. Faça um commit **sem** o padrão e confirme que o hook barra.
3. Refaça com `feat(REQ-001): ... #42`.
4. Escreva a ADR-007 com `Compliance`.
5. Rode `/code-review` pedindo só isto: *"todo requisito de SPEC.md tem teste e Issue?"*

## 8. Checklist

- [ ] Cada REQ tem Issue, teste unitário, teste HIL e ADR quando aplicável.
- [ ] Commit segue `tipo(REQ-NNN): ... #issue`.
- [ ] Validador roda local **e** no CI.
- [ ] MR fecha a Issue.
- [ ] Evidência do pipeline está anexada ao MR/release.
- [ ] `/code-review` já foi usado como auditoria de spec.

**Próximo:** [Módulo 09 — Releases e governança leve](09-releases.md).
