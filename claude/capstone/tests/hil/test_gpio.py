"""HIL: mede o intervalo do LED de status pela UART.

Requer runner com tag stm32-bench e firmware gravado pelo job flash:bench.
"""
import subprocess
import time

import pytest

UART = "/dev/ttyUSB0"
BIN = "build/firmware.bin"
ADDR = "0x8000000"


@pytest.fixture(scope="session", autouse=True)
def flash_and_boot():
    subprocess.run(["st-flash", "--reset", "write", BIN, ADDR], check=True)
    time.sleep(0.5)
    yield
    # after: desligar cargas e voltar ao estado seguro da bancada


def test_led_blinks_at_500ms():
    import serial  # pyserial

    edges = []
    with serial.Serial(UART, 115200, timeout=3) as ser:
        start = time.monotonic()
        while time.monotonic() - start < 3 and len(edges) < 4:
            line = ser.readline().decode(errors="replace").strip()
            if line.startswith("LED:"):
                edges.append((line, time.monotonic()))

    assert len(edges) >= 4, f"arestas insuficientes: {edges}"
    deltas = [t1 - t0 for (_, t0), (_, t1) in zip(edges, edges[1:])]
    assert all(0.45 <= d <= 0.55 for d in deltas), deltas
