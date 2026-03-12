#include "ymodem.h"
#include "crc16.h"

#define CRC16_TABLE_SIZE  256

static uint8_t crc16_inited = 0;
static uint16_t crc16_table[CRC16_TABLE_SIZE]; // 消耗 256 * 2 = 512 字节 RAM

// 内部函数：为单个字节生成 CRC16 表项
static uint16_t crc16_for_byte(uint16_t byte)
{
    uint16_t crc = byte << 8;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021; // YModem 标准多项式
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

// 动态生成 CRC16 表
void ymodem_crc16_init(void)
{
    for (uint16_t i = 0; i < CRC16_TABLE_SIZE; ++i)
    {
        crc16_table[i] = crc16_for_byte(i);
    }
    crc16_inited = 1;
}

// YModem 查表法计算 CRC16 (速度极快)
uint16_t ymodem_crc16(uint8_t *data, uint32_t len)
{
    uint16_t crc = 0x0000; // YModem 的初始值通常为 0

    // 懒加载：如果没有初始化，先建表
    if (!crc16_inited)
    {
        ymodem_crc16_init();
    }

    // 查表法核心计算逻辑
    for (uint32_t i = 0; i < len; i++)
    {
        // 将高 8 位与当前数据异或作为表索引，然后查表，再与左移后的低 8 位异或
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }

    return crc;
}