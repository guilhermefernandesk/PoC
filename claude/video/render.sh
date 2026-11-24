#!/usr/bin/env bash
# Renderiza a aula completa a partir de slides + narração + legendas.
# Este ambiente NÃO tem ffmpeg/TTS instalados (verificado). O script falha cedo
# com instruções claras em vez de gerar um arquivo quebrado.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VIDEO="$ROOT/video"
ASSETS="$VIDEO/assets"
OUT="$VIDEO/aula-completa.mp4"

say() { printf '\033[1;36m[render]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[render] ERRO:\033[0m %s\n' "$*" >&2; exit 1; }

need() { command -v "$1" >/dev/null 2>&1; }

say "Diretório do material: $ROOT"

# ---------------------------------------------------------------------------
# 1. Dependências obrigatórias
# ---------------------------------------------------------------------------
missing=()
for cmd in ffmpeg ffprobe; do
  need "$cmd" || missing+=("$cmd")
done

if (( ${#missing[@]} )); then
  cat >&2 <<EOF
Faltam dependências: ${missing[*]}

Instale no Debian/Ubuntu:
  sudo apt-get update
  sudo apt-get install -y ffmpeg

Debian/Ubuntu com snap/AppArmor pode exigir:
  sudo apt-get install -y bubblewrap socat

Depois rode novamente:
  bash "$VIDEO/render.sh"

Sem ffmpeg, o material ainda está utilizável:
  - roteiro:  $VIDEO/roteiro.md
  - legendas: $VIDEO/legendas.srt (quando gerado pelo prompt-claude-code.md)
  - slides:   $ROOT/workshop/slides.html (abrir no navegador e imprimir)
EOF
  exit 1
fi

# ---------------------------------------------------------------------------
# 2. Dependências opcionais
# ---------------------------------------------------------------------------
HAVE_CHROMIUM=0; HAVE_PDFTOPPM=0; HAVE_TTS=0
for c in chromium chromium-browser google-chrome; do
  if need "$c"; then HAVE_CHROMIUM=1; CHROMIUM_BIN="$c"; break; fi
done
need pdftoppm && HAVE_PDFTOPPM=1
if need edge-tts; then HAVE_TTS=1; TTS_CMD=(edge-tts);
elif need piper; then HAVE_TTS=1; TTS_CMD=(piper);
elif need espeak-ng; then HAVE_TTS=1; TTS_CMD=(espeak-ng);
fi

mkdir -p "$ASSETS/slides"

# ---------------------------------------------------------------------------
# 3. Slides: tenta gerar PNGs a partir do HTML
# ---------------------------------------------------------------------------
shopt -s nullglob
slide_pngs=("$ASSETS"/slides/slide-*.png)
if (( ${#slide_pngs[@]} == 0 )); then
  if (( HAVE_CHROMIUM && HAVE_PDFTOPPM )); then
    say "Gerando PDF dos slides..."
    "$CHROMIUM_BIN" --headless --disable-gpu --no-sandbox \
      --print-to-pdf="$ASSETS/slides.pdf" \
      "file://$ROOT/workshop/slides.html" >/dev/null 2>&1 || true
    if [[ -f "$ASSETS/slides.pdf" ]]; then
      pdftoppm -png -r 110 "$ASSETS/slides.pdf" "$ASSETS/slides/slide" >/dev/null 2>&1 || true
    fi
  fi
fi

# ---------------------------------------------------------------------------
# 4. Narração: tenta TTS se houver texto e sintetizador
# ---------------------------------------------------------------------------
if [[ ! -f "$ASSETS/narracao.txt" ]]; then
  cat > "$ASSETS/narracao.txt" <<'EOF'
Coloque aqui um parágrafo de narração por linha, na ordem das cenas do roteiro.md.
A linha 1 vira voice-01.mp3, a linha 2 vira voice-02.mp3 e assim por diante.
EOF
  say "Criei modelo em $ASSETS/narracao.txt — preencha com a narração final."
fi

if (( HAVE_TTS )) && [[ -s "$ASSETS/narracao.txt" ]]; then
  n=0
  while IFS= read -r line; do
    [[ -z "$line" || "$line" == Coloque\ aqui* ]] && continue
    n=$((n+1))
    dst=$(printf '%s/voice-%02d.mp3' "$ASSETS" "$n")
    [[ -f "$dst" ]] && continue
    say "Sintetizando $dst"
    if [[ "${TTS_CMD[0]}" == "edge-tts" ]]; then
      edge-tts --voice pt-BR-FranciscaNeural --text "$line" --write-media "$dst" >/dev/null 2>&1 || true
    elif [[ "${TTS_CMD[0]}" == "piper" ]]; then
      printf '%s\n' "$line" | piper --model pt_BR-faber-medium --output_file "$dst" >/dev/null 2>&1 || true
    else
      espeak-ng -v pt -s 150 -w "${dst%.mp3}.wav" "$line" >/dev/null 2>&1 || true
      [[ -f "${dst%.mp3}.wav" ]] && ffmpeg -y -i "${dst%.mp3}.wav" "$dst" >/dev/null 2>&1
    fi
  done < "$ASSETS/narracao.txt"
fi

# ---------------------------------------------------------------------------
# 5. Monta as cenas
# ---------------------------------------------------------------------------
voices=("$ASSETS"/voice-*.mp3)
slides=("$ASSETS"/slides/slide-*.png)

(( ${#voices[@]} )) || die "Nenhum voice-NN.mp3 em $ASSETS. Grave a narração ou preencha narracao.txt com TTS disponível."
(( ${#slides[@]} )) || die "Nenhum slide-NN.png em $ASSETS/slides. Capture slides.html (imprimir em PDF + pdftoppm)."

segments=()
idx=0
for voice in "${voices[@]}"; do
  idx=$((idx+1))
  slide=$(printf '%s/slides/slide-%02d.png' "$ASSETS" "$idx")
  [[ -f "$slide" ]] || slide="${slides[0]}"
  seg="$ASSETS/seg-$(printf '%02d' "$idx").mp4"
  say "Cena $idx: $(basename "$slide") + $(basename "$voice")"
  ffmpeg -y -loglevel error \
    -loop 1 -framerate 30 -i "$slide" \
    -i "$voice" \
    -c:v libx264 -tune stillimage -pix_fmt yuv420p \
    -c:a aac -b:a 128k -shortest "$seg"
  segments+=("$seg")
done

# ---------------------------------------------------------------------------
# 6. Concatena e anexa legendas
# ---------------------------------------------------------------------------
concat="$ASSETS/concat.txt"
: > "$concat"
for seg in "${segments[@]}"; do
  printf "file '%s'\n" "$seg" >> "$concat"
done

say "Concatenando cenas..."
ffmpeg -y -loglevel error -f concat -safe 0 -i "$concat" -c copy "$ASSETS/aula-sem-legendas.mp4"

if [[ -f "$VIDEO/legendas.srt" ]]; then
  say "Anexando legendas..."
  ffmpeg -y -loglevel error -i "$ASSETS/aula-sem-legendas.mp4" \
    -i "$VIDEO/legendas.srt" \
    -map 0 -map 1 -c copy -c:s mov_text "$OUT"
else
  cp "$ASSETS/aula-sem-legendas.mp4" "$OUT"
  say "Aviso: legendas.srt não encontrado; vídeo gerado sem legendas."
fi

say "Pronto: $OUT"
ffprobe -v error -show_entries format=duration,size -of default=nw=1 "$OUT" || true
