#include "nvs_api.h"
#include "hal_flash.h"
#include "crc32.h" 

// 把你的底层函数，打包成 NVS 需要的接口
const NvsPort_t stm32_nvs_port = {
    .flash_read         = hal_flash_read,
    .flash_write        = hal_flash_write,
    .flash_erase_sector = hal_flash_erase,
    .crc32              = crc32_update,
};