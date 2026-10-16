# Product watchdog validation

WWDT starts during `BOARD_InitBootPeripherals()` in reset mode. Window mode,
warning interrupts and the shared WDT/BOD interrupt are disabled. After all
product initialization and SysTick setup succeed, the product watchdog driver
changes the runtime timeout to 2 seconds using the actual WDT input clock.

Only a completed foreground-loop pass feeds WWDT, and no more than once every
100 ms of SysTick progress. Interrupt handlers, boot halt paths and application
services do not feed it. A stalled foreground loop or stalled SysTick therefore
leads to a watchdog reset.

Run the host regression test with:

```text
python3 _Product/Tests/run_watchdog_tests.py
```

The host test covers startup guards, runtime timing, frozen ticks, delayed
foreground execution and unsigned tick rollover. Hardware reset behavior still
requires target validation.

## LPC55S16 target validation

1. Clean and rebuild in MCUXpresso so the new L01 source is included.
2. Confirm `g_productBootStage == 9` and `g_productBootError == 0`.
3. Run for at least 60 seconds. `g_productWatchdogFeedCount` should advance at
   about ten counts per second while normal Modbus and I/O continue operating.
4. In a temporary test build, block the foreground before
   `ProductWatchdogDriver_Process()` while leaving SysTick enabled. Confirm a
   reset after about 2 seconds, then restore the test change.
5. Repeat with SysTick stopped while the foreground continues. The watchdog
   must not be fed and must reset the MCU.

Debugger halt behavior can depend on probe settings, so perform timeout tests
with running firmware rather than relying on a breakpoint pause.

## Config Tools regeneration rule

Keep WWDT in reset mode with windowing disabled, warning value zero and IRQ
initialization disabled. Generate both `board/peripherals.c` and
`board/peripherals.h`; never generate either file into the unit-test mocks.
