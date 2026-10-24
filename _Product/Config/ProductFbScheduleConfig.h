#ifndef PRODUCT_FB_SCHEDULE_CONFIG_H
#define PRODUCT_FB_SCHEDULE_CONFIG_H

#ifndef PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED false
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK NULL
#define PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CONTEXT NULL
#endif

/* Product-owned recommendations, not L04 constants. Units: milliseconds.
 * CH0 MovingAverage is enabled by the L05 integration table as a read-only
 * L03-to-L04 validation chain. Remaining tasks here are disabled placeholders.
 * CH0 PV processing and Alarm/Logic have separate ordered-chain entries in
 * the integration table. Processed PV is read-only, not yet a PID input.
 * No PWM/DAC or ISR is involved. Mathematical blocks normally run inline with
 * their consuming control chain; their entries are planning slots.
 * X(name, enabled, period_ms, callback, context) */
#define PRODUCT_FB_SCHEDULE_TASKS(X) \
    X(SetpointRamp, false, 20U, NULL, NULL) \
    X(ProfileSequencer, false, 20U, NULL, NULL) \
    X(InBandTimer, false, 20U, NULL, NULL) \
    X(Pid, false, 20U, NULL, NULL) \
    X(WeightedSum, false, 20U, NULL, NULL) \
    X(Divide, false, 20U, NULL, NULL) \
    X(Selector, false, 20U, NULL, NULL) \
    X(MovingAverage, PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_ENABLED, 20U, \
      PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CALLBACK, \
      PRODUCT_FB_SCHEDULE_CH0_MOVING_AVERAGE_CONTEXT) \
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
