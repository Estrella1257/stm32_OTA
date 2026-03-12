#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>
#include <stdio.h>

void crc32_init(void);
uint32_t crc32_update(uint32_t crc, const void *data, uint32_t len);

#endif