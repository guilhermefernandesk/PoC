#!/usr/bin/env python3
"""Black-box Modbus TCP validation for the PoC server.

Works against the host POSIX build and against the STM32 target:

    ./build-host/mb_server_posix 5020
    python3 tools/modbus_test.py --host 127.0.0.1 --port 5020

    python3 tools/modbus_test.py --host 192.168.1.10 --port 502

The tests deliberately write known values before reading them, so the script
can be run repeatedly against a server that keeps its register map in RAM.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time

FC_READ_HOLDING = 0x03
FC_READ_INPUT = 0x04
FC_WRITE_SINGLE = 0x06
FC_WRITE_MULTIPLE = 0x10


class TestFailure(AssertionError):
    pass


def recv_exact(sock: socket.socket, length: int, timeout: float) -> bytes:
    sock.settimeout(timeout)
    data = bytearray()
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("connection closed by server")
        data.extend(chunk)
    return bytes(data)


def connect(host: str, port: int, timeout: float) -> socket.socket:
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    return sock


def send_adu(sock: socket.socket, tid: int, unit: int, pdu: bytes) -> None:
    adu = struct.pack(">HHHB", tid, 0, len(pdu) + 1, unit) + pdu
    sock.sendall(adu)


def read_adu(sock: socket.socket, timeout: float) -> tuple[int, int, int, bytes]:
    header = recv_exact(sock, 7, timeout)
    tid, protocol, length, unit = struct.unpack(">HHHB", header)
    if protocol != 0:
        raise TestFailure(f"response protocol id is {protocol}, expected 0")
    if length < 2:
        raise TestFailure(f"invalid response length {length}")
    body = recv_exact(sock, length - 1, timeout)
    return tid, protocol, unit, body


def expect_response(
    sock: socket.socket,
    tid: int,
    unit: int,
    pdu: bytes,
    timeout: float,
) -> bytes:
    send_adu(sock, tid, unit, pdu)
    response_tid, _protocol, response_unit, body = read_adu(sock, timeout)

    if response_tid != tid:
        raise TestFailure(f"transaction id {response_tid:#06x}, expected {tid:#06x}")
    if response_unit != unit:
        raise TestFailure(f"unit id {response_unit}, expected {unit}")
    return body


def expect_exception(
    sock: socket.socket,
    tid: int,
    unit: int,
    pdu: bytes,
    function: int,
    exception: int,
    timeout: float,
) -> None:
    body = expect_response(sock, tid, unit, pdu, timeout)
    expected = bytes([function | 0x80, exception])
    if body != expected:
        raise TestFailure(f"exception PDU {body.hex()}, expected {expected.hex()}")


def test_fc03_and_fc16(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        values = (0x1111, 0x2222, 0x3333, 0x4444)
        write_pdu = struct.pack(
            ">BHHB" + "H" * len(values),
            FC_WRITE_MULTIPLE,
            0,
            len(values),
            2 * len(values),
            *values,
        )
        body = expect_response(sock, 0x1001, unit, write_pdu, timeout)
        if body != struct.pack(">BHH", FC_WRITE_MULTIPLE, 0, len(values)):
            raise TestFailure(f"FC16 echo {body.hex()}")

        read_pdu = struct.pack(">BHH", FC_READ_HOLDING, 0, len(values))
        body = expect_response(sock, 0x1002, unit, read_pdu, timeout)
        expected = struct.pack(">BB" + "H" * len(values), FC_READ_HOLDING,
                               2 * len(values), *values)
        if body != expected:
            raise TestFailure(f"FC03 values {body.hex()}, expected {expected.hex()}")


def test_fc04(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        read_pdu = struct.pack(">BHH", FC_READ_INPUT, 0, 4)
        body = expect_response(sock, 0x1003, unit, read_pdu, timeout)
        values = struct.unpack(">4H", body[2:])
        if body[0] != FC_READ_INPUT or body[1] != 8:
            raise TestFailure(f"FC04 header {body[:2].hex()}")
        if values != (1000, 1001, 1002, 1003):
            raise TestFailure(f"FC04 values {values}")


def test_fc06(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        write_pdu = struct.pack(">BHH", FC_WRITE_SINGLE, 5, 0xABCD)
        body = expect_response(sock, 0x1004, unit, write_pdu, timeout)
        if body != write_pdu:
            raise TestFailure(f"FC06 echo {body.hex()}")

        read_pdu = struct.pack(">BHH", FC_READ_HOLDING, 5, 1)
        body = expect_response(sock, 0x1005, unit, read_pdu, timeout)
        if body != struct.pack(">BBH", FC_READ_HOLDING, 2, 0xABCD):
            raise TestFailure(f"FC06 readback {body.hex()}")


def test_exceptions(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        expect_exception(
            sock, 0x2001, unit, struct.pack(">BHH", FC_READ_HOLDING, 1000, 1),
            FC_READ_HOLDING, 0x02, timeout,
        )
        expect_exception(
            sock, 0x2002, unit, struct.pack(">BHH", FC_READ_HOLDING, 0, 0),
            FC_READ_HOLDING, 0x03, timeout,
        )
        expect_exception(
            sock, 0x2003, unit, struct.pack(">BHH", FC_READ_HOLDING, 0, 126),
            FC_READ_HOLDING, 0x03, timeout,
        )
        expect_exception(
            sock, 0x2004, unit, struct.pack(">BHH", FC_WRITE_SINGLE, 1000, 1),
            FC_WRITE_SINGLE, 0x02, timeout,
        )
        expect_exception(
            sock, 0x2005, unit, struct.pack(">BHH", 0x05, 0, 1),
            0x05, 0x01, timeout,
        )


def test_fragmented_frame(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        pdu = struct.pack(">BHH", FC_READ_HOLDING, 0, 1)
        adu = struct.pack(">HHHB", 0x3001, 0, len(pdu) + 1, unit) + pdu
        sock.sendall(adu[:7])
        time.sleep(0.05)
        sock.sendall(adu[7:])
        response_tid, _protocol, response_unit, body = read_adu(sock, timeout)
        if response_tid != 0x3001 or response_unit != unit:
            raise TestFailure("fragmented frame response header mismatch")
        if body[0] != FC_READ_HOLDING or body[1] != 2:
            raise TestFailure(f"fragmented frame response {body.hex()}")


def test_coalesced_frames(host: str, port: int, unit: int, timeout: float) -> None:
    with connect(host, port, timeout) as sock:
        pdu = struct.pack(">BHH", FC_READ_HOLDING, 0, 1)
        adu1 = struct.pack(">HHHB", 0x4001, 0, len(pdu) + 1, unit) + pdu
        adu2 = struct.pack(">HHHB", 0x4002, 0, len(pdu) + 1, unit) + pdu
        sock.sendall(adu1 + adu2)

        tid1, _protocol1, unit1, body1 = read_adu(sock, timeout)
        tid2, _protocol2, unit2, body2 = read_adu(sock, timeout)
        if (tid1, unit1) != (0x4001, unit) or (tid2, unit2) != (0x4002, unit):
            raise TestFailure("coalesced frame headers mismatch")
        if body1[0] != FC_READ_HOLDING or body2[0] != FC_READ_HOLDING:
            raise TestFailure("coalesced frame payload mismatch")


def test_wrong_unit_is_ignored(
    host: str, port: int, unit: int, timeout: float
) -> None:
    with connect(host, port, timeout) as sock:
        pdu = struct.pack(">BHH", FC_READ_HOLDING, 0, 1)
        send_adu(sock, 0x5001, (unit + 1) & 0xFF, pdu)
        sock.settimeout(min(0.5, timeout))
        try:
            read_adu(sock, min(0.5, timeout))
        except socket.timeout:
            return
        except ConnectionError as exc:
            raise TestFailure(f"server closed connection for wrong unit: {exc}") from exc
        raise TestFailure("server answered a frame with the wrong unit id")


TESTS = (
    ("FC03 + FC16", test_fc03_and_fc16),
    ("FC04", test_fc04),
    ("FC06", test_fc06),
    ("exceptions", test_exceptions),
    ("fragmented frame", test_fragmented_frame),
    ("coalesced frames", test_coalesced_frames),
    ("wrong unit ignored", test_wrong_unit_is_ignored),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5020)
    parser.add_argument("--unit", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    if not 0 <= args.unit <= 255:
        parser.error("--unit must be in 0..255")

    failures = 0
    for name, test in TESTS:
        try:
            test(args.host, args.port, args.unit, args.timeout)
        except Exception as exc:  # noqa: BLE001 - report the test boundary
            failures += 1
            print(f"FAIL {name}: {exc}", file=sys.stderr)
        else:
            print(f"PASS {name}")

    if failures:
        print(f"{failures} test(s) failed", file=sys.stderr)
        return 1

    print("all Modbus TCP tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
