# Commands
- Build:       cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
- Unit tests:  ceedling test:all
- One test:    ceedling test:test_blink
- Coverage:    ceedling gcov:all
- Flash:       st-flash --reset write build/firmware.bin 0x8000000
- HIL:         pytest -q tests/hil --junitxml=reports/hil/results.xml
- MR status:   glab ci status

# Conventions
- Regras de negócio ficam em app/. Nunca em Drivers/.
- Drivers/ e Middlewares/ são gerados pelo CubeMX: somente leitura.
- Em test/, nunca inclua stm32f1xx_hal.h. Use mock_*.h do CMock.
- Sem malloc/printf em código de produção; use osal_log().
- Commit: tipo(REQ-NNN): descrição #issue
- Tag de release: vMAJOR.MINOR.PATCH+kREV

# Gotchas
- Rev C do ADC exige 1us extra após CS (ERRATA-042), ver docs/decisions/ADR-007.md.
- Timer de 32-bit @1kHz vira em 49,7 dias: compare deltas, nunca valores absolutos.
- O job de I2C/HIL não pode rodar em paralelo na mesma bancada (flock).
- O LED de status é ativo-alto na revisão k7 da placa.

# Verification
- Antes de dizer "pronto": ceedling test:all && ./scripts/shellcheck.sh
- Para mudanças em requisitos: rode ./scripts/check-traceability.sh
- Para HIL: use flock /tmp/stm32-bench.lock e anexe reports/hil/results.xml
