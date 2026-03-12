#ifndef SYSTEM_MEMORY_MAP_H
#define SYSTEM_MEMORY_MAP_H

// 全局 Flash 内存映射 (最高基本法) 

// Bootloader 区 (Sectors 0-1, 32KB)
#define FLASH_BASE_ADDR           (0x08000000)
#define BOOT_START_ADDR           (0x08000000)
#define BOOT_END_ADDR             (0x08007FFF)

// NVS 数据区 (Sectors 2-3, 16KB + 16KB)
#define NVS_A_START_ADDR           (0x08008000) // Sector 2
#define NVS_A_END_ADDR             (0x0800BFFF)
#define NVS_B_START_ADDR           (0x0800C000) // Sector 3
#define NVS_B_END_ADDR             (0x0800FFFF)
#define NVS_SECTOR_SIZE            (16 * 1024)     // 16KB

// App 应用区 (Sectors 4-11, 960KB)
#define APP_START_ADDR          (0x08010000)
#define APP_END_ADDR            (0x080FFFFF)

#endif // SYSTEM_MEMORY_MAP_H