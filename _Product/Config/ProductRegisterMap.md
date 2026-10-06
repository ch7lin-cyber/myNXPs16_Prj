# Product Register Map

All addresses are Modbus holding-register addresses. Multi-register writes use
FC10; individual values and Apply Keys may use FC06. PWM period and duty writes
are staged as Pending values and do not affect hardware until the PWM Apply Key
is accepted.

## Register persistence metadata

`_Product/RegisterMap/register_map.csv` classifies persistence explicitly; it
must not be inferred from the Modbus address or R/W access. The metadata columns
have the following meaning:

| Column | Values / meaning |
|---|---|
| `RegisterRole` | `Monitor`, `Configuration`, `Command`, `Identification`, or `Reserved` |
| `Persistence` | `None` for runtime-only values, `NVM` for user configuration, or `Factory` for manufacturing-only data |
| `ConfigGroup` | Atomic validation/apply group owning the register |
| `ApplyPolicy` | `N/A`, `Immediate`, `ApplyKey`, or `Command` |
| `NvmFieldId` | Stable field identifier in the versioned persistent schema; blank when `Persistence=None` |
| `WriteRatePolicy` | `Never`, `OnApply`, `Delayed`, `Immediate`, or `OnFactoryCommit` |

The register image itself is never copied wholesale to FRAM. Persistent fields
are serialized through a versioned configuration schema. Pending Modbus writes
remain in RAM until the owning Apply Key succeeds; only then may an `OnApply`
field be committed. Monitor values, diagnostics, selectors, command keys, DAC
codes, and hardware input states remain runtime-only.

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
| `0x1500` | R | DI state mask, bits 0..3 = DI0..DI3 |
| `0x1501` | R | DI change revision |

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
| `0x1510` | R/W | DO state mask, bits 0..3 = DO0..DO3; read Active, write Pending |
| `0x1511` | W | DO Apply Key, write `0xA5A5` |
| `0x1512` | R | Successful effective Apply revision |
| `0x1513` | R | Pending channel mask; `0x000F` while a complete mask is staged |
| `0x1514` | R | Status: 0 Ready, 1 Apply failed, 2 Rollback failed |
| `0x1515` | R | Last failed channel, 0..3 or `0xFFFF` |

FC03 reads the Active state mask. FC10 may write the complete mask plus the
Apply Key as `[DO mask, 0xA5A5]` beginning at `0x1510`. Values with bits 4..15
set are rejected with Illegal Data Value. If a hardware write
fails, outputs already changed by that Apply are rolled back, Active state and
revision remain unchanged, and Pending is retained for retry.

## U5 eight-position DIP switch

U5 is captured through the FLEXCOMM8 SPI parallel-in/serial-out interface every
10 ms. Switch polarity and optional bit reversal are configured in
`ProductDipSwitchConfig.h`. The default mapping is switch 1 to bit 0 through
switch 8 to bit 7; an ON switch is active-low. All registers are read-only.

| Address | Access | Meaning |
|---|---|---|
| `0x1520` | R | Logical ON mask, bits 0..7 = switch 1..8 |
| `0x1521` | R | Raw byte received from FLEXCOMM8 SPI |
| `0x1522` | R | Revision, increments on first valid sample and value changes |
| `0x1523` | R | Status: 0 Ready, 1 SPI I/O error, 2 not initialized |

If an SPI read fails, the last valid logical and raw values are retained and
Status changes to 1 so communication remains available for diagnostics.

## Low-voltage detector

The external `LV` signal is read from PIO1_31 and routed to PINT0. The default
configuration treats a high level as low voltage. Three consecutive 1 ms
samples assert the condition; 100 consecutive inactive samples release the
live condition. Assertion raises latched fault `0x0202` and applies a global
PWM safety inhibit. A Reset request is rejected while LV remains active.

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

## Fault diagnostics and reset

Fault Reset is an FC10 command containing the Fault Code and key `0xC1EA`.
For faults associated with a Safety source, Reset is rejected until the live
physical condition has cleared. A successful command clears both the fault
record and its Safety latch. FC06 writes to either command register are not
accepted.

