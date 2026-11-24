---
name: user-stories
description: Transforma um requisito de firmware em história INVEST com critérios Given-When-Then e campos de rastreabilidade. Use quando o usuário pedir spec, requisito, história, critério de aceite ou REQ-NNN.
---

# User stories para firmware

1. Leia `SPEC.md` e identifique o próximo `REQ-NNN` livre.
2. Escreva a história no formato: "Como <papel>, quero <ação> para <valor>".
3. Verifique INVEST: independente, negociável, valiosa, estimável, pequena, testável.
4. Escreva critérios **Given-When-Then**. Todo critério precisa de um efeito observável.
5. Preencha a tabela de rastreabilidade: Issue, teste unitário, teste HIL, ADR, evidência de CI.

## Regra de parada

Se algum campo não puder ser preenchido com um caminho/comando real, o requisito não está pronto. Diga isso explicitamente e proponha a pergunta que falta responder.

## Saída

Markdown pronto para colar em SPEC.md, sem explicações extras.
