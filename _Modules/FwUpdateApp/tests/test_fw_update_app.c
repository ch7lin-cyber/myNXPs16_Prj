#include "../include/fw_update_app.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    unsigned stops, resets;
    bool safe;
    uint16_t route, unit;
    uint32_t baud;
} Board;

static bool Stop(void *user) { Board *b = user; ++b->stops; return b->safe; }
static void Reset(void *user, uint16_t route, uint16_t unit, uint32_t baud)
{
    Board *b = user;
    ++b->resets; b->route = route; b->unit = unit; b->baud = baud;
}

int main(void)
{
    const FwUpdateAppConfig_t config = {
        .identity = {0xA016, 1, 1, 1, 0xC200, 8, 0, 1, 0, 1},
        .allowed_routes = FW_UPDATE_APP_ROUTE_USB | FW_UPDATE_APP_ROUTE_RS485,
        .reply_delay_ms = 200
    };
    static const uint32_t rates[] = {38400, 57600, 115200, 230400, 460800, 921600};
    Board b = {.safe = true};
    const FwUpdateAppPlatform_t platform = {.user = &b, .quiesce = Stop, .reset = Reset};
    FwUpdateApp_t app;
    uint16_t values[10];
    for (unsigned i = 0; i < 6; ++i) {
        b = (Board){.safe = true};
        assert(FwUpdateApp_Init(&app, &config, &platform));
        assert(!FwUpdateApp_Read(&app, 0x7000, 10, values));
        assert(values[0] == 0xA016 && values[9] == 1);
        assert(FwUpdateApp_Read(&app, 0xFFFF, 2, values) == 2);
        assert(FwUpdateApp_Read(&app, 0x7000, 0, values) == 3);
        assert(FwUpdateApp_Read(&app, 0x7000, 1, 0) == 3);
        FwUpdateApp_SetRequestContext(&app, 2, 247);
        assert(FwUpdateApp_Write(&app, 0x7052, 6) == 3);
        assert(!FwUpdateApp_Write(&app, 0x7052, (uint16_t)i));
        assert(!FwUpdateApp_Write(&app, 0x7050, 0xB008));
        assert(FwUpdateApp_Write(&app, 0x7050, 0xB008) == 6);
        assert(FwUpdateApp_IsPending(&app));
        FwUpdateApp_Process(&app, 0xFFFFFF80u);
        assert(b.stops == 1 && !b.resets);
        FwUpdateApp_Process(&app, 1000); /* no ACK, no reset */
        assert(!b.resets);
        FwUpdateApp_ReplySent(&app, 1); /* wrong transport cannot release reset */
        FwUpdateApp_Process(&app, 1001);
        assert(!b.resets);
        FwUpdateApp_ReplySent(&app, 2);
        FwUpdateApp_Process(&app, 1002);
        assert(b.resets == 1 && b.route == 2 && b.unit == 247 && b.baud == rates[i]);
        FwUpdateApp_Process(&app, 2000); /* returning reset callback is latched */
        assert(b.resets == 1);
        assert(!FwUpdateApp_Read(&app, 0x7051, 3, values));
        assert(values[0] == 4 && values[1] == 2 && values[2] == 247);
    }

    /* Re-init restores default baud; independent instances/configs do not leak. */
    FwUpdateApp_t other;
    FwUpdateAppConfig_t usb = config;
    usb.allowed_routes = FW_UPDATE_APP_ROUTE_USB;
    usb.reply_delay_ms = 500;
    usb.identity[9] = 42;
    assert(FwUpdateApp_Init(&app, &config, &platform));
    assert(FwUpdateApp_Init(&other, &usb, &platform));
    assert(!FwUpdateApp_Read(&other, 0x7009, 1, values) && values[0] == 42);
    FwUpdateApp_SetRequestContext(&other, 2, 2);
    assert(FwUpdateApp_Write(&other, 0x7052, 5) == 3);
    assert(FwUpdateApp_Write(&other, 0x7050, 0xB008) == 3);
    FwUpdateApp_SetRequestContext(&other, 0, 2);
    assert(FwUpdateApp_Write(&other, 0x7050, 0xB007) == 3);
    FwUpdateApp_SetRequestContext(&other, 1, 0);
    assert(FwUpdateApp_Write(&other, 0x7050, 0xB007) == 3);
    FwUpdateApp_SetRequestContext(&other, 1, 248);
    assert(FwUpdateApp_Write(&other, 0x7050, 0xB007) == 3);
    FwUpdateApp_SetRequestContext(&other, 1, 2);
    assert(!FwUpdateApp_Write(&other, 0x7050, 0xB007));
    assert(!FwUpdateApp_IsPending(&app));
    b = (Board){.safe = true};
    FwUpdateApp_ReplySent(&other, 1);
    FwUpdateApp_Process(&other, 10);
    FwUpdateApp_Process(&other, 509);
    assert(b.stops == 1 && !b.resets);
    FwUpdateApp_Process(&other, 510);
    assert(b.resets == 1 && b.route == 1 && b.baud == 115200);

    assert(!FwUpdateApp_Init(&app, 0, &platform));
    assert(FwUpdateApp_IsPending(&app));
    assert(FwUpdateApp_Write(&app, 0x7050, 0xB007) == 3);
    FwUpdateAppPlatform_t missing = platform;
    missing.quiesce = 0;
    assert(!FwUpdateApp_Init(&app, &config, &missing));
    FwUpdateApp_Process(&app, 1000);
    assert(b.resets == 1);
    b = (Board){.safe = false};
    assert(FwUpdateApp_Init(&app, &config, &platform));
    FwUpdateApp_SetRequestContext(&app, 1, 2);
    assert(!FwUpdateApp_Write(&app, 0x7050, 0xB007));
    FwUpdateApp_ReplySent(&app, 1);
    FwUpdateApp_Process(&app, 0);
    FwUpdateApp_Process(&app, 1000);
    assert(b.stops == 1 && !b.resets && app.state == FW_UPDATE_APP_FAILED);
    puts("Reusable library: six baud rates, USB, config isolation, ACK, wrap, failure guards PASS");
    return 0;
}
