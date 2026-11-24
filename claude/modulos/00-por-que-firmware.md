# Módulo 00 — Por que firmware é diferente (e por que isso muda o uso da IA)

**Tempo:** 25 min · **Entrega:** mental model das 6 camadas + `claude` funcionando no seu projeto.

---

## 1. O problema que este workshop resolve

Quase todo tutorial de IA para código follow along com uma API REST: roda teste em 2 segundos, vê erro, corrige, pronto. Firmware não é assim:

| Característica | Consequência para o agente |
|---|---|
| RAM/Flash contadas em bytes | "Funciona no meu host" não significa nada. O agente precisa saber o orçamento. |
| Hardware fora do processo | Compilar não prova que o pino 5 do GPIO está certo. Falta o loop HIL. |
| Erra de silício / errata | O modelo não vê o datasheet revisão C. Tem que entrar no contexto. |
| Feedback lento (flash, reset, serial) | Cada iteração custa tempo e desgaste da bancada. Retrabalho é caro. |
| Concorrência e ISR | Bug depende de timing; teste determinístico precisa de mock e disciplina. |
| Release é binário com versão | Rastrear `v1.4.2` até o requisito é parte do produto, não burocracia. |

> Frase para guardar: **a IA não é o problema; o problema é ela decidir com informação incompleta e ninguém ter um cheque determinístico para barrar.**

## 2. O mapa de 6 camadas

Todo o resto do workshop é uma das camadas abaixo:

```text
1. CONTEXTO      CLAUDE.md, SPEC.md, @arquivo, regras por caminho
2. SKILLS        procedimentos carregados só quando chamados
3. ISOLAMENTO    subagents, worktrees, permissões, sandbox
4. HOOKS         regras que o harness executa (não o modelo)
5. INTEGRAÇÃO    MCP/CLI, GitLab Issues e MRs, plugins
6. ORQUESTRAÇÃO  GitLab CI, scheduled tasks, Agent SDK (opcional)
```

Camadas 1–2 aumentam **acertividade**. Camadas 3–4 aumentam **segurança e previsibilidade**. Camadas 5–6 fecham o **ciclo de evidência**.

## 3. Superfícies: uma config, vários lugares

Claude Code roda em terminal, VS Code, JetBrains, Desktop e Web. **O motor é o mesmo**: `CLAUDE.md`, `.claude/settings.json`, skills e MCP valem em todos. Não estude "Claude Code da IDE" separado do da CLI.

```bash
# instalação nativa (Linux/macOS/WSL)
curl -fsSL https://claude.ai/install.sh | bash
claude --version

# primeiro contato
claude
> /quickstart
> /model
> /doctor
```

Se o projeto usar `AGENTS.md` em vez de `CLAUDE.md`, ele é suportado. Ter os dois é redundante, não errado — veja o Módulo 01.

## 4. Exercício 00 — explorar sem editar

No diretório do seu projeto GitLab (ou no `capstone/` deste material):

```text
Você é um novo dev no projeto. Sem modificar nenhum arquivo:
1. descubra como o firmware é compilado e gravado;
2. aponte onde ficam as regras de negócio versus os drivers;
3. diga qual comando você usaria para rodar só os testes unitários;
4. liste três coisas que você NÃO consegue saber lendo o código.
```

Por que "sem modificar"? Porque o objetivo do primeiro contato é medir **o que o agente consegue inferir**. A diferença entre o que ele inferiu e o que você sabia é exatamente o conteúdo do `CLAUDE.md` que você vai escrever no Módulo 01.

Saia com `Esc`. Se quiser desfazer algo: `/rewind`.

## 5. Erros comuns de quem vem de GitHub Copilot

- **Prompt vago:** "melhore esse firmware". Sem alvo de verificação, o agente escolhe o próprio critério de pronto.
- **Pedir para "testar no hardware" sem dizer qual hardware.** Ele não tem ST-Link; ele tem permissão para chamar `st-flash` se você deixar.
- **Colar o datasheet inteiro no prompt.** Contexto é recurso finito: use `@arquivo` e trechos, não o PDF completo.
- **Confiar que "a IDE avisou".** O agente na CLI não vê o mesmo estado do editor a menos que você dê o erro.
- **Achar que permissão é segurança.** Auto mode usa um classificador; regra dura é hook/deny. Ver Módulo 03.

## 6. Checklist de conclusão

- [ ] `claude --version` executa.
- [ ] Entendeu para que serve cada uma das 6 camadas.
- [ ] Rodou o Exercício 00 sem editar arquivos.
- [ ] Anotou pelo menos 3 fatos que o agente não conseguiu descobrir sozinho.

**Próximo:** [Módulo 01 — Contexto que não cabe no código](01-contexto.md).
