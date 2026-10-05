# Product Register Map

All addresses are Modbus holding-register addresses. Multi-register writes use
FC10; individual values and Apply Keys may use FC06. PWM period and duty writes
are staged as Pending values and do not affect hardware until the PWM Apply Key
is accepted.

## Four-channel temperature input configuration

Each AD7124 has an independent 10-register block. The offsets are identical:

| Input | Register block | Sensor Type | Reserved | Apply Key |
|---|---|---|---|---|
| CH0 / AD7124 #0 | `0x1000..0x1009` | `0x1007` | `0x1008` | `0x1009` |
| CH1 / AD7124 #1 | `0x1010..0x1019` | `0x1017` | `0x1018` | `0x1019` |
| CH2 / AD7124 #2 | `0x1020..0x1029` | `0x1027` | `0x1028` | `0x1029` |
| CH3 / AD7124 #3 | `0x1030..0x1039` | `0x1037` | `0x1038` | `0x1039` |

Thermocouple types are part of the single Sensor Type enumeration; for example,
K=`48`, J=`46`, and B=`11`. The factory default is K (`Sensor Type=48`). The
former TC Type register at offset `+8` is read-only Reserved and reads zero.
Writing Sensor Type or filter time with FC06/FC10 changes only that channel's
RAM Pending copy. Reading the normal configuration registers continues to show
Active values; there is no Modbus Pending readback block. Write `0xA5A5` to the
same channel's Apply Key to request an atomic change.

FC10 may combine Sensor Type and Apply as three registers beginning at offset
`+7`: `[Sensor Type, 0x0000 Reserved, 0xA5A5]`.

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

## Four-channel DAC8562 output

DAC output commands are runtime values and are not saved to FRAM. All four
outputs initialize to code zero after reset. FC06/FC10 writes stage Pending
codes; hardware changes only after a valid Apply Key is accepted.

| Address | Access | Meaning | Range / unit |
|---|---|---|---|
| `0x1400` | R/W | DAC output CH0 Active/Pending code | `0x0000..0xFFFF` |
| `0x1401` | R/W | DAC output CH1 Active/Pending code | `0x0000..0xFFFF` |
| `0x1402` | R/W | DAC output CH2 Active/Pending code | `0x0000..0xFFFF` |
| `0x1403` | R/W | DAC output CH3 Active/Pending code | `0x0000..0xFFFF` |
| `0x1404` | W | DAC Apply Key | write `0xA5A5` |
| `0x1405` | R | Successful Apply revision | `0..65535` |
| `0x1406` | R | Pending channel mask | bits 0..3 = CH0..CH3 |
| `0x1407` | R | DAC status | 0 Ready, 1 Apply failed, 2 Rollback failed |
| `0x1408` | R | Last failed channel | 0..3, `0xFFFF` = none |

FC03 reads Active codes; Pending values are available only through the internal
adapter API. An FC10 request may stage multiple consecutive channel codes and
include `0xA5A5` at `0x1404` as its final register. If a hardware write fails,
the request returns Server Device Failure, Active codes and revision remain
unchanged, Pending is retained for retry, and channels already written are
restored to their previous Active codes when possible.

With the current DAC8562 configuration (internal 2.5 V reference, gain 1), the
nominal DAC-pin voltage is `code / 65536 * 2.5 V`. External analog-output
conditioning, if fitted, must be handled by a later engineering-unit layer.

## Four-channel digital input

Digital inputs are sampled every 1 ms. Values are runtime states and are not
saved to FRAM. Input polarity is configured by
`PRODUCT_DIGITAL_INPUT_ACTIVE_LOW_MASK`.

| Channel | LPC55S16 pin |
|---|---|
| DI0 | PIO1_9 |
| DI1 | PIO0_16 |
| DI2 | PIO0_23 |
| DI3 | PIO1_8 |

