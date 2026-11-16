#!/usr/bin/env python3
"""Simulador Modbus TCP para validar o mestre STM32.

Perfis:
  uhx65a  (default)
      unit 0, offset 0, process data em 1..pd-count (default 120).
      Aceita somente FC03, FC06 e FC23. FC16/FC05/FC15/etc. respondem
      excecao 01 (illegal function), como o UHX65A.
      Layout de exemplo (2 eixos, 8 PD cada):
        eixo 1: 1=control, 2=status, 3..4=setpoint S32, 5..6=actual S32
        eixo 2: 9=control, 10=status, 11..12=setpoint, 13..14=actual
      Se o control word tiver o bit 0 ligado, o actual segue o setpoint.
      Apos --fieldbus-timeout ms sem escrita, o bit 15 do status liga.

  generic
      Aceita FC01/02/03/04/05/06/0F/10/14/15/16/17/18/2B.
      Util para testar o cliente e o mapa fora do perfil SEW.

Uso:
    python3 tools/sew_sim.py
    python3 tools/sew_sim.py --port 1502 --unit 0
    python3 tools/sew_sim.py --profile generic --unit 1
    python3 tools/sew_sim.py --fieldbus-timeout 500 --latency-ms 2

Sem dependencias externas.
"""
import argparse
import random
import socket
import struct
import threading
import time

REGS = [0] * 2048
IREGS = [0] * 2048
COILS = [0] * 2048
DISC = [0] * 2048
FILE_RECORDS = {}
FIFO_ADDR = 0x3000
FIFO_VALUES = [0x1111, 0x2222, 0x3333]
LAST_WRITE = time.time()
LOCK = threading.RLock()
ARGS = None


def hexdump(b):
    return " ".join(f"{x:02X}" for x in b)


def mbap(txid, unit, pdu):
    return struct.pack(">HHH", txid, 0, len(pdu) + 1) + bytes([unit & 0xFF]) + pdu


def exception(txid, unit, fc, code):
    return mbap(txid, unit, bytes([(fc | 0x80) & 0xFF, code & 0xFF]))


def init_state():
    global LAST_WRITE
    for i in range(len(REGS)):
        REGS[i] = i & 0xFFFF
        IREGS[i] = (i * 3) & 0xFFFF
        COILS[i] = i & 1
        DISC[i] = (i >> 1) & 1

    if ARGS.profile == "uhx65a":
        for start in ARGS.axis_starts:
            for off in range(ARGS.axis_len):
                REGS[start + off] = 0

    LAST_WRITE = time.time()


def in_reg_range(addr, qty):
    if ARGS.profile == "uhx65a":
        start = ARGS.pd_start
        end = ARGS.pd_start + ARGS.pd_count
        return addr >= start and (addr + qty) <= end
    return addr >= 0 and (addr + qty) <= len(REGS)


def in_bit_range(addr, qty):
    return addr >= 0 and (addr + qty) <= len(COILS)


def update_process_image():
    """Simula o comportamento dos dois eixos no process data."""
    if ARGS.profile != "uhx65a":
        return

    timed_out = False
    if ARGS.fieldbus_timeout > 0:
        elapsed_ms = (time.time() - LAST_WRITE) * 1000.0
        timed_out = elapsed_ms > ARGS.fieldbus_timeout

    for start in ARGS.axis_starts:
        if (start + ARGS.axis_len) > len(REGS):
            continue

        control = REGS[start]
        status = REGS[start + 1]
        setpoint = (REGS[start + 2] << 16) | REGS[start + 3]
        if setpoint & 0x80000000:
            setpoint -= 0x100000000

        if control & 0x0001:
            status |= 0x0001
            actual = setpoint
        else:
            status &= ~0x0001
            actual = 0

        if timed_out:
            status |= 0x8000
        else:
            status &= ~0x8000

        REGS[start + 1] = status & 0xFFFF
        actual &= 0xFFFFFFFF
        REGS[start + 4] = (actual >> 16) & 0xFFFF
        REGS[start + 5] = actual & 0xFFFF


