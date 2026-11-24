#!/usr/bin/env python3
"""Gera livro/index.html a partir dos Markdown em modulos/.

Sem dependências externas. A fonte canônica é o Markdown; o HTML é gerado.
Uso: python3 build-livro.py
"""
from __future__ import annotations

import html
import re
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "livro" / "index.html"

DOCS = [
    ("Introdução", ROOT / "README-claude-code.md"),
    ("00 — Por que firmware é diferente", ROOT / "modulos" / "00-por-que-firmware.md"),
    ("01 — Contexto", ROOT / "modulos" / "01-contexto.md"),
    ("02 — Skills", ROOT / "modulos" / "02-skills.md"),
    ("03 — Economia, acurácia e segurança", ROOT / "modulos" / "03-economia-acuracia-seguranca.md"),
    ("04 — Subagents, hooks e MCP", ROOT / "modulos" / "04-subagents-hooks-mcp.md"),
    ("05 — TDD e testes unitários", ROOT / "modulos" / "05-tdd-unitario-stm32.md"),
    ("06 — CI/CD no GitLab", ROOT / "modulos" / "06-gitlab-ci.md"),
    ("07 — HIL e bancada", ROOT / "modulos" / "07-hil-bancada.md"),
    ("08 — Rastreabilidade", ROOT / "modulos" / "08-rastreabilidade.md"),
    ("09 — Releases", ROOT / "modulos" / "09-releases.md"),
    ("10 — Capstone", ROOT / "modulos" / "10-capstone.md"),
    ("Apêndice — GitLab vs GitHub", ROOT / "modulos" / "apendice-gitlab-vs-github.md"),
    ("Referências", ROOT / "referencias.md"),
]


def slug(text: str) -> str:
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    text = re.sub(r"[^\w\s-]", "", text.lower())
    return re.sub(r"[-\s]+", "-", text).strip("-") or "secao"


