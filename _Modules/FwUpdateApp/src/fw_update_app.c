#include "../include/fw_update_app.h"

static bool Ready(const FwUpdateApp_t *self)
{
    return self && self->config && self->platform;
}

bool FwUpdateApp_Init(FwUpdateApp_t *self, const FwUpdateAppConfig_t *config,
                      const FwUpdateAppPlatform_t *platform)
{
    if (!self) return false;
    *self = (FwUpdateApp_t){0};
    if (!config || !platform || !platform->quiesce || !platform->reset ||
        !config->allowed_routes ||
        (config->allowed_routes & ~(FW_UPDATE_APP_ROUTE_USB | FW_UPDATE_APP_ROUTE_RS485)) ||
        config->reply_delay_ms >= 0x80000000u) {
        self->state = FW_UPDATE_APP_FAILED;
        return false;
    }
    self->config = config;
    self->platform = platform;
    self->baud = 115200u;
    return true;
}

void FwUpdateApp_SetRequestContext(FwUpdateApp_t *self, uint16_t route, uint16_t unit)
{
    if (!Ready(self)) return;
    self->context_route = route;
    self->context_unit = unit;
}

uint8_t FwUpdateApp_Read(const FwUpdateApp_t *self, uint16_t address,
                         uint16_t count, uint16_t *values)
{
    if (!Ready(self) || !values || !count) return 3;
    if (address >= FW_UPDATE_APP_ID_ADDRESS &&
        (uint32_t)address + count <= FW_UPDATE_APP_ID_ADDRESS + FW_UPDATE_APP_ID_REGISTERS) {
        for (unsigned i = 0; i < count; ++i)
            values[i] = self->config->identity[address - FW_UPDATE_APP_ID_ADDRESS + i];
        return 0;
    }
    if (address == FW_UPDATE_APP_STATUS_ADDRESS && count == 3u) {
        values[0] = (uint16_t)self->state;
        values[1] = self->route;
        values[2] = self->unit;
        return 0;
    }
    return 2;
}

uint8_t FwUpdateApp_Write(FwUpdateApp_t *self, uint16_t address, uint16_t value)
{
    if (!Ready(self)) return 3;
    if (address == FW_UPDATE_APP_BAUD_ADDRESS) {
        static const uint32_t rates[] = {38400u, 57600u, 115200u, 230400u, 460800u, 921600u};
        if (!(self->config->allowed_routes & FW_UPDATE_APP_ROUTE_RS485) ||
            self->context_route != BOOT_REQUEST_RS485 ||
            self->context_unit < 1u || self->context_unit > 247u ||
            value >= sizeof rates / sizeof rates[0] || self->state) return 3;
        self->baud = rates[value];
        return 0;
    }
    if (address != BOOT_REQUEST_REGISTER) return 2;
    uint16_t requested = value == BOOT_REQUEST_USB_KEY ? BOOT_REQUEST_USB :
                         value == BOOT_REQUEST_RS485_KEY ? BOOT_REQUEST_RS485 : 0;
    if (!requested || self->context_route != requested ||
        !(self->config->allowed_routes & (1u << requested)) ||
        self->context_unit < 1u || self->context_unit > 247u) return 3;
    if (self->state) return 6;
    if (requested == BOOT_REQUEST_USB) self->baud = 115200u;
    self->route = requested;
    self->unit = self->context_unit;
    self->state = FW_UPDATE_APP_QUIESCE;
    return 0;
}

bool FwUpdateApp_IsPending(const FwUpdateApp_t *self)
{
    return self && self->state != FW_UPDATE_APP_IDLE;
}

void FwUpdateApp_ReplySent(FwUpdateApp_t *self, uint16_t route)
{
    if (Ready(self) && self->state && route == self->route) self->reply_sent = true;
}

void FwUpdateApp_Process(FwUpdateApp_t *self, uint32_t now_ms)
{
    if (!Ready(self) || self->state == FW_UPDATE_APP_IDLE || self->state == FW_UPDATE_APP_FAILED) return;
    if (!self->clock_started) {
        self->started = now_ms;
        self->clock_started = true;
    }
    if (self->state == FW_UPDATE_APP_QUIESCE) {
        if (!self->platform->quiesce(self->platform->user)) {
            self->state = FW_UPDATE_APP_FAILED;
            return;
        }
        self->state = FW_UPDATE_APP_WAIT_REPLY;
    }
    /* Keep the tested behavior: never reset without TX completion and delay. */
    if (self->reply_sent && (uint32_t)(now_ms - self->started) >= self->config->reply_delay_ms) {
        self->state = FW_UPDATE_APP_RESET;
        self->platform->reset(self->platform->user, self->route, self->unit, self->baud);
        self->state = FW_UPDATE_APP_FAILED;
    }
}
