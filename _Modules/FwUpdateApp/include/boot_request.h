#ifndef NXP_BOOT_REQUEST_H
#define NXP_BOOT_REQUEST_H
#include <stdbool.h>
#include <stdint.h>
/* ABI v1. Both linker layouts exclude SRAMX's final 64 bytes. Soft reset only. */
#define BOOT_REQUEST_ADDRESS 0x04003FC0u
#define BOOT_REQUEST_BYTES 64u
#define BOOT_REQUEST_MAGIC 0x31515055u
#define BOOT_REQUEST_USB 1u
#define BOOT_REQUEST_RS485 2u
#define BOOT_REQUEST_RUN_APP 3u
#define BOOT_REQUEST_REGISTER 0x7050u
#define BOOT_REQUEST_USB_KEY 0xB007u
#define BOOT_REQUEST_RS485_KEY 0xB008u
/* Explicit word layout, zero all reserved words. CRC is accidental-corruption
 * protection, not authorization. Commit magic last and DSB before reset. */
static inline uint32_t BootRequest_Crc(const uint32_t *v) {
    uint32_t c=0xFFFFFFFFu;
    for(unsigned i=1;i<15;i++)for(unsigned b=0;b<4;b++) {
        c^=(v[i]>>(8*b))&255u;
        for(unsigned k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xEDB88320u);
    }
    return ~c;
}
static inline bool BootRequest_Valid(const uint32_t *v) {
    if(v[0]!=BOOT_REQUEST_MAGIC || v[1]!=1u || v[2]<1u || v[2]>3u ||
       v[3]<1u || v[3]>247u || v[15]!=BootRequest_Crc(v))return false;
    if(v[4]!=38400u && v[4]!=57600u && v[4]!=115200u && v[4]!=230400u &&
       v[4]!=460800u && v[4]!=921600u)return false;
    for(unsigned i=5;i<15;i++)if(v[i])return false;
    return true;
}
static inline void BootRequest_Encode(uint32_t *v,uint32_t mode,uint32_t unit,uint32_t baud) {
    for(unsigned i=0;i<16;i++)v[i]=0;
    v[1]=1;v[2]=mode;v[3]=unit;v[4]=baud;v[15]=BootRequest_Crc(v);v[0]=BOOT_REQUEST_MAGIC;
}
#endif
