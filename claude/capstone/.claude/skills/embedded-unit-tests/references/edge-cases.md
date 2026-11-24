# Casos de borda que o modelo esquece

Consulte este arquivo antes de declarar um teste completo.

## Timer / contador

- Rollover de timer 32-bit @1 kHz: 49,7 dias (0xFFFFFFFF → 0).
- Comparação de valores absolutos após reset: prefira deltas.
- Prescaler mudando em runtime.
- `HAL_GetTick()` com overflow durante delay longo.

## Buffer circular

- `head == tail` para cheio **e** vazio (use um slot a mais ou contador).
- Escrita durante leitura (ISR + task).
- Tamanho 0 e tamanho 1 (potência de 2 vs não potência).
- `memcpy` atravessando o fim do buffer.

## ISR e concorrência

- Variável compartilhada sem `volatile`.
- Seção crítica aninhada.
- Callback chamado no contexto de interrupção fazendo alocação.
- Prioridade de IRQ invertida por configuração do NVIC.

## Periféricos

- ADC: 0, `UINT32_MAX`, canal desconectado, reference drift.
- I2C: NACK, clock stretching, bus busy, timeout.
- SPI: CPOL/CPHA errado, CS não liberado, DMA underrun.
- UART: framing error, overflow, entrada parcial de linha.

## Reset e energia

- Watchdog estourado: causa foi lógica ou supply?
- Brownout durante escrita em flash.
- Estado inicial inválido após reset a quente.
- Bootloader deixando periférico configurado.
