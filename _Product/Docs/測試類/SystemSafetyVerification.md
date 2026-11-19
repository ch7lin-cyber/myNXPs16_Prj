# System Safety Verification

> 2026-10-24：上板操作改用 [Python 自動安全測試](SAFETY_PYTHON_TEST.md)。下方 debugger call 操作保留為歷史說明，因暫停／連線與 Watchdog 問題，不再作為建議測試流程。

## Purpose

Verify the MCU overtemperature and low-voltage protection paths without
heating the MCU package or reducing the MCU board supply. Host tests validate
the software decisions; target-board tests validate that a safety trip reaches
the real output inhibit and diagnostic paths.

## MCU overtemperature

### Implemented behavior

- MCU temperature sample interval: 1,000 ms.
- Trip threshold: 8,500 centi-degrees Celsius (85.00 °C), inclusive.
- Confirmation: 3 consecutive samples at or above the threshold.
- Fault: `0x0201`.
- The live overtemperature condition clears after a below-threshold sample,
  while the safety latch and fault remain until an explicit reset.

### Verification method

1. Do not heat the MCU. Use the test-only API
   `ProductInternalAdcDriver_SetMcuTemperatureTestOverride()` at the internal
   ADC result boundary. Compile the dedicated target test build with both
   `PRODUCT_SAFETY_TEST_BUILD` and `PRODUCT_SAFETY_TEST_TARGET`; defining only
   `PRODUCT_SAFETY_TEST_BUILD` is rejected unless this is a Host test build.
   Never define either test macro in the production build.
2. Host-test the boundary values: two samples at 85.00 °C must not trip; the
   third consecutive sample at 85.00 °C must trip. A below-threshold sample
   between high samples must restart the confirmation count. Also test an
   invalid sample does not count as a valid overtemperature sample. Invalid
   samples leave the consecutive-valid-sample counter unchanged; a valid
   below-threshold sample resets it.
3. On the target test build, hold a synthetic value at 85.00 °C and allow the
   normal 1 s sampling schedule to process three samples. Confirm fault
   `0x0201`, Safety inhibit, Event/Snapshot records, and all configured safe
   output states. Repeat at 84.99 °C and confirm no trip.
4. Return the injected value below 85.00 °C. Confirm the live condition clears
   but the output remains inhibited until Fault Reset succeeds. Use the
   existing FC10 reset procedure documented in `ProductRegisterMap.md`.

### MCUXpresso target-test build setup

The project has Debug and Release configurations. Keep their compiler symbols
unchanged. For a board test, duplicate Debug as a separate `SafetyTest`
configuration and add these compiler symbols only there:

- `PRODUCT_SAFETY_TEST_BUILD=1`
- `PRODUCT_SAFETY_TEST_TARGET=1`

Build and flash `SafetyTest`. In the debugger, call
`ProductInternalAdcDriver_SetMcuTemperatureTestOverride(true, true, 8500)`
once before the regular 1 s sampling requests. Do not repeatedly call the
setter; the application scheduler must supply each sample request. Then inspect
the existing diagnostics for Fault `0x0201`, Safety inhibit, Event/Snapshot
records, and output safe states. Set 8499 and allow subsequent regular samples
to confirm no new overtemperature trip. Set the override to false when
finished, then verify Fault Reset behavior. Finally clean-build Release and
confirm its preprocessor symbols do not include either test symbol.

The test setter returns false before ADC driver initialization. Disabling the
override clears the ADC driver's sample/count state; it does not clear the
FaultService record or Safety latch. The explicit reset path remains required.

## Low-voltage detector

### Implemented behavior

- Input: active-high `PIO1_31`, routed to PINT0.
- Assert debounce: 3 consecutive 1 ms samples.
- Release debounce: 100 consecutive inactive samples.
- Fault: `0x0202`.
- A reset is rejected while the debounced input remains active.

### Verification method

1. The Host test already mocks `ProductLowVoltageDriver_IsActive()` and checks
   the 3-sample assert and 100-sample release behavior.
2. On the target board, simulate the detector output at `PIO1_31`; do not
   brown out or undervolt the MCU supply. Use a board-approved test point or
   fixture, verify the pin's I/O voltage first, and avoid driving against the
   external detector output.
3. Change the input from inactive to active and hold it for at least three
   safety samples. Confirm raw/debounced/fault diagnostics at `0x1530..0x1532`,
   fault `0x0202`, Event/Snapshot records, and the configured safe output
   states.
4. Return the input inactive and hold it for at least 100 safety samples
   (100 ms at the configured 1 ms interval). Confirm the live condition clears
   but the latch remains until Fault Reset succeeds.

## Target-board acceptance record

Record firmware commit, build type, injected values or pin levels, sample
timestamps, fault code/detail, Safety state, Event/Snapshot identifiers,
PWM/DO measured states, reset result, and recovery time for each test. A Host
test pass is not a substitute for this target-board record.

## Implementation status

The guarded injection API and ADC driver Host threshold test are implemented.
The full SystemRoutine-to-output-inhibit path still requires target-board
verification with a dedicated test build and must be followed by a production
build with both test macros absent.
