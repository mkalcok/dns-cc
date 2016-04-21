//
// Created by Martin Kalcok on 21/04/16.
//

#ifndef DNS_CC_CRC_H
#define DNS_CC_CRC_H
#include <stdint.h>
#include <stddef.h>

uint32_t crc32(uint32_t crc, const void *buf, size_t size);
#endif //DNS_CC_CRC_H
