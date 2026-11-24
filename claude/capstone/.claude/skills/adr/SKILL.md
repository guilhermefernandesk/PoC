---
name: adr
description: Escreve Architecture Decision Records com seção obrigatória Compliance que aponta para a verificação automatizada da decisão. Use quando o usuário pedir ADR, decisão de arquitetura ou justificativa de design.
---

# ADR

Salve em `docs/decisions/ADR-NNN-titulo.md`.

```markdown
# ADR-NNN — <título>

- Status: proposto | aceito | rejeitado
- REQ: REQ-NNN

## Contexto
<qual problema concreto, com restrição de hardware/ferramenta>

## Decisão
<o que foi escolhido, em uma frase>

## Alternativas
1. <alternativa> — <por que foi rejeitada>

## Consequências
+ <benefício>
- <custo>

## Compliance
- Verificação: <comando ou check que prova a decisão>
- Onde roda: <hook local / job do GitLab CI>
- Evidência: <artefato gerado>
```

## Regra

Sem a seção `Compliance`, o ADR é opinião. Não entregue um ADR sem pelo menos um comando executável que falhe quando a decisão for violada.
