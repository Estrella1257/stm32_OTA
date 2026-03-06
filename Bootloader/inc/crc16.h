#ifndef __CRC16_H
#define __CRC16_H

#include <stdint.h>
#include <stdio.h>

void ymodem_crc16_init(void);
uint16_t YModem_CRC16(uint8_t *data, uint32_t len);


#endif