#include "ringbuffer.h"

// 初始化 (要求 capacity 必须是 2 的整数次幂，例如 256, 1024, 2048)
void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t capacity)
{
    rb->buffer = pool;
    rb->capacity = capacity;
    rb->mask = capacity - 1; // 例如 2048-1 = 2047 (二进制全1)，用于位与运算
    rb->head = 0;
    rb->tail = 0;
}

// 清空缓冲区
void ringbuffer_clear(ringbuffer_t *rb)
{
    rb->head = rb->tail = 0;
}

// 判空
bool ringbuffer_is_empty(ringbuffer_t *rb)
{
    return rb->head == rb->tail;
}

// 判满
bool ringbuffer_is_full(ringbuffer_t *rb)
{
    // 写指针的下一个位置如果是读指针，说明满了 (保留一个字节不存，用于区分空和满)
    return ((rb->head + 1) & rb->mask) == rb->tail;
}

// 获取当前已有数据长度
uint16_t ringbuffer_get_length(ringbuffer_t *rb)
{
    return (rb->head - rb->tail) & rb->mask;
}

// 获取剩余空闲长度
uint16_t ringbuffer_get_free(ringbuffer_t *rb)
{
    return rb->capacity - 1 - ringbuffer_get_length(rb);
}

// 写入单字节
bool ringbuffer_write_byte(ringbuffer_t *rb, uint8_t data)
{
    if (ringbuffer_is_full(rb)) {
        return false; // 满了，写入失败
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) & rb->mask; // 极速取模
    return true;
}

// 写入数据块 (常用于把 DMA 收到的数据直接甩进来)
uint16_t ringbuffer_write_block(ringbuffer_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t written = 0;
    while (written < len && !ringbuffer_is_full(rb)) {
        rb->buffer[rb->head] = data[written++];
        rb->head = (rb->head + 1) & rb->mask;
    }
    return written;
}

// 读取单字节 (读出后从缓冲区抹除)
bool ringbuffer_read_byte(ringbuffer_t *rb, uint8_t *data)
{
    if (ringbuffer_is_empty(rb)) {
        return false;
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) & rb->mask;
    return true;
}

// 读取数据块
uint16_t ringbuffer_read_block(ringbuffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t read_len = 0;
    while (read_len < len && !ringbuffer_is_empty(rb)) {
        data[read_len++] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) & rb->mask;
    }
    return read_len;
}

// 窥探单字节 (绝招：读取但不移动读指针 tail。用于状态机判断帧头)
// offset = 0 表示看当前要读的第一个字节，offset = 1 表示看第二个字节
bool ringbuffer_peek_byte(ringbuffer_t *rb, uint16_t offset, uint8_t *data)
{
    if (ringbuffer_get_length(rb) <= offset) {
        return false; // 数据不够长
    }
    uint16_t peek_pos = (rb->tail + offset) & rb->mask;
    *data = rb->buffer[peek_pos];
    return true;
}