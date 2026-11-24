# Módulo 03 — Menos tokens, mais acerto, menos risco

**Tempo:** 40 min · **Entrega:** `/usage` antes/depois, hook de filtro, `Stop` hook e sandbox strict.

> Este é o módulo mais importante do workshop. Economia, acurácia e segurança são o mesmo mecanismo visto de três ângulos: **contexto limpo → menos tokens e menos distração → mais acerto; regra determinística → menos erro e menos risco.**

---

## 1. Determinístico vs não determinístico

| Tipo | Onde mora | Quem executa | Garante? |
|---|---|---|---|
| Não determinístico | prompt, `CLAUDE.md`, skill | o **modelo** decide | não |
| Determinístico | hook, deny rule, managed setting | o **harness** executa | **sim** |

A documentação oficial dá o diagnóstico melhor possível:

> "Se você continuar reforçando uma linha do `CLAUDE.md` e Claude ainda a ignora, essa linha pertence a um hook. O inverso também vale: se você tem hooks para preferências, apague-os."

Exemplo prático em firmware:

- `"Nunca edite Drivers/ gerado pelo CubeMX"` → `PreToolUse` bloqueando o caminho.
- `"Sempre rode ceedling test:all antes de terminar"` → `Stop` hook com exit 2.
- `"Prefira nomes descritivos"` → fica no prompt, sem hook.

---

## 2. Menos tokens

### 2.1 Meça antes de otimizar

```text
/usage
```

O que olhar:

- **Session → Usage by model**: input, output, cache read/write e custo estimado por modelo.
- **Prompt cache (main)**: `14 requests · 91% of input tokens from cache · 2 misses ... warm (1h TTL)`. Se o hit rate está baixo, algo está invalidando o cache.
- **Atribuição**: porcentagem por **skill, subagent, plugin, MCP server e `/loop`**. Um MCP que consome 30% aparece aqui.
- **Recomendações**: linhas com share ≥10%, por exemplo `25% Trim ~/code/fw/CLAUDE.md · 49k tokens per request`.
- **Tecla `f`**: aplica as correções automáticas que o Claude consegue fazer (v2.1.305+).

O número ao lado do spinner mede **só o output do turno** (texto + tool calls + thinking), não o input. Não confunda com o total da sessão.

### 2.2 As seis ações de maior impacto

| Ação | Comando | Por quê |
|---|---|---|
| Limpar entre tarefas | `/rename` → `/clear` → `/resume` depois | `/clear` custa zero. Uma sessão com 4 tarefas reenvia as 4 em toda pergunta da 5ª. |
| Compactar com foco | `/compact keep the firmware changes and the test commands` | `/compact` é uma requisição grande; sem foco ele resume demais e você repete contexto. |
| Descobrir o que ocupa | `/context`, `/context all` | Mostra o consumo real e confirma se o `CLAUDE.md` foi carregado. |
| Usar CLI em vez de MCP | `glab`, `git`, `make`, `st-flash` | Definições de tools MCP entram no contexto; CLI não adiciona lista de ferramentas. |
| Modelo certo | `/model sonnet`, `model: haiku` no subagent | Sonnet para rotina; Opus para arquitetura; Haiku para subagent simples. |
| Esforço certo | `/effort low|high|xhigh|max` | Thinking é cobrado como output. |

Guia de esforço:

- `low` — brainstorm, rascunho, rename em poucos arquivos, você revisa cada passo.
- `high` / `xhigh` — bug que você não escreveu, teste flaky, mudança de schema com muitos leitores.
- `max` — problema difícil sem supervisão, ex.: caça a vulnerabilidade. Em tarefa comum ele "pensa demais" e cobra mais pelo mesmo resultado.
- `ultrathink` — quando só a **próxima** mensagem precisa de profundidade.
- `MAX_THINKING_TOKENS=8000` — para modelos com budget fixo; modelos adaptativos ignoram e usam `/effort`.

### 2.3 Cache: o que derruba o hit rate

Claude Code reprocessa o histórico a cada turno; o cache faz isso a preço de leitura cacheada. Em assinatura o TTL é ~1h; na API ~5min (confirme em `/usage`).

Cuidado com:

- pausa longa → cache frio;
- troca de modelo (`/model`) → turno não cacheado;
- `/compact` e limpeza de tool results → rebuild esperado;
- mudança em definições de tools (MCP ligado/desligado).

### 2.4 Hook de pré-processamento: menos log, mais sinal

Em vez de deixar o Claude ler 10.000 linhas de build, filtre antes:

```json
// .claude/settings.json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash",
        "hooks": [
          { "command": "~/.claude/hooks/filter-test-output.sh" }
        ]
      }
    ]
  }
}
```

