"""CALYS 1500 SCPI adapter; AOIP DE/15/160 V1.3, sections 1, 3, 6.

_X means there is no verified direct SCPI/firmware curve mapping. It does
not assert that the physical calibrator lacks that sensor function.
"""
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import math
import time


@dataclass(frozen=True)
class Sensor:
    code: int
    name: str
    kind: str
    token: str | None
    low: int
    high: int
    unit: str
    gain: int

    @property
    def label(self):
        return self.name + ("_X" if self.token is None else "")


SENSORS = {s.code: s for s in [
    Sensor(11, "B", "TC", "B", 100, 1800, "degC", 128),
    Sensor(15, "C", "TC", "C", 0, 2300, "degC", 64),
    Sensor(23, "D", "TC", None, 0, 2300, "degC", 32),
    Sensor(26, "E", "TC", "E", 0, 600, "degC", 32),
    Sensor(46, "J", "TC", "J", -200, 1200, "degC", 16),
    Sensor(48, "K", "TC", "K", -200, 1300, "degC", 32),
    Sensor(58, "N", "TC", "N", -200, 1300, "degC", 32),
    Sensor(62, "OFF", "OFF", None, 0, 0, "", 0),
    Sensor(80, "R", "TC", "R", 0, 1700, "degC", 64),
    Sensor(84, "S", "TC", "S", 0, 1700, "degC", 128),
    Sensor(93, "T", "TC", "T", -200, 400, "degC", 64),
    Sensor(100, "L", "TC", "L", -200, 850, "degC", 32),
    Sensor(101, "U", "TC", "U", -200, 500, "degC", 32),
    Sensor(102, "TXK", "TC", None, -150, 800, "degC", 32),
    Sensor(113, "Pt100", "RTD", "PT100", -200, 850, "degC", 8),
    Sensor(114, "Pt1000", "RTD", "PT1000", -200, 850, "degC", 1),
    Sensor(115, "JPt100", "RTD", None, -20, 400, "degC", 16),
    Sensor(116, "Ni120", "RTD", "NI120", -80, 300, "degC", 8),
    Sensor(117, "Cu50", "RTD", "CU50", -50, 150, "degC", 32),
    Sensor(120, "0-5V", "VOLT", "10V", 0, 5, "V", 32),
    Sensor(121, "0-10V", "VOLT", "10V", 0, 10, "V", 32),
    Sensor(122, "0-50mV", "VOLT", "100MV", 0, 50, "mV", 32),
    Sensor(123, "0-20mA", "CURR", "0MA", 0, 20, "mA", 64),
    Sensor(124, "4-20mA", "CURR", "4MA", 4, 20, "mA", 64),
]}


def numeric(text):
    try:
        value = Decimal(str(text))
    except InvalidOperation as exc:
        raise ValueError("請輸入有效數值") from exc
    if not value.is_finite():
        raise ValueError("數值不能是 NaN 或 Infinity")
    return value


def validate(sensor, value):
    value = numeric(value)
    if sensor.token is None:
        raise ValueError(f"{sensor.label} 尚無已驗證的模擬對映")
    if not sensor.low <= value <= sensor.high:
        raise ValueError(f"數值必須介於 {sensor.low}～{sensor.high} {sensor.unit}")
    return value


def ten_points(sensor):
    validate(sensor, sensor.low)
    low, high = Decimal(sensor.low), Decimal(sensor.high)
    # 0.001 in the sensor's displayed unit, not in implicit SCPI range units.
    return [(low + (high - low) * i / 9).quantize(Decimal("0.001"))
            for i in range(10)]


def setup_commands(sensor, cjc="INT", fixed_cjc="0", current_supply="ON"):
    validate(sensor, sensor.low)
    if cjc not in ("INT", "DIS", "FIX") or current_supply not in ("ON", "OFF"):
        raise ValueError("無效的冷端或電流供電設定")
    commands = ["SOUR:SCAL OFF"]
    if sensor.kind == "TC":
        commands += [f"SOUR:TC:TYPE {sensor.token}", "SOUR:TC:DISP CEL",
                     f"SOUR:TC:RJUN:TYPE {cjc}"]
        if cjc == "FIX":
            commands += [f"SOUR:TC:RJUN {numeric(fixed_cjc):f}"]
    elif sensor.kind == "RTD":
        commands += [f"SOUR:RTD:TYPE {sensor.token}", "SOUR:RTD:DISP CEL",
                     "SOUR:RTD:CURRENT CONT,1MA"]
    else:
        commands += [f"SOUR:{sensor.kind}:RANG {sensor.token}"]
        if sensor.kind == "CURR":
            commands += [f"SOUR:CURR:SUPP {current_supply}", "SOUR:CURR:SCAL LINEAR"]
    return commands + [f"SOUR:FUNC {sensor.kind}"]


