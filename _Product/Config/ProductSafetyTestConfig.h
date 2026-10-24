#ifndef PRODUCT_SAFETY_TEST_CONFIG_H
#define PRODUCT_SAFETY_TEST_CONFIG_H

/* Explicit opt-in: only the standalone EVK SafetyTest configuration sets 1. */
#ifndef PRODUCT_SAFETY_TEST_EVK
#define PRODUCT_SAFETY_TEST_EVK 0
#endif

#if PRODUCT_SAFETY_TEST_EVK
#if !defined(PRODUCT_SAFETY_TEST_BUILD) || !defined(PRODUCT_SAFETY_TEST_TARGET)
#error "EVK mode requires both SafetyTest build and target flags"
#elif PRODUCT_SAFETY_TEST_BUILD != 1 || PRODUCT_SAFETY_TEST_TARGET != 1
#error "EVK mode requires both SafetyTest flags equal to 1"
#endif
#endif

#endif
