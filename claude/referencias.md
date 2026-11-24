# Referências consolidadas

Pesquisa realizada em **24/11/2026**. Critério: priorizar fonte primária (documentação oficial, autores que mostram código, vídeos com demonstração) em vez de lista genérica de "IA para programar".

**Índice oficial atualizado da documentação:** <https://code.claude.com/docs/llms.txt> — use-o para confirmar mudanças de bandeiras, versões e páginas.

A coluna **Uso** diz em qual módulo do estudo a fonte é explorada.

---

## 1. Anthropic / Claude Code — fonte primária

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [Overview](https://code.claude.com/docs/en/overview) | Docs | Superfícies (CLI, VS Code, JetBrains, Desktop, Web), `CLAUDE.md`/`AGENTS.md`, hooks, skills, subagents, MCP, scheduled tasks, automação em **GitHub Actions ou GitLab CI/CD**. | 00, 04, 06 |
| [Quickstart](https://code.claude.com/docs/en/quickstart) | Docs | Instalação nativa, `/quickstart`, primeiro loop. | 00 |
| [Claude Academy — Claude Code 101](https://academy.claude.com/courses/claude-code-101) | Curso gratuito | Noção inicial em vídeo, bom para escalar o time. | 00, vídeo |
| [Claude Academy — Claude Code in Action](https://academy.claude.com/courses/claude-code-in-action) | Curso gratuito | Continuação com casos de uso. | 00, vídeo |
| [Best practices](https://code.claude.com/docs/en/best-practices) | Docs | **O texto mais importante do conjunto**: determinístico vs não determinístico, "diga o que Claude não vê", "dê um check para rodar", `Stop` hook, subagent revisor com `Read,Grep,Glob`, `/code-review`, `/verify`, `/effort`. | 01, 03, 04, 08 |
| [Manage costs effectively](https://code.claude.com/docs/en/costs) | Docs | `/usage`, atribuição por skill/subagent/MCP/`loop`, tecla `f`, cache hit, token ao lado do spinner, `/clear` vs `/compact`, hooks de pré-processamento, MCP vs CLI, modelo/effort, causas de consumo em sessão longa. | 03 |
| [Prompt caching](https://code.claude.com/docs/en/prompt-caching) | Docs | TTL (1h em assinatura, 5min na API), o que invalida cache, troca de modelo, `/compact`, como ler o hit rate. | 03 |
| [Model configuration](https://code.claude.com/docs/en/model-config) | Docs | `/model`, `/effort` (`low`/`high`/`xhigh`/`max`), `ultrathink`, `MAX_THINKING_TOKENS`, auto-compact. | 03 |
| [Permission modes](https://code.claude.com/docs/en/permission-modes) | Docs | Auto mode (classifier, padrão desde v2.1.283), manual, plan mode, `bypassPermissions`, o que o classifier bloqueia. | 03 |
| [Sandboxing](https://code.claude.com/docs/en/sandboxing) | Docs | O que o sandbox cobre e **o que fica fora** (file tools, hooks, MCP local, LSP, `!`), `bubblewrap`+`socat`, strict mode, `credentials` deny/mask, protected paths, `excludedCommands`. | 03 |
| [Memory / CLAUDE.md](https://code.claude.com/docs/en/memory) | Docs | Escrever instruções eficazes, alvo <200 linhas, regras por `paths:` em `.claude/rules/`, `CLAUDE.local.md`. | 01 |
| [Skills](https://code.claude.com/docs/en/skills) | Docs | `SKILL.md`, progressive disclosure, quando o conteúdo é carregado, `/skills`. | 02 |
| [Subagents](https://code.claude.com/docs/en/sub-agents) | Docs | `model: haiku`, `tools: Read, Grep, Glob`, foreground vs background, custo próprio. | 04 |
| [Hooks](https://code.claude.com/docs/en/hooks) e [guia](https://code.claude.com/docs/en/hooks-guide) | Docs | `PreToolUse`, `PostToolUse`, `Stop`, exit 2, `onFailure: "block"`, workspace trust. | 03, 04, 05 |
| [MCP](https://code.claude.com/docs/en/mcp) | Docs | Definições deferidas, `/mcp`, desligar servidor sem remover, tool search. | 04 |
| [Local code review](https://code.claude.com/docs/en/code-review-local) | Docs | `/code-review` funciona local em qualquer plano; revisor em subagent que não viu a conversa. | 08 |
| [Workflows](https://code.claude.com/docs/en/workflows) e [blog](https://claude.com/blog/a-harness-for-every-task-dynamic-workflows-in-claude-code) | Docs/Blog | Orquestração de muitos subagents; módulo avançado opcional. | 04 |
| [Scheduled tasks](https://code.claude.com/docs/en/scheduled-tasks) | Docs | `/schedule`, tarefas noturnas (ex.: análise de falhas HIL) com custo consciente. | 06 |
| [GitLab CI/CD com Claude Code](https://code.claude.com/docs/en/gitlab-ci-cd) | Docs | Configuração de review/triagem via GitLab CI, first-class ao lado do GitHub Actions. | 06 |
| [Agent SDK](https://code.claude.com/docs/en/agent-sdk/overview) | Docs | Quando sair da CLI e orquestrar programaticamente (opcional). | 10 |
| [Anthropic — agentic coding best practices](https://www.anthropic.com/research/claude-code-best-practices) | Artigo | Base conceitual por trás da página de best practices. | 00 |

## 2. IA aplicada a firmware / STM32

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [Why Claude Code for Firmware Development Matters](https://www.beningo.com/why-claude-code-for-firmware-development-matters/) — Jacob Beningo, 20/11/2025 | Artigo | `CLAUDE.md` com **STM32L476RG**, orçamento de RAM/Flash, "nunca malloc/printf", errata de I2C, harness de simulação, scaffolding de testes. | 00, 01 |
| [Claude Code Skills: A Practical Guide for Embedded Developers](https://www.beningo.com/claude-code-skills-embedded-developers/) — 19/03/2026 | Artigo | Skills `/user-stories` (INVEST + Given-When-Then), `/embedded-unit-tests` (Unity/CppUTest + `references/mocking.md` e `edge-cases.md`: rollover de timer 32-bit em 49,7 dias, wraparound de buffer circular, reentrância de ISR) e skill de ADR com seção **Compliance**. | 02, 05, 08 |
| [From AI-Assisted to Agentic Zephyr Development: Build, Flash, Fix](https://www.beningo.com/agentic-zephyr-development-build-flash-fix/) — 24/09/2026 | Artigo | Loop build→flash→observe→fix. Alvo é Zephyr/NXP, padrão serve ao STM32. | 07 |
| [Zephyr Devicetree with AI: A Self-Validating Skill Guide](https://www.beningo.com/zephyr-devicetree-with-ai/) | Artigo | Skill que valida o próprio resultado — ideia transferível para `device tree`/CubeMX. | 02 |
| [Tutorial: usar Claude Code em firmware de robótica](https://www.learnwitharobot.com/p/tutorial-use-anthropic-claude-code) | Tutorial | Passo a passo para escrever firmware com agente. | 00 |
| [Test Driven Development Meets Generative AI](https://www.btc-embedded.com/test-driven-development-meets-generative-ai) — Artem Oppermann, 19/05/2025 | Artigo (vendor) | IA gera teste a partir de requisito em linguagem natural; **se não consegue gerar, o requisito está ambíguo**. Não substitui model checking/ISO 26262. | 05, 08 |
| [BTC TestAgent](https://www.btc-embedded.com/product/btc-testagent/) | Produto (vendor) | Geração autônoma de testes HIL de nível de sistema. Referência de mercado, não receita gratuita. | 07 |
| [AI-Assisted Unit Test Writing and Test-Driven Code Refactoring: A Case Study](https://arxiv.org/abs/2604.03135) — 2026 | Artigo acadêmico | Evidência sobre geração de testes e refatoração segura. | 05 |
| [Embedded Build Process: 5 Signs It's Holding You Back](https://www.beningo.com/podcast/) | Podcast | Diagnóstico de build/testing lento — motivação para CI eficiente. | 06 |

## 3. GitLab + embedded / HIL

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [4 ways to accelerate embedded development with GitLab](https://about.gitlab.com/blog/4-ways-to-accelerate-embedded-development-with-gitlab/) — Matt DeLaney & Darwin Sanoy, 05/06/2025 | Blog oficial | SIL→PIL→HIL em pipelines, Device Cloud, **demo do Embedded World 2025 de 24:42 a 36:51**, compliance como código e MR gating. | 06, 07 |
| [Why now is the time for embedded DevSecOps](https://about.gitlab.com/blog/why-now-is-the-time-for-embedded-devsecops/) — 01/07/2025 | Blog oficial | Feedback lento, compliance manual, ilhas de desenvolvimento; componente [On-Prem Device Cloud](https://gitlab.com/guided-explorations/embedded/ci-components/device-cloud). | 06, 09 |
| [GitLab Unfiltered — Hardware In the CI Loop](https://www.youtube.com/watch?v=WFSQrmdDHIY) (update: <https://youtu.be/PHQ5sw6SPDQ>) | Vídeo | Darwin Sanoy sobre hardware dentro do CI loop. | 06, 07 |
| [Embedded DevOps Workshop](https://gitlab.com/guided-explorations/embedded/workshops/embedded-devops-workshop-refactoring-to-ci) + [TUTORIAL2.md](https://gitlab.com/guided-explorations/embedded/workshops/embedded-devops-workshop-refactoring-to-ci/-/blob/main/TUTORIAL2.md) | Tutorial oficial | Build automatizado em pipeline GitLab. | 06 |
| [Exemplos de HIL com GitLab CI](https://gitlab.com/explore/projects/topics/Working+Examples+for+Embedded+Hardware+In+the+Loop+%2528HIL%2529+Testing+Using+GitLab+CI) | Repositórios | Bancada, ambiente GUI e firmware prontos para study. | 07 |
| [GitLab as a Tool for CI/CD in Embedded Systems Development](https://321gang.com/gitlab-as-a-tool-for-ci-cd-in-embedded-systems-development-a-step-by-step-guide/) | Guia (vendor) | Git LFS, submodules, HIL, análise estática passo a passo. | 06 |
| [Custom Compliance Frameworks no GitLab](https://about.gitlab.com/blog/introducing-custom-compliance-frameworks-in-gitlab/) | Blog oficial | Como exigir aprovação/checks em MRs. Visão de governança, não implementação obrigatória. | 09 |
| [Requirements management](https://docs.gitlab.com/user/project/requirements/) e [epic #5171](https://gitlab.com/groups/gitlab-org/-/work_items/5171) | Docs | Atenção: requirements migram para work items. Prefira **spec versionada + Issues + MRs + evidência de CI**. | 08 |

## 4. STM32 + GitLab CI na prática

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [DevOps for Embedded — parte 2](https://www.stupid-projects.com/posts/devops-for-embedded-part-2/) — Dimitris Tassopoulos, 2019/att 2024 | Artigo | CDE em Docker/Packer, `gitlab-ci.yml` build→testes→artefato, runners EC2, comparação de tempos. | 06 |
| [DevOps for Embedded — parte 3](https://www.stupid-projects.com/posts/devops-for-embedded-part-3/) | Artigo | **O mais relevante do conjunto**: unit tests com CppUTest, mock de hardware, estágios `build → flash → test`, test farm com Nanopi rodando gitlab-runner, `st-flash --reset write ... 0x8000000`, testes com Robot Framework lendo GPIO via I2C, e por que não rodar dois jobs I2C no mesmo hardware em paralelo. | 05, 06, 07 |
| [Repo `stm32f103-i2c-io-expander`](https://gitlab.com/dimtass/stm32f103-i2c-io-expander) | Código | Firmware de exemplo usado na série. | 05, 07 |
| [STM32CubeIDE for VS Code — Testing and validation](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/development/testing_validation.html) | Docs ST | HIL no contexto STM32. | 07 |
| [How to Set Up CI/CD for Embedded Projects](https://hubble.com/community/guides/how-to-set-up-ci-cd-for-embedded-projects/) | Guia | Cross-compilação, unit tests em host, emulação (exemplo GitHub Actions; traduzir os conceitos para GitLab). | 06 |

> Os posts do stupid-projects são de 2019, atualizados em 2024. A arquitetura separando runner de build do runner com hardware continua válida.

## 5. Loop fechado: IA escreve → testa no hardware → corrige

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [From Idea to Working ESP32 — AI Does Everything!](https://www.youtube.com/watch?v=HmNyyT2poF8) — Andreas Spiess, Nov/2026 | Vídeo | **Embedded AI Harness**: container + testbench Raspberry Pi + quatro camadas **host → target → bench → field** + fault injection + testes caixa-preta. ESP32/ESP-IDF, padrão transplantável para STM32. | 07, 10 |
| [Automate ESP32 Development Using an Embedded AI Harness](https://www.geeky-gadgets.com/ai-driven-esp32-development/) — 09/11/2026 | Artigo | Resumo das quatro camadas e do loop de refinamento guiado por especificação. | 07 |
| [AI in hardware-in-the-loop testing: the loop embedded never closed](https://betterdevices.io/blog/ai-in-hardware-in-the-loop-testing-the-loop-embedded-never-closed/) — 21/09/2026 | Artigo | Tese: "compila, passa no unit test, grava e falha" é problema estrutural — o agente não vê sinal, rail de 3,3 V ou prioridade de IRQ sob carga. | 07 |
| [Hardware-in-the-Loop Testing: Kernels on Real CPUs](https://www.youtube.com/results?search_query=Hardware-in-the-Loop+Testing+Kernels+on+Real+CPUs+Richard+Seidl) | Podcast/vídeo | Conversa sobre HIL em kernels reais (buscar pelo título). | 07 |

## 6. Base TDD / testes em C

| Fonte | Tipo | O que buscar | Uso |
|---|---|---|---|
| [Unit-testing (embedded) C applications with Ceedling](https://dmitryfrank.com/articles/unit_testing_embedded_c_applications) | Artigo | Unity + CMock + CException, TDD host-based em C. | 05 |
| [Modern unit testing in C with TDD and Ceedling](https://www.embedded.com/modern-unit-testing-in-c-with-tdd-and-ceedling/) | Artigo | Introdução consolidada ao ecossistema. | 05 |
| [TDD for Embedded Software with Unity and Ceedling](https://www.debassociates.com/blog/test-driven-development-for-embedded-software-with-unity-and-ceedling/) | Artigo | Exemplo passo a passo. | 05 |
| [Test-Driven Development (TDD) in Firmware Writing](https://www.elektormagazine.com/articles/test-driven-development-tdd-in-firmware-writing) | Artigo | Mentalidade test-first para firmware. | 05 |
| [Test Driven Development for Embedded C — James W. Grenning](https://www.google.com/search?q=Test+Driven+Development+for+Embedded+C+Grenning) | Livro | Referência clássica (já listada no `readme.md` original). | 05 |

## 7. Como manter esta lista viva

1. Rode `curl -fsSL https://code.claude.com/docs/llms.txt | grep -i <assunto>` para descobrir novas páginas.
2. Para cada fonte nova, registre **data de acesso**, **o que muda na prática** e **qual módulo usa**.
3. Se a fonte for de vendor, marque `(vendor)` — não venda conclusão de ferramenta como verdade universal.
4. Se o vídeo estiver em outro idioma, anote o minuto exato da demonstração; é isso que economiza tempo na hora de gravar a aula.
