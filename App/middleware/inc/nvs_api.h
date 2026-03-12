#ifndef NVS_API_H
#define NVS_API_H

#include <stdint.h>
#include "nvs_types.h" 

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    NVS_OK = 0,
    NVS_ERR_PARAM = -1,
    NVS_ERR_FLASH = -2,
    NVS_ERR_CRC = -3,
    NVS_ERR_NOT_FOUND = -4,
    NVS_ERR_NO_SPACE = -5  
} nvs_status_t;

// 平台适配：接到你现有 hal_flash
typedef struct
{
    int (*flash_read)(uint32_t addr, void *buf, uint32_t len);
    int (*flash_write)(uint32_t addr, const void *buf, uint32_t len);
    int (*flash_erase_sector)(uint32_t sector_base_addr);
    uint32_t (*crc32)(uint32_t crc, const void *data, uint32_t len);
} NvsPort_t;

// 初始化：扫描双扇区，恢复 active/next_seq/next_write 
int nvs_init(NvsContext_t *ctx, const NvsPort_t *port);

// 读取指定 Key 的数据
int nvs_load(NvsContext_t *ctx, const NvsPort_t *port, SystemConfig_t *out_cfg);

// 追加保存一条新配置（空间不足时内部自动触发 GC） 
int nvs_save(NvsContext_t *ctx, const NvsPort_t *port, const SystemConfig_t *cfg);

// 主动触发 GC（如果需要手动整理碎片时调用）
int nvs_gc(NvsContext_t *ctx, const NvsPort_t *port);

#ifdef __cplusplus
}
#endif

#endif // NVS_API_H