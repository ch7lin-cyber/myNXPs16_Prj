#include "product_fw_update.h"
#include "../../../_Modules/PlatformCore/FwUpdateApp/include/fw_update_app.h"
#include "ProductFwUpdateConfig.h"

static FwUpdateApp_t update;
static const FwUpdateAppConfig_t config = {
    .identity = PRODUCT_FW_UPDATE_IDENTITY,
    .allowed_routes = PRODUCT_FW_UPDATE_ALLOWED_ROUTES,
    .reply_delay_ms = PRODUCT_FW_UPDATE_REPLY_DELAY_MS
};

static bool Quiesce(void *user)
{
    (void)user;
    return ProductFwUpdate_Quiesce();
}

static void Reset(void *user, uint16_t route, uint16_t unit, uint32_t baud)
{
    (void)user;
    ProductFwUpdate_Reset(route, unit, baud);
}

static const FwUpdateAppPlatform_t platform = { .user = 0, .quiesce = Quiesce, .reset = Reset };

void ProductFwUpdate_Init(void) { (void)FwUpdateApp_Init(&update, &config, &platform); }
void ProductFwUpdate_SetRequestContext(uint16_t route, uint16_t unit)
{ FwUpdateApp_SetRequestContext(&update, route, unit); }
uint8_t ProductFwUpdate_Read(uint16_t address, uint16_t count, uint16_t *values)
{ return FwUpdateApp_Read(&update, address, count, values); }
uint8_t ProductFwUpdate_Write(uint16_t address, uint16_t value)
{ return FwUpdateApp_Write(&update, address, value); }
bool ProductFwUpdate_IsPending(void) { return FwUpdateApp_IsPending(&update); }
void ProductFwUpdate_ReplySent(uint16_t route) { FwUpdateApp_ReplySent(&update, route); }
void ProductFwUpdate_Process(uint32_t now_ms) { FwUpdateApp_Process(&update, now_ms); }

