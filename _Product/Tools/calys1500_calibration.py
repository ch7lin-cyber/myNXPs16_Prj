"""Guided, two-point factory calibration for CALYS1500 / S16.

The operator supplies the CALYS setpoints for the firmware profile targets.
This is intentional: terminal wiring and several source-to-ADC mappings still
need validation on the product board. Applying to NVM is opt-in and separately
confirmed; the default workflow ends with an auditable preview.
"""
from collections import deque
from dataclasses import dataclass
import statistics
import time

from calys1500_protocol import SENSORS, Sensor, numeric, validate


CALIBRATION_MV_100 = Sensor(
    -1, "100mV electrical calibration", "VOLT", "100MV",
    0, 100, "mV", 0)


@dataclass(frozen=True)
class WiringGroup:
    key: str
    title: str
    wiring: str
    profiles: dict[int, tuple[int, ...]]  # Profile -> available CALYS/DUT sensor codes


WIRING_GROUPS = {
    "1": WiringGroup("voltage", "電壓／mV", "CALYS V 輸出與 COM 接 DUT 電壓輸入。", {
        0: (122,), 4: (121,),
    }),
    "2": WiringGroup("rtd", "RTD 三線", "CALYS RTD 三線端依接線圖接 DUT L1／L2／L3。", {
        # Profile 2 is shared with Ni120, but Pt100 is the fixed electrical
        # calibration representative. Ni120 remains available for validation.
        2: (113,), 5: (115,), 8: (114,), 9: (117,),
    }),
    "3": WiringGroup("tc", "熱電偶 TC",
        "Factory Gain 校正使用 CALYS V/mV 與 COM 接至 DUT TC+／TC−；"
        "此步驟輸出直接 mV，不使用 TC 溫度模擬。", {
        0: (23, 26, 48, 58, 100, 101, 102), 1: (15, 80, 93),
        3: (46,), 7: (11, 84),
    }),
    "4": WiringGroup("current", "電流", "CALYS mA 與 COM 串入 DUT 電流輸入回路。", {
        6: (123,),
    }),
}

PROFILE_LABELS = {
    0: "TC/mV Gain32", 1: "TC Gain64", 2: "RTD Gain8 / 500uA",
    3: "TC Gain16", 4: "電壓 Gain32", 5: "RTD Gain16 / 500uA",
    6: "電流 Gain64", 7: "TC Gain128", 8: "RTD Gain1 / 250uA",
    9: "RTD Gain32 / 500uA",
}


def _u32(registers):
    return (registers[0] << 16) | registers[1]


def _i32(registers):
    value = _u32(registers)
    return value - (1 << 32) if value & 0x80000000 else value


def _i32_regs(value):
    value &= 0xFFFFFFFF
    return [value >> 16, value & 0xFFFF]


def profile_record_address(channel, profile):
    return 0x4C00 + channel * 0x80 + profile * 8


def read_profile(dut, channel, profile):
    words = dut.read_holding_registers(profile_record_address(channel, profile), 8)
    return {"target_zero": _i32(words[0:2]), "target_span": _i32(words[2:4]),
            "valid": words[4], "calibrated": words[5]}


def _read_snapshot(dut, channel):
    base = channel * 0x40
    count = _u32(dut.read_holding_registers(0x4E11 + base, 2))
    pv = dut.read_holding_registers(0x1000 + channel * 0x10, 3)
    raw = dut.read_holding_registers(0x4E2F + base, 6)
    fault = dut.read_holding_registers(0x4E03 + base, 1)[0]
    return {"count": count, "pv": pv[0:2], "input_error": pv[2],
            "raw_code": _u32(raw[0:2]), "microvolts": _i32(raw[4:6]), "fault": fault}


def wait_stable_sample(dut, channel, timeout=30.0, stable_count=5,
                       spread_limit_uv=20, poll_seconds=0.1, clock=time.monotonic,
                       sleep=time.sleep):
    """Wait for fresh, healthy ADC readings and return a stable-window summary."""
    deadline = clock() + timeout
    previous_count = _u32(dut.read_holding_registers(0x4E11 + channel * 0x40, 2))
    values = deque(maxlen=stable_count)
    while clock() < deadline:
        sleep(poll_seconds)
        sample = _read_snapshot(dut, channel)
        if sample["count"] == previous_count:
            continue
        previous_count = sample["count"]
        # Factory capture uses uncalibrated ADC microvolts.  Do not reject a
        # healthy raw sample merely because an old bad calibration makes the
        # converted PV report an input error; that would prevent recovery.
        if (sample["fault"] != 0 or sample["raw_code"] in (0, 0xFFFFFF)):
            values.clear()
            continue
        values.append(sample["microvolts"])
        if len(values) == stable_count and max(values) - min(values) <= spread_limit_uv:
            return {"mean_uv": round(statistics.mean(values)),
                    "min_uv": min(values), "max_uv": max(values),
                    "spread_uv": max(values) - min(values),
                    "sample_count": sample["count"], "pv_words": sample["pv"]}
    raise TimeoutError(f"等待 {stable_count} 筆穩定有效 ADC 樣本逾時（{timeout:g}s）")


