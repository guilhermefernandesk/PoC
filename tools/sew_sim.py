#!/usr/bin/env python3
"""Simulador Modbus TCP (servidor) para validar o mestre STM32.

Uso:
    python3 sew_sim.py
    python3 sew_sim.py --port 502 --unit 1
    python3 sew_sim.py --host 0.0.0.0 --port 1502 --unit 1

Porta 1502 por padrao para nao precisar de root. A 502 fica reservada
para o SEW real.

Registradores (FC03/FC04):
    0..3 = 0x1111 0x2222 0x3333 0x4444
    resto = indice (0,1,2,...)

Suporta FC03, FC04, FC06 e FC16. Sem dependencias externas.
"""
import argparse
import socket
import struct
import threading

REGS = [0] * 2048


def init_regs():
    for i in range(len(REGS)):
        REGS[i] = i & 0xFFFF
    REGS[0] = 0x1111
    REGS[1] = 0x2222
    REGS[2] = 0x3333
    REGS[3] = 0x4444


def hexdump(b):
    return " ".join(f"{x:02X}" for x in b)


def exception(txid, unit, fc, code):
    return struct.pack(">HHHBBB", txid, 0, 3, unit, (fc | 0x80) & 0xFF, code)


def process(frame, expected_unit):
    """Recebe frame completo (MBAP + unit + PDU) e devolve a resposta."""
    if len(frame) < 8:
        return None

    txid, proto, length = struct.unpack(">HHH", frame[:6])
    unit = frame[6]

    if proto != 0 or length < 2 or length > 254:
        return None
    if unit != expected_unit:
        return None

    pdu = frame[7:7 + (length - 1)]
    if len(pdu) < 1:
        return None

    fc = pdu[0]

    # ---- leitura (FC03 / FC04) ----
    if fc in (3, 4):
        if len(pdu) != 5:
            return exception(txid, unit, fc, 3)
        addr, qty = struct.unpack(">HH", pdu[1:5])
        if qty < 1 or qty > 125:
            return exception(txid, unit, fc, 3)
        if addr + qty > len(REGS):
            return exception(txid, unit, fc, 2)

        data = b"".join(struct.pack(">H", REGS[addr + i]) for i in range(qty))
        return (struct.pack(">HHHBB", txid, 0, 3 + len(data), unit, fc)
                + bytes([len(data)]) + data)

    # ---- escrita simples (FC06) ----
    if fc == 6:
        if len(pdu) != 5:
            return exception(txid, unit, fc, 3)
        addr, value = struct.unpack(">HH", pdu[1:5])
        if addr >= len(REGS):
            return exception(txid, unit, fc, 2)
        REGS[addr] = value
        return struct.pack(">HHHBBHH", txid, 0, 6, unit, fc, addr, value)

    # ---- escrita multipla (FC16) ----
    if fc == 16:
        if len(pdu) < 6:
            return exception(txid, unit, fc, 3)
        addr, qty, bc = struct.unpack(">HHB", pdu[1:6])
        if qty < 1 or qty > 123 or bc != 2 * qty or len(pdu) != 6 + bc:
            return exception(txid, unit, fc, 3)
        if addr + qty > len(REGS):
            return exception(txid, unit, fc, 2)
        for i in range(qty):
            REGS[addr + i] = struct.unpack(">H", pdu[6 + 2 * i:8 + 2 * i])[0]
        return struct.pack(">HHHBBHH", txid, 0, 6, unit, fc, addr, qty)

    # ---- funcao nao suportada ----
    return exception(txid, unit, fc, 1)


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def client_thread(conn, addr, expected_unit):
    peer = f"{addr[0]}:{addr[1]}"
    conn.settimeout(30.0)

    try:
        while True:
            hdr = recv_exact(conn, 6)
            if hdr is None:
                break

            txid, proto, length = struct.unpack(">HHH", hdr)
            if proto != 0 or length < 2 or length > 254:
                print(f"[{peer}] MBAP invalido (len={length}), fechando")
                break

            body = recv_exact(conn, length)
            if body is None:
                break

            frame = hdr + body
            print(f"[{peer}] RX {hexdump(frame)}")

            resp = process(frame, expected_unit)
            if resp is None:
                print(f"[{peer}] unit != {expected_unit} (ou frame ruim); sem resposta")
                continue

            print(f"[{peer}] TX {hexdump(resp)}")
            conn.sendall(resp)

    except socket.timeout:
        print(f"[{peer}] timeout, fechando")
    except (ConnectionResetError, BrokenPipeError, OSError) as exc:
        print(f"[{peer}] {exc}")
    finally:
        conn.close()


def main():
    ap = argparse.ArgumentParser(description="Simulador Modbus TCP")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=1502)
    ap.add_argument("--unit", type=int, default=1)
    args = ap.parse_args()

    init_regs()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(8)

    print(f"sew_sim ouvindo em {args.host}:{args.port} unit={args.unit}")
    print("regs[0..3] = 1111 2222 3333 4444")
    print("Ctrl+C para sair\n")

    try:
        while True:
            conn, addr = srv.accept()
            threading.Thread(target=client_thread,
                             args=(conn, addr, args.unit),
                             daemon=True).start()
    except KeyboardInterrupt:
        print("\nsaindo")
    finally:
        srv.close()


if __name__ == "__main__":
    main()
