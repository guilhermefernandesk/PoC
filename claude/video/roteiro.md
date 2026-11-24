# Roteiro — Vídeo-aula completa (~23 min)

Versão **Claude Code**: narração + slides (`workshop/slides.html`) + demos reais de terminal. A versão curta do NotebookLM tem prompt próprio em `prompt-notebooklm.md`.

Convenção: `SLIDE n` = usar o slide n do `slides.html`. `DEMO` = gravar terminal. `VOZ` = narração.

---

## Cena 1 — Gancho (0:00–0:45) · SLIDE 1

**VOZ:**
"Compilei. Passei nos testes unitários. Gravei no STM32. E o LED não piscou. Se você já passou por isso, o problema não foi o compilador — foi o loop parar antes do hardware. Nesta aula eu vou mostrar como usar Claude Code para fechar esse loop sem queimar tokens, sem perder acurácia e sem deixar o agente mexer onde não deve."

---

## Cena 2 — O mapa (0:45–2:20) · SLIDE 4

**VOZ:**
"Tudo o que importa cabe em seis camadas. Contexto, skills, isolamento, hooks, integração e orquestração. As duas primeiras aumentam o acerto. Isolamento e hooks aumentam a segurança. Integração e CI fecham a evidência. Guarda esse desenho — toda decisão da aula volta para uma dessas caixas."

---

## Cena 3 — Contexto que o código não conta (2:20–5:10) · SLIDES 5–7 · DEMO

**VOZ:**
"Claude consegue ler seu código. Ele não consegue ler a revisão C do datasheet, o orçamento de RAM ou o fato de que o I2C trava se dois jobs usarem a placa. Então a primeira tarefa é escrever isso. Eu rodei `/init`, apaguei metade e deixei quatro seções: comandos, convenções, gotchas e verificação. Menos de duzentas linhas."

**DEMO (comandos exatos):**
```bash
cd /workspace/projetos/study/capstone
claude
> /init
> /context all
> /doctor prompt-audit
```

**FALA AO TERMINAL:**
"Repare: o `CLAUDE.md` não ensina C. Ele ensina o que é verdade neste projeto."

Transição: "E quando o procedimento é longo, ele não vai para o `CLAUDE.md` — vai para uma skill."

---

## Cena 4 — Skills e custo (5:10–9:00) · SLIDES 8–11 · DEMO

**VOZ:**
"Uma skill mostra nome e descrição em toda sessão; o conteúdo completo só entra quando ela é chamada. É progressive disclosure. Eu tenho três: user stories, embedded unit tests e ADR com seção de compliance. Agora vamos medir o custo antes de continuar."

**DEMO:**
```text
> /usage
> /compact keep the firmware changes and the test commands
> /clear
> /usage
```

**VOZ apontando a tela:**
"Olha a linha `Prompt cache (main)`: noventa e um por cento dos tokens de entrada vieram do cache. A atribuição mostra que o MCP do GitLab consumia trinta e um por cento — para três comandos por semana. Trocamos por `glab` via Bash e esse custo some. O número ao lado do spinner é só o output do turno; não confunda com o total."

**DICA NA TELA:** `f` em `/usage` aplica recomendações; `ultrathink` só na próxima mensagem.

---

## Cena 5 — Segurança determinística (9:00–13:30) · SLIDES 9, 12–14 · DEMO

**VOZ:**
"Escrever 'nunca edite Drivers' no `CLAUDE.md` é sugestão. O modelo quase sempre obedece — quase não é garantia. A documentação oficial diz: se você reforçou a linha e ainda falha, vai para hook. Então eu transformei isso em deny rule e coloquei um Stop hook que exige o teste verde."

**DEMO:**
```text
> edite Drivers/gpio.c para corrigir o LED
# bloqueio vem do harness, não do texto

> /hooks
> /sandbox
```

