# Internal CAN first implementation

## Scope

This increment brings up the LPC55S16 MCAN controller and a non-blocking
Classic CAN frame transport so the application can initialize, queue frames,
receive frames, and inspect basic link diagnostics. It is the transport layer
only; CANopen NMT, Heartbeat, PDO, SDO, EMCY, node IDs, and the system data map
are still pending.

## Current configuration

- CAN instance: `CAN0` / MCAN.
- Nominal bitrate default: 500 kbit/s, configurable in
  `ProductCanConfig.h`; every node on the bus must use the same bitrate.
- MCAN source clock: 15 MHz from `BOARD_BootClockPLL150M`.
- Frame format: Classic CAN, standard 11-bit identifiers, data frames, 0–8 data
  bytes; CAN FD, extended identifiers, and remote frames are rejected.
- RX hardware FIFO: 16 entries; application RX queue: 16 entries.
- Application TX queue: 8 entries, with one hardware transmit buffer.
- Diagnostics include queued/completed TX, queue overflow, RX count/drop count,
  TEC/REC, controller error state, protocol-error events, warning/passive
  events, bus-off events/recoveries, in-flight TX, and FIFO loss.

## Message RAM and pin setup

`BOARD_InitBootPins()` already configures the CAN0 TX/RX pins, and the board
clock setup already attaches the 15 MHz MCAN clock. The MCAN message RAM reserves
`0x04000000..0x04000FFF` in SRAMX. The project memory map starts normal SRAMX
allocations at `0x04001000`; the 64-byte boot request mailbox remains at
`0x04003FC0..0x04003FFF`.

## API flow

- `ProductCanDriver_Init()` configures timing, message RAM and a standard-ID
  receive filter.
- `ProductCanDriver_Send()` copies a frame into the bounded TX queue. It does
  not block for bus arbitration or an ACK.
- `ProductCanDriver_Process()` drains the hardware RX FIFO and submits queued
  TX frames from foreground context.
- `ProductCanDriver_Receive()` pops one received frame.
- `ProductCanDriver_GetDiagnostics()` returns counters and current MCAN error
  state.

## Basic error handling

`ProductCanDriver_Process()` polls MCAN status on every foreground pass. It
records error-active/warning/passive/bus-off state, TEC/REC, the most recent
protocol error, and event counters. RX FIFO overflow is kept as a sticky
diagnostic until the next driver initialization. TX and RX software queue
overflow are reported by their existing counters and API return values.

While the controller reports bus-off, the driver does not submit another
queued frame to the hardware. The bounded software queue remains available;
frames are submitted again after the controller leaves bus-off. The main loop
does not wait for recovery. Application code can poll
`ProductCanDriver_GetDiagnostics()` and decide whether to alarm, retry a data
sample, or log the fault. Physical bus-off recovery behavior still needs
verification with two CAN nodes and a CAN analyzer.

## Validation status

Source integration is complete. Hardware validation still needs two CAN nodes,
compatible transceivers, proper bus termination, a shared bitrate, and a known
CAN analyzer or peer. The single-node EVK boot can confirm initialization but
cannot prove bus communication or ACK behavior. CANopen and the previously
discussed 450-PDO schedule are outside this initial increment.
