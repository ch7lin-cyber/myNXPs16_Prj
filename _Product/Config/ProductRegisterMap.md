# Product Register Map

All addresses are Modbus holding-register addresses. Multi-register writes use
FC10; individual values and Apply Keys may use FC06. PWM period and duty writes
are staged as Pending values and do not affect hardware until the PWM Apply Key
is accepted.

## Four-channel temperature input configuration

Each AD7124 has an independent 10-register block. The offsets are identical:

| Input | Register block | Sensor Type | TC type | Apply Key |
|---|---|---|---|---|
| CH0 / AD7124 #0 | `0x1000..0x1009` | `0x1007` | `0x1008` | `0x1009` |
| CH1 / AD7124 #1 | `0x1010..0x1019` | `0x1017` | `0x1018` | `0x1019` |
| CH2 / AD7124 #2 | `0x1020..0x1029` | `0x1027` | `0x1028` | `0x1029` |
| CH3 / AD7124 #3 | `0x1030..0x1039` | `0x1037` | `0x1038` | `0x1039` |

The factory default is Thermocouple K (`Sensor Type=95`, `TC type=48`). Writing
Sensor Type, TC type, or filter time with FC06/FC10 only changes that channel's
RAM Pending copy. Write `0xA5A5` to the same channel's Apply Key to request an
atomic change. Apply revalidates linked fields before changing Active state.
FC10 may write Sensor Type, TC type, and `0xA5A5` together as three consecutive
registers beginning at offset `+7`.

Pending has no time-based expiry. A later write replaces the corresponding
Pending field; an explicit discard restores Pending from Active. If Apply fails
linked validation, Pending is retained so the controller/HMI can correct it,
while Active, ADC hardware, and NVM remain unchanged. Power loss or reset before
a successful Apply discards RAM Pending and restores the last CRC-valid Active
configuration from FRAM. After a successful Apply, Event consumers reconfigure
the AD7124 and dependent Alarm/Safety state; NVM is written only after the
required consumers acknowledge the event.

Sensor switching updates the AD7124 input pair, PGA gain, reference, excitation
current, and CV_SEL mode. The first conversion after reconfiguration is ignored
to allow the digital filter to settle. The AD7124 ERROR register is captured
once during device initialization/reinitialization and is not read by the
normal 100 Hz polling path.

## Four-channel PWM configuration

| Address | Access | Meaning | Range / unit |
|---|---|---|---|
| `0x1300` | R/W | PWM0 period | 10..10000 ms |
| `0x1301` | R/W | PWM0 duty | 0..1000 = 0.0..100.0% |
| `0x1302` | R/W | PWM0 period update mode | 0 immediate, 1 next cycle |
| `0x1303` | R/W | PWM1 period | 10..10000 ms |
| `0x1304` | R/W | PWM1 duty | 0..1000 = 0.0..100.0% |
| `0x1305` | R/W | PWM1 period update mode | 0 immediate, 1 next cycle |
| `0x1306` | R/W | PWM2 period | 10..10000 ms |
| `0x1307` | R/W | PWM2 duty | 0..1000 = 0.0..100.0% |
| `0x1308` | R/W | PWM2 period update mode | 0 immediate, 1 next cycle |
| `0x1309` | R/W | PWM3 period | 10..10000 ms |
| `0x130A` | R/W | PWM3 duty | 0..1000 = 0.0..100.0% |
| `0x130B` | R/W | PWM3 period update mode | 0 immediate, 1 next cycle |
| `0x130C` | W | PWM Apply Key | write `0xA5A5` |
| `0x130D` | R | PWM configuration revision | increments after effective Apply |
| `0x130E` | R | PWM Pending channel mask | bits 0..3 = PWM0..PWM3 |

Reading `0x1300..0x130B` returns the Active configuration. Writing stages the
Pending configuration. `0x130E` identifies which channels have staged values.
Pending values can also be inspected internally through
`ProductModbusRegisterAdapter_GetPendingPwmConfig()`.

FC10 may stage several channels and include `0x130C = 0xA5A5` as the final
register to apply them atomically. Invalid period, duty, update mode, Apply Key,
or an Apply with no Pending channel returns Modbus exception 03.

## Factory FRAM bank test

This is a destructive factory-only checkerboard test for the complete 256 KiB
external FRAM. Enter Factory Mode first by writing `0x1234` to both `0x4700`
and `0x4701`, then write command `1` to `0x4710`. Command `2` aborts a running
test.

Each of the four 64 KiB banks is processed in four phases: write alternating
`0xA5A5/0x5A5A`, verify, write the swapped `0x5A5A/0xA5A5` pattern, and verify
again. One 512-byte chunk is handled per application cycle. Existing FRAM
parameter data is destroyed.

| Address | Access | Meaning |
|---|---|---|
| `0x4710` | W | Command: 1 start, 2 abort |
| `0x4711` | R | State: 0 idle, 1..4 test phases, 5 pass, 6 fail, 7 aborted |
| `0x4712` | R | Error: 0 none, 1 locked, 2 NVM busy, 3 geometry, 4 write, 5 read, 6 verify |
| `0x4713` | R | Current bank, 0..3 |
| `0x4714` | R | Completed bank mask, bits 0..3 |
| `0x4715` | R | Failed bank mask, bits 0..3 |
| `0x4716` | R | Progress, 0..1000 = 0.0..100.0% |
| `0x4717..0x4718` | R | Current byte address, unsigned 32-bit |
| `0x4719..0x471A` | R | First failure byte address, unsigned 32-bit |
| `0x471B` | R | Expected 16-bit pattern |
| `0x471C` | R | Actual 16-bit value |

## MCU overtemperature diagnostic

The MCU temperature is sampled once per second. Three consecutive samples at
or above 85.00 degrees Celsius raise fault `0x0201` and assert the global Safety
output inhibit. All four PWM outputs are then driven to 0.0%.

The fault is visible through the existing diagnostic block at `0x4800`. Its
detail value is the MCU temperature in 0.01 degree Celsius (`8500` = 85.00
degrees Celsius). Cooling does not clear the latched fault. Clear it through
`0x4809 = 0x0201`, `0x480A = 0xC1EA`; if the MCU remains overtemperature, the
application raises it again and keeps all outputs inhibited.
