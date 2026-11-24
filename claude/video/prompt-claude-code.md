# Claude Code — gerando os assets da aula completa

Este prompt transforma o material consolidado em roteiro final, legendas, sugestões de captura de tela e um plano de gravação. Ele **não renderiza vídeo sozinho**: a renderização depende de `ffmpeg` + TTS (ver `render.sh`).

---

## 1. Prompt para uma sessão nova

Abra uma sessão limpa no diretório do material e cole:

```text
Você é o produtor desta vídeo-aula. Leia, nesta ordem:
  README-claude-code.md
  workshop/plano-de-aula.md
  workshop/slides.html
  video/roteiro.md
  video/storyboard.md
  modulos/03-economia-acuracia-seguranca.md
  modulos/05-tdd-unitario-stm32.md
  modulos/07-hil-bancada.md
  modulos/08-rastreabilidade.md

Tarefa: gere os assets finais em video/assets/.

1. Reescreva video/roteiro.md como narração contínua em pt-BR,
   com marcas [SLIDE n] e [DEMO], mantendo o tempo total entre 22 e 25 min.
   Não invente comandos. Todo comando deve existir no capstone/ ou nos módulos.

2. Gere video/legendas.srt no formato SubRip,
   com blocos de no máximo 2 linhas e 7 segundos,
   pontuação em pt-BR e os termos HIL, junit, hook e deny rule por extenso
   na primeira ocorrência.

3. Gere video/assets/comandos-por-cena.md:
   para cada cena, a lista exata de comandos a digitar e a saída esperada.

4. Gere video/assets/checklist-gravacao.md:
   pré-gravação (token mascarado, zoom 125%, fonte ≥16px),
   durante (pausas após cada demo), pós (revisar SRT).

5. Gere video/README.md explicando como reproduzir a gravação,
   inclusive o Plano B (sem hardware) e Plano C (sem TTS) do storyboard.

Restrições:
- Não use Claude Ship, Agent Teams ou scheduled tasks como se fossem padrão.
- Não troque GitLab por GitHub.
- Não afirme que o sandbox cobre Read/Edit/WebFetch/MCP local.
- Se alguma informação não estiver nas fontes, escreva "verificar na doc" em vez de chutar.
- Não execute render.sh; apenas escreva os arquivos.
```

## 2. Por que uma sessão nova

A sessão que escreveu o material já está contaminada com tentativas, correções e contexto acumulado — exatamente o problema que o Módulo 01 descreve para `SPEC.md`. Uma sessão nova começa com os arquivos como fonte da verdade.

## 3. Depois que os assets existirem

```bash
# 1. revise o SRT e o roteiro final
sed -n '1,60p' video/legendas.srt

# 2. confira os comandos contra o capstone
grep -R "ceedling\|st-flash\|glab\|flock" video/assets/comandos-por-cena.md

# 3. gere a versão curta no NotebookLM (prompt próprio)
# 4. capture os slides (ver render.sh) e grave a narração
# 5. só então execute o render
bash video/render.sh
```

## 4. O que o Claude Code **não** deve fazer nesta etapa

- Inventar `"demo falha proposital"` que não existe no capstone.
- Gerar narração com voz própria — ele não tem saída de áudio; use TTS ou gravação humana.
- Rodar `glab release create` de verdade durante a gravação. Use um projeto de teste.
- Colocar tokens reais no `.srt` ou nos screenshots.

## 5. Quality gate antes do render

Peça ao mesmo agente, agora em modo revisão:

```text
Use o subagent reviewer para auditar video/roteiro.md e video/legendas.srt
contra os arquivos do capstone/. Reporte:
- comandos que não existem;
- afirmações sobre recursos pagos ou não habilitados;
- troca indevida de GitLab por GitHub;
- erros de português.
Não edite nada: apenas relate.
```

Isso repete o padrão do Módulo 04: revisor independente, sem viés da sessão que escreveu.
