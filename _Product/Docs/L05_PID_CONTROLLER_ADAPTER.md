# L05 CKLIB Controller Adapter

## Status

L05 adapter and host unit test are present. This is not yet enabled in the product schedule or connected to a product setpoint/PWM path. Firmware linking and MCU behavior still require the target CKLIB binary.

## Mode mapping

| L05 mode | CKLIB factory/API | Notes |
|---|---|---|
| `PRODUCT_PID_CONTROLLER_MODE_AUTO_TUNE` | `ckCreateDtAutoTune()` plus a target PID controller | On completion, applies tuning parameters to the selected target controller and begins using that controller. |
| `PRODUCT_PID_CONTROLLER_MODE_ON_OFF_CONTROL` | `ckCreateDtOnOffController()` | Uses `DT_ON_OFF_PARAM_CONTROL_ACTION`. |
| `PRODUCT_PID_CONTROLLER_MODE_PID_CONTROL` | `ckCreateDtPidController()` | Uses `DT_PID_PARAM_CONTROL_ACTION`. |
| `PRODUCT_PID_CONTROLLER_MODE_PID_FB_CONTROL` | `ckCreateDtPidFbController()` | Uses the PID parameter interface for its control action and configuration. |

## L05 connection contract

`ProductPidControllerAdapter_Init()` creates the selected CKLIB instance and accepts a sample-provider callback. The provider supplies the input pins `enable`, `reset`, `pv`, `sv`, `sample_sequence`, and `valid`; `ProductPidControllerAdapter_Process()` has the same `(void *context, uint32_t elapsed_ms)` signature as a `ProductFbScheduleTask_t` callback.

`enable` is level-sensitive. On disable, the adapter invalidates the output and resets controller state once on the active-to-disabled transition. Re-enable waits for valid, fresh input and starts from reset state. AutoTune is reset on disable and starts a new tuning run when enabled again. `reset` is rising-edge triggered; it resets state and suppresses output for that scan. Hold `reset` high without repeatedly resetting the controller.

Keep the schedule callback registered while using this runtime pin. The scheduler's task `enabled` flag controls callback dispatch and cannot replace the FB's `enable` pin, because the adapter needs to observe the transition and reset its state.

CKLIB documents PV/SV in 0.1 °C. L03 temperature engineering values are in milli-degrees Celsius, so L05 must convert temperature values before returning them (divide by 100 with an agreed rounding rule). Reject non-temperature sensor types for this adapter unless a separate, explicit scale mapping is defined. CKLIB output values are returned unchanged in `CK_OUTPUT`; product code must interpret and limit them before sending anything to an output service.

General controller modes pass PV and SV to CKLIB and convert actual elapsed milliseconds to CKLIB's 10 ms step-time units by rounding to the nearest unit. AutoTune has no elapsed-time parameter, so its configured callback period and tolerance must match the fixed L05 schedule. It only accepts valid, sufficiently fresh samples. `max_sample_age_ms == 0` disables age checking.

Use `ProductPidControllerAdapter_GetControllerHandle()` and, for AutoTune, `ProductPidControllerAdapter_GetAutoTuneHandle()` to set CKLIB-specific parameters through the public CKLIB setters. These handles are borrowed; the adapter frees them in `Deinit()`.

An invalid, unavailable, or stale sample deactivates and resets an active controller, clears the published output validity, and does not call CKLIB for that scan. The consumer must ignore output when `valid == false`. SafetyService and the existing output service remain responsible for the final output permission and hardware write.

## CKLIB include and link dependency

The checked-in CKLIB headers include dependencies using the `cklib/inc/...` path although the submodule stores the headers directly under `CKLIB/INC`. Thin L05 compatibility headers under `Source/L05_Application/cklib/inc` preserve that public header layout without editing the submodule.

The checked-in `CKLIB/Lib/libcklib.a` is currently an empty file. The adapter compiles and its host unit test links against test doubles, but a firmware build that uses this adapter needs the matching non-empty CKLIB archive and project linker configuration.

## Host unit test

Compile the adapter and its unit test with C99 warnings enabled. The test supplies CKLIB test doubles and covers all four factory paths, ENABLE/DISABLE and RESET transitions, elapsed-time conversion, stale-input rejection, AutoTune timing, and the handoff to PID after tuning.
