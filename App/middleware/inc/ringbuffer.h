#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

// 环形缓冲区结构体 
typedef struct {
    uint8_t  *buffer;    // 指向实际存储数据的数组
    uint16_t capacity;   // 缓冲区总大小 (必须是2的幂次方)
    uint16_t mask;       // 掩码，用于极速取模运算 (capacity - 1)
    uint16_t head;       // 写指针 (生产者)
    uint16_t tail;       // 读指针 (消费者)
} ringbuffer_t;

void     ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t capacity);
void     ringbuffer_clear(ringbuffer_t *rb);

bool     ringbuffer_is_empty(ringbuffer_t *rb);
bool     ringbuffer_is_full(ringbuffer_t *rb);
uint16_t ringbuffer_get_length(ringbuffer_t *rb);
uint16_t ringbuffer_get_free(ringbuffer_t *rb);

bool     ringbuffer_write_byte(ringbuffer_t *rb, uint8_t data);
uint16_t ringbuffer_write_block(ringbuffer_t *rb, const uint8_t *data, uint16_t len);

bool     ringbuffer_read_byte(ringbuffer_t *rb, uint8_t *data);
uint16_t ringbuffer_read_block(ringbuffer_t *rb, uint8_t *data, uint16_t len);

// 魔法 API：只看不取 (为状态机解析帧头量身定制) 
bool     ringbuffer_peek_byte(ringbuffer_t *rb, uint16_t offset, uint8_t *data);

#endif 