def calibration_source_points(profile, sensor, input_fn=input):
    """Return the physical source points required by the firmware target."""
    electrical_points = {
        0: ("5", "45", "mV"),
        1: ("3", "27", "mV"),
        3: ("10", "90", "mV"),
        4: ("1", "9", "V"),
        6: ("2", "18", "mA"),
        7: ("1.5", "13.5", "mV"),
    }
    if profile in electrical_points:
        low, high, unit = electrical_points[profile]
        print(f"Profile {profile} 固定校正標準（電氣量程 10% / 90%）："
              f"{low} / {high} {unit}")
        return numeric(low), numeric(high)
    rtd_points = {
        # Fixed RTD 10 % / 90 % calibration points. The temperatures generate
        # the stated resistances for each representative sensor. Profile 8 is
        # specifically based on the CALYS PT1000 -220..740 degC output range.
        2: (113, "-111.170", "725.938", "55.716 / 353.285 ohm"),
        # CALYS PT1000 simulation range is -220 to 740 degC.
        8: (114, "-124.000", "644.000", "504.705 / 3277.435 ohm"),
        9: (117, "-30.000", "130.000", "43.580 / 77.820 ohm"),
    }
    if profile in rtd_points:
        required_code, low, high, resistance = rtd_points[profile]
        if sensor.code != required_code:
            raise ValueError(
                f"Profile {profile} 必須使用 Sensor code {required_code} 校正")
        basis = "CALYS 輸出量程" if profile == 8 else "電阻量程"
        print(f"Profile {profile} 固定校正標準（{basis} 10% / 90%）："
              f"{low} / {high} degC；{resistance}")
        return validate(sensor, low), validate(sensor, high)
    source_low = validate(sensor, input_fn(
        f"CALYS {sensor.label} 低點輸出 ({sensor.unit})："))
    source_high = validate(sensor, input_fn(
        f"CALYS {sensor.label} 高點輸出 ({sensor.unit})："))
    return source_low, source_high


def calibration_stimulus(group, profile, sensor):
    """Select the CALYS source independently from the DUT sensor profile."""
    if group.key == "tc":
        return CALIBRATION_MV_100
    return sensor


class FactoryCalibration:
    UNLOCK1, UNLOCK2, COMMAND, CHANNEL, PROFILE = 0x4700, 0x4701, 0x4702, 0x4703, 0x4704
    STATE, ERROR, LIVE_UV = 0x4705, 0x4706, 0x4707
    PENDING_ZERO, PENDING_SPAN = 0x4709, 0x470B
    TARGET_ZERO, STORAGE_READY = 0x4720, 0x4724
    SELECT, CAPTURE_ZERO, CAPTURE_SPAN, APPLY, ABORT = 1, 2, 3, 4, 5

    def __init__(self, dut):
        self.dut = dut

    def _write(self, address, value):
        # No retries on writes: a lost response must not duplicate an action.
        self.dut.write_single_register(address, value)

    def _command(self, command):
        self._write(self.COMMAND, command)
        error = self.dut.read_holding_registers(self.ERROR, 1)[0]
        if error != 0:
            raise RuntimeError(f"Factory command {command} rejected, error={error}")

    def select(self, channel, profile):
        if self.dut.read_holding_registers(self.STORAGE_READY, 1)[0] != 1:
            raise RuntimeError("Factory storage 尚未 ready，拒絕校正")
        # Firmware accepts the two unlock keys as one atomic FC10 write.
        self.dut.write_multiple_registers(self.UNLOCK1, [0x1234, 0x1234])
        self._write(self.CHANNEL, channel)
        self._write(self.PROFILE, profile)
        self._command(self.SELECT)
        targets = self.dut.read_holding_registers(self.TARGET_ZERO, 4)
        return _i32(targets[0:2]), _i32(targets[2:4])

    def capture(self, command):
        self._command(command)

    def preview(self):
        pending = self.dut.read_holding_registers(self.PENDING_ZERO, 4)
        return _i32(pending[0:2]), _i32(pending[2:4])

    def abort(self):
        self._command(self.ABORT)

    def apply(self):
        self._command(self.APPLY)