```bash
#!/usr/bin/env bash
# ~/.claude/hooks/filter-test-output.sh
input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')

case "$cmd" in
  *ceedling*|*ctest*|*pytest*|*make\ test*)
    filtered="${cmd% && *} 2>&1 | tail -n 100"
    printf '{"hookSpecificOutput":{"hookEventName":"PreToolUse","updatedInput":{"command":%s}}}' \
      "$(printf '%s' "$filtered" | jq -Rs .)"
    ;;
  *)
    printf '%s' "$input"
    ;;
esac
```

```bash
chmod +x ~/.claude/hooks/filter-test-output.sh
```

Verifique com `/hooks` e com `claude --debug-file ./claude-debug.txt` (procure por `modified tool input keys`).

**Outras fontes de consumo silencioso:** `/loop`, goals com check-in em idle, scheduled tasks, mensagens cross-session e sugestões de próximo prompt. Todos enviam o contexto cheio mesmo quando você não está digitando. Desligue o que não presta serviço.

---

## 3. Mais acurácia

### 3.1 Dê o que o agente não vê

```text
@src/app/adc.c            # arquivo específico, não o repo inteiro
[imagem do logic analyzer colada]
cat build.log | claude -p "qual é a primeira falha real?"
```

### 3.2 Sempre dê um alvo de verificação

```text
> Implemente REQ-001. Antes de terminar, rode:
>   ceedling test:all && ./scripts/shellcheck.sh
> Se falhar, corrija e rode de novo. Não mude os testes.
```

Formas de verificar, da mais fraca para a mais forte:

1. pedido no prompt;
2. `/goal all tests in tests/ pass and lint is clean` (outro modelo checa a cada turno);
3. **`Stop` hook** (harness barra o fim do turno);
4. revisor independente (`/code-review` local, subagent que não viu a conversa);
5. CI no GitLab (Módulo 06).

### 3.3 `Stop` hook: "done" só com teste verde

```json
// .claude/settings.json
{
  "hooks": {
    "Stop": [
      {
        "hooks": [
          {
            "command": "ceedling test:all >/dev/null 2>~/.claude/stop-error.txt || { cat ~/.claude/stop-error.txt >&2; exit 2; }"
          }
        ]
      }
    ]
  }
}
```

Exit `2` devolve a saída para o Claude e impede o término. O que vai para `stderr` vira o motivo. Se ele bloquear 8 vezes seguidas sem tool call, o harness cede na próxima (limite configurável com `CLAUDE_CODE_STOP_HOOK_BLOCK_CAP`).

### 3.4 Revisor que não pode editar

```markdown
<!-- .claude/agents/reviewer.md -->
---
name: reviewer
description: Revisa um diff contra SPEC.md sem modificar arquivos.
tools: Read, Grep, Glob
model: sonnet
---

Leia SPEC.md e o diff. Para cada requisito, diga se há teste cobrindo.
Ignore estilo. Reporte só problemas de correção.
```

A limitação de ferramentas é imposta pelo harness, não pedida em texto. É a versão barata do `/code-review`.

### 3.5 Outras alavancas

- `/verify` — Claude builda, roda e olha o resultado (pega o que teste estático não pega).
- `/rewind` — tente a abordagem ousada; se der errado, volta código e conversa.
- `Esc` — corrija no meio da ação, não depois de 5 turnos.
- Teste incremental: um arquivo → teste → próximo.

---

## 4. Menos risco

### 4.1 Camadas de proteção

1. **Permission mode** — `auto` (classifier, padrão no CLI/VS Code desde v2.1.283) bloqueia `curl | bash`, deploy de produção, migrações, force push sobre trabalho de outros, envio de dados sensíveis a endpoint externo e ações aparentemente induzidas por conteúdo lido. Ainda é **julgamento de modelo**: pode errar.
2. **Deny rules + hooks** — onde mora o seu "nunca". Determinístico.
3. **Sandbox do SO** — limita filesystem e rede de comandos de shell e processos filhos.

`bypassPermissions` só em container descartável. `manual` é seguro, mas depois da décima confirmação você começa a aprovar sem ler.

### 4.2 O que o sandbox cobre — e o que fica FORA

**Dentro:** Bash, PowerShell, Monitor e processos que eles iniciam.

**Fora (importantíssimo):**

- tools `Read`, `Edit`, `Write`, `WebFetch`, `WebSearch` — seguem *permission rules*, não o sandbox;
- hooks, MCP servers locais, LSP, status line, plugins monitors, mods;
- comandos que **você** digita no prompt `!`;
- `excludedCommands`;
- o retry `dangerouslyDisableSandbox`.

Consequência prática: `denyRead` pega `cat .env`, **não** pega um script Python que abre `.env`. Um deny de `WebFetch` para `gitlab.com` não barra `curl https://gitlab.com` **a menos que o sandbox esteja ligado**.

### 4.3 Strict sandbox no Linux (STM32 toolchain)

