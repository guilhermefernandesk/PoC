# Workshop — Claude Code para Firmware STM32 com GitLab

Material de estudo em português, mão na massa, para ensinar desenvolvedores a usar Claude Code **em um projeto real de firmware STM32 versionado no GitLab**: TDD, testes unitários, HIL, CI/CD, releases e rastreabilidade — com atenção explícita a custo de tokens, acurácia e segurança.

Este arquivo convive com o [`readme.md`](readme.md) original (arquitetura de firmware STM32). Ele é o índice do **novo** material.

---

## 1. Para quem é

- Desenvolvedor individual ou pequeno time que escreve firmware em C/C++ para STM32.
- Projeto versionado no **GitLab** (com MRs, Issues e Runners).
- Quer sair de "IA que autocompleta código" para "agente que obedece regras, roda testes e deixa rastro".
- Não é material de administração corporativa: MDM, managed settings e compliance frameworks aparecem só como referência rápida.

## 2. Pré-requisitos

```bash
# 1. Claude Code instalado (instalação nativa)
curl -fsSL https://claude.ai/install.sh | bash
claude --version

# 2. Ferramentas do dia a dia
sudo apt-get install -y bubblewrap socat git make cmake python3 pipx
pipx install ceedling          # Unity + CMock + CException
pipx install glab              # CLI do GitLab (mais barata em contexto que MCP)

# 3. Autenticação
glab auth login
claude /login

# 4. Confira o sandbox no Linux
claude
> /sandbox
```

Placa recomendada para os exercícios: STM32F103 (blue-pill) ou qualquer STM32 com ST-Link. Se não houver hardware, o Módulo 05 roda 100% no host e o Módulo 07 pode ser acompanhado com o harness simulado.

## 3. Árvore do material

```text
study/
├── readme.md                       # guia original de arquitetura STM32 (preservado)
├── README-claude-code.md           # este arquivo
├── referencias.md                  # fontes consolidadas (docs, artigos, vídeos)
├── build-livro.py                  # gera livro/index.html a partir dos módulos
├── livro/index.html                # livro single-file, offline, imprimível
├── modulos/00 … 10 + apêndice      # fonte canônica do conteúdo
├── capstone/                       # config funcional para copiar ao projeto GitLab
├── workshop/plano-de-aula.md       # aula de 90 minutos
├── workshop/slides.html            # slides single-file
└── video/                          # roteiro, storyboard e prompts de geração
```

## 4. Ordem de leitura

| Etapa | Arquivo | Tempo |
|---|---|---|
| 1 | [`referencias.md`](referencias.md) | 15 min |
| 2 | Módulos 00 → 02 | 45 min |
| 3 | **Módulo 03** (economia, acurácia, segurança) | 30 min |
| 4 | Módulos 04 → 07 | 90 min |
| 5 | Módulos 08 → 10 + apêndice | 45 min |
| 6 | [`workshop/plano-de-aula.md`](workshop/plano-de-aula.md) | 90 min de aula |
| 7 | [`video/roteiro.md`](video/roteiro.md) | assistir/gravar |

## 5. Gerar o livro HTML

Os `.md` em `modulos/` são a **fonte única**. O HTML é gerado:

```bash
cd /workspace/projetos/study
python3 build-livro.py
# saída: livro/index.html
```

Abra `livro/index.html` no navegador. Não há CDN: funciona offline. Use `Ctrl+P` para salvar em PDF (cada módulo começa em uma página nova).

## 6. Ministrar o workshop

```bash
# slides com navegação por ←/→ e tecla P para imprimir
xdg-open workshop/slides.html
```

O plano de aula traz cronograma, exercícios, gabarito comentado e rubrica de avaliação.

## 7. Gerar a vídeo-aula

Dois caminhos, complementares:

1. **NotebookLM** — resumo de 5–8 min. Siga [`video/prompt-notebooklm.md`](video/prompt-notebooklm.md): suba `referencias.md`, este README, `modulos/*.md` e `workshop/plano-de-aula.md`, cole o prompt em pt-BR. Revise os comandos antes de publicar.
2. **Claude Code** — aula completa de ~20–25 min. Siga [`video/roteiro.md`](video/roteiro.md), [`video/storyboard.md`](video/storyboard.md) e [`video/prompt-claude-code.md`](video/prompt-claude-code.md). O `render.sh` só deve ser executado depois que as dependências (`ffmpeg`, TTS) forem confirmadas.

## 8. Princípios que atravessam todo o material

1. **O que não pode ter exceção vira hook, deny rule ou managed setting — nunca só texto.**
2. **Claude recebe o que não consegue ver**: restrições de RAM/Flash, erros de silício, processo de release, decisões de ADR.
3. **Contexto é dinheiro e atenção**: `/clear` entre tarefas, skills para procedimentos longos, `CLAUDE.md` com menos de 200 linhas.
4. **Verificação antes de "pronunciar done"**: comando de teste, exit code, fixture ou revisor independente.
5. **Rastreabilidade é cadeia, não documento**: `REQ → Issue → commit → MR → pipeline → release`.

## 9. Avisos

- Material pesquisado em **24/11/2026**. A documentação muda rápido; confirme versões com `claude --version` e [`llms.txt`](https://code.claude.com/docs/llms.txt).
- Recursos citados como disponíveis em certos planos/versões estão marcados no texto. Não presuma Claude Ship, Agent Teams ou scheduled tasks habilitados sem checar.
- Nenhum projeto original foi modificado na produção deste material. O capstone é um esqueleto novo, pronto para ser copiado.
