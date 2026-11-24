---
name: embedded-unit-tests
description: Escreve testes unitários em C para código STM32 com Unity/CMock/Ceedling. Use quando o usuário pedir teste, TDD, cobertura, mock de HAL ou caso de borda de firmware.
---

# Embedded unit tests

1. Descubra o alvo: arquivo `.c`, função e efeito observável.
2. Escreva **primeiro** o teste em `test/test_<modulo>.c`.
3. Use `mock_*.h` gerado pelo CMock. Nunca inclua `stm32f1xx_hal.h` em `test/`.
4. Cubra os casos de `references/edge-cases.md` relevantes ao periférico.
5. Rode `ceedling test:test_<modulo>` e mostre a falha antes de implementar.
6. Implemente o mínimo em `app/`, sem `malloc`/`printf`.
7. Rode `ceedling test:all` até ficar verde. Nunca enfraqueça um teste para passar.
8. Reporte: arquivo do teste, comando rodado, resultado, REQ coberto.

## Regras

- Teste unitário roda no host, sem placa.
- Se o teste exigir hardware real, ele não é unitário: vá para `tests/hil/`.
- Se o teste não puder ser escrito a partir do critério de aceite, o requisito está ambíguo: pare e pergunte.
