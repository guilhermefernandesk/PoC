#!/usr/bin/env python3
"""Varre unit IDs (e opcionalmente enderecos) de um escravo Modbus TCP.

Uso:
    python3 scan.py HOST PORT
    python3 scan.py HOST PORT --units 1-10,20,255
    python3 scan.py HOST PORT --units 1 --range --end 32

Cada unit e' testado com uma leitura FC03 de 1 registrador (addr 0).
Depois, com --range, varre enderecos para os units que responderam.

Cuidado: nao rode isso contra o SEW real sem saber o que esta fazendo.
Contra o simulador e' seguro.
"""
import argparse
import socket
import struct
import sys
import time


def parse_units(text):
    out = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            out.extend(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def probe(host, port, unit, fc, addr, qty, timeout):
    frame = struct.pack(">HHHBBHH", 1, 0, 6, unit, fc, addr, qty)
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(frame)

            hdr = recv_exact(sock, 6)
            if hdr is None:
                return "conn", "sem header"

            _, _, length = struct.unpack(">HHH", hdr)
            body = recv_exact(sock, length)
            if body is None:
                return "conn", "sem body"

            resp = hdr + body
            if resp[7] & 0x80:
                return "exc", resp[8]
            return "ok", resp
    except socket.timeout:
        return "timeout", None
    except OSError as exc:
        return "conn", str(exc)


def main():
    ap = argparse.ArgumentParser(description="Scanner Modbus TCP")
    ap.add_argument("host")
    ap.add_argument("port", type=int)
    ap.add_argument("--units", default="1",
                    help="lista/faixa, ex: 1-10,20,255 (default: 1)")
    ap.add_argument("--fc", type=int, default=3, choices=[3, 4])
    ap.add_argument("--addr", type=int, default=0)
    ap.add_argument("--qty", type=int, default=1)
    ap.add_argument("--timeout", type=float, default=0.3)
    ap.add_argument("--delay", type=float, default=0.05,
                    help="pausa entre requisicoes (default: 0.05 s)")
    ap.add_argument("--range", action="store_true",
                    help="depois do scan de units, varre enderecos")
    ap.add_argument("--end", type=int, default=32,
                    help="ultimo endereco da varredura com --range")
    args = ap.parse_args()

    units = parse_units(args.units)
    print(f"alvo {args.host}:{args.port} fc={args.fc} "
          f"addr={args.addr} qty={args.qty} timeout={args.timeout}s")
    print(f"units: {units}\n")

    found = []

    for unit in units:
        status, detail = probe(args.host, args.port, unit, args.fc,
                               args.addr, args.qty, args.timeout)

        if status == "ok":
            resp = detail
            bc = resp[8]
            vals = [struct.unpack(">H", resp[9 + 2 * i:11 + 2 * i])[0]
                    for i in range(bc // 2)]
            txt = " ".join(f"0x{v:04X}" for v in vals)
            print(f"unit {unit:3d}: OK   {txt}")
            found.append(unit)
        elif status == "exc":
            print(f"unit {unit:3d}: EXC  codigo {detail}")
        elif status == "timeout":
            print(f"unit {unit:3d}: --   timeout")
        else:
            print(f"unit {unit:3d}: ERR  {detail}")

        time.sleep(args.delay)

    if not args.range:
        return 0

    if not found:
        print("\nnenhum unit respondeu; nada para varrer")
        return 0

    for unit in found:
        print(f"\n-- unit {unit}: enderecos 0..{args.end} --")
        for addr in range(args.end + 1):
            status, detail = probe(args.host, args.port, unit, args.fc,
                                   addr, 1, args.timeout)
            if status == "ok":
                value = struct.unpack(">H", detail[9:11])[0]
                print(f"  addr {addr:5d} = 0x{value:04X} ({value})")
            elif status == "exc":
                print(f"  addr {addr:5d} exc {detail}")
            elif status == "timeout":
                print(f"  addr {addr:5d} timeout")
            else:
                print(f"  addr {addr:5d} err {detail}")
            time.sleep(args.delay)

    return 0


if __name__ == "__main__":
    sys.exit(main())