```bash
sudo apt-get install -y bubblewrap socat
claude --settings '{"sandbox":{"enabled":true,"allowUnsandboxedCommands":false,"failIfUnavailable":true}}'
```

- `allowUnsandboxedCommands: false` — ignora o escape hatch `dangerouslyDisableSandbox`.
- `failIfUnavailable: true` — se `bwrap`/`socat` faltarem, o Claude Code **não inicia**. É o comportamento correto quando sandbox é requisito.
- Dentro: `/sandbox` → abas *Mode*, *Overrides*, *Config*, *Dependencies*.

Padrões de escrita: working dir + temp + `--add-dir`/`permissions.additionalDirectories`. Rede: sem rota direta, proxy com allowlist que começa **vazia**.

### 4.4 Credenciais: `deny` e `mask`

```json
{
  "sandbox": {
    "credentials": {
      "files": [
        { "path": "~/.ssh", "mode": "deny" },
        { "path": "~/.aws/credentials", "mode": "deny" }
      ],
      "envVars": [
        { "name": "GITLAB_TOKEN", "mode": "mask", "injectHosts": ["gitlab.com"] },
        { "name": "NPM_TOKEN", "mode": "deny" }
      ]
    },
    "network": {
      "allowedDomains": ["gitlab.com"],
      "tlsTerminate": true
    }
  }
}
```

- `deny` → arquivo bloqueado, variável removida antes do comando.
- `mask` → comando vê um *sentinel*; o proxy troca pelo valor real apenas em `injectHosts` permitidos. Exige `network.tlsTerminate: true`.
- `mask` com injeção real só é aceito em **user settings, managed settings ou `--settings`** — não em `.claude/settings.json` do repo.
- Não existe deny list padrão: se você não declarar, nada é protegido.

### 4.5 Hooks que não deixam passar

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Edit|Write",
        "hooks": [
          {
            "command": "./scripts/block-generated.sh",
            "onFailure": "block"
          }
        ]
      }
    ]
  }
}
```

`onFailure: "block"` impede a ação se o scanner cair ou der timeout. Sem isso, um hook de segredos quebrado deixa o commit passar **sem varredura** — o que é pior do que não ter hook.

Use `command`, `http` ou `mcp_tool` para imposição. Hooks do tipo `prompt` ou `agent` consultam outro modelo, logo **não são determinísticos**.

### 4.6 Escopo das configurações

| Arquivo | Quem | Efeito |
|---|---|---|
| `~/.claude/settings.json` | você | global pessoal |
| `.claude/settings.local.json` | você, neste repo | não versionado |
| `.claude/settings.json` | time, versionado | deny vale na hora; allow depende do trust dialog |
| managed settings | admin/MDM | ninguém sobrescreve — referência rápida neste workshop |

Mantenha o conjunto imposto pequeno. Todo hook e deny rule vai disparar errado alguma vez e alguém tem que manter. 40 regras quase sempre são 37 preferências disfarçadas.

---

## 5. Exercício 03 ( três entregas )

```bash
cd /workspace/projetos/study/capstone
```

**A. Medir**

```text
/usage
> anote: % de cache hit, share de MCP/subagents, maior recomendação
/clear
```

**B. Hook de filtro** — implemente o script da §2.4 e prove com `/hooks` que ele aparece em `PreToolUse`. Rode `ceedling test:all` pelo Claude e cheque no `--debug-file` que o comando foi reescrito.

**C. Regra dura + sandbox**

1. Crie `.claude/settings.json` com deny rule para `Drivers/**` e `Stop` hook do teste.
2. Peça: *"edite Drivers/gpio.c para corrigir o LED"*. O bloqueio deve acontecer pelo harness.
3. Rode o comando de strict sandbox da §4.3 e confirme em `/sandbox` que *Overrides* mostra strict.

## 6. Checklist

- [ ] `/usage` interpretado (cache, atribuição, recomendações).
- [ ] Usei `/clear` entre tarefas e `/compact` com foco.
- [ ] `CLAUDE.md` < 200 linhas; procedimentos estão em skills.
- [ ] Hook `PreToolUse` filtrando saída de teste.
- [ ] `Stop` hook com exit 2 exigindo teste verde.
- [ ] Deny rule impõe o "nunca" (não é só texto).
- [ ] Sandbox strict ativo no Linux; credenciais declaradas.
- [ ] Sei o que fica **fora** do sandbox.

**Próximo:** [Módulo 04 — Subagents, hooks e MCP/CLI](04-subagents-hooks-mcp.md).

**Fontes:** [costs](https://code.claude.com/docs/en/costs.md) · [prompt-caching](https://code.claude.com/docs/en/prompt-caching.md) · [best-practices](https://code.claude.com/docs/en/best-practices.md) · [permission-modes](https://code.claude.com/docs/en/permission-modes.md) · [sandboxing](https://code.claude.com/docs/en/sandboxing.md) · [hooks](https://code.claude.com/docs/en/hooks).