def read_bits(table, addr, qty):
    data = bytearray((qty + 7) // 8)
    for i in range(qty):
        if table[addr + i]:
            data[i // 8] |= 1 << (i % 8)
    return bytes(data)


def write_bits(table, addr, qty, data):
    for i in range(qty):
        table[addr + i] = (data[i // 8] >> (i % 8)) & 1


def device_id_objects():
    return {
        0x00: b"SEW-EURODRIVE",
        0x01: b"UHX65A-SIM",
        0x02: b"1.0",
        0x03: b"https://www.sew-eurodrive.com",
        0x04: b"UHX65A Simulator",
        0x05: b"UHX65A",
        0x06: b"sew_sim",
    }


def device_id_response(txid, unit, category, obj_id):
    objs = device_id_objects()

    if category == 4:
        if obj_id not in objs:
            return exception(txid, unit, 0x2B, 2)
        selected = [(obj_id, objs[obj_id])]
        conformity = 0x83
    else:
        max_id = {1: 2, 2: 5, 3: 6}.get(category, 2)
        selected = [(i, objs[i]) for i in range(max_id + 1)]
        conformity = {1: 1, 2: 2, 3: 3}.get(category, 1)

    data = bytearray([
        0x2B, 0x0E, category, conformity,
        0x00, 0x00, len(selected),
    ])
    for oid, value in selected:
        data += bytes([oid, len(value)]) + value
    return mbap(txid, unit, bytes(data))


def process(frame):
    """Recebe frame completo (MBAP + unit + PDU) e devolve a resposta."""
    global LAST_WRITE

    if len(frame) < 8:
        return None

    txid, proto, length = struct.unpack(">HHH", frame[:6])
    unit = frame[6]

    if proto != 0 or length < 2 or length > 254:
        return None
    if len(frame) != 6 + length:
        return None

    if unit != ARGS.unit:
        return None

    pdu = frame[7:6 + length]
    if len(pdu) < 1:
        return None
    fc = pdu[0]

    if ARGS.drop_rate > 0.0 and random.random() < ARGS.drop_rate:
        return None

    if fc == ARGS.fault_fc:
        return exception(txid, unit, fc, 4)

    if ARGS.latency_ms > 0:
        time.sleep(ARGS.latency_ms / 1000.0)

    if ARGS.profile == "uhx65a" and fc not in (3, 6, 23):
        return exception(txid, unit, fc, 1)

    with LOCK:
        # Atualiza status/actual antes de qualquer leitura de process data.
        update_process_image()

        # ---- FC01/FC02: coils / discrete inputs ----
        if fc in (1, 2):
            if len(pdu) != 5:
                return exception(txid, unit, fc, 3)
            addr, qty = struct.unpack(">HH", pdu[1:5])
            if qty < 1 or qty > 2000:
                return exception(txid, unit, fc, 3)
            if not in_bit_range(addr, qty):
                return exception(txid, unit, fc, 2)
            table = COILS if fc == 1 else DISC
            data = read_bits(table, addr, qty)
            return mbap(txid, unit, bytes([fc, len(data)]) + data)

        # ---- FC03/FC04: holding / input registers ----
        if fc in (3, 4):
            if len(pdu) != 5:
                return exception(txid, unit, fc, 3)
            addr, qty = struct.unpack(">HH", pdu[1:5])
            if qty < 1 or qty > 125:
                return exception(txid, unit, fc, 3)
            if fc == 3:
                if not in_reg_range(addr, qty):
                    return exception(txid, unit, fc, 2)
                table = REGS
            else:
                if not in_reg_range(addr, qty):
                    return exception(txid, unit, fc, 2)
                table = IREGS
            data = b"".join(struct.pack(">H", table[addr + i])
                            for i in range(qty))
            return mbap(txid, unit, bytes([fc, len(data)]) + data)

        # ---- FC05: write single coil ----
        if fc == 5:
            if len(pdu) != 5:
                return exception(txid, unit, fc, 3)
            addr, value = struct.unpack(">HH", pdu[1:5])
            if value not in (0x0000, 0xFF00):
                return exception(txid, unit, fc, 3)
            if not in_bit_range(addr, 1):
                return exception(txid, unit, fc, 2)
            COILS[addr] = 1 if value == 0xFF00 else 0
            return mbap(txid, unit, pdu[0:5])

        # ---- FC06: write single register ----
        if fc == 6:
            if len(pdu) != 5:
                return exception(txid, unit, fc, 3)
            addr, value = struct.unpack(">HH", pdu[1:5])
            if not in_reg_range(addr, 1):
                return exception(txid, unit, fc, 2)
            REGS[addr] = value
            LAST_WRITE = time.time()
            update_process_image()
            return mbap(txid, unit, pdu[0:5])

        # ---- FC15: write multiple coils ----
        if fc == 15:
            if len(pdu) < 6:
                return exception(txid, unit, fc, 3)
            addr, qty, bc = struct.unpack(">HHB", pdu[1:6])
            if qty < 1 or qty > 1968 or bc != (qty + 7) // 8:
                return exception(txid, unit, fc, 3)
            if len(pdu) != 6 + bc:
                return exception(txid, unit, fc, 3)
            if not in_bit_range(addr, qty):
                return exception(txid, unit, fc, 2)
            write_bits(COILS, addr, qty, pdu[6:])
            return mbap(txid, unit, struct.pack(">BHH", fc, addr, qty))

        # ---- FC16: write multiple registers ----
        if fc == 16:
            if len(pdu) < 6:
                return exception(txid, unit, fc, 3)
            addr, qty, bc = struct.unpack(">HHB", pdu[1:6])
            if qty < 1 or qty > 123 or bc != 2 * qty:
                return exception(txid, unit, fc, 3)
            if len(pdu) != 6 + bc:
                return exception(txid, unit, fc, 3)
            if not in_reg_range(addr, qty):
                return exception(txid, unit, fc, 2)
            for i in range(qty):
                REGS[addr + i] = struct.unpack(">H", pdu[6 + 2 * i:8 + 2 * i])[0]
            LAST_WRITE = time.time()
            return mbap(txid, unit, struct.pack(">BHH", fc, addr, qty))

        # ---- FC22 (0x16): mask write register ----
        if fc == 22:
            if len(pdu) != 7:
                return exception(txid, unit, fc, 3)
            addr, and_mask, or_mask = struct.unpack(">HHH", pdu[1:7])
            if not in_reg_range(addr, 1):
                return exception(txid, unit, fc, 2)
            cur = REGS[addr]
            REGS[addr] = ((cur & and_mask) | (or_mask & (~and_mask))) & 0xFFFF
            LAST_WRITE = time.time()
            return mbap(txid, unit, pdu[0:7])

        # ---- FC23 (0x17): read/write multiple ----
        if fc == 23:
            if len(pdu) < 10:
                return exception(txid, unit, fc, 3)
            raddr, rqty, waddr, wqty, wbc = struct.unpack(">HHHHB", pdu[1:10])
            if rqty < 1 or rqty > 125 or wqty < 1 or wqty > 121:
                return exception(txid, unit, fc, 3)
            if wbc != 2 * wqty or len(pdu) != 10 + wbc:
                return exception(txid, unit, fc, 3)
            if not in_reg_range(raddr, rqty) or not in_reg_range(waddr, wqty):
                return exception(txid, unit, fc, 2)

            for i in range(wqty):
                REGS[waddr + i] = struct.unpack(
                    ">H", pdu[10 + 2 * i:12 + 2 * i])[0]
            LAST_WRITE = time.time()
            update_process_image()

            data = b"".join(struct.pack(">H", REGS[raddr + i])
                            for i in range(rqty))
            return mbap(txid, unit, bytes([fc, len(data)]) + data)

        # ---- FC24 (0x18): read FIFO ----
        if fc == 24:
            if len(pdu) != 3:
                return exception(txid, unit, fc, 3)
            addr = struct.unpack(">H", pdu[1:3])[0]
            if addr != FIFO_ADDR:
                return exception(txid, unit, fc, 2)
            n = len(FIFO_VALUES)
            data = struct.pack(">H", n) + b"".join(
                struct.pack(">H", v) for v in FIFO_VALUES)
            return mbap(txid, unit, bytes([fc, len(data)]) + data)

        # ---- FC20 (0x14): read file record ----
        if fc == 20:
            if len(pdu) != 9:
                return exception(txid, unit, fc, 3)
            bc = pdu[1]
            ref = pdu[2]
            file, record, qty = struct.unpack(">HHH", pdu[3:9])
            if bc != 7 or ref != 6 or qty < 1 or qty > 124:
                return exception(txid, unit, fc, 3)
            values = FILE_RECORDS.get((file, record), [])
            values = list(values) + [0] * (qty - len(values))
            data = b"".join(struct.pack(">H", v) for v in values[:qty])
            sub = bytes([1 + len(data), 6]) + data
            return mbap(txid, unit, bytes([fc, len(sub)]) + sub)

        # ---- FC21 (0x15): write file record ----
        if fc == 21:
            if len(pdu) < 9:
                return exception(txid, unit, fc, 3)
            bc = pdu[1]
            ref = pdu[2]
            file, record, qty = struct.unpack(">HHH", pdu[3:9])
            if bc != 7 + 2 * qty or len(pdu) != 2 + bc:
                return exception(txid, unit, fc, 3)
            if ref != 6 or qty < 1 or qty > 122:
                return exception(txid, unit, fc, 3)
            values = [struct.unpack(">H", pdu[9 + 2 * i:11 + 2 * i])[0]
                      for i in range(qty)]
            FILE_RECORDS[(file, record)] = values
            return mbap(txid, unit, pdu)

        # ---- FC43/14 (0x2B/0x0E): device identification ----
        if fc == 0x2B:
            if len(pdu) != 4 or pdu[1] != 0x0E:
                return exception(txid, unit, fc, 3)
            category = pdu[2]
            obj_id = pdu[3]
            if category < 1 or category > 4:
                return exception(txid, unit, fc, 3)
            return device_id_response(txid, unit, category, obj_id)

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


def client_thread(conn, addr):
    peer = f"{addr[0]}:{addr[1]}"
    conn.settimeout(120.0)

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

            resp = process(frame)
            if resp is None:
                print(f"[{peer}] sem resposta (drop/unit)")
                continue

            print(f"[{peer}] TX {hexdump(resp)}")
            conn.sendall(resp)

    except socket.timeout:
        print(f"[{peer}] timeout, fechando")
    except (ConnectionResetError, BrokenPipeError, OSError) as exc:
        print(f"[{peer}] {exc}")
    finally:
        conn.close()


def parse_axis_starts(text):
    out = []
    for part in text.split(","):
        part = part.strip()
        if part:
            out.append(int(part, 0))
    return out


def main():
    global ARGS

    ap = argparse.ArgumentParser(description="Simulador Modbus TCP")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=1502)
    ap.add_argument("--unit", type=int, default=0)
    ap.add_argument("--profile", choices=["uhx65a", "generic"],
                    default="uhx65a")
    ap.add_argument("--pd-start", type=int, default=1)
    ap.add_argument("--pd-count", type=int, default=120)
    ap.add_argument("--axis-start", default="1,9",
                    help="inicios dos eixos, separados por virgula")
    ap.add_argument("--axis-len", type=int, default=8)
    ap.add_argument("--fieldbus-timeout", type=int, default=1000,
                    help="ms sem escrita para ligar o bit 15 do status; 0 desliga")
    ap.add_argument("--latency-ms", type=float, default=0.0)
    ap.add_argument("--drop-rate", type=float, default=0.0)
    ap.add_argument("--fault-fc", type=int, default=-1)
    args = ap.parse_args()

    args.axis_starts = parse_axis_starts(args.axis_start)
    if args.pd_start < 0 or args.pd_count < 1 or args.pd_start + args.pd_count > len(REGS):
        ap.error("area de process data fora de 0..2047")
    if args.axis_len < 6:
        ap.error("--axis-len precisa ser >= 6 (control/status/setpoint/actual)")
    if args.profile == "uhx65a":
        for start in args.axis_starts:
            if start < args.pd_start or (start + args.axis_len) > (args.pd_start + args.pd_count):
                ap.error("eixo fora da area de process data")

    ARGS = args
    init_state()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(8)

    print(f"sew_sim perfil={args.profile} ouvindo em {args.host}:{args.port} "
          f"unit={args.unit}")
    print(f"PD={args.pd_start}..{args.pd_start + args.pd_count - 1} "
          f"eixos={args.axis_starts} len={args.axis_len} "
          f"fieldbus_timeout={args.fieldbus_timeout}ms")
    print("Ctrl+C para sair\n")

    try:
        while True:
            conn, addr = srv.accept()
            threading.Thread(target=client_thread,
                             args=(conn, addr),
                             daemon=True).start()
    except KeyboardInterrupt:
        print("\nsaindo")
    finally:
        srv.close()


if __name__ == "__main__":
    main()
