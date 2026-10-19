#ifndef FW_UPDATE_APP_H
#define FW_UPDATE_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "boot_request.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_UPDATE_APP_ID_ADDRESS 0x7000u
#define FW_UPDATE_APP_ID_REGISTERS 10u
#define FW_UPDATE_APP_STATUS_ADDRESS 0x7051u
#define FW_UPDATE_APP_BAUD_ADDRESS 0x7052u
#define FW_UPDATE_APP_ROUTE_USB (1u << BOOT_REQUEST_USB)
#define FW_UPDATE_APP_ROUTE_RS485 (1u << BOOT_REQUEST_RS485)

typedef enum {
    FW_UPDATE_APP_IDLE = 0,
    FW_UPDATE_APP_QUIESCE = 1,
    FW_UPDATE_APP_WAIT_REPLY = 2,
    FW_UPDATE_APP_RESET = 3,
    FW_UPDATE_APP_FAILED = 4
} FwUpdateAppState_t;

typedef struct {
    uint16_t identity[FW_UPDATE_APP_ID_REGISTERS];
    uint16_t allowed_routes;
    uint32_t reply_delay_ms;
} FwUpdateAppConfig_t;

typedef struct {
    void *user;
    /* Foreground only. Must synchronously stop outputs; false latches failure. */
    bool (*quiesce)(void *user);
    /* Commit the mailbox, detach USB when needed, then reset. Must not return. */
    void (*reset)(void *user, uint16_t route, uint16_t unit, uint32_t baud);
} FwUpdateAppPlatform_t;

/* Caller-owned storage, one instance per device, shared by its USB/RS485 ports.
 * Call all APIs serially from foreground, never concurrently from an ISR.
 * Config/platform must remain valid and immutable for the instance lifetime. */
typedef struct {
    const FwUpdateAppConfig_t *config;
    const FwUpdateAppPlatform_t *platform;
    uint16_t context_route, context_unit, route, unit;
    FwUpdateAppState_t state;
    uint32_t baud, started;
    bool reply_sent, clock_started;
} FwUpdateApp_t;

bool FwUpdateApp_Init(FwUpdateApp_t *self, const FwUpdateAppConfig_t *config,
                      const FwUpdateAppPlatform_t *platform);
/* Use route 0 for broadcasts and clear context after every request callback. */
void FwUpdateApp_SetRequestContext(FwUpdateApp_t *self, uint16_t route, uint16_t unit);
/* Return Modbus exception 0/2/3/6. Write handles FC06 single-register writes only.
 * Product adapter must reject bulk writes to the control window. */
uint8_t FwUpdateApp_Read(const FwUpdateApp_t *self, uint16_t address,
                         uint16_t count, uint16_t *values);
uint8_t FwUpdateApp_Write(FwUpdateApp_t *self, uint16_t address, uint16_t value);
bool FwUpdateApp_IsPending(const FwUpdateApp_t *self);
/* Notify only after the command response physically completes on its route. */
void FwUpdateApp_ReplySent(FwUpdateApp_t *self, uint16_t route);
void FwUpdateApp_Process(FwUpdateApp_t *self, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
