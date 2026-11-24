# NotebookLM — vídeo curto de 5–8 minutos

Objetivo: um **resumo visual** para divulgar o workshop e servir de entrada para a aula completa. Não tente ensinar todos os comandos aqui — isso é trabalho da versão Claude Code.

---

## 1. Preparar as fontes

O NotebookLM aceita URLs, PDFs, Google Docs e arquivos de texto. Se `.md` não for aceito na sua conta, gere um PDF a partir do livro (`Ctrl+P` → Salvar como PDF) e um `.txt` consolidado:

```bash
cd /workspace/projetos/study
cat README-claude-code.md referencias.md modulos/*.md \
  > /tmp/workshop-fontes.txt
# ou: abra livro/index.html e salve em PDF
```

Carregue no notebook, nesta ordem:

1. `livro/index.html` (ou o PDF gerado dele) — visão geral;
2. `README-claude-code.md` — índice e pré-requisitos;
3. `modulos/00-por-que-firmware.md` — motivação;
4. `modulos/03-economia-acuracia-seguranca.md` — eixo custo/acurácia/segurança;
5. `modulos/05-tdd-unitario-stm32.md` e `modulos/07-hil-bancada.md` — prática;
6. `modulos/08-rastreabilidade.md` e `modulos/09-releases.md` — fechamento;
7. `referencias.md` — para o slide final de fontes.

> Se houver limite de fontes, priorize 1, 4, 5 e 6. O resto pode ir junto no `.txt` consolidado.

## 2. Prompt para o Video Overview

Cole no campo de personalização do **Video Overview**:

```text
Crie um vídeo em português do Brasil, com duração de 6 a 8 minutos,
dirigido a desenvolvedores de firmware que usam STM32 e GitLab.

Tom: didático, direto, sem hype. Comece com o problema concreto:
"compila, passa no unit test, grava no STM32 e falha".

Estruture em 5 blocos:
1. Por que firmware é diferente (restrições de RAM/Flash, hardware fora do processo, errata).
2. As 6 camadas do Claude Code aplicadas ao firmware.
3. Regra central: o que não pode ter exceção vira hook/deny rule, não texto.
4. Contexto limpo e verificação: /usage, /clear, skills, teste antes de "pronto", HIL com lock.
5. Rastreabilidade: REQ → Issue → commit → MR → pipeline → release.

Use APENAS informações presentes nas fontes.
Não invente comandos, nomes de ferramentas, versões nem citações.
Se algum comando não estiver explicitamente nas fontes, omita-o.
No slide final, liste 3 fontes oficiais e 3 fontes de firmware/GitLab.

Evite: promessas de produtividade sem evidência, termos em inglês sem explicação,
e qualquer menção a recursos pagos como se estivessem habilitados por padrão.
```

## 3. Se o NotebookLM oferecer "Personalizar" com foco

Marque: **Estilo de explicação = didático**, **Formato = vídeo explicativo**, **Idioma = Português (Brasil)**. Se houver opção de "voz", escolha a mais neutra possível; vozes muito teatrais atrapalham um público técnico.

## 4. Revisão obrigatória antes de publicar

O NotebookLM parafraseia e pode trocar detalhes. Confira especificamente:

- [ ] Os comandos mostrados existem de verdade (`/usage`, `/clear`, `ceedling test:all`, `st-flash`, `glab`).
- [ ] Não disse que Claude Ship está habilitado por padrão.
- [ ] Não trocou GitLab por GitHub no exemplo.
- [ ] Não inventou que o sandbox cobre `Read`/`Edit` ou MCP local.
- [ ] A cadeia de rastreabilidade termina em release com evidência.
- [ ] Os créditos incluem Anthropic, Beningo, GitLab e a série STM32/GitLab.

## 5. Como usar o resultado

- Coloque o MP4 em `video/assets/resumo-notebooklm.mp4`.
- Use-o no início do workshop (antes dos slides) como "abertura".
- Na aula completa, faça uma pausa após o vídeo e pergunte: *"qual das 6 camadas apareceu aí sem você perceber?"*

## 6. Alternativa: conversa em podcast

Se o NotebookLM oferecer **Deep Dive conversation**, use o mesmo prompt trocando o primeira linha por:

```text
Crie uma conversa em português do Brasil entre dois desenvolvedores de firmware,
com 8 minutos, ...
```

Útil como material de estudo para quem escuta no trajeto; não substitui a aula com comandos na tela.
