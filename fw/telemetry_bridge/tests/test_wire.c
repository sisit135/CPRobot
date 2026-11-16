// SPDX-License-Identifier: GPL-3.0-only

#include "wire.h"
#include "telemetry_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    axi_ekf_log log={.boot_id=77,.monotonic_us=123456789,.sequence=9,
      .x_m=0.25f,.y_m=0.45f,.yaw_rad=1.5f,.gyro_bias_rps=-0.02f,
      .sigma_x_m=0.01f,.sigma_y_m=0.02f,.sigma_yaw_rad=0.03f,
      .grid_x=-1,.grid_y=2,.tracking=1,.grid_confident=0,.queue_dropped=3};
    uint8_t proto[AXI_MAX_PROTO], cell[AXI_SPI_SIZE];
    size_t n=axi_ekf_encode(&log,proto,sizeof proto); assert(n>0);
    assert(axi_spi_pack(proto,n,cell));
    const uint8_t *p=NULL; size_t len=0;
    assert(axi_spi_unpack(cell,&p,&len) && len==n && memcmp(p,proto,n)==0);
    axi_control_pack(cell+246,AXI_SPI_ACK_MAGIC1,123,2,0);
    uint32_t id=0; uint8_t op=0,status=1;
    assert(axi_control_unpack(cell+246,AXI_SPI_ACK_MAGIC1,&id,&op,&status));
    assert(id==123 && op==2 && status==0);
    assert(axi_spi_unpack(cell,&p,&len));
    cell[8]^=1; assert(!axi_spi_unpack(cell,&p,&len));
    printf("%zu ",n);
    for(size_t i=0;i<n;++i) printf("%02x",proto[i]);
    putchar('\n');
    return 0;
}
