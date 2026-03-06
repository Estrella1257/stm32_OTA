#include "stm32f4xx.h"
#include "hal_flash.h"
#include <stdio.h>
#include <string.h>

static uint32_t GetSector(uint32_t address) {
    if ((address >= 0x08000000) && (address < 0x08004000)) return FLASH_Sector_0;    
    if ((address >= 0x08004000) && (address < 0x08008000)) return FLASH_Sector_1;    
    if ((address >= 0x08008000) && (address < 0x0800C000)) return FLASH_Sector_2;    
    if ((address >= 0x0800C000) && (address < 0x08010000)) return FLASH_Sector_3;    
    if ((address >= 0x08010000) && (address < 0x08020000)) return FLASH_Sector_4;    
    if ((address >= 0x08020000) && (address < 0x08040000)) return FLASH_Sector_5;    
    if ((address >= 0x08040000) && (address < 0x08060000)) return FLASH_Sector_6;    
    if ((address >= 0x08060000) && (address < 0x08080000)) return FLASH_Sector_7;    
    if ((address >= 0x08080000) && (address < 0x080A0000)) return FLASH_Sector_8;    
    if ((address >= 0x080A0000) && (address < 0x080C0000)) return FLASH_Sector_9;    
    if ((address >= 0x080C0000) && (address < 0x080E0000)) return FLASH_Sector_10;        
    if ((address >= 0x080E0000) && (address < 0x08100000)) return FLASH_Sector_11;    
    return 0xFFFFFFFF;
}

static int check_address_bounds(uint32_t address, uint32_t len) {
    uint32_t end_addr = address + len;
    
    // 1. 检查是否完全在 NVS 区 (Sector 4)
    if (address >= NVS_SECTOR_ADDR && end_addr <= (NVS_SECTOR_ADDR + NVS_SECTOR_SIZE)) return 1;
    
    // 2. 检查是否完全在 APP_A 区 (Sector 5, 6, 7)
    if (address >= APP_A_ADDR && end_addr <= (APP_A_ADDR + APP_PART_SIZE)) return 1;
    
    // 3. 检查是否完全在 APP_B 区 (Sector 8, 9, 10)
    if (address >= APP_B_ADDR && end_addr <= (APP_B_ADDR + APP_PART_SIZE)) return 1;

    // 4. (可选) 检查是否完全在 资源 区 (Sector 11)
    if (address >= 0x080E0000 && end_addr <= (0x080E0000 + RES_PART_SIZE)) return 1;

    // 任何越界、跨区访问、或者试图触碰 Bootloader 的行为，一律封杀
    return 0;
}

int hal_flash_init(void) {
    return HAL_OK;
}

int hal_flash_erase(uint32_t address) {
    // 1. 计算扇区索引
    uint32_t sector_ID = GetSector(address);
    if (sector_ID < FLASH_Sector_4 || sector_ID > FLASH_Sector_11) {
        printf("[flash] error: invalid erase address 0x%08lX (Protected Area)!\n", address);
        return HAL_ERR_PARAM;
    }

    int ret = HAL_OK;

    // 2. 临界区保护 (关中断)
    // 防止擦除期间中断向量表被访问(如果向量表在Flash中)或看门狗中断
    __disable_irq();

    // 3. 解锁并清理标志位
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_PGAERR | 
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR);

    // 4. 等待上一次操作 (防止状态机忙)
    if (FLASH_WaitForLastOperation() != FLASH_COMPLETE) {
        ret = HAL_ERR_FLASH;
        goto cleanup;
    }

    // 5. 执行扇区擦除
    if (FLASH_EraseSector(sector_ID, VoltageRange_3) != FLASH_COMPLETE) {
        printf("[flash] error: sector erase failed!\n");
        ret = HAL_ERR_FLASH;
        goto cleanup;
    }

cleanup:
    FLASH_Lock();
    __enable_irq(); // 恢复中断
    return ret;
}

int hal_flash_write(uint32_t address, const uint8_t *buffer, uint32_t len) {
    // 1. 极其严格的写入沙盒检查
    if (!check_address_bounds(address, len)) {
        printf("[flash] error: write 0x%08lX (len %ld) out of sandbox bounds!\n", address, len);
        return HAL_ERR_PARAM;
    }

    // 地址必须4字节对齐
    if (address % 4 != 0) {
        printf("[flash] error: write address 0x%08lX not 4-byte aligned!\n", address);
        return HAL_ERR_PARAM;
    }

    // 避免触发空指针导致的 HardFault
    // 向 memcmp 传入 NULL 指针属于未定义行为
    if (len == 0) return HAL_OK;

    int ret = HAL_OK;
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t current_adddress = address;
    uint32_t bytes = len;
    
    __disable_irq();
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_PGAERR | 
                    FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR);

    //2. 按4字节(word)写入                
    while (bytes > 0) {
        uint32_t word_data = 0xFFFFFFFF;
        uint8_t copy_len = (bytes >= 4) ? 4 : bytes;
        
        memcpy(&word_data, src, copy_len);

        if (FLASH_ProgramWord(current_adddress, word_data) != FLASH_COMPLETE) {
            ret = HAL_ERR_FLASH;
            goto cleanup;
        } 
        current_adddress += 4;
        src += copy_len;
        bytes -= copy_len;  
    }
cleanup:
    FLASH_Lock();
    __enable_irq();

    if (ret != HAL_OK) return ret;

    // 3.写入后回读校验 (Check-After-Write)
    // 只比对用户要求的有效长度，忽略 Padding
    if (memcmp((void*)address, buffer, len) != 0) {
        printf("[flash] error: verify failed at 0x%08lX!\n", address);
        return HAL_ERR_VERIFY;
    }

    return HAL_OK;
}


int hal_flash_read(uint32_t address, const uint8_t *buffer, uint32_t len) {
    // 读操作同样受限于沙盒，防止踩到野指针触发 HardFault
    if (!check_address_bounds(address, len)) {
        return HAL_ERR_PARAM;
    }

    memcpy(buffer, (const void *)address, len);

    return HAL_OK;
}
