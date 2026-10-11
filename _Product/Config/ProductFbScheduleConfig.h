#ifndef PRODUCT_FB_SCHEDULE_CONFIG_H
#define PRODUCT_FB_SCHEDULE_CONFIG_H

/* Product-owned recommendations, not L04 constants. Units: milliseconds.
 * All tasks are disabled placeholders. Supply L05 adapters only after the FB
 * and its tests are complete. No ISR, L03/L04/L05 data wiring is enabled here.
 * Mathematical blocks normally run inline with their consuming control chain;
 * their entries below are planning slots, not a requirement for separate tasks.
 * X(name, enabled, period_ms, callback, context) */
#define PRODUCT_FB_SCHEDULE_TASKS(X) \
    X(SetpointRamp, false, 20U, NULL, NULL) \
    X(ProfileSequencer, false, 20U, NULL, NULL) \
    X(InBandTimer, false, 20U, NULL, NULL) \
    X(Pid, false, 20U, NULL, NULL) \
    X(WeightedSum, false, 20U, NULL, NULL) \
    X(Divide, false, 20U, NULL, NULL) \
    X(Selector, false, 20U, NULL, NULL) \
    X(MovingAverage, false, 20U, NULL, NULL) \
    X(Linearization, false, 20U, NULL, NULL) \
    X(LeadLag, false, 20U, NULL, NULL) \
    X(DeadTime, false, 20U, NULL, NULL) \
    X(PowerVoltageCompensation, false, 20U, NULL, NULL) \
    X(CascadeCoordinator, false, 20U, NULL, NULL) \
    X(OutputLimiter, false, 20U, NULL, NULL) \
    X(HeaterResistance, false, 100U, NULL, NULL) \
    X(BatchMetrics, false, 20U, NULL, NULL) \
    X(ProcessResponse, false, 100U, NULL, NULL) \
    X(RecordTrigger, false, 100U, NULL, NULL)

#endif /* PRODUCT_FB_SCHEDULE_CONFIG_H */