| Address | Access | Meaning |
|---|---|---|
| `0x4800` | R | Active fault count |
| `0x4801` | R/W | Active fault selection index |
| `0x4802` | R | Selected fault code |
| `0x4803` | R | Selected fault detail |
| `0x4804` | R | Selected fault configuration revision |
| `0x4805..0x4806` | R | Selected fault correlation event ID |
| `0x4807..0x4808` | R | Selected fault occurrence count |
| `0x4809` | W | Reset target fault code |
| `0x480A` | W | Reset key, `0xC1EA` |
| `0x480B` | R | Reset result |
| `0x480C` | R | Last Reset target fault code |
| `0x480D..0x480E` | R | Last Reset attempt timestamp in ms |

Reset result values are: 0 Ready, 1 Success, 2 Fault not active, 3 physical
condition still active, 4 invalid Fault Code, 5 internal failure, and 6 invalid
key. Rejected commands also return Modbus exception 03; an internal failure
returns exception 04.

## SystemRoutine diagnostic summary

All 32-bit values use high-word first. These registers are read-only.

| Address | Access | Meaning |
|---|---|---|
| `0x4810..0x4811` | R | Active or latched Warning source mask |
| `0x4812..0x4813` | R | Live Safety source mask |
| `0x4814..0x4815` | R | Latched Safety source mask |
| `0x4816..0x4817` | R | Aggregate Safety trip source mask |
| `0x4818` | R | Retained Runtime Event count, maximum 32 |
| `0x4819` | R | Retained Snapshot count, maximum 16 |
| `0x481A..0x481B` | R | Latest Runtime Event sequence |
| `0x481C..0x481D` | R | Latest Snapshot sequence |
| `0x481E` | R | Safety state: 0 Normal, 1 Tripped, 2 Action error |
| `0x481F` | R | Safety output inhibited, 0/1 |

## Runtime Event browser

Write the retained-event index to `0x4920`; index zero selects the oldest
retained event. Only the index is writable. An index outside the current count
returns Modbus exception 03.

| Address | Access | Meaning |
|---|---|---|
| `0x4920` | R/W | Selected retained-event index |
| `0x4921..0x4922` | R | Event sequence |
| `0x4923..0x4924` | R | Timestamp in ms |
| `0x4925` | R | Domain: 1 Warning, 2 Safety, 3 Fault, 4 System |
| `0x4926` | R | State: 1 Asserted, 2 Cleared, 3 Reset, 4 Action error |
| `0x4927` | R | Source or Fault code |
| `0x4928` | R | Detail |
| `0x4929` | R | Configuration revision |
| `0x492A..0x492B` | R | Correlation event ID |

## Snapshot browser

Write the retained-snapshot index to `0x4940`; index zero selects the oldest
retained Snapshot. Each diagnostic value is a signed 32-bit integer stored
high-word first.

| Address | Access | Meaning |
|---|---|---|
| `0x4940` | R/W | Selected retained-snapshot index |
| `0x4941..0x4942` | R | Snapshot sequence |
| `0x4943..0x4944` | R | Timestamp in ms |
| `0x4945` | R | Source: 1 Event, 2 Warning, 3 Safety, 4 Fault, 5 Application |
| `0x4946` | R | Source or Fault code |
| `0x4947` | R | Detail |
| `0x4948` | R | Configuration revision |
| `0x4949..0x494A` | R | Correlation event ID |
| `0x494B..0x495A` | R | Diagnostic values 0..7, signed 32-bit each |

## SW2 rotary switch

SW2 is sampled every 10 ms from the four GPIO inputs labelled `RR_SW_1`,
`RR_SW_2`, `RR_SW_4`, and `RR_SW_8`. The default board mapping is active-low,
so the four logical bits form the rotary position value 0..15. Polarity can be
changed through `ProductRotarySwitchConfig.h`. All registers are read-only.

| Address | Access | Meaning |
|---|---|---|
| `0x1540` | R | Logical rotary position, 0..15 |
| `0x1541` | R | Raw GPIO nibble, bits 0..3 = 1/2/4/8 inputs |
| `0x1542` | R | Revision, increments on first sample and position changes |
| `0x1543` | R | Status: 0 Ready, 2 not initialized |

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
degrees Celsius). Cooling does not clear the latched fault. Reset it through
`0x4809 = 0x0201`, `0x480A = 0xC1EA`; the command is rejected while the MCU
remains overtemperature.
