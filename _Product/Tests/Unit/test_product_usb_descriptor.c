/* Exercise the descriptors Windows receives, using the actual SDK definitions. */
#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_cdc_acm.h"
#include "usb_device_descriptor.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    usb_device_get_device_descriptor_struct_t d={0};
    assert(USB_DeviceGetDeviceDescriptor(NULL,&d)==kStatus_USB_Success);
    assert(d.length==18 && d.buffer[0]==18 && d.buffer[1]==1);
    assert(d.buffer[4]==2 && d.buffer[5]==2 && d.buffer[6]==0);
    assert(d.buffer[7]==64 && d.buffer[17]==1);
    usb_device_get_configuration_descriptor_struct_t c={0};
    assert(USB_DeviceGetConfigurationDescriptor(NULL,&c)==kStatus_USB_Success);
    assert(c.length==(unsigned)(c.buffer[2]|(c.buffer[3]<<8)));
    assert(c.buffer[4]==2 && c.buffer[5]==1 && c.buffer[7]==(0x80 | (USB_DEVICE_CONFIG_SELF_POWER << 6)));
    unsigned interfaces=0,notify=0,bulk_in=0,bulk_out=0,offset=0;
    while(offset<c.length) {
        const uint8_t *p=c.buffer+offset;
        assert(offset+2<=c.length && p[0]>=2 && offset+p[0]<=c.length);
        if(p[1]==4) {
            assert(p[0]==9 && p[3]==0);
            if(interfaces==0)assert(p[2]==0 && p[4]==1 && p[5]==2 && p[6]==2);
            else assert(p[2]==1 && p[4]==2 && p[5]==10);
            interfaces++;
        } else if(p[1]==5) {
            assert(p[0]==7);
            if(p[3]==3) { assert((p[2]&0x80)!=0);notify++; }
            else { assert(p[3]==2 && p[4]==64 && p[5]==0);if(p[2]&0x80)bulk_in++;else bulk_out++; }
        }
        offset+=p[0];
    }
    assert(offset==c.length && interfaces==2 && notify==1 && bulk_in==1 && bulk_out==1);
    assert(USB_DeviceSetSpeed(NULL,USB_SPEED_FULL)==kStatus_USB_Success);
    c.configuration=255;assert(USB_DeviceGetConfigurationDescriptor(NULL,&c)==kStatus_USB_InvalidRequest);
    puts("USB CDC 02/02 driver match, configuration length, interfaces and FS endpoints passed");
    return 0;
}