**VOZ:**
"No Linux, sandbox de verdade precisa de `bubblewrap` e `socat`. Strict mode desliga o escape hatch e faz o Claude falhar na startup se o sandbox não subir. E atenção: Read, Edit, Write, hooks e MCP local rodam **fora** do sandbox. Um `denyRead` pega `cat .env`, mas não pega um Python que abre o arquivo. Por isso eu declaro `GITLAB_TOKEN` como `mask` com `injectHosts` — e isso só vale em user settings, não no repo."

---

## Cena 6 — TDD (13:30–17:00) · SLIDES 15–16 · DEMO

**VOZ:**
"Agora o loop de qualidade. Eu peço para a skill escrever o teste primeiro. O primeiro resultado é uma falha de link — e isso é sucesso. Só depois eu implemento o mínimo."

**DEMO:**
```text
> Use a skill embedded-unit-tests para testar blink_enter_error() (REQ-001)
> ceedling test:test_blink       # VERMELHO
> implemente o mínimo em app/blink.c, sem malloc/printf
> ceedling test:all              # VERDE
```

**VOZ:**
"Note que o `Stop` hook não deixa eu terminar a mensagem com a suíte vermelha. O agente pode até querer dizer 'pronto'; o harness devolve o erro do compilador e ele continua. E o arquivo `edge-cases.md` da skill cobre o que o modelo esquece: rollover de timer de 32 bits em quarenta e nove dias, buffer circular cheio com `head == tail`, reentrância de ISR."

---

## Cena 7 — GitLab CI + HIL (17:00–20:00) · SLIDES 17–18 · DEMO

**VOZ:**
"No GitLab eu separo runner de build de runner de bancada. O runner docker compila e roda Ceedling; o runner com tag `stm32-bench` grava e mede. O `flock` existe porque dois jobs I2C na mesma placa se corrompem. HIL é manual ou por label — não vou gastar a bancada em todo push."

**DEMO:**
```bash
glab ci status
glab ci trace <job-id>
sed -n '1,80p' .gitlab-ci.yml
```

**VOZ sobre fault injection:**
"Um bom harness tem quatro camadas: host, target, bench e field. E injeta falha: corta o rail, manda NACK, estoura o watchdog. Se o agente não vê o sinal, ele continua adivinhando. O resumo do log volta para ele em sessenta linhas, não em dez mil."

---

## Cena 8 — Rastreabilidade e release (20:00–22:00) · SLIDES 19–20 · DEMO

**DEMO:**
```bash
./scripts/check-traceability.sh <(echo "wip: mudança")
# ERRO

./scripts/check-traceability.sh <(echo "feat(REQ-001): piscar LED #42")
# OK

glab issue create --title "REQ-001: LED de status" --label REQ-001
glab mr create --fill --title "REQ-001: piscar LED de status"
glab release create v1.4.2+k7 build/firmware.bin --notes "$(./scripts/changelog.sh)"
```

**VOZ:**
"A cadeia é: requisito, Issue, commit, MR, pipeline, release. O `k` da versão é a revisão da placa — gravar `k6` em placa `k7` é um clássico. O changelog sai do histórico Git, não da memória da conversa. E a evidência do HIL vai anexada ao release."

---

## Cena 9 — Entrega e recap (22:00–23:00) · SLIDES 21–23

**VOZ:**
"Cada participante leva três coisas: o livro HTML offline, o capstone pronto para copiar e um `efficiency-and-security.md` com o antes e depois do `/usage`. A pergunta final é simples: o agente tem informação suficiente e existe um cheque determinístico antes de dizer 'pronto'? Se a resposta for sim, você pode deixar ele trabalhar — inclusive de madrugada. Obrigado."

**FIM.**

---

## Notas de gravação

- Grave o terminal com fonte grande (≥16 px) e tema escuro; aumente o zoom para 125%.
- Entre cenas, segure o slide por 1 s para dar tempo de leitura.
- Não mostre tokens reais: use `GITLAB_TOKEN` mascarado ou placeholder.
- Se não houver placa, substitua a demo de `st-flash` por `pytest --collect-only` e explique o contrato do job.
- Legendas: gerar `.srt` a partir deste roteiro; `render.sh` faz o mux quando `ffmpeg` estiver disponível.
