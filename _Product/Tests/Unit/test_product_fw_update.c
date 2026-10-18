#include "product_fw_update.h"
#include "boot_request.h"
#include <assert.h>
#include <stdio.h>
static unsigned stops,resets;static bool safe=true;
bool ProductFwUpdate_Quiesce(void){stops++;return safe;}
void ProductFwUpdate_Reset(uint16_t r,uint16_t u){assert(r==2 && u==9);resets++;}
int main(void){
 uint32_t request[16];BootRequest_Encode(request,2,9);assert(BootRequest_Valid(request));
 for(unsigned i=0;i<16;i++){request[i]^=1;assert(!BootRequest_Valid(request));request[i]^=1;}
 ProductFwUpdate_Init();uint16_t id[10];assert(!ProductFwUpdate_Read(0x7000,10,id) && id[0]==0xA016);
 assert(ProductFwUpdate_Read(0x7009,2,id)==2);
 assert(ProductFwUpdate_Write(0x7050,0xB008)==3); /* no transport / broadcast */
 ProductFwUpdate_SetRequestContext(1,9);assert(ProductFwUpdate_Write(0x7050,0xB008)==3);
 ProductFwUpdate_SetRequestContext(2,9);assert(!ProductFwUpdate_Write(0x7050,0xB008));
 assert(ProductFwUpdate_Write(0x7050,0xB008)==6);
 ProductFwUpdate_Process(0xFFFFFF80u);assert(stops==1 && resets==0);
 ProductFwUpdate_Process(1000);assert(resets==0); /* no reply, never reset */
 ProductFwUpdate_ReplySent(1);ProductFwUpdate_Process(1001);assert(resets==0);
 ProductFwUpdate_ReplySent(2);ProductFwUpdate_Process(1002);assert(resets==1);
 ProductFwUpdate_Init();stops=resets=0;ProductFwUpdate_SetRequestContext(2,9);
 assert(!ProductFwUpdate_Write(0x7050,0xB008));ProductFwUpdate_Process(10);
 ProductFwUpdate_ReplySent(2);ProductFwUpdate_Process(209);assert(resets==0);
 ProductFwUpdate_Process(210);assert(resets==1);
 ProductFwUpdate_Init();safe=false;resets=0;ProductFwUpdate_SetRequestContext(2,9);
 assert(!ProductFwUpdate_Write(0x7050,0xB008));ProductFwUpdate_ReplySent(2);ProductFwUpdate_Process(0);ProductFwUpdate_Process(999);assert(resets==0);
 puts("Update request CRC, routes, broadcast rejection, ACK completion, timing/wrap and fail-safe PASS");
}
