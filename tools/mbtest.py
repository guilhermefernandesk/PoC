#!/usr/bin/env python3
"""Cliente Modbus TCP de linha de comando, para testar o simulador.

Uso:
    python3 mbtest.py HOST PORT UNIT FC ADDR [N | VALOR...]

Exemplos:
    python3 mbtest.py 127.0.0.1 1502 1 3 0 4      # le 4 registradores
    python3 mbtest.py 127.0.0.1 1502 1 4 10 2     # FC04, 2 registradores
    python3 mbtest.py 127.0.0.1 1502 1 6 5 1234   # escreve 1234 no addr 5
    python3 mbtest.py 127.0.0.1 1502 1 16 10 1 2 3  # escreve 3 registradores

Imprime TX/RX em hex e decodifica a resposta. Sem dependencias externas.
"""
import argparse
import socket
import struct
import sys

EXC = {
    1: "illegal function",
    2: "illegal address",
    3: "illegal value",
    4: "slave failure",
    5: "acknowledge",
    6: "slave busy",
    8: "memory parity",
    10: "gateway path unavailable",
    11: "gateway target failed",
}


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def transact(host, port, frame, timeout):
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(frame)

        hdr = recv_exact(sock, 6)
        if hdr is None:
            raise RuntimeError("sem resposta (header)")

        _, _, length = struct.unpack(">HHH", hdr)
        body = recv_exact(sock, length)
        if body is None:
            raise RuntimeError("sem resposta (body)")

        return hdr + body


def build(unit, fc, addr, extra):
    txid = 1

    if fc in (3, 4):
        qty = extra[0] if extra else 1
        return struct.pack(">HHHBBHH", txid, 0, 6, unit, fc, addr, qty)

    if fc == 6:
        if not extra:
            raise SystemExit("fc6 exige um valor")
        return struct.pack(">HHHBBHH", txid, 0, 6, unit, fc, addr, extra[0])

    if fc == 16:
        if not extra:
            raise SystemExit("fc16 exige ao menos um valor")
        qty = len(extra)
        data = b"".join(struct.pack(">H", v & 0xFFFF) for v in extra)
        return (struct.pack(">HHHBBHHB", txid, 0, 7 + len(data),
                            unit, fc, addr, qty, len(data)) + data)

    raise SystemExit("fc deve ser 3, 4, 6 ou 16")


def show(resp):
    txid, proto, length = struct.unpack(">HHH", resp[:6])
    unit = resp[6]
    fc = resp[7]

    print(f"MBAP txid={txid} proto={proto} len={length} unit={unit} fc={fc}")

    if fc & 0x80:
        code = resp[8]
        print(f"EXCECAO {code}: {EXC.get(code, 'desconhecida')}")
        return 1

    if fc in (3, 4):
        bc = resp[8]
        if bc != length - 3:
            print(f"bytecount inconsistente: {bc} vs {length - 3}")
            return 1
        vals = [struct.unpack(">H", resp[9 + 2 * i:11 + 2 * i])[0]
                for i in range(bc // 2)]
        print("valores:", " ".join(f"0x{v:04X} ({v})" for v in vals))
        return 0

    if fc == 6:
        addr, value = struct.unpack(">HH", resp[8:12])
        print(f"escrito addr={addr} valor=0x{value:04X} ({value})")
        return 0

    if fc == 16:
        addr, qty = struct.unpack(">HH", resp[8:12])
        print(f"escrito addr={addr} qty={qty}")
        return 0

    print("fc desconhecido na resposta")
    return 1


def main():
    ap = argparse.ArgumentParser(description="Cliente Modbus TCP")
    ap.add_argument("host")
    ap.add_argument("port", type=int)
    ap.add_argument("unit", type=int)
    ap.add_argument("fc", type=int, choices=[3, 4, 6, 16])
    ap.add_argument("addr", type=int)
    ap.add_argument("extra", nargs="*", type=int)
    ap.add_argument("--timeout", type=float, default=2.0)
    args = ap.parse_args()

    frame = build(args.unit, args.fc, args.addr, args.extra)

    print("TX", " ".join(f"{b:02X}" for b in frame))

    try:
        resp = transact(args.host, args.port, frame, args.timeout)
    except (OSError, RuntimeError) as exc:
        print(f"ERRO: {exc}")
        return 2

    print("RX", " ".join(f"{b:02X}" for b in resp))
    return show(resp)


if __name__ == "__main__":
    sys.exit(main())
