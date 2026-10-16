#include <assert.h>
#include "product_dip_switch_driver.h"

int main(void)
{
    ProductDipSwitchSnapshot_t snapshot;
    ProductDipSwitchDriver_Initialize();
    assert(!ProductDipSwitchDriver_Process());
    assert(!ProductDipSwitchDriver_Process());
    assert(ProductDipSwitchDriver_GetSnapshot(&snapshot));
    assert(snapshot.status == PRODUCT_DIP_SWITCH_STATUS_NOT_INITIALIZED);
    assert(snapshot.revision == 0U);
    /* No SPI implementation is linked: absent hardware must never be called. */
    return 0;
}
