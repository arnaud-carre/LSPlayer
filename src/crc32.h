#pragma once
#include <stdint.h>

uint32_t CrcUpdate(uint32_t crc, const uint8_t *buffer, int length);
