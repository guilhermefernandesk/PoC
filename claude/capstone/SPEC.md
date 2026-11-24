# SPEC — Firmware de exemplo (STM32F103 + GitLab)

## REQ-001 — Piscar LED de status

**História:** Como operador, quero ver o LED de status piscar para saber que o firmware está vivo.

### Critérios de aceite

- [ ] Dado o estado IDLE, quando 500 ms se passam, então o LED inverte de estado.
- [ ] Dado o estado ERROR, quando a task executa, então o LED fica aceso contínuo.
- [ ] A lógica de temporização não usa busy-wait.
- [ ] Existe teste unitário para a transição IDLE → ERROR.
- [ ] Existe teste HIL que mede os intervalos na UART.

### Rastreabilidade

| Item | Valor |
|---|---|
| Issue | #42 |
| Teste unitário | `test/test_blink.c` |
| Teste HIL | `tests/hil/test_gpio.py` |
| ADR | `docs/decisions/ADR-007.md` |
| Evidência | job `hil:gpio` → `reports/hil/results.xml` |

### Fora de escopo

PWM, MQTT, modos de baixo consumo, atualização por OTA.

---

## REQ-002 — Isolar a HAL

**História:** Como mantenedor, quero trocar a família de MCU sem reescrever as regras de negócio.

### Critérios de aceite

- [ ] `app/` não inclui nenhum header `stm32f1xx_hal.h`.
- [ ] A porta GPIO é injetada via `GpioPort`.
- [ ] O job `unit:ceedling` falha se `app/` incluir HAL.

### Rastreabilidade

| Item | Valor |
|---|---|
| Issue | #43 |
| ADR | `docs/decisions/ADR-007.md` |
| Evidência | job `static:shellcheck` (grep) + `unit:ceedling` |