def inline(text: str) -> str:
    text = html.escape(text, quote=False)
    codes: list[str] = []

    def stash(match: re.Match[str]) -> str:
        codes.append(match.group(1))
        return f"\x00CODE{len(codes) - 1}\x00"

    text = re.sub(r"`([^`]+)`", stash, text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\w/])(https?://[^\s<)]+)", r'<a href="\1">\1</a>', text)

    for idx, code in enumerate(codes):
        text = text.replace(f"\x00CODE{idx}\x00", f"<code>{code}</code>")
    return text


def is_table_sep(line: str) -> bool:
    return bool(re.match(r"^\s*\|?[\s:|-]+\|?\s*$", line)) and "|" in line and "-" in line


def render_table(lines: list[str]) -> tuple[str, int]:
    rows = []
    i = 0
    while i < len(lines) and lines[i].strip().startswith("|"):
        rows.append(lines[i].strip())
        i += 1
    if len(rows) < 2 or not is_table_sep(rows[1]):
        return "", 0

    def cells(row: str) -> list[str]:
        return [c.strip() for c in row.strip().strip("|").split("|")]

    head = cells(rows[0])
    body = [cells(r) for r in rows[2:]]
    out = ["<table><thead><tr>"]
    out += [f"<th>{inline(c)}</th>" for c in head]
    out.append("</tr></thead><tbody>")
    for row in body:
        out.append("<tr>")
        out += [f"<td>{inline(c)}</td>" for c in row]
        out.append("</tr>")
    out.append("</tbody></table>")
    return "\n".join(out), i


LIST_RE = re.compile(r"^(\s*)([-*]|\d+\.)\s+(.*)$")


def render_list(lines: list[str]) -> tuple[str, int]:
    out: list[str] = []
    stack: list[int] = []
    i = 0
    while i < len(lines):
        m = LIST_RE.match(lines[i])
        if not m:
            break
        indent = len(m.group(1).replace("\t", "  "))
        marker = m.group(2)
        content = m.group(3)
        tag = "ol" if marker.endswith(".") else "ul"

        while stack and indent < stack[-1][0]:
            _, closing = stack.pop()
            out.append(f"</{closing}>")
        if not stack or indent > stack[-1][0]:
            out.append(f"<{tag}>")
            stack.append((indent, tag))
        elif stack[-1][1] != tag:
            out.append(f"</{stack[-1][1]}>")
            stack.pop()
            out.append(f"<{tag}>")
            stack.append((indent, tag))

        checked = re.match(r"^\[([ xX])\]\s*(.*)$", content)
        if checked:
            mark = "✓" if checked.group(1).lower() == "x" else "□"
            content = f"{mark} {checked.group(2)}"
        out.append(f"<li>{inline(content)}</li>")
        i += 1

    while stack:
        _, closing = stack.pop()
        out.append(f"</{closing}>")
    return "\n".join(out), i


def render_markdown(md: str, prefix: str) -> tuple[str, list[tuple[int, str, str]]]:
    lines = md.splitlines()
    out: list[str] = []
    toc: list[tuple[int, str, str]] = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        if stripped.startswith("```"):
            lang = stripped[3:].strip()
            i += 1
            code_lines = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code_lines.append(lines[i])
                i += 1
            i += 1
            cls = f' class="language-{html.escape(lang)}"' if lang else ""
            out.append(
                '<div class="code-wrap"><button class="copy" type="button">copiar</button>'
                f"<pre><code{cls}>{html.escape(chr(10).join(code_lines))}</code></pre></div>"
            )
            continue

        heading = re.match(r"^(#{1,6})\s+(.*)$", stripped)
        if heading:
            level = len(heading.group(1))
            title = heading.group(2).strip()
            ident = f"{prefix}-{slug(title)}"
            out.append(f'<h{level} id="{ident}">{inline(title)}</h{level}>')
            if level in (1, 2):
                toc.append((level, title, ident))
            i += 1
            continue

        if stripped in ("---", "***", "___"):
            out.append("<hr>")
            i += 1
            continue

        if stripped.startswith(">"):
            quoted = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quoted.append(lines[i].strip().lstrip(">").strip())
                i += 1
            inner, _ = render_markdown("\n".join(quoted), prefix + "-q")
            out.append(f"<blockquote>{inner}</blockquote>")
            continue

        if stripped.startswith("|"):
            table, count = render_table(lines[i:])
            if count:
                out.append(table)
                i += count
                continue

        if LIST_RE.match(line):
            listing, count = render_list(lines[i:])
            out.append(listing)
            i += count
            continue

        paragraph = []
        while i < len(lines) and lines[i].strip():
            current = lines[i]
            if (
                current.strip().startswith("```")
                or re.match(r"^#{1,6}\s", current.strip())
                or current.strip().startswith(">")
                or current.strip().startswith("|")
                or LIST_RE.match(current)
                or current.strip() in ("---", "***", "___")
            ):
                break
            paragraph.append(current.strip())
            i += 1
        if paragraph:
            out.append(f"<p>{inline(' '.join(paragraph))}</p>")
        else:
            i += 1

    return "\n".join(out), toc


CSS = r"""
:root {
  --bg: #0f1419; --panel: #171e26; --fg: #dce4ec; --muted: #9aa8b6;
  --accent: #6cc0ff; --border: #2b3845; --code: #0b1015; --ok: #7ee2a8;
  --sidebar: 290px;
}
body.light {
  --bg: #f7f9fb; --panel: #ffffff; --fg: #1d2833; --muted: #5c6b7a;
  --accent: #0b62b3; --border: #dce4ec; --code: #eef2f6; --ok: #0a7a3d;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body { margin: 0; background: var(--bg); color: var(--fg);
  font: 16px/1.65 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
#progress { position: fixed; top: 0; left: 0; height: 3px; background: var(--accent); z-index: 50; width: 0; }
#sidebar { position: fixed; inset: 0 auto 0 0; width: var(--sidebar); background: var(--panel);
  border-right: 1px solid var(--border); padding: 18px 14px 40px; overflow-y: auto; z-index: 20; }
#sidebar h1 { font-size: 18px; margin: 0 0 4px; }
#sidebar .sub { font-size: 12px; color: var(--muted); margin-bottom: 14px; }
#search { width: 100%; padding: 8px 10px; border-radius: 6px; border: 1px solid var(--border);
  background: var(--bg); color: var(--fg); margin-bottom: 12px; }
#toc a { display: block; color: var(--muted); text-decoration: none; padding: 3px 6px;
  border-radius: 4px; font-size: 13px; }
#toc a:hover, #toc a.active { color: var(--accent); background: rgba(108,192,255,.09); }
#toc .group { margin-top: 14px; font-weight: 700; color: var(--fg); font-size: 13px; }
#toc .sub-item { padding-left: 14px; }
#toc a.hidden { display: none; }
main { margin-left: var(--sidebar); padding: 36px clamp(20px, 5vw, 72px) 100px; max-width: 1100px; }
section.doc { margin-bottom: 70px; }
h1 { font-size: 30px; line-height: 1.25; margin-top: 10px; }
h2 { font-size: 23px; margin-top: 38px; border-bottom: 1px solid var(--border); padding-bottom: 6px; }
h3 { font-size: 19px; margin-top: 28px; }
h4 { font-size: 16px; margin-top: 22px; }
a { color: var(--accent); }
p, li { max-width: 82ch; }
li { margin: 4px 0; }
code { background: var(--code); padding: 1px 5px; border-radius: 4px; font-size: .9em; }
.code-wrap { position: relative; margin: 16px 0; }
pre { background: var(--code); border: 1px solid var(--border); border-radius: 8px; padding: 16px;
  overflow-x: auto; font-size: 13.5px; line-height: 1.5; }
pre code { background: none; padding: 0; }
.copy { position: absolute; right: 8px; top: 8px; background: var(--panel); color: var(--muted);
  border: 1px solid var(--border); border-radius: 5px; font-size: 11px; padding: 3px 8px; cursor: pointer; }
.copy:hover { color: var(--accent); }
blockquote { border-left: 3px solid var(--accent); margin: 18px 0; padding: 8px 16px;
  background: rgba(108,192,255,.06); color: var(--muted); }
table { border-collapse: collapse; width: 100%; margin: 18px 0; font-size: 14px; display: block; overflow-x: auto; }
th, td { border: 1px solid var(--border); padding: 8px 10px; text-align: left; vertical-align: top; }
th { background: var(--panel); }
hr { border: 0; border-top: 1px solid var(--border); margin: 32px 0; }
.top-actions { position: fixed; right: 18px; bottom: 18px; display: flex; gap: 8px; z-index: 30; }
.top-actions button { background: var(--panel); color: var(--fg); border: 1px solid var(--border);
  border-radius: 20px; padding: 8px 14px; cursor: pointer; font-size: 13px; }
@media print {
  #sidebar, #progress, .top-actions, .copy { display: none !important; }
  main { margin: 0; padding: 0; max-width: none; }
  section.doc { page-break-before: always; }
  a { color: inherit; text-decoration: none; }
  pre, table { white-space: pre-wrap; word-break: break-word; }
  body { background: #fff; color: #000; font-size: 11pt; }
}
@media (max-width: 860px) {
  #sidebar { position: static; width: 100%; height: auto; border-right: 0; border-bottom: 1px solid var(--border); }
  main { margin-left: 0; }
}
"""


JS = r"""
const sidebar = document.getElementById('sidebar');
const search = document.getElementById('search');
const links = [...document.querySelectorAll('#toc a')];
const progress = document.getElementById('progress');

search.addEventListener('input', () => {
  const q = search.value.toLowerCase().trim();
  links.forEach(a => {
    a.classList.toggle('hidden', q !== '' && !a.textContent.toLowerCase().includes(q));
  });
});

document.querySelectorAll('.copy').forEach(btn => {
  btn.addEventListener('click', async () => {
    const text = btn.nextElementSibling.innerText;
    try {
      await navigator.clipboard.writeText(text);
      btn.textContent = 'copiado';
    } catch (e) {
      btn.textContent = 'falhou';
    }
    setTimeout(() => btn.textContent = 'copiar', 1200);
  });
});

function updateActive() {
  const scroll = window.scrollY;
  const height = document.documentElement.scrollHeight - window.innerHeight;
  progress.style.width = (height > 0 ? (scroll / height) * 100 : 0) + '%';
  let current = links[0];
  for (const a of links) {
    const el = document.getElementById(a.dataset.target);
    if (el && el.getBoundingClientRect().top < 160) current = a;
  }
  links.forEach(a => a.classList.remove('active'));
  if (current) current.classList.add('active');
}
window.addEventListener('scroll', updateActive, { passive: true });
updateActive();

document.getElementById('theme').addEventListener('click', () => {
  document.body.classList.toggle('light');
  localStorage.setItem('livro-tema', document.body.classList.contains('light') ? 'light' : 'dark');
});
if (localStorage.getItem('livro-tema') === 'light') document.body.classList.add('light');
"""


def main() -> None:
    toc_html: list[str] = []
    sections: list[str] = []
    for idx, (label, path) in enumerate(DOCS):
        md = path.read_text(encoding="utf-8")
        body, toc = render_markdown(md, f"d{idx}")
        sections.append(f'<section class="doc" id="doc-{idx}">{body}</section>')
        toc_html.append(f'<div class="group">{html.escape(label)}</div>')
        for level, title, ident in toc:
            cls = "sub-item" if level > 1 else ""
            toc_html.append(
                f'<a class="{cls}" href="#{ident}" data-target="{ident}">{html.escape(title)}</a>'
            )

    page = (
        "<!doctype html><html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Claude Code para Firmware STM32 + GitLab — Livro de estudos</title>"
        f"<style>{CSS}</style></head><body>"
        "<div id=\"progress\"></div>"
        "<aside id=\"sidebar\">"
        "<h1>Claude Code · STM32 · GitLab</h1>"
        "<div class=\"sub\">Livro de estudos — workshop para devs</div>"
        "<input id=\"search\" placeholder=\"Buscar no índice...\" autocomplete=\"off\">"
        f"<nav id=\"toc\">{''.join(toc_html)}</nav>"
        "</aside>"
        f"<main>{''.join(sections)}</main>"
        "<div class=\"top-actions\">"
        "<button id=\"theme\" type=\"button\">tema</button>"
        "<button onclick=\"window.print()\" type=\"button\">imprimir / PDF</button>"
        "</div>"
        f"<script>{JS}</script></body></html>"
    )
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(page, encoding="utf-8")
    print(f"gerado: {OUT.relative_to(ROOT)} ({len(page):,} bytes)")


if __name__ == "__main__":
    main()
