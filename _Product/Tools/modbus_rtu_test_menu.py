#!/usr/bin/env python3
"""Interactive Modbus RTU test menu for product ADC diagnostics.

Default communication settings:
    Slave address: 2
    Baud rate:     115200
    Data format:   8N1

Dependency:
    python -m pip install pyserial
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from typing import Iterable, Sequence

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    raise SystemExit(
        "Missing pyserial. Install it with: python -m pip install pyserial"
    ) from exc


DEFAULT_SLAVE_ID = 2
DEFAULT_BAUD_RATE = 115200
DEFAULT_TIMEOUT_SECONDS = 1.0

PV_BASE_ADDRESS = 0x1000
PV_CHANNEL_STRIDE = 0x0010
ADC_DIAGNOSTIC_BASE_ADDRESS = 0x4820
ADC_DIAGNOSTIC_DEVICE_STRIDE = 0x0040

ADC_DIAGNOSTIC_NAMES = {
    0x00: "Initialized",
    0x01: "Device ID",
    0x02: "STATUS",
    0x03: "Active fault categories",
    0x04: "Last fault categories",
    0x05: "Last driver status (high)",
    0x06: "Last driver status (low)",
    0x07: "Initial ERROR (high)",
    0x08: "Initial ERROR (low)",
    0x09: "First ERROR (high)",
    0x0A: "First ERROR (low)",
    0x0B: "Last ERROR (high)",
    0x0C: "Last ERROR (low)",
    0x0D: "Latched ERROR OR (high)",
    0x0E: "Latched ERROR OR (low)",
    0x0F: "Initialization attempts (high)",
    0x10: "Initialization attempts (low)",
    0x11: "Successful samples (high)",
    0x12: "Successful samples (low)",
    0x13: "Discarded samples (high)",
    0x14: "Discarded samples (low)",
    0x2F: "Latest raw code (high)",
    0x30: "Latest raw code (low)",
    0x31: "Latest ADC channel",
    0x32: "Transaction/clean streaks",
    0x33: "Latest signed microvolts (high)",
    0x34: "Latest signed microvolts (low)",
    0x35: "Configure attempts (high)",
    0x36: "Configure attempts (low)",
    0x37: "IO_CONTROL1 readback (high)",
    0x38: "IO_CONTROL1 readback (low)",
    0x39: "CHANNEL0 readback (high)",
    0x3A: "CHANNEL0 readback (low)",
    0x3B: "CONFIG0 readback (high)",
    0x3C: "CONFIG0 readback (low)",
    0x3D: "FILTER0 readback (high)",
    0x3E: "FILTER0 readback (low)",
    0x3F: "Configuration readback valid",
}


class ModbusError(RuntimeError):
    """Raised when an RTU response is invalid or reports an exception."""


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else crc >> 1
    return crc & 0xFFFF


def append_crc(frame: bytes) -> bytes:
    return frame + crc16_modbus(frame).to_bytes(2, byteorder="little")


def parse_integer(text: str, minimum: int = 0, maximum: int = 0xFFFF) -> int:
    value = int(text.strip(), 0)
    if not minimum <= value <= maximum:
        raise ValueError(f"value must be in range {minimum}..{maximum}")
    return value


def uint32_from_registers(registers: Sequence[int]) -> int:
    return (registers[0] << 16) | registers[1]


def int32_from_registers(registers: Sequence[int]) -> int:
    value = uint32_from_registers(registers)
    return value - 0x100000000 if value & 0x80000000 else value


def float32_from_registers(registers: Sequence[int]) -> float:
    packed = struct.pack(">HH", registers[0], registers[1])
    return struct.unpack(">f", packed)[0]


def print_registers(
    start_address: int,
    registers: Sequence[int],
    names: dict[int, str] | None = None,
    name_offset_base: int = 0,
) -> None:
    for index, value in enumerate(registers):
        address = start_address + index
        label = ""
        if names is not None:
            name = names.get(name_offset_base + index)
            if name:
                label = f"  {name}"
        print(f"  0x{address:04X} = 0x{value:04X}{label}")


@dataclass
class ModbusRtuClient:
    port: str
    slave_id: int = DEFAULT_SLAVE_ID
    baud_rate: int = DEFAULT_BAUD_RATE
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS

    def __post_init__(self) -> None:
        self.serial = serial.Serial(
            port=self.port,
            baudrate=self.baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout_seconds,
            write_timeout=self.timeout_seconds,
        )

    def close(self) -> None:
        if self.serial.is_open:
            self.serial.close()

    def _exchange(self, request_pdu: bytes, expected_function: int) -> bytes:
        request = append_crc(bytes([self.slave_id]) + request_pdu)
        self.serial.reset_input_buffer()
        print(f"  TX: {request.hex().upper()}")
        self.serial.write(request)
        self.serial.flush()

        header = self.serial.read(3)
        if len(header) != 3:
            raise ModbusError(
                f"response timeout: expected 3-byte header, received {len(header)}"
            )

        function = header[1]
        if function & 0x80:
            response = header + self.serial.read(2)
        elif function in (0x03, 0x04):
            response = header + self.serial.read(header[2] + 2)
        elif function in (0x06, 0x10):
            response = header + self.serial.read(5)
        else:
            raise ModbusError(f"unsupported response function 0x{function:02X}")

        print(f"  RX: {response.hex().upper()}")
        if len(response) < 5:
            raise ModbusError("response is incomplete")
        if crc16_modbus(response[:-2]) != int.from_bytes(response[-2:], "little"):
            raise ModbusError("response CRC error")
        if response[0] != self.slave_id:
            raise ModbusError(
                f"unexpected slave address {response[0]}, expected {self.slave_id}"
            )
        if function & 0x80:
            raise ModbusError(
                f"Modbus exception: function=0x{function:02X}, "
                f"code=0x{response[2]:02X}"
            )
        if function != expected_function:
            raise ModbusError(
                f"unexpected function 0x{function:02X}, "
                f"expected 0x{expected_function:02X}"
            )
        return response

    def read_holding_registers(self, start_address: int, quantity: int) -> list[int]:
        if not 1 <= quantity <= 125:
            raise ValueError("one FC03 request supports 1..125 registers")
        pdu = struct.pack(">BHH", 0x03, start_address, quantity)
        response = self._exchange(pdu, 0x03)
        byte_count = response[2]
        if byte_count != quantity * 2:
            raise ModbusError(
                f"unexpected byte count {byte_count}, expected {quantity * 2}"
            )
        return list(struct.unpack(f">{quantity}H", response[3 : 3 + byte_count]))

    def read_holding_register_range(
        self, start_address: int, quantity: int
    ) -> list[int]:
        if not 1 <= quantity <= 0x10000 - start_address:
            raise ValueError("requested register range is invalid")
        values: list[int] = []
        address = start_address
        remaining = quantity
        while remaining:
            count = min(remaining, 125)
            values.extend(self.read_holding_registers(address, count))
            address += count
            remaining -= count
        return values

    def write_single_register(self, address: int, value: int) -> None:
        pdu = struct.pack(">BHH", 0x06, address, value)
        response = self._exchange(pdu, 0x06)
        if response[2:6] != pdu[1:5]:
            raise ModbusError("FC06 response does not echo address/value")

    def write_multiple_registers(self, address: int, values: Sequence[int]) -> None:
        if not 1 <= len(values) <= 123:
            raise ValueError("one FC10 request supports 1..123 registers")
        encoded_values = struct.pack(f">{len(values)}H", *values)
        pdu = (
            struct.pack(">BHHB", 0x10, address, len(values), len(encoded_values))
            + encoded_values
        )
        response = self._exchange(pdu, 0x10)
        echoed_address, echoed_quantity = struct.unpack(">HH", response[2:6])
        if echoed_address != address or echoed_quantity != len(values):
            raise ModbusError("FC10 response does not echo address/quantity")


def choose_serial_port(command_line_port: str | None) -> str:
    if command_line_port:
        return command_line_port

    ports = list(list_ports.comports())
    if ports:
        print("Available serial ports:")
        for index, port_info in enumerate(ports, start=1):
            print(f"  {index}. {port_info.device}  {port_info.description}")
        selection = input("Select port number or enter port name: ").strip()
        if selection.isdigit() and 1 <= int(selection) <= len(ports):
            return ports[int(selection) - 1].device
        if selection:
            return selection

    port = input("Serial port (for example COM8 or /dev/ttyUSB0): ").strip()
    if not port:
        raise ValueError("serial port is required")
    return port


def read_channel_diagnostics(client: ModbusRtuClient) -> None:
    channel = parse_integer(input("Channel (0/1/2/3): "), 0, 3)
    pv_address = PV_BASE_ADDRESS + channel * PV_CHANNEL_STRIDE
    diagnostic_base = (
        ADC_DIAGNOSTIC_BASE_ADDRESS + channel * ADC_DIAGNOSTIC_DEVICE_STRIDE
    )

    print(f"\n[CH{channel} PV]")
    pv_registers = client.read_holding_registers(pv_address, 2)
    print_registers(pv_address, pv_registers)
    print(f"  Float32 = {float32_from_registers(pv_registers):.6f}")

    print(f"\n[CH{channel} ADC diagnostics]")
    diagnostics = client.read_holding_registers(diagnostic_base, 16)
    print_registers(
        diagnostic_base,
        diagnostics,
        ADC_DIAGNOSTIC_NAMES,
        name_offset_base=0x00,
    )
    print(f"  Last driver status = {int32_from_registers(diagnostics[5:7])}")
    print(f"  Initial ERROR       = 0x{uint32_from_registers(diagnostics[7:9]):08X}")
    print(f"  First ERROR         = 0x{uint32_from_registers(diagnostics[9:11]):08X}")
    print(f"  Last ERROR          = 0x{uint32_from_registers(diagnostics[11:13]):08X}")
    print(f"  Latched ERROR OR    = 0x{uint32_from_registers(diagnostics[13:15]):08X}")

    sample_address = diagnostic_base + 0x11
    print(f"\n[CH{channel} successful/discarded samples]")
    sample_registers = client.read_holding_registers(sample_address, 4)
    print_registers(
        sample_address,
        sample_registers,
        ADC_DIAGNOSTIC_NAMES,
        name_offset_base=0x11,
    )
    print(f"  Successful samples = {uint32_from_registers(sample_registers[0:2])}")
    print(f"  Discarded samples  = {uint32_from_registers(sample_registers[2:4])}")

    raw_address = diagnostic_base + 0x2F
    print(f"\n[CH{channel} raw code/microvolts]")
    raw_registers = client.read_holding_registers(raw_address, 6)
    print_registers(
        raw_address,
        raw_registers,
        ADC_DIAGNOSTIC_NAMES,
        name_offset_base=0x2F,
    )
    print(f"  Raw Code   = 0x{uint32_from_registers(raw_registers[0:2]):08X}")
    print(f"  ADC channel= {raw_registers[2]}")
    print(f"  Streaks    = 0x{raw_registers[3]:04X}")
    print(f"  Microvolts = {int32_from_registers(raw_registers[4:6])} uV")

    configuration_address = diagnostic_base + 0x35
    print(f"\n[CH{channel} AD7124 configuration readback]")
    configuration_registers = client.read_holding_registers(
        configuration_address, 11
    )
    print_registers(
        configuration_address,
        configuration_registers,
        ADC_DIAGNOSTIC_NAMES,
        name_offset_base=0x35,
    )
    print(
        "  Configure attempts = "
        f"{uint32_from_registers(configuration_registers[0:2])}"
    )
    print(
        "  IO_CONTROL1       = "
        f"0x{uint32_from_registers(configuration_registers[2:4]):08X}"
    )
    print(
        "  CHANNEL0          = "
        f"0x{uint32_from_registers(configuration_registers[4:6]):08X}"
    )
    print(
        "  CONFIG0           = "
        f"0x{uint32_from_registers(configuration_registers[6:8]):08X}"
    )
    print(
        "  FILTER0           = "
        f"0x{uint32_from_registers(configuration_registers[8:10]):08X}"
    )
    print(f"  Readback valid    = {configuration_registers[10]}")


    trace_address = 0x4A00 + channel * ADC_DIAGNOSTIC_DEVICE_STRIDE
    print(f"\n[CH{channel} ADC configuration trace]")
    trace = client.read_holding_registers(trace_address, 35)
    sources = {0: "unknown", 1: "startup", 2: "sensor event", 3: "recovery"}
    stages = {0: "none", 1: "validate", 2: "ADC registers",
              3: "ERROR_ENABLE write", 4: "ERROR_ENABLE verify",
              5: "configuration readback", 6: "complete"}
    event_stages = {0: "idle", 1: "route", 2: "configure",
                    3: "sensor class", 4: "ACK", 5: "complete"}
    print(f"  Last source       = {sources.get(trace[0], trace[0])}")
    print(f"  Last stage        = {stages.get(trace[1], trace[1])}")
    print(f"  HAL result        = {trace[2]} (0 = OK)")
    print(f"  Discard pending   = {trace[3]}")
    for offset, label in [(4, "Startup config"), (6, "Event config"),
                          (8, "Recovery config"), (10, "Unknown config"),
                          (12, "Event ID"), (16, "Event attempts"),
                          (18, "Apply failures"), (20, "ACK attempts"),
                          (22, "First discards"), (24, "Fault discards"),
                          (28, "Driver errors"), (30, "Config successes"),
                          (32, "ACK failures")]:
        print(f"  {label:18s}= {uint32_from_registers(trace[offset:offset + 2])}")
    print(f"  Event revision    = {trace[14]}")
    print(f"  Event apply result= {trace[15]} (0 = OK)")
    print(f"  Online            = {trace[26]}")
    print(f"  Consecutive errors= {trace[27]}")
    print(f"  Event stage       = {event_stages.get(trace[34], trace[34])}")


def read_arbitrary_registers(client: ModbusRtuClient) -> None:
    address = parse_integer(input("Start address (example 0x1000): "))
    maximum_quantity = 0x10000 - address
    quantity = parse_integer(
        input("Register quantity: "), 1, maximum_quantity
    )
    registers = client.read_holding_register_range(address, quantity)
    print_registers(address, registers)


def parse_write_values(text: str) -> list[int]:
    tokens: Iterable[str] = text.replace(",", " ").split()
    values = [parse_integer(token) for token in tokens]
    if not values:
        raise ValueError("at least one value is required")
    return values


def write_registers(client: ModbusRtuClient) -> None:
    address = parse_integer(input("Write start address (example 0x1007): "))
    values = parse_write_values(
        input("Value(s), hexadecimal uses 0x prefix; separate multiple values with spaces: ")
    )
    if address + len(values) > 0x10000:
        raise ValueError("write range exceeds 0xFFFF")

    print("Values to write:")
    print_registers(address, values)
    confirmation = input("Confirm write? (y/N): ").strip().lower()
    if confirmation != "y":
        print("Write cancelled.")
        return

    if len(values) == 1:
        client.write_single_register(address, values[0])
    else:
        client.write_multiple_registers(address, values)
    print("Write completed.")


def run_menu(client: ModbusRtuClient) -> None:
    while True:
        print(
            "\n========== Modbus RTU Test Menu ==========\n"
            "1. Read channel PV and ADC diagnostics\n"
            "3. Read arbitrary holding registers\n"
            "6. Write holding register(s)\n"
            "0. Exit\n"
            "=========================================="
        )
        selection = input("Select: ").strip()
        try:
            if selection == "1":
                read_channel_diagnostics(client)
            elif selection == "3":
                read_arbitrary_registers(client)
            elif selection == "6":
                write_registers(client)
            elif selection == "0":
                return
            else:
                print("Unknown selection. Use 1, 3, 6, or 0.")
        except (ValueError, ModbusError, serial.SerialException) as exc:
            print(f"ERROR: {exc}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port, for example COM8")
    parser.add_argument("--slave", type=int, default=DEFAULT_SLAVE_ID)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD_RATE)
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()

    if not 1 <= args.slave <= 247:
        parser.error("--slave must be in range 1..247")
    if args.baud <= 0:
        parser.error("--baud must be positive")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    try:
        port = choose_serial_port(args.port)
        client = ModbusRtuClient(
            port=port,
            slave_id=args.slave,
            baud_rate=args.baud,
            timeout_seconds=args.timeout,
        )
    except (ValueError, serial.SerialException) as exc:
        print(f"Unable to open serial port: {exc}", file=sys.stderr)
        return 1

    print(
        f"Connected: {port}, slave={args.slave}, "
        f"{args.baud} baud, 8N1, timeout={args.timeout:.1f}s"
    )
    try:
        run_menu(client)
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
