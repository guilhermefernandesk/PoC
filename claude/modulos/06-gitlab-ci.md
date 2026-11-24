# Módulo 06 — CI/CD no GitLab: do commit ao binário com evidência

**Tempo:** 40 min · **Entrega:** pipeline `build → unit → flash → hil → release` no `capstone/`.

---

## 1. O desenho correto para firmware

Separe **runners que compilam** de **runners que tocam hardware**. É a decisão de arquitetura mais importante do CI embarcado.

```text
MR aberta
   │
   ├─ build        (runner docker, cross-compiler)  → artefato .elf/.hex/.bin + mapa
   ├─ unit         (runner docker, Ceedling)        → junit + cobertura
   ├─ static       (runner docker, shellcheck/cppcheck)
   │
   └─ manual/tag
      ├─ flash     (runner com ST-Link, tag " stm32-bench")
      └─ hil       (mesmo runner, lock de bancada)   → log + screenshot + junit
          │
          └─ tag v* → release (changelog de MRs + evidência anexada)
```

Por que separar: um job I2C na bancada não pode rodar em paralelo com outro I2C na mesma placa. A série do stupid-projects mostra exatamente isso com uma test farm de Nanopis rodando gitlab-runner.

## 2. `.gitlab-ci.yml` funcional

```yaml
stages: [build, unit, static, flash, hil, release]

variables:
  GIT_DEPTH: "1"
  FIRMWARE: "build/firmware.bin"
  FLASH_ADDR: "0x8000000"

.default_docker: &docker
  image: registry.gitlab.com/grupo/fw-toolchain:arm-gcc-12
  tags: [docker]
  cache:
    key: "$CI_COMMIT_REF_SLUG"
    paths: [build/, .deps/]

build:fw:
  <<: *docker
  stage: build
  script:
    - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    - cmake --build build -j
    - arm-none-eabi-size "$FIRMWARE" || arm-none-eabi-size build/firmware.elf
  artifacts:
    name: "fw-$CI_COMMIT_SHORT_SHA"
    paths: [build/firmware.elf, build/firmware.bin, build/firmware.map]
    expire_in: 1 week

unit:ceedling:
  <<: *docker
  stage: unit
  script:
    - ceedling test:all
    - ceedling gcov:all || true
  artifacts:
    when: always
    reports:
      junit: build/artifacts/test/*.xml
    paths: [build/artifacts/gcov/]
    expire_in: 1 week

static:shellcheck:
  <<: *docker
  stage: static
  script:
    - ./scripts/shellcheck.sh
    - cppcheck --enable=warning,style --error-exitcode=1 app/ || true

flash:bench:
  stage: flash
  tags: [stm32-bench]
  when: manual
  allow_failure: false
  dependencies: [build:fw]
  script:
    - flock /tmp/stm32-bench.lock -c "st-flash --reset write $FIRMWARE $FLASH_ADDR"
  artifacts:
    reports:
      junit: reports/hil/flash.xml
    when: always

hil:gpio:
  stage: hil
  tags: [stm32-bench]
  when: manual
  dependencies: [flash:bench]
  script:
    - flock /tmp/stm32-bench.lock -c "pytest -q tests/hil --junitxml=reports/hil/results.xml"
  artifacts:
    when: always
    paths: [reports/hil/]
    reports:
      junit: reports/hil/results.xml

release:tag:
  stage: release
  tags: [docker]
  rules:
    - if: '$CI_COMMIT_TAG =~ /^v\d+\.\d+\.\d+/'
  script:
    - glab release create "$CI_COMMIT_TAG" build/firmware.bin --notes "Ver evidência em $CI_PIPELINE_URL"
```

**Pontos a notar:**

- `flock` resolve o lock de hardware no mesmo host. Se a bancada é compartilhada entre hosts, use o Device Cloud do GitLab ou um serviço de alocação.
- `when: manual` evita que todo MR gaste a placa. Jobs HIL disparam em MRs marcadas ou em tags.
- `dependencies` limita o que é baixado de estágios anteriores.
- `artifacts:reports:junit` faz o GitLab mostrar falhas na interface do MR.
- `|| true` só onde a falha é informativa — **nunca** no gate principal.

## 3. Regras que reduzem custo sem perder sinal

```yaml
#rode unit/build em todo push, mas HIL só em MRs com-label ou tags
hil:gpio:
  rules:
    - if: '$CI_PIPELINE_SOURCE == "merge_request_event" && $CI_MERGE_REQUEST_LABELS =~ /run-hil/'
    - if: '$CI_COMMIT_TAG =~ /^v/'
```

Use `interruptible: true` em pipelines de MR para cancelar execuções obsoletas quando você faz push de novo.

```yaml
default:
  interruptible: true
```

## 4. Revisão e triagem com Claude no GitLab CI

A documentação oficial coloca **GitHub Actions ou GitLab CI/CD** como superfícies de automação. O padrão é o mesmo:

```yaml
claude:review:
  stage: static
  image: registry.gitlab.com/grupo/claude-code:latest
  tags: [docker]
  rules:
    - if: '$CI_PIPELINE_SOURCE == "merge_request_event"'
  variables:
    ANTHROPIC_API_KEY: $ANTHROPIC_API_KEY
  script:
    - claude -p "Revise este diff contra SPEC.md. Reporte só problemas de correção e requisitos sem teste." \
        --allowedTools "Read,Grep,Glob,Bash(glab mr diff *)"
```

Cuidados:

- **Não rode com `bypassPermissions`** em CI compartilhado. Use `--allowedTools` explícito e sandbox.
- `/code-review` local funciona em qualquer plano. O recurso **Claude Ship** (review gerenciado) é Team/Enterprise, ligado por Owner, cobrado à parte e indisponível com ZDR/HIPAA/CMEK — não presuma.
- Em GitLab, revisão por linha de comando depende de `glab` autenticado; a variável deve ser um token com escopo mínimo e `mask` no CI.

## 5. Scheduled tasks: HIL noturno

```text
/schedule 0 3 * * * Rode a suíte HIL, agrupe falhas por causa raiz e abra uma Issue com o REQ afetado. Não grave firmware em produção.
```

Ou, de forma mais barata e determinística, um **pipeline agendado** no GitLab (`CI_PIPELINE_SOURCE == "schedule"`) com job HIL e um subagent Haiku só para resumir o log. Lembre do Módulo 03: cada disparo de `/schedule` envia o contexto da sessão.

## 6. Exercício 06

1. Copie o `.gitlab-ci.yml` acima para o `capstone/`.
2. Ajuste a imagem para a sua toolchain (ou use `gcc` e compile só os testes no início).
3. Rode `glab ci status`.
4. Verifique no MR: artefato baixável, junit aparecendo, job HIL manual.
5. Adicione `rules` para HIL só com label `run-hil`.

## 7. Erros comuns

- Rodar HIL em todo push → bancada vira gargalo.
- Não travar a placa → dois jobs I2C se corrompem.
- `artifacts` sem `when: always` → você perde o log exatamente quando falha.
- Colocar token de GitLab em `variables` sem `mask`.
- Pedir ao Claude para "consertar a CI" sem dar o log — use `glab ci trace`.

## 8. Checklist

- [ ] Runners separados: docker vs bancada.
- [ ] Lock de hardware implementado.
- [ ] Artefatos com log mesmo na falha.
- [ ] JUnit reportado no MR.
- [ ] HIL manual ou por label.
- [ ] Token mascarado, permissões mínimas.

**Próximo:** [Módulo 07 — HIL e bancada](07-hil-bancada.md).
