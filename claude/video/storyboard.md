# Storyboard — aula completa

| # | Tempo | Slide(s) | Ação na tela | Resultado esperado |
|---|---|---|---|---|
| 1 | 0:00–0:45 | 1 | Narração sobre o LED que não pisca | Gancho |
| 2 | 0:45–2:20 | 4 | Desenho das 6 camadas (slide estático) | Modelo mental |
| 3 | 2:20–5:10 | 5–7 | `claude`, `/init`, `/context all`, `/doctor` | `CLAUDE.md` com 4 seções |
| 4 | 5:10–9:00 | 8–11 | `/usage`, `/compact`, `/clear`, `/usage` | Cache hit sobe, MCP cai |
| 5 | 9:00–13:30 | 9, 12–14 | deny rule + pedido de edição + `/sandbox` | Bloqueio pelo harness |
| 6 | 13:30–17:00 | 15–16 | skill → teste vermelho → verde | TDD visível |
| 7 | 17:00–20:00 | 17–18 | `glab ci status`, YAML, `flock` | Pipeline com HIL |
| 8 | 20:00–22:00 | 19–20 | validador, Issue, MR, release | Cadeia REQ→tag |
| 9 | 22:00–23:00 | 21–23 | recap + entrega | Fechamento |

## Assets por cena

```text
video/assets/
├── slide-01.png … slide-23.png     # capturas do slides.html
├── voice-01.mp3 … voice-09.mp3     # narração (edge-tts / piper / humana)
└── demo-03.mp4 … demo-08.mp4       # capturas de terminal (opcional)
```

## Plano B (sem hardware)

Se não houver STM32 na gravação:

- cena 7 substitui `st-flash` por `pytest --collect-only tests/hil`;
- mostre o `flock` no YAML e explique o lock;
- use o log de exemplo em `capstone/reports/hil/` (quando gerado) para o resumo.

## Plano C (sem TTS)

Gere só `video/legendas.srt` a partir do `roteiro.md` e grave a voz depois. O `render.sh` funciona com áudio humano colocado em `video/assets/voice-NN.mp3`.

## Checklist de qualidade antes de publicar

- [ ] Nenhum token real aparece na tela.
- [ ] Comandos do roteiro batem com os arquivos do `capstone/`.
- [ ] `glab`/`st-flash` não foram inventados para a câmera.
- [ ] Legendas revisadas (termos: HIL, junit, hook, deny rule).
- [ ] Versão curta do NotebookLM revisada quanto a comandos alucinados.