def value_command(sensor, value):
    value = validate(sensor, value)
    if sensor.unit in ("mV", "mA"):
        value /= 1000  # Explicit SOUR:VOLT / SOUR:CURR values default to V / A.
    return f"SOUR:{sensor.kind} {value:f}"


class CalysError(RuntimeError):
    pass


class Calys1500:
    def __init__(self, port, timeout=5, transport=None, write_delay=0.15,
                 trace=False):
        if not math.isfinite(timeout) or timeout <= 0:
            raise ValueError("通訊 timeout 必須是有限正數")
        if not math.isfinite(write_delay) or write_delay < 0:
            raise ValueError("指令間隔必須是有限非負數")
        if transport is None:
            import serial
            transport = serial.Serial(port, 115200, timeout=timeout,
                                      write_timeout=timeout, bytesize=8,
                                      parity="N", stopbits=1, xonxoff=False,
                                      rtscts=False, dsrdtr=False)
        self.serial = transport
        self.port = port
        self.timeout = timeout
        self.write_delay = write_delay
        self.trace = trace
        self.remote = False
        try:
            # Allow USB/serial opening and the calibrator command parser to settle.
            time.sleep(max(0.3, write_delay))
            self.serial.reset_input_buffer()
            self.send("REM")
            self.remote = True
            self.send("*CLS")
            self.identity = self.identify()
            if "CALYS1500" not in self.identity.upper().replace("_", "").replace(" ", ""):
                raise CalysError(f"不是 CALYS 1500：{self.identity}")
            self.check_error("連線")
        except BaseException:
            self.close()
            raise

    def send(self, command):
        if self.trace:
            print(f"CALYS TX [{self.port}]: {command}")
        self.serial.write((command + "\n").encode("ascii"))
        self.serial.flush()
        # flush() only drains the host buffer; it does not mean CALYS has
        # parsed the command. Pace REM, *CLS, queries and source settings alike.
        time.sleep(self.write_delay)

    def identify(self):
        for attempt in range(3):
            try:
                return self.query("*IDN?")
            except CalysError:
                if attempt == 2:
                    raise
                if self.trace:
                    print(f"CALYS 身分查詢重試 {attempt + 2}/3")
                self.serial.reset_input_buffer()
                self.send("REM")
        raise AssertionError("unreachable")

    def query(self, command):
        self.send(command)
        deadline = time.monotonic() + self.timeout
        raw = bytearray()
        old_timeout = getattr(self.serial, "timeout", self.timeout)
        try:
            while time.monotonic() < deadline:
                self.serial.timeout = max(0.001, min(0.25, deadline - time.monotonic()))
                raw.extend(self.serial.readline())
                if raw.endswith(b"\n"):
                    response = raw.decode("ascii").strip()
                    if response:
                        if self.trace:
                            print(f"CALYS RX [{self.port}]: {response}")
                        return response
                    raw.clear()  # Ignore a leading empty CRLF, not an IDN reply.
        finally:
            self.serial.timeout = old_timeout
        raise CalysError(f"{command} 回應逾時或不完整 "
                         f"(port={self.port}, 115200/8N1, timeout={self.timeout}s, "
                         f"RX={bytes(raw)!r})")

    def check_error(self, command):
        response = self.query("ERR?")
        try:
            code = int(response.split(",", 1)[0])
        except ValueError as exc:
            raise CalysError(f"ERR? 回應無效：{response}") from exc
        if code:
            raise CalysError(f"{command}：{response}")

    def command(self, command):
        self.send(command)
        self.check_error(command)

    def configure(self, sensor, **settings):
        for command in setup_commands(sensor, **settings):
            self.command(command)

    def output(self, sensor, value):
        self.command(value_command(sensor, value))

    def close(self):
        try:
            if self.remote:
                self.send("LOC")
                self.remote = False
        finally:
            self.serial.close()
