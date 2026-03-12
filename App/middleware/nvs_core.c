#include "nvs_api.h"
#include <stddef.h> 
#include <string.h>

// 内部静态辅助函数

// 格式化一个扇区
static int nvs_format_sector(const NvsPort_t *port, uint32_t sector_addr, uint32_t generation)
{
    int ret;
    NvsSectorHeader_t hdr;

    ret = port->flash_erase_sector(sector_addr);
    if (ret != 0) return NVS_ERR_FLASH;

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = NVS_SECTOR_MAGIC;
    hdr.layout_ver = NVS_LAYOUT_VERSION;
    hdr.hdr_len = sizeof(NvsSectorHeader_t);
    hdr.generation = generation;
    
    hdr.hdr_crc32 = port->crc32(0xFFFFFFFF, &hdr, offsetof(NvsSectorHeader_t, hdr_crc32));

    ret = port->flash_write(sector_addr, &hdr, sizeof(hdr));
    if (ret != 0) return NVS_ERR_FLASH;

    return NVS_OK;
}

// 深度扫描单个物理扇区
static void nvs_scan_sector(const NvsPort_t *port, uint32_t sector_addr, NvsScanResult_t *result)
{
    NvsSectorHeader_t sec_hdr;
    NvsRecord_t record;
    uint32_t current_addr = sector_addr;
    
    memset(result, 0, sizeof(NvsScanResult_t));
    result->sector_addr = sector_addr;

    port->flash_read(current_addr, &sec_hdr, sizeof(sec_hdr));
    if (sec_hdr.magic != NVS_SECTOR_MAGIC) return;
    
    uint32_t calc_crc = port->crc32(0xFFFFFFFF, &sec_hdr, offsetof(NvsSectorHeader_t, hdr_crc32));
    if (calc_crc != sec_hdr.hdr_crc32) return;

    result->valid = 1;
    result->sector_generation = sec_hdr.generation;
    current_addr += sizeof(NvsSectorHeader_t);
    result->next_write_addr = current_addr; 

    while ((current_addr + sizeof(NvsRecord_t)) <= (sector_addr + NVS_SECTOR_SIZE)) 
    {
        port->flash_read(current_addr, &record, sizeof(record));

        // 1. 如果是完美的空白区 (全0xFF)，正常停止扫描，游标就停在这里
        if (record.hdr.magic == 0xFFFFFFFF && record.commit == 0xFFFFFFFF) {
            break; 
        }

        // 2. 如果遇到未完成的写入 (这就是病灶1的脏数据！)
        if (record.hdr.magic != NVS_RECORD_MAGIC || record.commit != NVS_COMMIT_DONE) {
            // 【核心自愈逻辑】：绝对不能在脏数据上覆盖！强行将游标推到扇区末尾标记为“已满”
            current_addr = sector_addr + NVS_SECTOR_SIZE; 
            break;
        }

        // 3. 校验 CRC (已修复 offsetof 致命 Bug)
        calc_crc = port->crc32(0xFFFFFFFF, &record, offsetof(NvsRecord_t, rec_crc32));
        if (calc_crc == record.rec_crc32) 
        {
            // 这是一条完美的记录，正常推进游标
            result->last_record_addr = current_addr;
            result->last_seq = record.hdr.seq;
            current_addr += sizeof(NvsRecord_t);
            result->next_write_addr = current_addr;
        } 
        else 
        {
            // CRC 错误同样是脏数据！强行标记为“已满”
            current_addr = sector_addr + NVS_SECTOR_SIZE; 
            break; 
        }
    }
    
    // 最终同步游标
    result->next_write_addr = current_addr;
}

// 对外暴露的业务 API
int nvs_init(NvsContext_t *ctx, const NvsPort_t *port)
{
    if (!ctx || !port) return NVS_ERR_PARAM;

    NvsScanResult_t res_a, res_b;
    NvsScanResult_t *active_res = NULL;
    NvsScanResult_t *standby_res = NULL;

    nvs_scan_sector(port, NVS_A_START_ADDR, &res_a);
    nvs_scan_sector(port, NVS_B_START_ADDR, &res_b);

    if (res_a.valid && res_b.valid) 
    {
        if ((int32_t)(res_a.sector_generation - res_b.sector_generation) > 0) {
            active_res = &res_a; standby_res = &res_b;
        } else {
            active_res = &res_b; standby_res = &res_a;
        }
    } 
    else if (res_a.valid) { active_res = &res_a; standby_res = &res_b; } 
    else if (res_b.valid) { active_res = &res_b; standby_res = &res_a; } 
    else 
    {
        int ret = nvs_format_sector(port, NVS_A_START_ADDR, 1);
        if (ret != NVS_OK) return ret;
        nvs_scan_sector(port, NVS_A_START_ADDR, &res_a);
        active_res = &res_a;
        standby_res = &res_b;
    }

    ctx->active_sector_addr  = active_res->sector_addr;
    ctx->standby_sector_addr = standby_res->sector_addr;
    ctx->active_generation   = active_res->sector_generation;
    ctx->next_write_addr     = active_res->next_write_addr;
    
    if (active_res->last_record_addr != 0) {
        ctx->next_seq = active_res->last_seq + 1;
    } else {
        ctx->next_seq = 1;
    }

    return NVS_OK;
}

