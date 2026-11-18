#!/usr/bin/env python3
"""Cliente Modbus TCP de linha de comando, para testar o simulador.

Uso:
    python3 mbtest.py HOST PORT UNIT FC ADDR [ARG...]

Exemplos:
    python3 mbtest.py 127.0.0.1 1502 0 3 1 16
        FC03, le 16 registradores em 1..16 (process data do UHX15A).

    python3 mbtest.py 127.0.0.1 1502 0 6 1 1
        FC06, escreve 1 no registrador 1 (control word do azimute).

    python3 mbtest.py 127.0.0.1 1502 0 23 3 2 3 0 45
        FC23: le 2 em 3 e escreve 2 em 3 (setpoint S32 = 45 no azimute).

    python3 mbtest.py 127.0.0.1 1502 0 16 10 1 2 3
        FC16, escreve 3 registradores (o perfil uhx15a rejeita).

    python3 mbtest.py 127.0.0.1 1502 0 20 1 0 4
        FC20, le 4 registradores do file 1, record 0.

    python3 mbtest.py 127.0.0.1 1502 0 43 2
        FC43/14, Read Device Identification categoria 2.

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

TXID = 1


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


def int0(text):
    """Aceita 123, 0x7B e 0b1111011."""
    return int(text, 0)


def mbap(unit, pdu):
    return struct.pack(">HHH", TXID, 0, len(pdu) + 1) + bytes([unit & 0xFF]) + pdu


def build(unit, fc, addr, extra):
    if fc in (1, 2, 3, 4):
        qty = extra[0] if extra else 1
        return mbap(unit, struct.pack(">BHH", fc, addr, qty))

    if fc == 5:
        if not extra:
            raise SystemExit("fc5 exige 0 ou 1")
        value = 0xFF00 if extra[0] else 0x0000
        return mbap(unit, struct.pack(">BHH", fc, addr, value))

    if fc == 6:
        if not extra:
            raise SystemExit("fc6 exige um valor")
        return mbap(unit, struct.pack(">BHH", fc, addr, extra[0] & 0xFFFF))

    if fc == 15:
        if not extra:
            raise SystemExit("fc15 exige ao menos um bit")
        qty = len(extra)
        data = bytearray((qty + 7) // 8)
        for i, v in enumerate(extra):
            if v:
                data[i // 8] |= 1 << (i % 8)
        pdu = struct.pack(">BHHB", fc, addr, qty, len(data)) + bytes(data)
        return mbap(unit, pdu)

    if fc == 16:
        if not extra:
            raise SystemExit("fc16 exige ao menos um valor")
        qty = len(extra)
        data = b"".join(struct.pack(">H", v & 0xFFFF) for v in extra)
        pdu = struct.pack(">BHHB", fc, addr, qty, len(data)) + data
        return mbap(unit, pdu)

    if fc == 22:
        if len(extra) < 2:
            raise SystemExit("fc22 exige and_mask e or_mask")
        return mbap(unit, struct.pack(">BHHH", fc, addr,
                                      extra[0] & 0xFFFF, extra[1] & 0xFFFF))

    if fc == 23:
        # addr = read addr, extra = [rqty, waddr, wvalue...]
        if len(extra) < 3:
            raise SystemExit("fc23 exige rqty, waddr e ao menos um valor")
        rqty = extra[0]
        waddr = extra[1]
        values = extra[2:]
        wqty = len(values)
        data = b"".join(struct.pack(">H", v & 0xFFFF) for v in values)
        pdu = struct.pack(">BHHHHB", fc, addr, rqty, waddr, wqty,
                          len(data)) + data
        return mbap(unit, pdu)

    if fc == 24:
        return mbap(unit, struct.pack(">BH", fc, addr))

    if fc == 20:
        if len(extra) < 2:
            raise SystemExit("fc20: addr=file, extra=[record, qty]")
        record, qty = extra[0], extra[1]
        pdu = struct.pack(">BBBHHH", fc, 7, 6, addr, record, qty)
        return mbap(unit, pdu)

    if fc == 21:
        if len(extra) < 2:
            raise SystemExit("fc21: addr=file, extra=[record, value...]")
        record = extra[0]
        values = extra[1:]
        qty = len(values)
        data = b"".join(struct.pack(">H", v & 0xFFFF) for v in values)
        pdu = struct.pack(">BBBHHH", fc, 7 + len(data), 6,
                          addr, record, qty) + data
        return mbap(unit, pdu)

    if fc == 43:
        category = addr
        obj_id = extra[0] if extra else 0
        return mbap(unit, struct.pack(">BBBB", 0x2B, 0x0E,
                                      category & 0xFF, obj_id & 0xFF))

    raise SystemExit("fc invalida")


def show(resp, bit_qty=None):
    txid, proto, length = struct.unpack(">HHH", resp[:6])
    unit = resp[6]
    pdu = resp[7:]

    print(f"MBAP txid={txid} proto={proto} len={length} unit={unit} "
          f"pdu={len(pdu)}B")

    if proto != 0 or length != len(pdu) + 1 or len(pdu) < 1:
        print("frame inconsistente")
        return 1

    fc = pdu[0]

    if fc & 0x80:
        code = pdu[1] if len(pdu) > 1 else -1
        print(f"EXCECAO {code}: {EXC.get(code, 'desconhecida')}")
        return 1

    if fc in (1, 2):
        bc = pdu[1]
        data = pdu[2:2 + bc]
        n = bit_qty if bit_qty is not None else len(data) * 8
        bits = [((data[i // 8] >> (i % 8)) & 1) for i in range(min(n, len(data) * 8))]
        print(f"bits ({bc} bytes):", " ".join(str(b) for b in bits))
        return 0

    if fc in (3, 4):
        bc = pdu[1]
        data = pdu[2:2 + bc]
        vals = [struct.unpack(">H", data[2 * i:2 * i + 2])[0]
                for i in range(bc // 2)]
        print("valores:", " ".join(f"0x{v:04X} ({v})" for v in vals))
        return 0

    if fc == 5:
        addr, value = struct.unpack(">HH", pdu[1:5])
        print(f"coil addr={addr} valor=0x{value:04X}")
        return 0

    if fc == 6:
        addr, value = struct.unpack(">HH", pdu[1:5])
        print(f"escrito addr={addr} valor=0x{value:04X} ({value})")
        return 0

    if fc == 15:
        addr, qty = struct.unpack(">HH", pdu[1:5])
        print(f"coils escritos addr={addr} qty={qty}")
        return 0

    if fc == 16:
        addr, qty = struct.unpack(">HH", pdu[1:5])
        print(f"escrito addr={addr} qty={qty}")
        return 0

    if fc == 22:
        addr, and_mask, or_mask = struct.unpack(">HHH", pdu[1:7])
        print(f"mask write addr={addr} and=0x{and_mask:04X} or=0x{or_mask:04X}")
        return 0

    if fc == 23:
        bc = pdu[1]
        data = pdu[2:2 + bc]
        vals = [struct.unpack(">H", data[2 * i:2 * i + 2])[0]
                for i in range(bc // 2)]
        print("FC23 leitura:", " ".join(f"0x{v:04X} ({v})" for v in vals))
        return 0

    if fc == 24:
        bc = pdu[1]
        n = struct.unpack(">H", pdu[2:4])[0]
        data = pdu[4:2 + bc]
        vals = [struct.unpack(">H", data[2 * i:2 * i + 2])[0]
                for i in range(n)]
        print(f"FIFO count={n}:", " ".join(f"0x{v:04X}" for v in vals))
        return 0

    if fc == 20:
        bc = pdu[1]
        sub_len = pdu[2]
        ref = pdu[3]
        data = pdu[4:3 + bc]
        vals = [struct.unpack(">H", data[2 * i:2 * i + 2])[0]
                for i in range(len(data) // 2)]
        print(f"file record bc={bc} sub_len={sub_len} ref={ref}:",
              " ".join(f"0x{v:04X}" for v in vals))
        return 0

    if fc == 21:
        bc = pdu[1]
        ref = pdu[2]
        file, record, qty = struct.unpack(">HHH", pdu[3:9])
        vals = [struct.unpack(">H", pdu[9 + 2 * i:11 + 2 * i])[0]
                for i in range(qty)]
        print(f"file record escrito bc={bc} ref={ref} file={file} "
              f"record={record} qty={qty}:",
              " ".join(f"0x{v:04X}" for v in vals))
        return 0

    if fc == 0x2B:
        if len(pdu) < 7 or pdu[1] != 0x0E:
            print("device id invalido")
            return 1
        category = pdu[2]
        conformity = pdu[3]
        more = pdu[4]
        next_id = pdu[5]
        n = pdu[6]
        print(f"device id categoria={category} conformity=0x{conformity:02X} "
              f"more={more} next={next_id} objetos={n}")
        off = 7
        for _ in range(n):
            if off + 2 > len(pdu):
                print("device id truncado")
                return 1
            oid = pdu[off]
            olen = pdu[off + 1]
            if off + 2 + olen > len(pdu):
                print("device id truncado")
                return 1
            value = pdu[off + 2:off + 2 + olen]
            off += 2 + olen
            try:
                text = value.decode("ascii")
            except UnicodeDecodeError:
                text = value.hex()
            print(f"  0x{oid:02X}: {text!r}")
        return 0

    print("fc desconhecido na resposta")
    return 1


def main():
    ap = argparse.ArgumentParser(description="Cliente Modbus TCP")
    ap.add_argument("host")
    ap.add_argument("port", type=int0)
    ap.add_argument("unit", type=int0)
    ap.add_argument("fc", type=int0,
                    choices=[1, 2, 3, 4, 5, 6, 15, 16, 20, 21, 22, 23, 24, 43])
    ap.add_argument("addr", type=int0)
    ap.add_argument("extra", nargs="*", type=int0)
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
    bit_qty = None
    if args.fc in (1, 2) and args.extra:
        bit_qty = args.extra[0]
    return show(resp, bit_qty)


if __name__ == "__main__":
    sys.exit(main())