def guided_profile_run(calys, dut, channel, group, profile, sensor,
                       allow_apply=False, dwell=2.0, stable_timeout=30.0,
                       spread_limit_uv=20, settings=None, input_fn=input, sleep=time.sleep):
    """Capture and preview one Profile. The operator enters known source points."""
    if profile not in group.profiles or sensor.code not in group.profiles[profile]:
        raise ValueError("Sensor type 不屬於所選接線類別／Profile")
    validate(sensor, sensor.low)
    factory = FactoryCalibration(dut)
    target_zero, target_span = factory.select(channel, profile)
    previous = read_profile(dut, channel, profile)
    print(f"CH{channel} Profile {profile} {PROFILE_LABELS[profile]} targets："
          f"{target_zero} / {target_span} uV；原校正={previous}")
    stimulus = calibration_stimulus(group, profile, sensor)
    source_low, source_high = calibration_source_points(profile, sensor, input_fn)
    if source_high == source_low:
        raise ValueError("CALYS 高低點不可相同")
    settings = settings or {}
    calys.configure(stimulus, **settings)
    measurements = []
    try:
        for label, source_value, action in (("低點", source_low, factory.CAPTURE_ZERO),
                                            ("高點", source_high, factory.CAPTURE_SPAN)):
            print(f"輸出 {label}：{source_value} {stimulus.unit}")
            calys.output(stimulus, source_value)
            sleep(dwell)
            stable = wait_stable_sample(dut, channel, timeout=stable_timeout,
                                        spread_limit_uv=spread_limit_uv, sleep=sleep)
            factory.capture(action)
            measurements.append(stable)
            print(f"{label}穩定 ADC={stable['mean_uv']} uV，範圍="
                  f"{stable['min_uv']}..{stable['max_uv']} uV（span {stable['spread_uv']} uV）")
        pending_zero, pending_span = factory.preview()
        measured_delta = pending_span - pending_zero
        target_delta = target_span - target_zero
        if measured_delta == 0 or measured_delta * target_delta <= 0:
            raise RuntimeError("實測跨度方向錯誤或為零，拒絕 Apply")
        gain_ratio = target_delta / measured_delta
        if not 0.25 <= gain_ratio <= 4.0:
            raise RuntimeError(f"增益修正比例 {gain_ratio:.6f} 超出保護範圍 0.25..4.0")
        preview = {"channel": channel, "profile": profile, "sensor": sensor.label,
                   "calibration_stimulus": stimulus.label,
                   "target_zero_uv": target_zero, "target_span_uv": target_span,
                   "measured_zero_uv": pending_zero, "measured_span_uv": pending_span,
                   "gain_ratio": gain_ratio, "before": previous,
                   "low_stability": measurements[0], "high_stability": measurements[1],
                   "applied": False}
        print("\n校正預覽（實測 ADC µV 將映射至 Firmware target）：")
        print(f"  Profile {profile}／CH{channel}／{sensor.label}")
        print(f"  measured {pending_zero} → {pending_span} uV")
        print(f"  target   {target_zero} → {target_span} uV")
        print(f"  gain 修正比例 {gain_ratio:.8f}")
        if not allow_apply:
            factory.abort()
            print("預覽模式：未寫入 NVM。若刺激值與接線已在產品板驗證，"
                  "加上 --allow-calibration-apply 才能進入 Apply 確認。")
            return preview
        phrase = f"APPLY_CH{channel}_PROFILE_{profile}"
        confirmation = input_fn(
            f"確認來源、端子接線與校正標準後，輸入 {phrase}：").strip().upper()
        if confirmation != phrase:
            factory.abort()
            print("確認字串不符，已 Abort；未寫入 NVM。")
            return preview
        factory.apply()
        applied = read_profile(dut, channel, profile)
        storage_error = dut.read_holding_registers(0x4726, 1)[0]
        if (applied["calibrated"] != 1 or applied["valid"] != 1 or
                applied["target_zero"] != pending_zero or
                applied["target_span"] != pending_span or storage_error != 0):
            raise RuntimeError(f"Apply 後校正槽／FRAM 驗證失敗："
                               f"slot={applied}, storage_error={storage_error}")
        preview["applied"] = True
        preview["after"] = applied
        return preview
    except BaseException:
        # Once Apply may have reached the DUT, never send a second command to
        # infer/retry its outcome. The readback above is the only verification.
        if "confirmation" not in locals() or confirmation != phrase:
            try:
                factory.abort()
            except Exception:
                pass
        raise