int nvs_load(NvsContext_t *ctx, const NvsPort_t *port, SystemConfig_t *out_cfg)
{
    if (!ctx || !port || !out_cfg) return NVS_ERR_PARAM;

    uint32_t sector_start_addr = ctx->active_sector_addr + sizeof(NvsSectorHeader_t);

    if (ctx->next_write_addr <= sector_start_addr) {
        return NVS_ERR_NOT_FOUND;
    }

    // 只需要拿最后一条合法记录即可
    uint32_t read_addr = ctx->next_write_addr - sizeof(NvsRecord_t);
    NvsRecord_t record;

    port->flash_read(read_addr, &record, sizeof(record));

    if (record.hdr.magic == NVS_RECORD_MAGIC && record.commit == NVS_COMMIT_DONE)
    {
        uint32_t calc_crc = port->crc32(0xFFFFFFFF, &record, offsetof(NvsRecord_t, rec_crc32));
        if (calc_crc == record.rec_crc32)
        {
            *out_cfg = record.payload; // 直接结构体赋值
            return NVS_OK;
        }
    }

    return NVS_ERR_NOT_FOUND; 
}

int nvs_gc(NvsContext_t *ctx, const NvsPort_t *port)
{
    int ret;
    uint32_t new_generation = ctx->active_generation + 1;
    uint32_t new_active_addr = ctx->standby_sector_addr;
    
    SystemConfig_t latest_cfg;
    uint8_t has_valid_record = (nvs_load(ctx, port, &latest_cfg) == NVS_OK);

    // 1. 格式化备用扇区
    ret = nvs_format_sector(port, new_active_addr, new_generation);
    if (ret != NVS_OK) return ret;

    // 2. 切换上下文身份
    ctx->standby_sector_addr = ctx->active_sector_addr;
    ctx->active_sector_addr  = new_active_addr;
    ctx->active_generation   = new_generation;
    ctx->next_write_addr     = new_active_addr + sizeof(NvsSectorHeader_t);
    ctx->next_seq            = 1;

    // 3. 将刚读出来的最新配置，作为新扇区的第一条记录保存进去
    if (has_valid_record) {
        return nvs_save(ctx, port, &latest_cfg); 
    }

    return NVS_OK;
}

int nvs_save(NvsContext_t *ctx, const NvsPort_t *port, const SystemConfig_t *cfg)
{
    if (!ctx || !port || !cfg) return NVS_ERR_PARAM;
    int ret;

    // 1. 空间检查与 GC 触发
    if ((ctx->next_write_addr + sizeof(NvsRecord_t)) > (ctx->active_sector_addr + NVS_SECTOR_SIZE))
    {
        ret = nvs_gc(ctx, port);
        if (ret != NVS_OK) return ret;
        
        if ((ctx->next_write_addr + sizeof(NvsRecord_t)) > (ctx->active_sector_addr + NVS_SECTOR_SIZE)) {
            return NVS_ERR_NO_SPACE;
        }
    }

    // 2. 构造数据包 (严格按照 nvs_types.h 的定义)
    NvsRecord_t record;
    memset(&record, 0xFF, sizeof(NvsRecord_t)); 
    record.hdr.magic         = NVS_RECORD_MAGIC;
    record.hdr.rec_ver       = NVS_RECORD_VERSION;
    record.hdr.rec_len       = sizeof(NvsRecord_t);
    record.hdr.seq           = ctx->next_seq;
    record.hdr.payload_len   = sizeof(SystemConfig_t);
    
    record.payload           = *cfg; // 核心数据装载
    record.hdr.payload_crc32 = port->crc32(0xFFFFFFFF, &record.payload, sizeof(SystemConfig_t));

    // 1. 计算 CRC 
    record.rec_crc32 = port->crc32(0xFFFFFFFF, &record, offsetof(NvsRecord_t, rec_crc32));
    record.commit    = 0xFFFFFFFF; 

    // 2. Phase 1：极其精准地写入 commit 之前的所有数据
    ret = port->flash_write(ctx->next_write_addr, &record, offsetof(NvsRecord_t, commit));
    if (ret != 0) return NVS_ERR_FLASH;

    // 3. Phase 2：单独给 commit 字段打上终极钢印
    uint32_t commit_val = NVS_COMMIT_DONE;
    uint32_t commit_addr = ctx->next_write_addr + offsetof(NvsRecord_t, commit); 
    ret = port->flash_write(commit_addr, &commit_val, sizeof(commit_val));
    if (ret != 0) return NVS_ERR_FLASH;
    // 4. 更新内存游标
    ctx->next_write_addr += sizeof(NvsRecord_t);
    ctx->next_seq++;

    return NVS_OK;
}