# DTX watchdog validation

The MEX and board initialization start WWDT in reset mode with an 8-second
startup timeout (1 MHz WDT input, fixed /4 counter, TC=2000000). Window mode
and warning interrupts are disabled. After successful startup the product
driver switches to a 2-second timeout using the actual WDT input clock.

Only the completed foreground loop feeds WWDT, at most once every 100 ms of
SysTick progress. Interrupt handlers, startup halt paths and application
functions do not feed it. This detects a stalled foreground or stalled timebase;
it does not prove that every sensor or communication transaction is successful.

Run `python3 _Product/Tests/run_watchdog_tests.py` for host tests of the production
driver's timing, rollover, frozen tick and invalid configuration handling.

On LPC55S16 hardware:

1. Clean/rebuild in MCUXpresso so the new L01 source is included. Confirm boot
   reaches `g_productBootStage=9` and `g_productBootError=0`.
2. Run for at least 60 seconds. Confirm `g_productWatchdogFeedCount` advances
   about ten times per second and normal I/O and Modbus continue operating.
3. In a temporary test build, enter an infinite foreground loop before the
   watchdog process call while leaving SysTick enabled. Verify reset in roughly
   2 seconds. Repeat with SysTick disabled while foreground continues.
4. In a temporary test build, halt initialization after board peripherals start.
   Verify reset in roughly 8 seconds. Restore test changes after verification.

Use running firmware for timeout tests: debugger halt behavior depends on the
probe/device settings. Hardware timing and reset recovery require board testing;
host tests and native SDK syntax checks do not validate ARM linking or reset.

Regeneration: keep MEX WWDT timeout at 8000000 us, warning=0, window disabled,
and interrupt disabled. Regenerate both C and H files together; the board headers
in this revision also repair missing MAO_SEL and station-input definitions.