| Address | Access | Meaning |
|---|---|---|
| `0x1500` | R | DI0 logical state, 0/1 |
| `0x1501` | R | DI1 logical state, 0/1 |
| `0x1502` | R | DI2 logical state, 0/1 |
| `0x1503` | R | DI3 logical state, 0/1 |
| `0x1504` | R | DI state mask, bits 0..3 |
| `0x1505` | R | DI change revision |

The revision increments whenever at least one sampled logical input changes.

## Four-channel digital output

Digital outputs initialize OFF and are not saved to FRAM. FC06/FC10 writes
stage Pending values; hardware changes only after a valid Apply Key. Output
polarity is configured by `PRODUCT_DIGITAL_OUTPUT_ACTIVE_LOW_MASK`.

| Channel | LPC55S16 pin |
|---|---|
| DO0 | PIO0_19 |
| DO1 | PIO0_26 |
| DO2 | PIO0_25 |
| DO3 | PIO1_25 |

| Address | Access | Meaning |
|---|---|---|
| `0x1510` | R/W | DO0 Active/Pending state, 0/1 |
| `0x1511` | R/W | DO1 Active/Pending state, 0/1 |
| `0x1512` | R/W | DO2 Active/Pending state, 0/1 |
| `0x1513` | R/W | DO3 Active/Pending state, 0/1 |
| `0x1514` | W | DO Apply Key, write `0xA5A5` |
| `0x1515` | R | Successful effective Apply revision |
| `0x1516` | R | Pending channel mask, bits 0..3 |
| `0x1517` | R | Active output mask, bits 0..3 |
| `0x1518` | R | Status: 0 Ready, 1 Apply failed, 2 Rollback failed |
| `0x1519` | R | Last failed channel, 0..3 or `0xFFFF` |

FC03 reads Active states. FC10 may write all four states plus the Apply Key as
`[DO0, DO1, DO2, DO3, 0xA5A5]` beginning at `0x1510`. If a hardware write
fails, outputs already changed by that Apply are rolled back, Active state and
revision remain unchanged, and Pending is retained for retry.

## U5 eight-position DIP switch

U5 is captured through the FLEXCOMM8 SPI parallel-in/serial-out interface every
10 ms. Switch polarity and optional bit reversal are configured in
`ProductDipSwitchConfig.h`. The default mapping is switch 1 to bit 0 through
switch 8 to bit 7; an ON switch is active-low. All registers are read-only.

| Address | Access | Meaning |
|---|---|---|
| `0x1520..0x1527` | R | U5 switch 1..8 logical state, 0/1 |
| `0x1528` | R | Logical ON mask, bits 0..7 = switch 1..8 |
| `0x1529` | R | Raw byte received from FLEXCOMM8 SPI |
| `0x152A` | R | Revision, increments on first valid sample and value changes |
| `0x152B` | R | Status: 0 Ready, 1 SPI I/O error, 2 not initialized |

If an SPI read fails, the last valid logical and raw values are retained and
Status changes to 1 so communication remains available for diagnostics.

## Low-voltage detector

The external `LV` signal is read from PIO1_31 and routed to PINT0. The default
configuration treats a high level as low voltage. Three consecutive 1 ms
samples assert the condition; 100 consecutive inactive samples release the
live condition. Assertion raises latched fault `0x0202` and applies a global
PWM safety inhibit. Clearing the fault while LV remains active causes it to be
raised again on the next application cycle.

| Address | Access | Meaning |
|---|---|---|
| `0x1530` | R | Raw active state, 0/1 |
| `0x1531` | R | Debounced active state, 0/1 |
| `0x1532` | R | Latched fault active, 0/1 |
| `0x1533` | R | Debounced-state revision |
| `0x1534..0x1535` | R | PINT rising-edge count, unsigned 32-bit |
| `0x1536` | R | Status: 0 Ready, 2 not initialized |

The low-voltage and MCU-overtemperature inhibits use independent source bits,
so clearing one condition cannot release an output while the other remains
active.

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
